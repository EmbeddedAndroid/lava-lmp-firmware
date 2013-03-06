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

const char * const json_info_usb =
		","
		"\"type\":\"usb\","
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
		"],"
		"\"modes\":["
			"{"
				"\"name\":\"usb\","
				"\"options\":["
					"{"
						"\"name\":\"device\","
						"\"mux\":[{\"DUT\":\"dev\"}]"
					"},{"
						"\"name\":\"host\","
						"\"mux\":[{\"DUT\":\"host\"}]"
					"},{"
						"\"name\":\"disconnect\","
						"\"mux\":[{\"DUT\":null}]"
					"}"
				"]"
			"}"
		"]"
	"}\x04"
;

/*
 * json:
 * 	{
 * 		"schema": "org.linaro.lmp.hdmi",
 * 		"mode: "off" ( | "on" | "fake" }
 * 	}
 */

char lmp_json_callback_board_usb(struct lejp_ctx *ctx, char reason)
{
	static int q;

	if (reason == REASON_SEND_REPORT) {
		lmp_issue_report_header("DUT.5V\",\"val\":\"");
		lava_lmp_write_voltage();
		usb_queue_string("\",\"unit\":\"mV\"}]}\x04");
		return 0;
	}

	if (ctx) {
		switch (ctx->path_match) {
		case 1: /* schema */
			if (strcmp(&ctx->buf[15], "usb"))
				return -1; /* fail it */
			break;
		case 4: /* mode */
			if (!(reason & LEJP_FLAG_CB_IS_VALUE))
				break;

			if (!strcmp(ctx->buf, "off")) {
				LPC_GPIO->CLR[0] = 1 << 8;
				/* disable power to device */
				LPC_GPIO->CLR[0] = 2 << 8;
				lava_lmp_actuate_relay(RL1_CLR);
				lava_lmp_actuate_relay(RL2_CLR);
			}
			if (!strcmp(ctx->buf, "host")) {
				/* forced DUT ID low */
				LPC_GPIO->SET[0] = 1 << 8;
				/* disable power to device */
				LPC_GPIO->CLR[0] = 2 << 8;
				lava_lmp_actuate_relay(RL1_SET);
				lava_lmp_actuate_relay(RL2_SET);
				break;
			}
			if (!strcmp(ctx->buf, "device")) {
				lava_lmp_actuate_relay(RL1_CLR);
				lava_lmp_actuate_relay(RL2_CLR);
				LPC_GPIO->CLR[0] = 1 << 8;
				/* enable power to device */
				LPC_GPIO->SET[0] = 2 << 8;
			}
			lmp_json_callback_board_usb(ctx, REASON_SEND_REPORT);
			break;
		}
		return 0;
	}

	 /* idle processing */

	q++;
	if (idle_ok && (q & 0x7fff) == 0)
		lmp_json_callback_board_usb(ctx, REASON_SEND_REPORT);

	return 0;
}
