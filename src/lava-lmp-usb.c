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

static const char *json =
		"\","
		"\"type\":\"lmp-usb\","
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
;

enum rx_states {
	CMD,
	MODE,
};

void lava_lmp_usb(int c)
{
	static int q;

	if (c < 0) {
		q++;
		if (idle_ok && (q & 0x7fff) == 0) {
			lmp_issue_report_header("DUT.5V\",\"val\":\"");
			lava_lmp_write_voltage();
			usb_queue_string("\",\"unit\":\"mV\"}]}\x04");
		}
		return;
	}

	switch (rx_state) {
	case CMD:
		switch (c) {
		case 'M':
			rx_state = MODE;
			break;
		default:
			lmp_default_cmd(c, json);
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

