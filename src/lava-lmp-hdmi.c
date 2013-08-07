/*
 * lava-lmp firmware: hdmi board
 *
 * Copyright (C) 2012-2013 Linaro Ltd
 * Author: Andy Green <andy.green@linaro.org>
 *
 * Licensed under LGPL2
 */

#include "LPC11Uxx.h"            
#include <power_api.h>
#include "lava-lmp.h"


const char * const json_info_hdmi =
		","
		"\"type\":\"hdmi\","
		"\"if\":["
			"{"
				"\"name\":\"HDMI\","
				"\"pins\":[\"5V\",\"HPD\",\"EDID\"]"
			"}"
		"],"
		"\"io\":["
			"{"
				"\"if\":\"HDMI\","
				"\"name\":\"DUT\","
				"\"grp\":\"0\""
			"},{"
				"\"if\":\"HDMI\","
				"\"name\":\"MON\","
				"\"grp\":\"1\""
			"}"
		"],"
		"\"int\":["
			"{"
				"\"name\":\"fake-edid\","
				"\"setting\":\"eeprom\""
			"}"
		"],"
		"\"mux\":["
			"{"
				"\"sink\":\"MON.5V\","
				"\"src\":[null,\"DUT.5V\"]"
			"},{"
				"\"sink\":\"DUT.HPD\","
				"\"src\":[null,\"MON.HPD\"]"
			"},{"
				"\"sink\":\"DUT.EDID\","
				"\"src\":[\"MON.EDID\",\"fake-edid\"]"
			"}"
		"],"
		"\"modes\":["
			"{"
				"\"name\":\"hdmi\","
				"\"options\":["
					"{"
						"\"name\":\"fake\","
						"\"mux\":["
							"{\"MON.5V\":\"DUT.5V\"},"
							"{\"DUT.HPD\":\"MON.HPD\"},"
							"{\"DUT.EDID\":\"fake-edid\"}"
						"]"
					"},{"
						"\"name\":\"passthru\","
						"\"mux\":["
							"{\"MON.5V\":\"DUT.5V\"},"
							"{\"DUT.HPD\":\"MON.HPD\"},"
							"{\"DUT.EDID\":\"MON.EDID\"}"
						"]"
					"},{"
						"\"name\":\"disconnect\","
						"\"mux\":["
							"{\"MON.5V\":null},"
							"{\"DUT.HPD\":null},"
							"{\"DUT.EDID\":\"MON.EDID\"}"
						"]"
					"}"
				"]"
			"}"
		"]"
	"}\x04"
;

#define DEBUG_I2C 0

enum rx_states {
	CMD,
	HPD,
};

enum i2c {
	IS_IDLE,
	IS_ADS_DIR,
	IS_DATA
};

enum i2c i2c_state;

static unsigned char eeprom[256];

#ifdef DEBUG_I2C
static unsigned char dump[256];
static int dump_pos;
#endif

void dc(char c)
{
#ifdef DEBUG_I2C
	if (dump_pos < sizeof dump)
		dump[dump_pos++] = c;
#endif
}

struct reading_session {
	unsigned char ads;
	unsigned int bytes;
};

static volatile struct reading_session ring[4];
static volatile unsigned char head;
static unsigned char tail;
static unsigned char hotplug_passthru;

/* on the wire, SCL just changed state */

void NMI_Handler(void)
{
	static unsigned char newdata;
	static unsigned char bit_counter;
	static unsigned char i2c_shifter;
	static unsigned char i2c_ads;
	static unsigned char eeprom_ads;

	newdata = !LPC_GPIO->W0[10];
	
	if (newdata)
		LPC_GPIO->SET[0] = 1 << 12;
	else
		LPC_GPIO->CLR[0] = 1 << 12;

	if (LPC_GPIO->W0[8]) { /* SCL is low */
		if (!(LPC_GPIO_GROUP_INT0->CTRL & 1))
			goto bail;

		bit_counter = 0;
		if (LPC_GPIO_GROUP_INT0->PORT_POL[0] & (1 << 9)) { /* START */
			dc('S');
			i2c_state = IS_ADS_DIR;
		} else { /* STOP */
			dc('P');
//			i2c_state = IS_IDLE;
		}
		goto bail;
	}

	/* SCL has gone high */

	/* prepare detection of SDA inversion while we are high */
	LPC_GPIO_GROUP_INT0->PORT_POL[0] = newdata << 9 | 0 << 8;
	/* edge-trigger, AND combine, clear pending */
	LPC_GPIO_GROUP_INT0->CTRL = 0 << 2 | 1 << 1 | 1 << 0;

	if (i2c_state == IS_IDLE)
		goto bail;

	if (bit_counter++ != 8)
		goto bail2;

	bit_counter = 0;

	switch (i2c_state) {
	case IS_ADS_DIR:
		i2c_ads = i2c_shifter;
//		if ((i2c_ads >> 1) != 0x50)
//			break;
		if (i2c_ads & 1) { /* reading */
			eeprom_ads &= 0x80; /* !!! workaround for misread i2c address on laptop */
			ring[head].ads = eeprom_ads;
			ring[head].bytes = 0;
			head = (head + 1) & 3;
		}
		break;
	case IS_DATA:
//		if ((i2c_ads >> 1) != 0x50)
//			break;			
		if (i2c_ads & 1) { /* reading */
			eeprom[eeprom_ads++] = i2c_shifter;
			ring[(head - 1) & 3].bytes++;
		} else /* writing: set eeprom address */
			eeprom_ads = i2c_shifter;
		break;
	default:
		break;
	}

	if (newdata) { /* NAK */
		dc('N');
//		i2c_state = IS_IDLE;
	} else { /* ACK */
		dc('A');
		i2c_state = IS_DATA;
	}

bail2:
	dc('0' + newdata);
	i2c_shifter = (i2c_shifter << 1) | newdata;

bail:
	LPC_GPIO_PIN_INT->IST = 1 << 0;
}

/*
 * json:
 * 	{
 * 		"schema": "org.linaro.lmp.hdmi",
 * 		"mode": "off" ( | "on" | "fake" }
 * 	}
 */

char lmp_json_callback_board_hdmi(struct lejp_ctx *ctx, char reason)
{
	char str[10];
	unsigned char n, m;
	unsigned char cs;
	static int q;

	if (reason == REASON_SEND_REPORT) {
		lmp_issue_report_header("DUT.HPD\",\"val\":\"");
		usb_queue_string("NULL");
		usb_queue_string("\"},{\"name\":\"DUT.EDID\",\"val\":\"");
		usb_queue_string("NULL");
		usb_queue_string("\"}, {\"name\":\"DUT.5V\",\"val\":\"");
		lava_lmp_write_voltage();
		usb_queue_string("\",\"unit\":\"mV\"}]}\x04");
		return 0;
	}

	if (ctx) {
		switch (ctx->path_match) {
		case LMPPT_schema:
			if (strcmp(&ctx->buf[15], "hdmi"))
				return -1; /* fail it */
			break;
		case LMPPT_modes___name:
			if (strcmp(ctx->buf, "hdmi"))
				return -1; /* fail it */
			ctx->st[ctx->sp - 1].b |= 1;
			break;

		case LMPPT_modes___option:
			/* require that we had a correct modes[] name */
			if (ctx->st[ctx->sp - 1].b != 1)
				return -1;

			if (!strcmp(ctx->buf, "disconnect")) {
				hotplug_passthru = 0;
				LPC_GPIO->CLR[0] = 4 << 16; /* inv LSBD2 DUT HPD force to 0 */
				LPC_GPIO->CLR[0] = 8 << 16; /* stop 5V going to monitor */
				lava_lmp_actuate_relay(RL1_SET);
			}

			if (!strcmp(ctx->buf, "passthru")) {
				hotplug_passthru = 1;
				/* allow 5V to monitor, DUT HPD leave pulled to 1*/
				LPC_GPIO->SET[0] = (8 + 4) << 16;
				lava_lmp_actuate_relay(RL1_CLR);
				break;
			}
			if (!strcmp(ctx->buf, "fake")) {
				LPC_GPIO->CLR[0] = 4 << 16;
				lava_lmp_actuate_relay(RL1_CLR);
			}
			lmp_json_callback_board_hdmi(ctx, REASON_SEND_REPORT);
			break;
		}
		return 0;
	}

	if (hotplug_passthru) {
		if (LPC_GPIO->PIN[0] & (1 << (2 + 8))) /* inv LSAD2 in real hpd = 0 */
			LPC_GPIO->CLR[0] = 1 << (2 + 16); /* inv LSBD2 DUT HP force 0 */
		else /* real hpd = 1 */
			LPC_GPIO->SET[0] = 1 << (2 + 16); /* inv LSBD2 DUT HP pullup 1 */
	}

	 /* idle processing */
	q++;
	if (!(q & 0x7fff))
		return lmp_json_callback_board_hdmi(ctx, REASON_SEND_REPORT);

	if (head == tail)
		return 0;

	if (ring[tail].bytes == 0x80) {
		cs = 0;
		m = ring[tail].ads;
		for (n = 0; n < 0x80; n++)
			cs += eeprom[m++];
		lmp_issue_report_header("rxedid\",\"ads\":\"0x");
		hex4(ring[tail].ads, str);
		usb_queue_string(str);
		if (!cs)
			usb_queue_string("\",\"valid\":\"1\",\"val\":\"");
		else
			usb_queue_string("\",\"valid\":\"0\",\"val\":\"");
		hexdump(eeprom + ring[tail].ads, 0x80);
		tail = (tail + 1) & 3;
		usb_queue_string("\"]}\x04");
		return 0;
	}
	if (((tail + 1) & 3) == head)
		return 0;

/*
	usb_queue_string("\x01""abandoned-EDID-rx.hex\x02");
	hex4(ring[tail].ads, str);
	usb_queue_string(str);
	usb_queue_string(" ");
	hex4(ring[tail].bytes, str);
	usb_queue_string(str);
	usb_queue_string("\x04");
*/
	tail = (tail + 1) & 3;

	return 0;
}
