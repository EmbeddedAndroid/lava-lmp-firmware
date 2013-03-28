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
						"\"name\":\"disconnect\","
						"\"mux\":[{\"DUT\":null}]"
					"},{"
						"\"name\":\"uSDA\","
						"\"mux\":[{\"DUT\":\"uSDA\"}]"
					"},{"
						"\"name\":\"uSDB\","
						"\"mux\":[{\"DUT\":\"uSDB\"}]"
					"}"
				"]"
			"},{"
				"\"name\":\"host\","
				"\"options\":["
					"{"
						"\"name\":\"disconnect\","
						"\"mux\":[{\"DUT\":null}]"
					"},{"
						"\"name\":\"uSDA\","
						"\"mux\":[{\"DUT\":\"uSDA\"}]"
					"},{"
						"\"name\":\"uSDB\","
						"\"mux\":[{\"DUT\":\"uSDB\"}]"
					"}"
				"]"
			"},{"
				"\"name\":\"dut-power\","
				"\"options\":["
					"{"
						"\"name\":\"short-for-off\","
						"\"mux\":[{\"DUT\":null}]"
					"},{"
						"\"name\":\"short-for-on\","
						"\"mux\":[{\"DUT\":\"uSDA\"}]"
					"}"
				"]"
			"}"
		"]"

	"}\x04"
;

/* how to set BUS A for the various modes
 * (4 * host) + dut
 *
 * A_D0 = DUT MUX nEnable
 * A_D1 = DUT SEL SDA / SDB
 * A_D2 = HOST MUX nEnable
 * A_D3 = HOST SEL SDA / SDB
 * A_D4 = 1 = SDA_VDD <- DUT_VDD
 * A_D5 = 1 = SDA_VDD <- HOST_VDD
 * A_D6 = 1 = SDB_VDD <- DUT_VDD
 * A_D7 = 1 = SDB_VDD <- HOST_VDD
 *
 *          Power  Mux
 * DUT  HOST
 * x    x     0_   _5
 * SDA  x     1_   _4
 * SDB  x     4_   _6
 * x    SDA   2_   _1
 * x    SDB   8_   _9
 * SDA  SDB   9_   _8
 * SDB  SDA   6_   _2
 *
 * 3 * HOST + DUT lookup
 */

static const unsigned char busa_modes[] = {
	0x05, /* DUT: disconect, HOST: disconnect */
	0x14, /* DUT: SDA, HOST: disconnect */
	0x46, /* DUT: SDB, HOST: disconnect */

	0x21, /* DUT: disconnect, HOST: SDA */
	0x21, /* DUT: SDA, HOST:SDA -- illegal host wins */
	0x62, /* DUT: SDB, HOST: SDB */

	0x89, /* DUT: disconnect, HOST: SDB */
	0x98, /* DUT: SDA, HOST: SDB */
	0x89, /* DUT: SDB , HOST: SDB -- illegal host wins */
};

static unsigned char muxmode = 0;
static unsigned char dut_power_polarity = 0;
static const char const * sides[] = {
	"disconnect",
	"uSDA",
	"uSDB"
};

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
	static int q;
	static char mul;
	static char masked;
	static char update;
	static unsigned char old_muxmode;
	char str[10];
	const char SET_DUT_PWR = 99;
	const char UPDATE_MUX = 1;
	const char UPDATE_RLY = 2;

	if (!ctx) {
		/* idle processing */
		q++;
		reason = REASON_SEND_REPORT;
		if (idle_ok && (q & 0x7fff))
			return 0;
	}

	switch (reason) {
	case LEJPCB_START:
		mul = -1;
		update = 0;
		old_muxmode = muxmode;
		break;

	case LEJPCB_VAL_STR_END:
		switch (ctx->path_match) {
		case 1: /* schema */
			if (strcmp(&ctx->buf[15], "sdmux"))
				return -1; /* fail it */
			break;
		default:
			if (!strcmp(ctx->path, "modes[].name")) {

				if (!strcmp(ctx->buf, "dut")) {
					mul = 1;
					masked = muxmode - (muxmode % 3);
				} else
					if (!strcmp(ctx->buf, "host")) {
						mul = 3;
						masked = muxmode % 3;
					} else
						if (!strcmp(ctx->buf, "dut-power")) {
							mul = SET_DUT_PWR;
						} else
							/* illegal name */
							return -1;
			}
			if (!strcmp(ctx->path, "modes[].option")) {
				/* require a previous name */
				if (mul == -1)
					return -1;

				update |= UPDATE_MUX;

				if (mul == SET_DUT_PWR) {
					if (!strcmp(ctx->buf, "short-for-off")) {
						if (dut_power_polarity) {
							dut_power_polarity = 0;
							update |= UPDATE_RLY;
						}
						break;
					}
					if (!strcmp(ctx->buf, "short-for-on")) {
						if (!dut_power_polarity) {
							dut_power_polarity = 1;
							update |= UPDATE_RLY;
						}
						break;
					}
					/* illegal option */
					return -1;
				}

				if (!strcmp(ctx->buf, "disconnect")) {
					muxmode = masked;
					break;
				}
				if (!strcmp(ctx->buf, "uSDA")) {
					muxmode = masked + mul;
					break;
				}
				if (!strcmp(ctx->buf, "uSDB")) {
					muxmode = masked + (2 * mul);
					break;
				}
				/* illegal option */
				return -1;
			}
			break;
		}
		break;

	case LEJPCB_COMPLETE:

		if (update & UPDATE_RLY) {
			if (muxmode % 3) {
				if (dut_power_polarity)
					lava_lmp_actuate_relay(RL1_CLR);
				else
					lava_lmp_actuate_relay(RL1_SET);
			} else {
				if (dut_power_polarity)
					lava_lmp_actuate_relay(RL1_SET);
				else
					lava_lmp_actuate_relay(RL1_CLR);
			}
		}

		if (!(update & UPDATE_MUX))
			break;

		if ((old_muxmode / 3) != (muxmode / 3))
			/* changed host, so remove power from Host SD Reader */
			LPC_GPIO->CLR[0] = 1 << 17;

		if ((old_muxmode % 3) != (muxmode % 3)) {
			/* any change in DUT card --> disconnect CD / power */
			if (dut_power_polarity)
				lava_lmp_actuate_relay(RL1_SET);
			else
				lava_lmp_actuate_relay(RL1_CLR);
		}

		/* actually change the power and mux arrangements */
		lava_lmp_bus_write(0, busa_modes[muxmode]);

		if ((muxmode % 3) && (old_muxmode % 3) != (muxmode % 3)) {
			/* wait for disconnect to go through */
			lmp_delay(1000000);
			/* DUT has a card, connect CD / power */
			if (dut_power_polarity)
				lava_lmp_actuate_relay(RL1_CLR);
			else
				lava_lmp_actuate_relay(RL1_SET);
		}

		/* host has a new card now, power the reader... */
		if ((old_muxmode / 3) != (muxmode / 3) && (muxmode / 3)) {
			/* wait for host to handle the reader disappearing */
			lmp_delay(2000000);
			/* ...allow power to Host SD Reader */
			LPC_GPIO->SET[0] = 1 << 17;
		}

		/* fallthru */

	case REASON_SEND_REPORT:
		lmp_issue_report_header("DUT.pwr\",\"val\":\"");
		lava_lmp_write_voltage();
		usb_queue_string("\",\"unit\":\"mV\"},{\"name\":\"modes\",\"modes\":[{\"name\":\"host\",\"mode\":\"");
		usb_queue_string(sides[muxmode / 3]);
		usb_queue_string("\"},{\"name\":\"dut\",\"mode\":\"");
		usb_queue_string(sides[muxmode % 3]);
		usb_queue_string("\"},{\"name\":\"dut-power\",\"mode\":\"");
		if (dut_power_polarity)
			usb_queue_string("short-for-off");
		else
			usb_queue_string("short-for-on");
		usb_queue_string("\"}]},{\"name\":\"DUT.muxmode\",\"val\":\"");
		dec(muxmode,str);
		usb_queue_string(str);
		usb_queue_string("\"}]}\x04");
		break;
	default:
		break;
	}

	return 0;
}

