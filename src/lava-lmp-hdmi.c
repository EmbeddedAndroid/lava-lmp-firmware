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

static char json[] = {
	"\x01board.json\x02{"
		"\"if\":["
			"{\"name\":\"HDMI\"},"
			"{\"pins\":[\"5V\",\"HPD\",\"EDID\"]}"
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
		"\"int\":[\"fake-edid\"],"
		"\"mux\":["
			"{"
				"\"sink\":\"MON.5V\","
				"\"src\":[\"NULL\",\"DUT.5V\"]"
			"},{"
				"\"sink\":\"DUT.HPD\","
				"\"src\":[\"NULL\",\"MON.HPD\"]"
			"},{"
				"\"sink\":\"DUT.EDID\","
				"\"src\":[\"MON.EDID\",\"fake-edid\"]"
			"}"
		"]"
	"}\x04"
};

static char json_state1[] =
	"\x01state.json\x02{"
		"["
			"\"DUT.HPD\":\""
;
static char json_state2[] =
			"\","
			"\"DUT.EDID\":\""
;
static char json_state3[] =
			"\""
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

void issue_json_state(void)
{
	usb_queue_string(json_state1);
	usb_queue_string("NULL");
	usb_queue_string(json_state2);
	usb_queue_string("MON.EDID");
	usb_queue_string(json_state3);
}


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

void lava_lmp_hdmi(int c)
{
	char str[10];
	unsigned char n, m;
	unsigned char cs;
	static int q;

	if (c < 0) { /* idle */

		q++;
		if ((q & 0x7fff) == 0) {
/*
		if (q & 0x8000)
				LPC_GPIO->SET[0] = 1 << 2;
			else
				LPC_GPIO->CLR[0] = 1 << 2;
*/
			lava_lmp_write_voltage("\x01report-DUT.5V\x02");
			return;
		}

		if (head == tail)
			return;
		if (ring[tail].bytes == 0x80) {
			cs = 0;
			m = ring[tail].ads;
			for (n = 0; n < 0x80; n++)
				cs += eeprom[m++];
			hex4(ring[tail].ads, str);
			usb_queue_string(str);
			if (!cs)
				usb_queue_string("\x01valid-edid-rx.hex\x02");
			else
				usb_queue_string("\x01invalid-edid-rx.hex\x02");
			hexdump(eeprom + ring[tail].ads, 0x80);
			tail = (tail + 1) & 3;
			usb_queue_string("\x04");
			return;
		}
		if (((tail + 1) & 3) == head)
			return;

		usb_queue_string("\x01""abandoned-EDID-rx.hex\x02");
		hex4(ring[tail].ads, str);
		usb_queue_string(str);
		usb_queue_string(" ");
		hex4(ring[tail].bytes, str);
		usb_queue_string(str);
		usb_queue_string("\x04");
		tail = (tail + 1) & 3;

		return;
	}

	switch (rx_state) {
	case CMD:
		switch (c) {
#if 0
		case 'S':
			hexdump(dump, dump_pos);
			break;
#endif
		case 'H':
			rx_state = HPD;
			break;
		case 'j':
			usb_queue_string(json);
			break;
		case 's':
			issue_json_state();
			break;
		}
		break;
	case HPD:  /* HPD connectivity mode */
		switch (c) {
		case 'X':
			lava_lmp_actuate_relay(RL2_SET);
			break;
		case '0':
			LPC_GPIO->SET[0] = 4 << 16;
			lava_lmp_actuate_relay(RL2_CLR);
			break;
		case '1':
			LPC_GPIO->CLR[0] = 4 << 16;
			lava_lmp_actuate_relay(RL2_CLR);
			break;
		}
		issue_json_state();
		rx_state = CMD;
		break;
	default:
		rx_state = CMD;
		break;
	}	
}

