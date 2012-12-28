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

static int count;
static unsigned char str[32]  __attribute__ ((aligned(4)));

/*
 * Serial format: LLnn____________
 * LL = literal LL (Lava-LMP)
 * nn = hex number representing the board type
 * __... = 12 arbitrary characters sent over serial at programming time
 */

void lava_lmp_serial_write(unsigned char c)
{
	volatile int n;
	int r;

	switch (rx_state) {
	case CMD:
		switch (c) {
		case '?':
			usb_queue_string("lava-lmp-SET_SERIAL 1 1.0\r\n");
			break;
		case 'S':
			rx_state = SET;
			count = 0;
			str[count++] = 'L';
			str[count++] = '\0';
			str[count++] = 'L';
			str[count++] = '\0';
			str[count++] = hex[(mode >> 4) &0xf];
			str[count++] = '\0';
			str[count++] = hex[mode & 0xf];
			str[count++] = '\0';

			usb_queue_string("Waiting for 12 chars...\r\n");
			break;
		}
		break;
	case SET:
		str[count++] = c;
		str[count++] = '\0';
		if (count != 2 * USB_SERIAL_NUMBER_CHARS)
			break;

		usb_queue_string("Writing...\r\n");

		for (n = 0; n < 10000; n++) ;

		NVIC_DisableIRQ(USB_IRQn);
		NVIC_DisableIRQ(ADC_IRQn);

		r = lava_lmp_eeprom(EEPROM_RESERVED_OFFSET, EEPROM_WRITE, str, count);

		NVIC_EnableIRQ(USB_IRQn);

		if (r)
			usb_queue_string("Write failed\r\n");
		else
			usb_queue_string("Write OK\r\n");

		for (n = 0; n < 1000000; n++) ;

		NVIC_SystemReset();

		rx_state = CMD;
		break;
	default:
		rx_state = CMD;
		break;
	}
}

