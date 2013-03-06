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

const char *json_info_eth =
		","
		"\"type\":\"eth\","
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
				"\"src\":[null,\"ext\"]"
			"}"
		"],"
		"\"modes\":["
			"{"
				"\"name\":\"eth\","
				"\"options\":["
					"{"
						"\"name\":\"passthru\","
						"\"mux\":[{\"DUT\":\"ext\"}]"
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
 * 		"schema": "org.linaro.lmp.eth",
 * 		"mode: true | false
 * 	}
 */

char lmp_json_callback_board_eth(struct lejp_ctx *ctx, char reason)
{
	if (!ctx)
		return 0;

	if (reason == REASON_SEND_REPORT) {
		lmp_issue_report_header("modes\",\"modes\":[{\"name\":\"eth\",\"mode\":\"");
		if (LPC_GPIO->PIN[0] & 1 << 7)
			usb_queue_string("disconnect");
		else
			usb_queue_string("passthru");
		usb_queue_string("\"}]}]}\x04");
		return 0;
	}

	switch (ctx->path_match) {
	case LMPPT_schema:
		if (strcmp(&ctx->buf[15], "eth"))
			return -1; /* fail it */
		break;
	case LMPPT_modes___name:
		if (!strcmp(ctx->buf, "eth"))
			return -1; /* fail it */
		ctx->st[ctx->sp - 1].b |= 1;
		break;

	case LMPPT_modes___option:
		/* require that we had a correct modes[] name */
		if (ctx->st[ctx->sp - 1].b != 1)
			return -1;
		if (!strcmp(ctx->buf, "passthru")) {
			lava_lmp_actuate_relay(RL1_CLR);
			lava_lmp_actuate_relay(RL2_CLR);
			LPC_GPIO->CLR[0] = 1 << 7;
		}
		if (!strcmp(ctx->buf, "disconnect")) {
			lava_lmp_actuate_relay(RL1_SET);
			lava_lmp_actuate_relay(RL2_SET);
			LPC_GPIO->SET[0] = 1 << 7;
		}
		lmp_json_callback_board_eth(ctx, REASON_SEND_REPORT);
		break;
	}
	return 0;
}
