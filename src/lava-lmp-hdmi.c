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

#if 0

static const unsigned char edid[] = {
/*
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x09, 0xd1, 0x01, 0x80, 0x45, 0x54, 0x00, 0x00,
	0x08, 0x15, 0x01, 0x03, 0x80, 0x35, 0x1e, 0x78, 0x2e, 0xb7, 0xd5, 0xa4, 0x56, 0x54, 0x9f, 0x27,
	0x0c, 0x50, 0x54, 0x21, 0x08, 0x00, 0x81, 0x00, 0x81, 0xc0, 0x81, 0x80, 0xa9, 0xc0, 0xb3, 0x00,
	0xd1, 0xc0, 0x81, 0xc0, 0x01, 0x01, 0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
	0x45, 0x00, 0xdd, 0x0c, 0x11, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0xff, 0x00, 0x42, 0x32, 0x42,
	0x30, 0x30, 0x36, 0x31, 0x36, 0x53, 0x4c, 0x30, 0x0a, 0x20, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x32,
	0x4c, 0x1e, 0x53, 0x11, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfc,
	0x00, 0x42, 0x65, 0x6e, 0x51, 0x20, 0x42, 0x4c, 0x32, 0x34, 0x30, 0x30, 0x0a, 0x20, 0x00, 0x0f,
*/

	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,

	0x1a, 0x8c, /* "FSL" */
	0x8a, 0x23, /* product code */
	0x01, 0x23, 0x45, 0x67, /* serial # */
	0x08, 2013 - 1990, /* week 8, 2013 */
	0x01, /* edid structure version */
	0x03, /* edid structure rev number */
	0x82, /* support HDMA-A connector style */
	0x35, /* H screen size cm */
	0x1e, /* V screen size cm */
	0xff, /* gamma - undefined */
	0x20 | (3 << 3) | (1 << 2) | (1 << 1), /* feature support */

	0xb7, 0xd5, 0xa4, 0x56, 0x54, 0x9f, 0x27, 0x0c, 0x50, 0x54, /* chromaticity */

	0x21, 0x08, 0x00, /* established timings */

	/* standard timings */
	0x81, 0x00,  0x81, 0xc0,  0x81, 0x80,  0xa9, 0xc0,
	0xb3, 0x00,  0xd1, 0xc0,  0x81, 0xc0,  0x01, 0x01,

	/* detailed timing descriptors */

	0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
	0x45, 0x00, 0xdd, 0x0c, 0x11, 0x00, 0x00, 0x1e,

	0x00, 0x00, 0x00, 0xff, 0x00, 0x42, 0x32, 0x42, 0x30, 0x30,
	0x36, 0x31, 0x36, 0x53, 0x4c, 0x30, 0x0a, 0x20,

	0x00, 0x00, 0x00, 0xfd, 0x00, 0x32, 0x4c, 0x1e, 0x53, 0x11,
	0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,

	/* product name */
	0x00, 0x00, 0x00, 0xfc, 0x00, 'T', 'r', 'i', 't', 'o',
	'n', ' ', 'E', 'V', 'B', ' ', ':', ')',

	0x00,

	0x0f,

};
#endif

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
static unsigned int write_edid;

enum {
	PKT_IDLE,

	PKT_DO_START_SETUP,
	PKT_DO_START,

	PKT_GET_BYTE,

	PKT_DO_POST_START_CLK_DOWN,
	PKT_DO_PAYLOAD_SETUP,
	PKT_DO_PAYLOAD_CLK,
	PKT_DO_POST_WAIT,

	PKT_DO_STOP_SETUP1,
	PKT_DO_STOP_SETUP2,
	PKT_DO_STOP_SETUP3,
	PKT_DO_STOP,

	PKT_DO_INTRA_START_SETUP1,
	PKT_DO_INTRA_START_SETUP2,
	PKT_DO_INTRA_START_SETUP3,
	PKT_DO_INTRA_START,
};

static int transfer_state = PKT_IDLE;
static unsigned char payload;
static unsigned char payload_bit_ctr;
static unsigned char payload_ptr;
static unsigned char payload_byte_ctr;
static char reading;
static char coding;
static char verified;

#define I2C_CODING_READ 1
#define I2C_CODING_WRITE 2
#define I2C_CODING_FAILED 3

int decode_hex_edid(unsigned char c)
{
	static unsigned char c1;

	if (c >= 'a' && c <='f')
		c = 10 + (c - 'a');
	else
		if (c >= 'A' && c <= 'F')
			c = 10 + (c - 'A');
		else
			if (c >= '0' && c <= '9')
				c -= '0';
			else
				return 1;

	if (write_edid & 1)
		eeprom[write_edid >> 1] = c1 | c;
	else
		c1 = c << 4; 

	write_edid++;
	return (write_edid >> 1) >= sizeof(eeprom);
}


/* 0_16 = SCL, 0_17 = SDA */
#define SCL (1 << 16)
#define SDA (1 << 17)

void i2c(void)
{
	if (coding == I2C_CODING_FAILED) {
		if (!(write_edid++ & 0x3fff))
			LPC_GPIO->NOT[0] = 1 << 7;
	} else
		/* LED2 flickering */
		LPC_GPIO->NOT[0] = 1 << 7;

	switch (transfer_state) {

	case PKT_IDLE:
		LPC_GPIO->SET[0] = SCL | SDA;

		if (coding)
			transfer_state = PKT_DO_START_SETUP;
		payload_ptr = 0;
		payload_bit_ctr = 10;
		break;

	case PKT_DO_START_SETUP:
		LPC_GPIO->SET[0] = SDA;
		if (payload_bit_ctr) {
			payload_bit_ctr--;

			LPC_GPIO->NOT[0] = SCL;
		}

		if (!payload_bit_ctr)
			transfer_state++;
		break;
	case PKT_DO_START:
		LPC_GPIO->CLR[0] = SDA;
		payload_byte_ctr = 0;
		transfer_state++;
		break;

	case PKT_GET_BYTE:
		reading = 0;

		switch (payload_byte_ctr++) {
		case 0:
			payload = (0x50 << 1) | 0;
			break;
		case 1:
			payload = payload_ptr;
			break;
		case 2:
			if (coding == I2C_CODING_WRITE) {
				payload = eeprom[payload_ptr++];
				payload_byte_ctr = 99;
			} else
				payload = (0x50 << 1) | 1; /* read */
			break;
		case 3:
			reading = 1;
			payload_byte_ctr = 99;
			break;
		}
		payload_bit_ctr = 9;

		/* fallthru */

	/* send a bit */

	case PKT_DO_POST_START_CLK_DOWN:
		LPC_GPIO->CLR[0] = SCL;
		if (reading)
			LPC_GPIO->SET[0] = SDA;
		transfer_state = PKT_DO_PAYLOAD_SETUP;
		break;

	case PKT_DO_PAYLOAD_SETUP:
		payload_bit_ctr--;

		/* read: leave high including b9, do not acknowledge */
		if ((reading) ||
				/* write: write data until b9, force high */
				(!reading && (payload & 128)))
			LPC_GPIO->SET[0] = SDA;
		else
			LPC_GPIO->CLR[0] = SDA;
		if (!reading || (reading && payload_bit_ctr)) {
			payload <<= 1;

			if (!reading)
				payload |= 1; /* make sure b8 is high */
		}
		transfer_state++;
		break;

	case PKT_DO_PAYLOAD_CLK:

		LPC_GPIO->SET[0] = SCL;

		if (payload_bit_ctr && reading && !LPC_GPIO->W0[9])
			payload |= 1;

		if (!payload_bit_ctr) { /* ack */
			if (!reading) {
				if (!LPC_GPIO->W0[9])
					payload_byte_ctr = 100; /* force retry */
			} else {
				if (eeprom[payload_ptr] != payload)
					verified = 0;
				eeprom[payload_ptr++] = payload;
			}
		}

		/* ready to move on */
		transfer_state++;
		break;

	case PKT_DO_POST_WAIT:
		if (payload_bit_ctr) {
			transfer_state = PKT_DO_POST_START_CLK_DOWN;
			break;
		}
		/* at end of byte */

		/* after sending the read ads, we need to do a START */
		if (coding == I2C_CODING_READ && payload_byte_ctr == 2) {
			transfer_state = PKT_DO_INTRA_START_SETUP1;
			break;
		}

		/* byte is done and ack was present */
		if (payload_byte_ctr < 99) {
			transfer_state = PKT_GET_BYTE;
			break;
		}
		if (payload_byte_ctr == 100)
			transfer_state = PKT_DO_START_SETUP;
		else
			transfer_state++;
		break;

	case PKT_DO_STOP_SETUP1:
		LPC_GPIO->CLR[0] = SCL;
		transfer_state++;
		break;

	case PKT_DO_STOP_SETUP2:
		LPC_GPIO->CLR[0] = SDA;
		transfer_state++;
		break;

	case PKT_DO_STOP_SETUP3:
		LPC_GPIO->SET[0] = SCL;
		transfer_state++;
		break;

	case PKT_DO_STOP:
		LPC_GPIO->SET[0] = SDA; /* going hi with clock hi means STOP */

		if (payload_ptr < (write_edid >> 1))
			transfer_state = PKT_DO_START_SETUP;
		else {
			if (coding == I2C_CODING_WRITE) {
				coding = I2C_CODING_READ;
				verified = 1;
			} else {
				coding = 0;
				if (!verified)
					coding = I2C_CODING_FAILED;
			}
			transfer_state = PKT_IDLE;
			/* LED */
			LPC_GPIO->SET[0] = 1 << 7;
		}
		break;


	case PKT_DO_INTRA_START_SETUP1:
		LPC_GPIO->CLR[0] = SCL;
		transfer_state++;
		break;

	case PKT_DO_INTRA_START_SETUP2:
		LPC_GPIO->SET[0] = SDA;
		transfer_state++;
		break;

	case PKT_DO_INTRA_START_SETUP3:
		LPC_GPIO->SET[0] = SCL;
		transfer_state++;
		break;

	case PKT_DO_INTRA_START:
		LPC_GPIO->CLR[0] = SDA;
		transfer_state = PKT_GET_BYTE;
		break;
	}
}

/* on the wire, SCL just changed state */

void NMI_Handler(void)
{
	static unsigned char newdata;
	static unsigned char bit_counter;
	static unsigned char i2c_shifter;
	static unsigned char i2c_ads;
	static unsigned char eeprom_ads;


	if (coding)
		goto bail;

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
		usb_queue_string("\",\"unit\":\"mV\"},{\"name\":\"data\",\"val\":\"");
		hexdump(eeprom, 0x80);
		usb_queue_string("\"},{\"verified\":\"");
		str[0] = '0' + verified;
		str[1] = '\0';
		usb_queue_string(str);
		usb_queue_string("\"}]}\x04");
		return 0;
	}

	if (ctx) {
		switch (ctx->path_match) {
		case LMPPT_schema:
			if (strcmp(&ctx->buf[15], "hdmi"))
				return -1; /* fail it */
			write_edid = 0;
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
				lava_lmp_actuate_relay(RL1_SET);
			}
			lmp_json_callback_board_hdmi(ctx, REASON_SEND_REPORT);
			break;

		case LMPPT_edid:
			LPC_GPIO->CLR[0] = 4 << 16;
			lava_lmp_actuate_relay(RL1_SET);

			if (reason == LEJPCB_VAL_STR_CHUNK ||
					reason == LEJPCB_VAL_STR_END)
				for (n = 0; n < ctx->npos; n++)
					if (decode_hex_edid(ctx->buf[n]))
						return -1;

			if (reason == LEJPCB_VAL_STR_END)
				coding = I2C_CODING_WRITE;
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

	if (coding && !(q & 0x3))
		i2c();

	if ((!coding || coding == I2C_CODING_FAILED) && !(q & 0x7fff))
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
