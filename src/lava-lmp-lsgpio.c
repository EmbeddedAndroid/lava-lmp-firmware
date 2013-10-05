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

/*
 * SPI mapping
 *
 * A0: (out) nCS0
 * A1: (out) SCK
 *
 * B0: (in/out) SO0
 * B1: (in/out) SO1
 * B2: (in/out) SO2
 * B3: (in/out) SO3
 *
 * A_nOE  1_27  0 = drive
 * A_DIR  1_26  0 = in, 1 = out
 * B_nOE  1_29  0 = drive
 * B_DIR  1_28  0 = in, 1 = out
 */

#define SPI_NCS (1 << 0)
#define SPI_SCK (0x82) /* b7 and b1 */

static char width = 1;
static unsigned int read_size = 0;
static char spi_mode = 0;
static int errors;

enum {
	UTF8_VIOL__4BIT = 0xff,
	UTF8_VIOL__INIT = 0xfe,
	UTF8_VIOL__CS_HILO = 0xfd,
	UTF8_VIOL__WAIT_FLASH_DONE = 0xfc,
};

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
		"\"spi\":{"
			"\"write\":\"iarray\","
			"\"read\":\"int\""
		"},"
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
			"},"
			"{"
				"\"name\":\"a-dir\","
				"\"options\":["
					"{"
						"\"name\":\"in\""
					"},{"
						"\"name\":\"out\""
					"}"
				"]"
			"},"
			"{"
				"\"name\":\"b-dir\","
				"\"options\":["
					"{"
						"\"name\":\"in\""
					"},{"
						"\"name\":\"out\""
					"}"
				"]"
			"},"
			"{"
				"\"name\":\"a-data\","
				"\"data\":\"scalar8\""
			"},"
			"{"
				"\"name\":\"b-data\","
				"\"data\":\"scalar8\""
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

enum {
	name_valid = 1 << 0,
	name_audio = 1 << 1,
	name_a_dir = 1 << 2,
	name_b_dir = 1 << 3,
	name_a_data = 1 << 4,
	name_b_data = 1 << 5,
};

static const char const *names[] = {
	"",
	"audio",
	"a-dir",
	"b-dir",
	"a-data",
	"b-data"
};


static void spi_mode_init(void)
{
	width = 1;
	lava_lmp_ls_bus_mode(0, LS_DIR_OUT);
	lava_lmp_ls_bus_mode(1, LS_DIR_OUT);
	LPC_GPIO->SET[0] = (0xc << 16) | ((SPI_SCK | SPI_NCS) << 8);
}

static void spi_mode_nselect(char level)
{
	width = 1;

	if (!level) {
		LPC_GPIO->CLR[0] = SPI_NCS << 8;
		return;
	}
	LPC_GPIO->SET[0] = (0xc << 16) | ((SPI_SCK | SPI_NCS) << 8);
	lava_lmp_ls_bus_mode(1, LS_DIR_OUT);
}

static unsigned char spi_mode_shift_in(void)
{
	unsigned char c = 0;
	int n;

	switch (width) {
	case 4:
		LPC_GPIO->CLR[0] = SPI_SCK << 8;
		c = (LPC_GPIO->PIN[0] >> 16) << 4;
		LPC_GPIO->SET[0] = SPI_SCK << 8;
		LPC_GPIO->CLR[0] = SPI_SCK << 8;
		c |= (LPC_GPIO->PIN[0] >> 16) & 0xf;
		LPC_GPIO->SET[0] = SPI_SCK << 8;
		break;
	default:
		for (n = 0; n < 8; n++) {
			/* driver changes at lowgoing edge */

			LPC_GPIO->CLR[0] = SPI_SCK << 8;
			c <<= 1;
			if (LPC_GPIO->PIN[0] & (1 << 17))
				c |= 1;
			LPC_GPIO->SET[0] = SPI_SCK << 8;

		}
		break;
	}

	return c;
}

static unsigned char esc = 0;

char lmp_json_callback_board_lsgpio(struct lejp_ctx *ctx, char reason)
{
	unsigned char n, u, j;
	unsigned char bus = 0;
	unsigned char c;
	unsigned int w;

	if (!ctx)
		return 0;

	if (reason == REASON_SEND_REPORT ||
				(read_size && reason == LEJPCB_COMPLETE)) {
		char str[64];

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
		usb_queue_string("}]");

		usb_queue_string(",\"read\":\"");

		while (read_size) {
			for (n = 0; n < (sizeof(str) - 2) && read_size;) {
				read_size--;
				c = spi_mode_shift_in();

				if (c < 0x80 && c >= 0x20 &&
						c != '\"' && c != '\\')
					str[n++] = c;
				else {
					str[n++] = 0xc0 | (c >> 6);
					str[n++] = 0x80 | (c & 0x3f);
				}
			}
			str[n] = '\0';
			usb_queue_string(str);
			LPC_GPIO->NOT[0] = 1 << 7;
		}
		usb_queue_string("\",\"err\":\"");
		dec(errors, str);
		usb_queue_string(str);
		usb_queue_string("\"}\x04");

		spi_mode_nselect(1);
		LPC_GPIO->SET[0] = 1 << 7;

		return 0;
	}

	if (reason == LEJPCB_START)
		width = 1;

	if (!(reason & LEJP_FLAG_CB_IS_VALUE) && reason != LEJPCB_VAL_STR_START)
		return 0;

	switch (ctx->path_match) {
	case LMPPT_schema:
		if (strcmp(&ctx->buf[15], "lsgpio"))
			return -1; /* fail it */
		break;
	case LMPPT_modes___name:
		for (n = 1; n < ARRAY_SIZE(names); n++)
			if (!strcmp(ctx->buf, names[n])) {
				ctx->st[ctx->sp - 1].b =
						(1 << n) | name_valid;
				return 0;
			}

		return -1; /* fail it */

	case LMPPT_modes___option:
		/* require that we had a correct modes[] name */
		if (!(ctx->st[ctx->sp - 1].b & name_valid))
			return -1;

		if (ctx->st[ctx->sp - 1].b & name_audio) {

			if (!strcmp(ctx->buf, "disconnect")) {
				lava_lmp_actuate_relay(RL1_SET);
				lava_lmp_actuate_relay(RL2_SET);
				break;
			}

			if (!strcmp(ctx->buf, "passthru")) {
				LPC_GPIO->SET[0] = 4 << 16;
				lava_lmp_actuate_relay(RL1_CLR);
				lava_lmp_actuate_relay(RL2_CLR);
				break;
			}
			return -1;
		}

		if (ctx->st[ctx->sp - 1].b &
					(name_a_dir | name_b_dir)) {

			if (ctx->st[ctx->sp - 1].b & name_b_dir)
				bus = 1;

			if (!strcmp(ctx->buf, "in")) {
				lava_lmp_ls_bus_mode(bus, 1);
				break;
			}

			if (!strcmp(ctx->buf, "out")) {
				lava_lmp_ls_bus_mode(bus, 0);
				break;
			}
			return -1;
		}

		if (ctx->st[ctx->sp - 1].b &
					(name_a_data | name_b_data)) {

			if (ctx->st[ctx->sp - 1].b & name_b_data)
				bus = 1;

			if (strcmp(ctx->buf, "data"))
				return -1;

			if (ctx->npos != 2)
				return -1;
			n = hex_char(ctx->buf[0]);
			if (n == 0x10)
				return -1;
			u = n << 4;
			n = hex_char(ctx->buf[1]);
			if (n == 0x10)
				return -1;
			lava_lmp_bus_write(bus, u | n);
		}

		lmp_json_callback_board_lsgpio(ctx, REASON_SEND_REPORT);
		break;

	case LMPPT_spi__write:

		LPC_GPIO->NOT[0] = 1 << 7;

		if (reason == LEJPCB_VAL_STR_START) {
			if (!spi_mode) {
				spi_mode_init();
				spi_mode = 1;
			}
			lava_lmp_ls_bus_mode(1, LS_DIR_OUT);
			LPC_GPIO->SET[0] = 0xe << 16;
			spi_mode_nselect(0);
			break;
		}

//		if (width == 1)
			LPC_GPIO->CLR[0] = SPI_NCS << 8;

		w = LPC_GPIO->PIN[0] & ~((0xf << 16) | (SPI_SCK << 8));

		n = 0;
		while (n < ctx->npos) {

			u = ctx->buf[n++];
			if (!(u & 0x80))
				goto write;

			switch (u & 0xe0) {
			case 0xe0:
				switch (u) {
				case UTF8_VIOL__4BIT:
					width = 4;
					continue;

				case UTF8_VIOL__INIT:
					errors = 0;
					spi_mode = 0;
					width = 1;
					continue;

				case UTF8_VIOL__CS_HILO:
					LPC_GPIO->SET[0] = SPI_NCS << 8;
					LPC_GPIO->CLR[0] = SPI_NCS << 8;
					width = 1;
					continue;

				case UTF8_VIOL__WAIT_FLASH_DONE:
					width = 1;
					lava_lmp_ls_bus_mode(1, LS_DIR_IN);
					u = 1;
					while (u & 1)
						u = spi_mode_shift_in();
					lava_lmp_ls_bus_mode(1, LS_DIR_OUT);
					if (u & (1 << 6))
						errors++;
					continue;
				}
				continue;
			case 0xc0:
				esc = u << 6;
				continue;
			case 0x80:
			case 0xa0:
				u = esc | (u & 0x3f);
				break;
			}
write:
			switch (width) {
			case 4:
				LPC_GPIO->PIN[0] = w | ((u & 0xf0) << 12);
				LPC_GPIO->SET[0] = SPI_SCK << 8;
				LPC_GPIO->PIN[0] = w | ((u & 0xf) << 16);
				LPC_GPIO->SET[0] = SPI_SCK << 8;
				break;

			default:
				for (j = 0; j < 8; j++) {
					LPC_GPIO->PIN[0] = w | ((u & 128) << 9);
					LPC_GPIO->SET[0] = SPI_SCK << 8;
					u <<= 1;
				}
				break;
			}
		}

		if (reason == LEJPCB_VAL_STR_END)
			lava_lmp_ls_bus_mode(1, LS_DIR_IN);
		break;

	case LMPPT_spi__read:
		if (reason != LEJPCB_VAL_STR_END)
			break;

		n = 0;
		if (ctx->buf[0] == 'p') {
			/* poll status */
			while (spi_mode_shift_in() & 1)
				;
			read_size = 1;
			break;
		}
		if (ctx->buf[0] == 'q' && ctx->buf[1] == ',') {
			width = 4;
			n += 2;
		}
		read_size = atoi(&ctx->buf[n]);
		if (!read_size)
			spi_mode_nselect(1);

		LPC_GPIO->SET[0] = 1 << 7;

		break;

	}
	return 0;

#if 0
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
#endif
	return 0;
}
