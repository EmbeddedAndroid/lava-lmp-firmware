/*
 * lava-lmp firmware: usb board
 *
 * Copyright (C) 2012 Linaro Ltd
 * Author: Andy Green <andy.green@linaro.org>
 *
 * Licensed under LGPL2
 */

#include "LPC11Uxx.h"            
#include <power_api.h>
#include "lava-lmp.h"

static char json[] = {
	"{"
		"\"if\":["
			"{"
				"\"name\":\"USB-minib\","
				"\"pins\":[\"5V\"]"
			"},{"
				"\"name\":\"USB-A\""
			"}"
		"],"
		"\"io\":["
			"{"
				"\"if\":\"USB-minib\","
				"\"name\":\"DUT\","
				"\"grp\":\"0\""
			"},{"
				"\"if\":\"USB-A\","
				"\"name\":\"dev\","
				"\"grp\":\"1\""
			"},{"
				"\"if\":\"USB-minib\","
				"\"name\":\"host\","
				"\"grp\":\"1\""
			"}"
		"],"
		"\"mux\":["
			"{"
				"\"sink\":\"DUT\","
				"\"src\":[\"NULL\",\"dev\",\"host\"]"
			"}"
		"]"
	"}\x04"
};

enum rx_states {
	CMD,
	MODE,
};

void lava_lmp_usb(int c)
{
	if (c < 0)
		return;

	switch (rx_state) {
	case CMD:
		switch (c) {
		case '?':
			usb_queue_string("lava-lmp-usb 1 1.0\r\n");
			break;
		case 'M':
			rx_state = MODE;
			break;
		case 'V':
			lava_lmp_write_voltage();
			break;
		case 'j':
			usb_queue_string(json);
			break;
		}
		break;
	case MODE:  /* USB connectivity mode */
		switch (c) {
		case 'H':
			LPC_GPIO->SET[0] = 1 << 8; /* forced DUT ID low */
			LPC_GPIO->CLR[0] = 2 << 8; /* disable power to device */
			lava_lmp_actuate_relay(RL1_SET);
			lava_lmp_actuate_relay(RL2_SET);
			break;
		case 'D':
			lava_lmp_actuate_relay(RL1_CLR);
			lava_lmp_actuate_relay(RL2_CLR);
			LPC_GPIO->CLR[0] = 1 << 8;
			LPC_GPIO->SET[0] = 2 << 8; /* enable power to device */
			break;
		case 'X':
			LPC_GPIO->CLR[0] = 1 << 8;
			LPC_GPIO->CLR[0] = 2 << 8; /* disable power to device */
			lava_lmp_actuate_relay(RL1_CLR);
			lava_lmp_actuate_relay(RL2_CLR);
			break;
		}
		rx_state = CMD;
		break;
	default:
		rx_state = CMD;
		break;
	}	
}

