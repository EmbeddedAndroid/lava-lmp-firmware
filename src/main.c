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

extern char __top_RamFull, __top_RamUsb2;
#define CDC_SIZE 0x200
#define USB_SIZE 0x1000

static USBD_API_T *usbapi;

/* this state and buffers go into the middle of the "USB RAM" */

struct vcom_data {
	USBD_HANDLE_T hUsb;
	USBD_HANDLE_T hCdc;
	unsigned char rxBuf[USB_HS_MAX_BULK_PACKET];
	unsigned char txBuf[USB_HS_MAX_BULK_PACKET];
	volatile uint8_t rxlen;
	volatile uint8_t txlen;
	volatile uint32_t sof_counter;
	volatile uint8_t pend;
};
#define vcom ((struct vcom_data *) \
			(& __top_RamUsb2 - CDC_SIZE - sizeof(struct vcom_data)))

static ErrorCode_t VCOM_sof_event(USBD_HANDLE_T hUsb);
static USBD_API_INIT_PARAM_T usb_param = {
	.usb_reg_base = LPC_USB_BASE,
	.mem_base = (long)(&__top_RamFull - USB_SIZE),
	.mem_size = USB_SIZE,
	.max_num_ep = 3,
	.USB_SOF_Event = VCOM_sof_event,
};

static USB_CORE_DESCS_T desc = {
	.device_desc = VCOM_DeviceDescriptor,
	.string_desc = VCOM_StringDescriptor,
	.full_speed_desc = VCOM_ConfigDescriptor,
	.high_speed_desc = VCOM_ConfigDescriptor,
};

/* end of usb ram for cdc http://knowledgebase.nxp.com/showthread.php?t=3444 */

static USBD_CDC_INIT_PARAM_T cdc_param = {
	.mem_base = (long)(& __top_RamUsb2  - CDC_SIZE),
	.mem_size = CDC_SIZE,
	.cif_intf_desc = &VCOM_ConfigDescriptor[USB_CONFIGUARTION_DESC_SIZE],
	.dif_intf_desc = &VCOM_ConfigDescriptor[USB_CONFIGUARTION_DESC_SIZE +
		USB_INTERFACE_DESC_SIZE + 0x0013 + USB_ENDPOINT_DESC_SIZE],
};

enum {
	PEND_TX = 1,
	PEND_RX = 2,
};

static void USB_pin_clk_init(void)
{
	/* Enable AHB clock to the GPIO domain. */
	LPC_SYSCON->SYSAHBCLKCTRL |= 1 << 6;

	/* Enable AHB clock to the USB block and USB RAM. */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 14) | (1 << 27);

	LPC_IOCON->PIO0_3 = 1 << 0; /* Secondary function VBUS */
	LPC_IOCON->PIO0_6 = 1 << 0; /* Secondary function SoftConn */

	LPC_GPIO->DIR[0] |= 1 << 6;
	LPC_GPIO->CLR[0] = 1 << 6;
}

/*
 * ms counter
 */

static ErrorCode_t VCOM_sof_event(USBD_HANDLE_T hUsb)
{
	vcom->sof_counter++;

	lava_lmp_1ms_tick();

	return LPC_OK;
}

void usb_queue_tx(const unsigned char *buf, int len)
{
	while (vcom->txlen)
		;

	NVIC_DisableIRQ(USB_IRQn);

	memcpy(vcom->txBuf, buf, len);
	vcom->txlen = len;

	/* not expecting any "in" IRQ to send anything, do it ourselves */
	if (!(vcom->pend & PEND_TX)) {
		vcom->txlen -= usbapi->hw->WriteEP(vcom->hUsb,
					  USB_CDC_EP_BULK_IN, vcom->txBuf, len);
		vcom->pend |= PEND_TX;
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

static ErrorCode_t VCOM_in(USBD_HANDLE_T hUsb, void *data, uint32_t event) 
{
	if (event != USB_EVT_IN)
		return LPC_OK;

	if (!vcom->txlen) {
		vcom->pend &= ~PEND_TX;
		return LPC_OK;
	}

	vcom->pend |= PEND_TX;
	vcom->txlen -= usbapi->hw->WriteEP(hUsb, USB_CDC_EP_BULK_IN,
						     vcom->txBuf, vcom->txlen);
	return LPC_OK;
}

/*
 * something has arrived from the host
 */

static ErrorCode_t VCOM_out(USBD_HANDLE_T hUsb, void *data, uint32_t event) 
{
	if (event != USB_EVT_OUT)
		return LPC_OK;

	if (vcom->rxlen || vcom->pend & PEND_RX) {
		/* can't cope with it right now, foreground will get it later */
		vcom->pend |= PEND_RX;

		return LPC_OK;
	}

	vcom->rxlen = usbapi->hw->ReadEP(hUsb, USB_CDC_EP_BULK_OUT, vcom->rxBuf);

	return LPC_OK;
}

void USB_IRQHandler(void)
{
       usbapi->hw->ISR(vcom->hUsb);
}

int main(void)
{
	int pos = 0;
	unsigned char c;

	SystemInit();
	SystemCoreClockUpdate();
	lava_lmp_pin_init();
	USB_pin_clk_init();

	memset(vcom, 0, sizeof *vcom);

	usbapi = (USBD_API_T *)((*(ROM **)(0x1FFF1FF8))->pUSBD);

	usbapi->hw->Init(&vcom->hUsb, &desc, &usb_param);
	usbapi->cdc->init(vcom->hUsb, &cdc_param, &vcom->hCdc);
	usbapi->core->RegisterEpHandler(vcom->hUsb,
			((USB_CDC_EP_BULK_IN & 0xf) << 1) + 1, VCOM_in, vcom);
	usbapi->core->RegisterEpHandler(vcom->hUsb,
			(USB_CDC_EP_BULK_OUT & 0xf) << 1, VCOM_out, vcom);

	NVIC_EnableIRQ(USB_IRQn); /* enable USB0 IRQ */

	usbapi->hw->Connect(vcom->hUsb, 1); /* USB Connect */

	/* foreground code feeds board-specific state machine */

	while (1) {
		if (!vcom->rxlen) {
			if (!(vcom->pend & PEND_RX))
				continue;
			vcom->rxlen = usbapi->hw->ReadEP(
				vcom->hUsb, USB_CDC_EP_BULK_OUT, vcom->rxBuf);
			pos = 0;
			vcom->pend &= ~PEND_RX;
			continue;
		}
		c = vcom->rxBuf[pos++];
		if (pos >= vcom->rxlen) {
			vcom->rxlen = 0;
			pos = 0;
		}

		if (lava_lmp_rx)
			lava_lmp_rx(c);
		else
			usb_queue_string("unknown board\r\n");
	}
}

void _exit(int a)
{
	while (1)
		;
}

