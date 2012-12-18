/*
 * lava-lmp firmware: sdmux board
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
	BOOL_P,
	BOOL_T,
	BOOL_D,
	MODE_M,
	MS_I,
	COUNT_C
};

/* how to set BUS A for the various modes */

static unsigned char busa_modes[] = {
	0x05, /* all NC */
	0x11, /* SDA to HOST */
	0x14, /* SDA to DUT */
	0x89, /* SDB to HOST */
	0x46, /* SDB to DUT */
	0x62, /* SDA to HOST, SDB to DUT */
	0x98, /* SDB to HOST, SDA to DUT */
	0x05, /* all NC */
};

static unsigned int count, i, dynamic_switching;

void lava_lmp_sdmux(unsigned char c)
{
	switch (rx_state) {
	case CMD:
		switch (c) {
		case '?':
			usb_queue_string("lava-lmp-sdmux 1 1.0\n");
			break;
		case 'P':
			rx_state = BOOL_P;
			break;
		case 'T':
			rx_state = BOOL_T;
			break;
		case 'D':
			rx_state = BOOL_D;
			break;
		case 'M':
			rx_state = MODE_M;
			break;
		case 'I':
			i = 0;
			rx_state = MS_I;
			break;
		case 'C':
			count = 0;
			rx_state = COUNT_C;
			break;
		}
		break;
	case BOOL_P:  /* DUT power passthru state */
		if (c & 1)
			lava_lmp_actuate_relay(RL2_SET);
		else
			lava_lmp_actuate_relay(RL2_CLR);
		rx_state = CMD;
		break;
	case BOOL_T:  /* Card detect passthru state */
		if (c & 1)
			lava_lmp_actuate_relay(RL1_SET);
		else
			lava_lmp_actuate_relay(RL1_CLR);
		rx_state = CMD;
		break;

	case BOOL_D:
		dynamic_switching = c & 1;
		rx_state = CMD;
		break;
	case MODE_M:
		lava_lmp_actuate_relay(RL1_CLR);
		lava_lmp_bus_write(0, busa_modes[c & 7]);
		lava_lmp_actuate_relay(RL1_SET);
		rx_state = CMD;
		break;
	case MS_I:
		if ((c >= '0') && (c <= '9')) {
			i = i * 10;
			i |= c & 0xf;
			break;
		}
		rx_state = CMD;
		break;
	case COUNT_C:
		if ((c >= '0') && (c <= '9')) {
			count = count * 10;
			count |= c & 0xf;
			break;
		}
		rx_state = CMD;
		break;
	default:
		rx_state = CMD;
		break;
	}	
}

