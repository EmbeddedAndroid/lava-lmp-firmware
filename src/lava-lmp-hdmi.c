/*
 * lava-lmp firmware: hdmi board
 *
 * Copyright (C) 2012 Linaro Ltd
 * Author: Andy Green <andy.green@linaro.org>
 *
 * Licensed under LGPL2
 */

#include "LPC11Uxx.h"            
#include <power_api.h>
#include "lava-lmp.h"

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

	if (c < 0) { /* idle */
		if (head != tail) {
			if (ring[tail].bytes == 0x80) {
				cs = 0;
				m = ring[tail].ads;
				for (n = 0; n < 0x80; n++)
					cs += eeprom[m++];
				usb_queue_string("EDID read ");
				hex4(ring[tail].ads, str);
				usb_queue_string(str);
				if (!cs)
					usb_queue_string(" valid\r\n");
				else
					usb_queue_string(" INVALID\r\n");
				hexdump(eeprom + ring[tail].ads, 0x80);
				tail = (tail + 1) & 3;
			} else
				if (((tail + 1) & 3) != head) {
					usb_queue_string("abandoned EDID read ");
					hex4(ring[tail].ads, str);
					usb_queue_string(str);
					usb_queue_string(" len ");
					hex4(ring[tail].bytes, str);
					usb_queue_string(str);
					usb_queue_string("\r\n");
					tail = (tail + 1) & 3;
				}
		}
		return;
	}

	switch (rx_state) {
	case CMD:
		switch (c) {
		case '?':
			usb_queue_string("lava-lmp-hdmi 1 1.0\r\n");
			dec(SystemCoreClock, str);
			usb_queue_string(str);
			break;
#ifdef DEBUG_I2C
		case 's':
			hexdump(dump, dump_pos);
			break;
#endif
		case 'H':
			rx_state = HPD;
			break;
		case 'V':
			lava_lmp_write_voltage();
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
		rx_state = CMD;
		break;
	default:
		rx_state = CMD;
		break;
	}	
}

