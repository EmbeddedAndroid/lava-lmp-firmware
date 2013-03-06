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

const char * const json_info_sdmux =
		","
		"\"type\":\"sdmux\","
		"\"if\":["
			"{"
				"\"name\":\"SD\","
				"\"pins\":[\"pwr\"]"
			"},{"
				"\"name\":\"uSD\""
			"}"
		"],"
		"\"io\":["
			"{"
				"\"if\":\"SD\","
				"\"name\":\"DUT\","
				"\"grp\":\"0\""
			"},{"
				"\"if\":\"SD\","
				"\"name\":\"HOST\","
				"\"grp\":\"1\""
			"}"
		"],"
		"\"int\":["
			"{"
				"\"if\":\"uSD\","
				"\"name\":\"uSDA\""
			"},{"
				"\"if\":\"uSD\","
				"\"name\":\"uSDB\""
			"}"
		"],"
		"\"mux\":["
			"{"
				"\"sink\":\"DUT\","
				"\"src\":[\"NULL\",\"uSDA\",\"uSDB\"]"
			"},{"
				"\"sink\":\"HOST\","
				"\"src\":[\"NULL\",\"uSDA\",\"uSDB\"]"
			"}"
		"],"
		"\"modes\":["
			"{"
				"\"name\":\"dut\","
				"\"options\":["
					"{"
						"\"name\":\"uSDA\","
						"\"mux\":[{\"DUT\":\"uSDA\"}]"
					"},{"
						"\"name\":\"uSDB\","
						"\"mux\":[{\"DUT\":\"uSDB\"}]"
					"},{"
						"\"name\":\"disconnect\","
						"\"mux\":[{\"DUT\":null}]"
					"}"
				"]"
			"},{"
				"\"name\":\"host\","
				"\"options\":["
					"{"
						"\"name\":\"uSDA\","
						"\"mux\":[{\"DUT\":\"uSDA\"}]"
					"},{"
						"\"name\":\"uSDB\","
						"\"mux\":[{\"DUT\":\"uSDB\"}]"
					"},{"
						"\"name\":\"disconnect\","
						"\"mux\":[{\"DUT\":null}]"
					"}"
				"]"
			"}"
		"]"

	"}\x04"
;

/* how to set BUS A for the various modes
 * (4 * host) + dut
 */

static const unsigned char busa_modes[] = {
	0x05, /* 0: all NC */
	0x24, /* 2: SDA to DUT */
	0x46, /* 4: SDB to DUT */
	0x05, /* illegal */

	0x21, /* 1: SDA to HOST, NC to DUT */
	0x05, /* illegal, SDA to both */
	0x52, /* 5: SDA to HOST, SDB to DUT */
	0x05, /* illegal */

	0x89, /* 3: SDB to HOST */
	0xa8, /* 6: SDB to HOST, SDA to DUT */
	0x05, /* illegal, SDB to both */
	0x05, /* illegal */
};

static unsigned char muxmode = 0;
/*
 * json:
 * 	{
 * 		"schema": "org.linaro.lmp.sdmux",
 * 		"mode": {"DUT": null|"uSDA"|"uSDB",
 * 			 "HOST": null|"uSDA"|"uSDB" }
 * 	}
 */

char lmp_json_callback_board_sdmux(struct lejp_ctx *ctx, char reason)
{
	unsigned char n;
	static int q;

	if (reason == REASON_SEND_REPORT) {
		lmp_issue_report_header("DUT.pwr\",\"val\":\"");
		lava_lmp_write_voltage();
		usb_queue_string("\",\"unit\":\"mV\"}]}\x04");
		return 0;
	}

	if (ctx) {
		switch (ctx->path_match) {
		case 1: /* schema */
			if (strcmp(&ctx->buf[15], "sdmux"))
				return -1; /* fail it */
			break;
		case 4: /* mode */
			if (!(reason & LEJP_FLAG_CB_IS_VALUE))
				break;

			n = 2;
			if (!strcmp(&ctx->path[ctx->path_match_len], "DUT")) {
				n = 0;
				muxmode &= 3 << 2;
			}
			if (!strcmp(&ctx->path[ctx->path_match_len], "HOST")) {
				n = 1;
				muxmode &= 3;
			}
			if (n == 2)
				return -1; /* fail it */

			if (reason == LEJPCB_VAL_NULL)
				goto okay;

			if (!strcmp(ctx->buf, "uSDA")) {
				muxmode |= 1 << (2 * n);
				goto okay;
			}
			if (!strcmp(ctx->buf, "uSDB")) {
				muxmode |= 2 << (2 * n);
				goto okay;
			}
			return -1;
okay:
			/* power the host or not */
			if (mode >> 2)
				LPC_GPIO->SET[0] = 1 << 17;
			else
				LPC_GPIO->CLR[0] = 1 << 17;
			lava_lmp_actuate_relay(RL1_CLR);
			{ volatile int n = 0; while (n < 1000000) n++; }
			lava_lmp_bus_write(0, busa_modes[muxmode]);
			lava_lmp_actuate_relay(RL1_SET);
			{ volatile int n = 0; while (n < 1000000) n++; }

			lmp_json_callback_board_sdmux(ctx, REASON_SEND_REPORT);

			break;
		}
		return 0;
	}

	 /* idle processing */
	q++;
	if (idle_ok && (q & 0x7fff) == 0)
		lmp_json_callback_board_sdmux(ctx, REASON_SEND_REPORT);

	return 0;
}

