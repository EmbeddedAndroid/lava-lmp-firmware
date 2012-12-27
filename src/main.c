/*
 * lava-lmp firmware: init
 *
 * This originally came from the NXP example code for CDC ACM
 * http://www.lpcware.com/content/forum/usb-rom-driver-examples-using-lpcxpresso-lpc11uxx#comment-2526
 *
 * I stripped it down and modified it considerably.  My changes are -->
 *
 * Copyright (C) 2012 Linaro Ltd
 * Author: Andy Green <andy.green@linaro.org>
 *
 * Licensed under LGPL2
 *
 * The original copyright notice is retained below
 */


/***********************************************************************
*   Copyright(C) 2011, NXP Semiconductor
*   All rights reserved.
*
* Software that is described herein is for illustrative purposes only
* which provides customers with programming information regarding the
* products. This software is supplied "AS IS" without any warranties.
* NXP Semiconductors assumes no responsibility or liability for the
* use of the software, conveys no license or title under any patent,
* copyright, or mask work right to the product. NXP Semiconductors
* reserves the right to make changes in the software without
* notification. NXP Semiconductors also make no representation or
* warranty that such application will be suitable for the specified
* use without further testing or modification.
**********************************************************************/
#include <string.h>
#include "LPC11Uxx.h"            
#include <power_api.h>
#include "mw_usbd_rom_api.h"
#include "lava-lmp.h"

extern uint8_t VCOM_DeviceDescriptor[];
extern uint8_t VCOM_StringDescriptor[];
extern uint8_t VCOM_ConfigDescriptor[];

USBD_API_T *usbapi;

struct vcom_data {
	USBD_HANDLE_T hUsb;
	USBD_HANDLE_T hCdc;
	uint8_t *rxBuf;
	uint8_t *txBuf;
	volatile uint16_t rxlen;
	volatile uint16_t txlen;
	volatile uint32_t sof_counter;
	volatile uint16_t usbrx_pend;
	volatile uint16_t usbtx_pend;
}; 

struct vcom_data vcom;

void USB_pin_clk_init(void)
{
	/* Enable AHB clock to the GPIO domain. */
	LPC_SYSCON->SYSAHBCLKCTRL |= 1 << 6;

	/* Enable AHB clock to the USB block and USB RAM. */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 14) | (1 << 27);

	/* Pull-down is needed, or internally, VBUS will be floating. This is to
	address the wrong status in VBUSDebouncing bit in CmdStatus register. It
	happens on the NXP Validation Board only
	that a wrong ESD protection chip is used. */

	LPC_IOCON->PIO0_3 &= ~0x1F; 
//	LPC_IOCON->PIO0_3 |= (1 << 3) | (1 << 0); /* Secondary function VBUS */
	LPC_IOCON->PIO0_3 |= 1 << 0; /* Secondary function VBUS */
	LPC_IOCON->PIO0_6 &= ~7;
	LPC_IOCON->PIO0_6 |= 1 << 0; /* Secondary function SoftConn */
}

/*
 * ms counter
 */

ErrorCode_t VCOM_sof_event(USBD_HANDLE_T hUsb)
{
	vcom.sof_counter++;

	lava_lmp_1ms_tick();

	return LPC_OK;
}

ErrorCode_t VCOM_SendBreak(USBD_HANDLE_T hCDC, uint16_t mstime)
{
	return LPC_OK;
}

void usb_queue_tx(const unsigned char *buf, int len)
{
	while (vcom.txlen)
		;

	NVIC_DisableIRQ(USB_IRQn);

	memcpy(vcom.txBuf, buf, len);
	vcom.txlen = len;

	/* not expecting any "in" IRQ to send anything, do it ourselves */
	if (!vcom.usbtx_pend) {
		vcom.txlen -= usbapi->hw->WriteEP(vcom.hUsb,
					  USB_CDC_EP_BULK_IN, vcom.txBuf, len);
		vcom.usbtx_pend = 1;
	}

	/* a pending "in" IRQ should happen soon and send buffered stuff */

	NVIC_EnableIRQ(USB_IRQn);
}

void usb_queue_string(const char *buf)
{
	usb_queue_tx((unsigned char *)buf, strlen(buf));
}

/*
 * we just sent something back to host, if there's more waiting send it now
 */

ErrorCode_t VCOM_in(USBD_HANDLE_T hUsb, void *data, uint32_t event) 
{
	struct vcom_data *vcom = data;

	if (event != USB_EVT_IN)
		return LPC_OK;

	if (!vcom->txlen) {
		vcom->usbtx_pend = 0;
		return LPC_OK;
	}

	vcom->usbtx_pend = 1;
	vcom->txlen -= usbapi->hw->WriteEP(vcom->hUsb, USB_CDC_EP_BULK_IN,
						      vcom->txBuf, vcom->txlen);
	return LPC_OK;
}

/*
 * something has arrived from the host
 */

ErrorCode_t VCOM_out(USBD_HANDLE_T hUsb, void *data, uint32_t event) 
{
	struct vcom_data *vcom = data;

	if (event != USB_EVT_OUT)
		return LPC_OK;

	if (vcom->rxlen || vcom->usbrx_pend) {
		/*
		 * can't cope with it right now, foreground will get it later
		 */
		vcom->usbrx_pend = 1;
		return LPC_OK;
	}

	vcom->rxlen = usbapi->hw->ReadEP(
				       hUsb, USB_CDC_EP_BULK_OUT, vcom->rxBuf);

	return LPC_OK;
}

void USB_IRQHandler(void)
{
	usbapi->hw->ISR(vcom.hUsb);
}

/*****************************************************************************
**   Main Function  main()
*****************************************************************************/
int main(void)
{
	USBD_API_INIT_PARAM_T usb_param;
	USBD_CDC_INIT_PARAM_T cdc_param;
	USB_CORE_DESCS_T desc;
	USBD_HANDLE_T hUsb, hCdc;
	ErrorCode_t ret = LPC_OK;
	uint32_t ep_indx;
	unsigned long usb_mem_end = 0x10001800;
	int pos;
	unsigned char c;

	SystemInit();
	SystemCoreClockUpdate();

	lava_lmp_pin_init();

	/* get USB API table pointer from magic ROM table address */
	usbapi = (USBD_API_T *)((*(ROM **)(0x1FFF1FF8))->pUSBD);

	/* enable clocks and pinmux for usb0 */
	USB_pin_clk_init();

	/* initilize call back structures */
	memset(&usb_param, 0, sizeof(USBD_API_INIT_PARAM_T));
	usb_param.usb_reg_base = LPC_USB_BASE;
	usb_mem_end -= 0x1000;
	usb_param.mem_base = usb_mem_end;
	usb_param.mem_size = 0x1000;
	usb_param.max_num_ep = 3;

	/* init CDC params */
	memset(&cdc_param, 0, sizeof(USBD_CDC_INIT_PARAM_T));
	memset(&vcom, 0, sizeof(struct vcom_data));

	cdc_param.SendBreak = VCOM_SendBreak;

	/* Initialize Descriptor pointers */
	memset(&desc, 0, sizeof(USB_CORE_DESCS_T));
	desc.device_desc = (uint8_t *)&VCOM_DeviceDescriptor[0];
	desc.string_desc = (uint8_t *)&VCOM_StringDescriptor[0];
	desc.full_speed_desc = (uint8_t *)&VCOM_ConfigDescriptor[0];
	desc.high_speed_desc = (uint8_t *)&VCOM_ConfigDescriptor[0];

	usb_param.USB_SOF_Event = VCOM_sof_event;

	/* USB Initialization */
	ret = usbapi->hw->Init(&hUsb, &desc, &usb_param);  
	if (ret != LPC_OK)
		return 1;

	/* init CDC params */

	usb_mem_end -= 0x300;
	cdc_param.mem_base = usb_mem_end;
	cdc_param.mem_size = 0x300;

	cdc_param.cif_intf_desc = (uint8_t *)&VCOM_ConfigDescriptor[
						   USB_CONFIGUARTION_DESC_SIZE];
	cdc_param.dif_intf_desc = (uint8_t *)&VCOM_ConfigDescriptor[
			USB_CONFIGUARTION_DESC_SIZE + USB_INTERFACE_DESC_SIZE +
			0x0013 + USB_ENDPOINT_DESC_SIZE];

	ret = usbapi->cdc->init(hUsb, &cdc_param, &hCdc);

	if (ret != LPC_OK)
		return 1;

	/* store USB handle */
	vcom.hUsb = hUsb;
	vcom.hCdc = hCdc;

	/* allocate transfer buffers */
	vcom.rxBuf = (uint8_t *)cdc_param.mem_base;
	vcom.txBuf = (uint8_t *)cdc_param.mem_base + USB_HS_MAX_BULK_PACKET;
	cdc_param.mem_size -= 4 * USB_HS_MAX_BULK_PACKET;

	/* register endpoint interrupt handler */
	ep_indx = ((USB_CDC_EP_BULK_IN & 0xf) << 1) + 1;
	ret = usbapi->core->RegisterEpHandler(hUsb, ep_indx, VCOM_in, &vcom);
	if (ret != LPC_OK)
		return 1;

	/* register endpoint interrupt handler */
	ep_indx = (USB_CDC_EP_BULK_OUT & 0xf) << 1;
	ret = usbapi->core->RegisterEpHandler(hUsb, ep_indx, VCOM_out, &vcom);
	if (ret != LPC_OK)
		return 1;

	/* so we can initially transmit OK */
	vcom.usbtx_pend = 0;
 
	/* enable USB0 IRQ */
	NVIC_EnableIRQ(USB_IRQn);

	/* USB Connect */
	usbapi->hw->Connect(hUsb, 1);

	/* foreground code feeds board-specific state machine */

	while (1) {
		if (!vcom.rxlen) {
			if (vcom.usbrx_pend) {
				vcom.rxlen = usbapi->hw->ReadEP(
					hUsb, USB_CDC_EP_BULK_OUT, vcom.rxBuf);
				pos = 0;
				vcom.usbrx_pend = 0;
			}
			continue;
		}
		c = vcom.rxBuf[pos++];
		if (pos == vcom.rxlen)
			vcom.rxlen = 0;

		lava_lmp_rx(c);
	}
}

void _exit(int a)
{
	while (1)
		;
}

