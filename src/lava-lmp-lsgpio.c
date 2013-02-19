/*
 * lava-lmp firmware: lsgpio board
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
				"\"name\":\"bus8\","
				"\"pins\":[\"0\",\"1\",\"2\",\"3\",\"4\",\"5\",\"6\",\"7\"]"
				"\"state\":[{\"name\":\"dir\",\"s\":[\"in\",\"out\"]},{\"name\":\"data\"}]"
			"},{"
				"\"name\":\"jack4\""
			"}"
		"],"
		"\"io\":["
			"{"
				"\"if\":\"bus8\","
				"\"name\":\"A\","
				"\"grp\":\"1\""
			"},{"
				"\"if\":\"bus8\","
				"\"name\":\"B\","
				"\"grp\":\"1\""
			"},{"
				"\"if\":\"jack4\","
				"\"name\":\"audio\","
				"\"grp\":\"1\""
			"},{"
				"\"if\":\"jack4\","
				"\"name\":\"DUT\","
				"\"grp\":\"0\""
			"}"
		"],"
		"\"mux\":["
			"{"
				"\"sink\":\"DUT\","
				"\"src\":[\"NULL\",\"audio\"]"
			"}"
		"]"
	"}\x04"
};

enum rx_states {
	CMD,
	MODE1,
	MODE2,
	MODE3,
	KEY_ROW,
	KEY_COL,
	KEY_UPDOWN,
	W_BUS,
	W_HEX1,
	W_HEX2,
	J_BOOL
};

static char buf[5];
static char keyscan_mode, row, col, up, bus, val;

void lava_lmp_lsgpio(int c)
{
	if (c < 0)
		return;

	switch (rx_state) {
	case CMD:
		switch (c) {
		case '?':
			usb_queue_string("lava-lmp-lsgpio 1 1.0\n");
			break;
		case 'M':
			rx_state = MODE1;
			break;
		case 'J':
			rx_state = J_BOOL;
			break;
		case 'K':
			rx_state = KEY_ROW;
			break;
		case 'W':
			rx_state = W_BUS;
			break;
		case 'R':
			c = lava_lmp_bus_read(0);
			buf[0] = hex[c >> 4];
			buf[1] = hex[c & 0xf];
			c = lava_lmp_bus_read(1);
			buf[2] = hex[c >> 4];
			buf[3] = hex[c & 0xf];
			buf[4] = '\0';
			usb_queue_string(buf);
			break;
		case 'j':
			usb_queue_string(json);
			break;
		}
		break;

	case MODE1:
		lava_lmp_gpio_bus_mode(0, !!(c == 'I'));
		rx_state = MODE2;
		break;

	case MODE2:
		lava_lmp_gpio_bus_mode(1, !!(c == 'I'));
		rx_state = MODE3;
		break;

	case MODE3:
		keyscan_mode = !! (c == 'K');
		rx_state = CMD;
		break;

	case KEY_ROW:
		row = c & 7;
		rx_state = KEY_COL;
		break;

	case KEY_COL:
		col = c & 7;
		rx_state = KEY_UPDOWN;
		break;

	case KEY_UPDOWN:
		up = c & 1;
		/* todo */
		rx_state = CMD;
		break;

	case W_BUS:
		bus = c & 1;
		rx_state = W_HEX1;
		break;

	case W_HEX1:
		val = hex_char(c) << 4;
		rx_state = W_HEX2;
		break;

	case W_HEX2:
		val |= hex_char(c);
		lava_lmp_bus_write(bus, val);
		rx_state = CMD;
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

