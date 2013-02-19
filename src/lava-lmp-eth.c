/*
 * lava-lmp firmware: ethernet board
 *
 * Copyright (C) 2013 Linaro Ltd
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
				"\"name\":\"RJ45\""
			"}"
		"],"
		"\"io\":["
			"{"
				"\"if\":\"RJ45\","
				"\"name\":\"DUT\","
				"\"grp\":\"0\""
			"},{"
				"\"if\":\"RJ45\","
				"\"name\":\"ext\","
				"\"grp\":\"1\""
			"}"
		"],"
		"\"mux\":["
			"{"
				"\"sink\":\"DUT\","
				"\"src\":[\"NULL\",\"ext\"]"
			"}"
		"]"
	"}\x04"
};

enum rx_states {
	CMD,
	J_BOOL
};


void lava_lmp_eth(int c)
{
	if (c < 0)
		return;

	switch (rx_state) {
	case CMD:
		switch (c) {
		case '?':
			usb_queue_string("lava-lmp-eth 1 1.0\n");
			break;
		case 'J':
			rx_state = J_BOOL;
			break;
		case 'j':
			usb_queue_string(json);
			break;
		}
		break;

	case J_BOOL:
		if (c & 1) {
			lava_lmp_actuate_relay(RL1_CLR);
			lava_lmp_actuate_relay(RL2_CLR);
		} else {
			lava_lmp_actuate_relay(RL1_SET);
			lava_lmp_actuate_relay(RL2_SET);
		}
		rx_state = CMD;
		break;

	default:
		rx_state = CMD;
		break;
	}
}

