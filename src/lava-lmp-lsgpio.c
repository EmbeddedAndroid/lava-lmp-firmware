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

const char * const json_info_lsgpio =
		","
		"\"type\":\"lsgpio\","
		"\"if\":["
			"{"
				"\"name\":\"bus8\","
				"\"pins\":[\"0\",\"1\",\"2\",\"3\",\"4\",\"5\",\"6\",\"7\"],"
				"\"setting\":[{\"name\":\"dir\",\"s\":[\"in\",\"out\"]},{\"name\":\"data\"}]"
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
				"\"src\":[null,\"audio\"]"
			"}"
		"],"
		"\"modes\":["
			"{"
				"\"name\":\"audio\","
				"\"options\":["
					"{"
						"\"name\":\"passthru\","
						"\"mux\":[{\"DUT\":\"audio\"}]"
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
 * 		"schema": "org.linaro.lmp.lsgpio",
 * 		"mode": { "bus": 0|1, "type": "output"|"input" },
 * 		"write": { "bus": 0|1, "data": "XX" },
 * 		"jack": "on" | "off",
 * 	}
 */

char lmp_json_callback_board_lsgpio(struct lejp_ctx *ctx, char reason)
{
	static char bus;
	unsigned char n;
	unsigned char u;

	if (!ctx)
		return 0;
	if (reason == REASON_SEND_REPORT) {
		char str[10];
		char n;

		lmp_issue_report_header("lsgpio\",\"bus\":[");
		for (n = 0; n < 2; n++) {
			usb_queue_string("{\"type\":\"");
			if (lava_lmp_get_bus_mode(n))
				usb_queue_string("output");
			else
				usb_queue_string("input");
			usb_queue_string("\",\"data\":\"");
			hex2(lava_lmp_bus_read(n), str);
			usb_queue_string(str);
			usb_queue_string("\"}");
			if (n == 0)
				usb_queue_string(",");
		}
		usb_queue_string("]},{\"name\":\"jack\",\"state\":");
		usb_queue_true_or_false(LPC_GPIO->PIN[0] & 1 << 7);
		usb_queue_string("}]}\x04");
		return 0;
	}

	if (!(reason & LEJP_FLAG_CB_IS_VALUE)) {
		switch (ctx->path_match) {
		case LMPPT_schema:
			if (strcmp(&ctx->buf[15], "lsgpio"))
				return -1; /* fail it */
			return 0;
		case 4: /* mode */
			if (!strcmp(&ctx->path[ctx->path_match_len], "bus"))
				bus = ctx->buf[0] - '0';
			if (!strcmp(&ctx->path[ctx->path_match_len], "type")) {
				if (bus < 0) /* need to have the bus first */
					return -1;
				if (!strcmp(ctx->buf, "output"))
					lava_lmp_ls_bus_mode(bus, 0);
				if (!strcmp(ctx->buf, "input"))
					lava_lmp_ls_bus_mode(bus, 1);
				lmp_json_callback_board_lsgpio(ctx,
							REASON_SEND_REPORT);
				return 0;
			}

			return 0;
		case 5: /* write */
			if (!strcmp(&ctx->path[ctx->path_match_len], "bus")) {
				bus = ctx->buf[0] - '0';
				return 0;
			}
			if (!strcmp(&ctx->path[ctx->path_match_len], "write")) {
				if (bus < 0) /* need to have the bus first */
					return -1;
				if (ctx->npos != 2)
					return -1;
				n = hex_char(ctx->buf[0]);
				if (n == 0x10)
					return -1;
				u = n << 4;
				n = hex_char(ctx->buf[0]);
				if (n == 0x10)
					return -1;
				u |= n;
				lava_lmp_bus_write(bus, u);
				lmp_json_callback_board_lsgpio(ctx,
							REASON_SEND_REPORT);

				return 0;
			}

			return 0;

		case 6: /* jack */
			if (ctx->buf[0] & 1) {
				lava_lmp_actuate_relay(RL1_CLR);
				lava_lmp_actuate_relay(RL2_CLR);
				LPC_GPIO->CLR[0] = 1 << 7;
			} else {
				lava_lmp_actuate_relay(RL1_SET);
				lava_lmp_actuate_relay(RL2_SET);
				LPC_GPIO->SET[0] = 1 << 7;
			}
			lmp_json_callback_board_lsgpio(ctx,
						REASON_SEND_REPORT);

			return 0;
		}
	}

	switch (reason) {
	case LEJPCB_START:
		bus = 0; /* ie, not told */
		break;
	case LEJPCB_COMPLETE:
		break;
	}

	return 0;
}
