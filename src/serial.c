/*
 * lava-lmp firmware: serial.c
 *
 * Copyright (C) 2012 Linaro Ltd
 * Author: Andy Green <andy.green@linaro.org>
 *
 * Licensed under LGPL2
 */

#include "LPC11Uxx.h"            
#include <power_api.h>
#include "lava-lmp.h"

enum rx_states {
	CMD,
	SET,
};

static unsigned char count;
static unsigned char str[32]  __attribute__ ((aligned(4)));

/*
 * Serial format: LLnn____________
 * LL = literal LL (Lava-LMP)
 * nn = hex number representing the board type
 * __... = 12 arbitrary characters sent over serial at programming time
 *
 * necessary json looks like:
 *
 * {
 * 	"schema": "org.linaro.lmp.set-serial",
 * 	"serial": "abcdefghijkl"
 * }
 */

char lmp_json_callback_set_serial(struct lejp_ctx *ctx, char reason)
{
	unsigned char n;
	volatile int j;

	switch (reason) {
	case LEJPCB_START:
		count = 0;
		str[count++] = 'L';
		str[count++] = '\0';
		str[count++] = 'L';
		str[count++] = '\0';
		str[count++] = hex[(mode >> 4) &0xf];
		str[count++] = '\0';
		str[count++] = hex[mode & 0xf];
		str[count++] = '\0';
		break;

	case LEJPCB_VAL_STR_END:
		if (ctx->path_match != 2) /* "serial" */
			break;
		n = 0;
		while (ctx->buf[n] && (count < 2 * USB_SERIAL_NUMBER_CHARS)) {
			str[count++] = ctx->buf[n++];
			str[count++] = '\0';
		}
		break;

	case LEJPCB_COMPLETE:
		if (count != 2 * USB_SERIAL_NUMBER_CHARS)
			break;

		usb_queue_string("Writing...\r\n");

		for (j = 0; j < 10000; j++) ;

		NVIC_DisableIRQ(USB_IRQn);
		NVIC_DisableIRQ(ADC_IRQn);

		n = lava_lmp_eeprom(EEPROM_RESERVED_OFFSET,
						EEPROM_WRITE, str, count);

		NVIC_EnableIRQ(USB_IRQn);

		if (n)
			usb_queue_string("Write failed\r\n");
		else
			usb_queue_string("Write OK\r\n");

		for (j = 0; j < 1000000; j++) ;

		NVIC_SystemReset();
		break;
	}
	return 0;
}
