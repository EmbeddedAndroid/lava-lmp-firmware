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
extern uint8_t VCOM_ConfigDescriptor[];

static USBD_API_T *usbapi;

extern void lava_lmp_serial_write(int c);

#define _(c) c, 0

/* This is the static part of the USB string descriptors */

const uint8_t VCOM_StringDescriptor[] = {

	/* Index 0x00: LANGID Codes */
	0x04,                              /* bLength */
	USB_STRING_DESCRIPTOR_TYPE,        /* bDescriptorType */
	WBVAL(0x0409), /* US English */    /* wLANGID */

	/* Index 0x01: Manufacturer */
	(10 * 2 + 2),                        /* bLength (3 Char + Type + len) */
	USB_STRING_DESCRIPTOR_TYPE,        /* bDescriptorType */
	_('L'), _('i'), _('n'), _('a'), _('r'), _('o'), _(' '),
						_('L'), _('t'), _('d'), 

	/* Index 0x02: Product */
	(7 * 2 + 2),                        /* bLength (3 Char + Type + len) */
	USB_STRING_DESCRIPTOR_TYPE,        /* bDescriptorType */
	_('L'), _('a'), _('v'), _('a'), _('L'), _('M'), _('P'), 

	/* Index 0x03: Interface 0, Alternate Setting 0 */
	(4 * 2 + 2),			/* bLength (4 Char + Type + len) */
	USB_STRING_DESCRIPTOR_TYPE,	/* bDescriptorType */\
	_('V'), _('C'), _('O'), _('M'),

	/* Index 0x04: Serial Number */
	(16 * 2 + 2),			/* bLength (16 Char + Type + len) */
	USB_STRING_DESCRIPTOR_TYPE,	/* bDescriptorType */

	/* 
	 * add 16 x 2-byte wide chars here in copied version
	 * for effective serial
	 */
};

const uint8_t VCOM_UnsetSerial[] = {
	_('U'), _('n'), _('s'), _('e'), _('t'), _(' '), _('S'), _('e'),
	_('r'), _('i'), _('a'), _('l'), _(' '), _('N'), _('u'), _('m'),
};

/* this state and buffers go into the middle of the "USB RAM" */

struct vcom_data {
	unsigned char string_descriptor[sizeof(VCOM_StringDescriptor) +
		(USB_SERIAL_NUMBER_CHARS * 2)] __attribute__ ((aligned(4)));
	USBD_HANDLE_T hUsb;
	USBD_HANDLE_T hCdc;
	unsigned char rxBuf[USB_HS_MAX_BULK_PACKET];
	unsigned char txBuf[USB_HS_MAX_BULK_PACKET];
	volatile uint8_t rxlen;
	volatile uint8_t txlen;
	volatile uint8_t pend;
	uint8_t _stuff;
};

struct vcom_data _vcom;
struct vcom_data * const vcom = &_vcom;

static USBD_API_INIT_PARAM_T usb_param = {
	.usb_reg_base = LPC_USB_BASE,
	.mem_base = 0x10001000,
	.mem_size = 0x600,
	.max_num_ep = 3,
};

static const USB_CORE_DESCS_T desc = {
	.device_desc = VCOM_DeviceDescriptor,
	.string_desc = (unsigned char *)&_vcom.string_descriptor,
	.full_speed_desc = VCOM_ConfigDescriptor,
	.high_speed_desc = VCOM_ConfigDescriptor,
};

static USBD_CDC_INIT_PARAM_T cdc_param = {
	.mem_base = 0x10001400,
	.mem_size = 0x1c0,
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
	unsigned char initial = 1;

	SystemInit();
	SystemCoreClockUpdate();
	lava_lmp_pin_init();
	USB_pin_clk_init();

	/* sthnthesize custom string descriptor using serial from EEPROM */
	memcpy(&vcom->string_descriptor, VCOM_StringDescriptor,
						  sizeof VCOM_StringDescriptor);
	lava_lmp_eeprom(EEPROM_RESERVED_OFFSET, EEPROM_READ,
			&vcom->string_descriptor[sizeof VCOM_StringDescriptor],
			USB_SERIAL_NUMBER_CHARS * 2);

	if (vcom->string_descriptor[sizeof VCOM_StringDescriptor] == 0xff)
		memcpy(&vcom->string_descriptor[sizeof VCOM_StringDescriptor],
			VCOM_UnsetSerial, sizeof(VCOM_UnsetSerial));

	usbapi = (USBD_API_T *)((*(ROM **)(0x1FFF1FF8))->pUSBD);
	if (usbapi->hw->Init(&vcom->hUsb, (USB_CORE_DESCS_T *)&desc, &usb_param))
		goto spin;
	if (usbapi->cdc->init(vcom->hUsb, &cdc_param, &vcom->hCdc))
		goto spin;
	usbapi->core->RegisterEpHandler(vcom->hUsb,
			((USB_CDC_EP_BULK_IN & 0xf) << 1) + 1, VCOM_in, vcom);
	usbapi->core->RegisterEpHandler(vcom->hUsb,
			(USB_CDC_EP_BULK_OUT & 0xf) << 1, VCOM_out, vcom);

	NVIC_EnableIRQ(USB_IRQn); /* enable USB0 IRQ */
	usbapi->hw->Connect(vcom->hUsb, 1); /* USB Connect */

	/* foreground code feeds board-specific state machine
	 * if first incoming char is '#', enter EEPROM programming state machine
	 */

	while (1) {

		if (!vcom->rxlen) {

			/* idle */
			lava_lmp_rx(-1);

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

		if (initial && c == '#')
			lava_lmp_rx = lava_lmp_serial_write;

		initial = 0;

		lava_lmp_rx(c);
	}

spin:
	while (1) {
		LPC_GPIO->SET[1] = 1 << 20;
		LPC_GPIO->CLR[1] = 1 << 20;
	}
}

void _exit(int a)
{
	while (1)
		;
}

