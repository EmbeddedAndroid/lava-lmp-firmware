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

enum rx_states {
	CMD,
	HPD,
};

static unsigned char data;
static unsigned char bit_counter;
static unsigned char i2c_shifter;

enum i2c {
	IS_IDLE,
	IS_ADS_DIR,
	IS_DATA
};

enum i2c i2c_state;

static unsigned char eeprom[256];
static unsigned char i2c_ads;
static unsigned char eeprom_ads;

struct reading_session {
	unsigned char ads;
	unsigned int bytes;
};

static struct reading_session ring[4];
static unsigned char head; //, tail;

/* on the wire, SCL just changed state */

void NMI_Handler(void)
{
	int newdata = !LPC_GPIO->W0[9];

	if (LPC_GPIO->W0[8]) { /* SCL is low */
		if (!(LPC_GPIO_GROUP_INT0->CTRL & 1))
			goto bail;

		bit_counter = 0;
		if (data) /* START */
			i2c_state = IS_ADS_DIR;
		else /* STOP */
			i2c_state = IS_IDLE;
		goto bail;
	}

	/* SCL has gone high */

	/* prepare derection of SDA inversion while we are high */
	LPC_GPIO_GROUP_INT0->PORT_POL[0] = newdata << 9 | 0 << 8;
	/* edge-trigger, AND combine, clear pending */
	LPC_GPIO_GROUP_INT0->CTRL = 0 << 2 | 1 << 1 | 1 << 0;

	if (i2c_state == IS_IDLE)
		goto bail1;

	if (bit_counter++ != 8)
		goto bail2;

	bit_counter = 0;

	switch (i2c_state) {
	case IS_ADS_DIR:
		i2c_ads = i2c_shifter;
		break;
	case IS_DATA:
		if (i2c_ads & 1) { /* reading */
			eeprom[eeprom_ads++] = i2c_shifter;
			ring[(head - 1) & 3].bytes++;
		} else { /* writing: set eeprom address */
			eeprom_ads = i2c_shifter;
			ring[head].ads = eeprom_ads;
			ring[head].bytes = 0;
			head = (head + 1) & 3;
		}
		break;
	default:
		break;
	}

	if (newdata) /* NAK */
		i2c_state = IS_IDLE;
	else /* ACK */
		i2c_state = IS_DATA;

bail2:
	i2c_shifter = (i2c_shifter << 1) | newdata;
bail1:
	data = newdata;
bail:
	LPC_GPIO_PIN_INT->IST = 1 << 0;
}

void lava_lmp_hdmi(int c)
{
	char str[10];

	if (c < 0) { /* idle */

	}

	switch (rx_state) {
	case CMD:
		switch (c) {
		case '?':
			usb_queue_string("lava-lmp-hdmi 1 1.0\r\n");
			dec(SystemCoreClock, str);
			usb_queue_string(str);
			break;
		case 's':
			hexdump(eeprom, sizeof eeprom);
			break;
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

