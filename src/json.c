/*
 * lava-lmp firmware: json.c
 *
 * Copyright (C) 2013 Linaro Ltd
 * Author: Andy Green <andy.green@linaro.org>
 *
 * Licensed under LGPL2
 */

#include "LPC11Uxx.h"
#include <power_api.h>
#include "lava-lmp.h"
#include <core_cm0.h>
#include <string.h>

extern char lmp_json_callback_set_serial(struct lejp_ctx *ctx, char reason);
extern const char * json_info;
extern char flash_led;

static struct lejp_ctx ctx;

const char * const schema_base = "org.linaro.lmp.";

const char * const json_report_open =
	"\x02{"
		"\"schema\":\"org.linaro.lmp.report\","
		"\"serial\":\""
;
const char *const json_board_start =
	"\x02{"
		"\"schema\":\"org.linaro.lmp.board\","
		"\"serial\":\""
;

/* note... maintain the enum in lava-lmp.h to match */
static const char * const paths[] = {
	"schema",		/* 1 */
	"serial",		/* 2 */
	"board",		/* 3 */
	"mode",			/* 4 */
	"write",		/* 5 */
	"jack",			/* 6 */
	"identify",		/* 7 */
	"modes[].name",		/* 8 */
	"modes[].option",	/* 9 */
	"edid",			/* 10 */
	"spi.write",		/* 11 */
	"spi.read",		/* 12 */
	"reset",		/* 13 */
};

void lmp_issue_report_header(const char *name)
{
	usb_queue_string(json_report_open);
	usb_queue_string(ascii_serial);
	usb_queue_string("\",\"identify\":");
	usb_queue_true_or_false(flash_led);
	usb_queue_string(",\"report\":[{\"name\":\"");
	usb_queue_string(name);
}


static char
lmp_json_callback_info(struct lejp_ctx *ctx, char reason)
{
	switch (reason) {
	case LEJPCB_COMPLETE:
		usb_queue_string(json_board_start);
		usb_queue_string(ascii_serial);
		usb_queue_string("\"");
		usb_queue_string(json_info);
		lmp_json_callback_board(ctx, REASON_SEND_REPORT);
		idle_ok = 1;
		break;
	}
	return 0;
}

/*
 * json supported by all boards
 * 
 * { "schema":"org.linaro.lmp.info" }
 * { "schema": "org.linaro.lmp.set-serial", "serial": "abcdefghijkl" }
 */

static char
lmp_json_callback(struct lejp_ctx *ctx, char reason)
{
	int j = 0;

	if (!(reason & LEJP_FLAG_CB_IS_VALUE))
		return 0;

	switch (ctx->path_match) {
	case LMPPT_schema:
		/* schema has to be in "org.linaro.lmp" namespace */
		if (strncmp(ctx->buf, schema_base, 15))
			return -1;

		/* is it something supported by all boards? */

		if (!strcmp(&ctx->buf[15], "info")) {
			lejp_change_callback(ctx, lmp_json_callback_info);
			return 0; /* handled */
		}
		if (!strcmp(&ctx->buf[15], "set-serial")) {
			lejp_change_callback(ctx, lmp_json_callback_set_serial);
			return 0; /* handled */
		}
		if (!strcmp(&ctx->buf[15], "base"))
			/* something we will deal with here */
			return 0; /* handled */

		/* schema is presumably for the board-specific stuff */
		lejp_change_callback(ctx, lmp_json_callback_board);
		return lmp_json_callback_board(ctx, reason);

	case LMPPT_identify:
		LPC_GPIO->CLR[0] = 1 << 2;
		if (reason == LEJPCB_VAL_STR_END && !strcmp(ctx->buf, "toggle"))
			flash_led ^= 1;
		else
			flash_led = ctx->buf[0] & 1;
		lmp_json_callback_board(ctx, REASON_SEND_REPORT);
		break;

	case LMPPT_reset:
		LPC_GPIO->CLR[0] = 1 << 7;

		lmp_issue_report_header("reset\",\"state\":");
		usb_queue_true_or_false(ctx->buf[0] & 1);
		usb_queue_string("}]}\x04");

		if (!strcmp(ctx->buf, "1")) {
			usb_queue_string("LMP module is resetting...");
			for (j = 0; j < 1000000; j++) ;
			NVIC_SystemReset();
		}

		LPC_GPIO->SET[0] = 1 << 7;
		break;
	}

	return 0;
}

void lmp_parse(const unsigned char *buf, int len)
{
	int n;
	static char framing;

	do {
		/* synchronize until SOT if needed */
		while (!framing && len) {
			len--;
			if (*buf++ != '\x02')
				continue;
			lejp_construct(&ctx, lmp_json_callback, NULL,
						paths, ARRAY_SIZE(paths));
			framing = 1;
		}
		if (!len)
			return;

		n = lejp_parse(&ctx, buf, len);
		if (n == LEJP_CONTINUE)
			/* more input needed */
			return;

		/* error or completion... destroy either way */
		lejp_destruct(&ctx);
		framing = 0;
		if (n >= 0) {
			buf += (len - n);
			len = n;
		}
	} while (len);
}
