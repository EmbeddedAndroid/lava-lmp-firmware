/*
 * lava-lmp firmware: lava-lmp.c
 *
 * Copyright (C) 2012 Linaro Ltd
 * Author: Andy Green <andy.green@linaro.org>
 *
 * Licensed under LGPL2
 */

#include "LPC11Uxx.h"            
#include <power_api.h>
#include "lava-lmp.h"
#include <core_cm0.h>
#include <string.h>

#define RELAY_ACTUATION_MS 20

enum board_id {
	BOARDID_SDMUX = (SENSE_FLOAT * 9) + (SENSE_LOW * 3) + SENSE_FLOAT,
	BOARDID_LSGPIO = (SENSE_FLOAT * 9) + (SENSE_LOW * 3) + SENSE_LOW,
	BOARDID_HDMI = (SENSE_FLOAT * 9) + (SENSE_FLOAT * 3) + SENSE_LOW,
	BOARDID_USB = (SENSE_LOW * 9) + (SENSE_FLOAT * 3) + SENSE_FLOAT,
	BOARDID_ETH = (SENSE_HIGH * 9) + (SENSE_FLOAT * 3) + SENSE_FLOAT,
	BOARDID_SATA = (SENSE_HIGH * 9) + (SENSE_HIGH * 3) + SENSE_FLOAT,
};

int idle_ok;
char (*lmp_json_callback_board)(struct lejp_ctx *ctx, char reason) = NULL;
const char * json_info;

extern const char * const json_info_sdmux;
extern const char * const json_info_lsgpio;
extern const char * const json_info_hdmi;
extern const char * const json_info_usb;
extern const char * const json_info_eth;
extern const char * const json_info_sata;

extern char lmp_json_callback_board_hdmi(struct lejp_ctx *ctx, char reason);
extern char lmp_json_callback_board_sdmux(struct lejp_ctx *ctx, char reason);
extern char lmp_json_callback_board_lsgpio(struct lejp_ctx *ctx, char reason);
extern char lmp_json_callback_board_usb(struct lejp_ctx *ctx, char reason);
extern char lmp_json_callback_board_eth(struct lejp_ctx *ctx, char reason);
extern char lmp_json_callback_board_sata(struct lejp_ctx *ctx, char reason);

int mode;
static volatile unsigned char actuate[4];

volatile int adc7, adc7sum, adc7count;
int bump;

const char * const hex = "0123456789abcdef";

/* shared by all the implementations for their rx sm, 0 at init */
int rx_state;

static const unsigned char gpio1_relay[] = {
	[RL1_CLR] = 20,
	[RL1_SET] = 21,
	[RL2_CLR] = 23,
	[RL2_SET] = 22,
};

const char * const tf[] = {
	"false",
	"true"
};

void usb_queue_true_or_false(char b)
{
	usb_queue_string(tf[b & 1]);
}

unsigned char hex_char(const char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return 10 + c - 'a';
	if (c >= 'A' && c <= 'F')
		return 10 + c - 'A';

	return 0x55;
}

unsigned int atoi(const char *s)
{
	unsigned int u = 0;

	while (*s)
		u = (u * 10) + (*s++ - '0');

	return u;
}

static void _hexn(unsigned int val, char *buf, int start)
{
	int n;

	for (n = start; n >= 0; n -= 4)
		*buf++ = hex[(val >> n) & 0xf];

	*buf++ = ' ' ;
	*buf++ = '\0';
}

void hex2(unsigned char val, char *buf)
{
	_hexn(val, buf, 4);
}

void hex4(unsigned int val, char *buf)
{
	_hexn(val, buf, 12);
}

void hex8(unsigned int val, char *buf)
{
	_hexn(val, buf, 28);
}

void _hexdump(unsigned char *p, int len)
{
	char str[48];
	int n;
	int y = 0;

	for (n = 0; n < len; n++) {
		str[y++] = hex[(p[n] >> 4) & 0xf];
		str[y++] = hex[p[n] & 0xf];
		if (y > sizeof(str) - 3) {
			str[y] = '\0';
			y = 0;
			usb_queue_string(str);
		}
	}
	if (y) {
		str[y] = '\0';
		usb_queue_string(str);
	}
}

void hexdump(unsigned char *p, int len)
{
	char str[48];
	int n, y = 0, m, c;

	str[y++] = '\r';
	str[y++] = '\n';
	str[y++] = '\0';
	usb_queue_string(str);
	y = 0;

	for (n = 0; n < len; n++) {
		if (!y) {
			hex4(n, str);
			y = 4;
			str[y++] = ':';
			str[y++] = ' ' ;
		}
		str[y++] = hex[(p[n] >> 4) & 0xf];
		str[y++] = hex[p[n] & 0xf];
		if ((n & 7) == 7 || n == len - 1) {
			m = n;
			while ((m & 7) != 7) {
				str[y++] = ' ';
				str[y++] = ' ';
				str[y++] = ' ';
				m++;
			}
			str[y++] = ' ';
			str[y++] = ' ';
			c = 8;
			m = n & ~7;
			if (m + 8 > len)
				c = len - m;
			while (c--) {
				if (p[m] < 32 || p[m] > 126)
					str[y++] = '.';
				else
					str[y++] = p[m];
				m++;
			}
			
			str[y++] = '\r';
			str[y++] = '\n';
			str[y++] = '\0';
			usb_queue_string(str);
			y = 0;
		} else
			str[y++] = ' ' ;
	}
}

void lmp_delay(int count)
{
	volatile int n = 0;

	while (n < count)
		n++;
}

int _dec(unsigned int val, char *buf, int nonzero, int d)
{
	int n;
	char *oldbuf = buf;

	while (d) {
		n = val / d;
		if (d == 1 || n || nonzero) {
			*buf++ = '0' + n;
			nonzero = 1;
			val -= n * d;
		}
		d /= 10;
	}
	*buf = '\0';

	return buf - oldbuf;
}

int dec(unsigned int val, char *buf)
{
	return _dec(val, buf, 0, 1000000000);
}

/* returns 0 = tied low, 1 = floating, 2 = tied high */

static int lava_lmp_gpio_sense( __IO uint32_t *p, int bank, int bit)
{
	int n = 0;

	/*  set gpio to input */
	LPC_GPIO->DIR[bank] &= ~(1 << bit);

	*p = (1 << 3) | (0 << 0); /* pulldown */
	if (LPC_GPIO->PIN[bank] & (1 << bit))
		n = 1;

	*p = (2 << 3) | (0 << 0); /* pullup */
	if (LPC_GPIO->PIN[bank] & (1 << bit))
		n |= 2;

	/* nb GPIO is left in pulled-up input mode */

	switch (n) {
	case 0:
		return SENSE_LOW;
	case 2:
		return SENSE_FLOAT;
	case 3:
		return SENSE_HIGH;
	}

	/* hm 1 would represent reading the opposite of what was driven... */

	return SENSE_FLOAT;
}

unsigned char lava_lmp_bus_read(int bus)
{
	if (bus)
		bus = 16;
	else
		bus = 8;

	return (LPC_GPIO->PIN[0] >> bus) & 0xff;
}

void lava_lmp_bus_write(int bus, unsigned char byte)
{
	if (bus)
		bus = 16;
	else
		bus = 8;

	LPC_GPIO->SET[0] = byte << bus;
	LPC_GPIO->CLR[0] = (byte ^ 0xff) << bus;
}

void lava_lmp_gpio_bus_mode(int bus, int nInOut)
{
	if (bus)
		bus = 16;
	else
		bus = 8;

	if (nInOut) /* output */
		LPC_GPIO->DIR[0] |= 0xff << bus;
	else
		LPC_GPIO->DIR[0] &= ~(0xff << bus);
}

/* 0 = input, 1 = output */
char lava_lmp_get_bus_mode(int bus)
{
	if (bus)
		bus = 16;
	else
		bus = 8;

	return !!((LPC_GPIO->DIR[0] >> bus) & 0xff);
}

void lava_lmp_ls_bus_mode(int bus, enum ls_direction nInOut)
{
	int n = 26;

	if (bus)
		n +=2;
/*
	if (lava_lmp_get_bus_mode(bus) != nInOut)
		goto set;

	if (LPC_GPIO->PIN[1] & (2 << n))
		goto set;

	if (((LPC_GPIO->PIN[1] >> n) & 1) == nInOut)
		return;

set:
*/
	/* kill any driving from level shifter */
//	LPC_GPIO->SET[1] = 2 << n;

	if (!nInOut)
		/* set B -> A direction */
		LPC_GPIO->CLR[1] = 1 << n;

	/* set gpio mode appropriately */
	lava_lmp_gpio_bus_mode(bus, nInOut);

	if (nInOut)
		/* set -> B direction */
		LPC_GPIO->SET[1] = 1 << n;

	/* enable level shifter to drive one side or the other */
	LPC_GPIO->CLR[1] = 2 << n;
}


void ADC_IRQHandler(void) 
{
	unsigned int reg = LPC_ADC->STAT;

	if (reg & (1 << 7))
		adc7sum += LPC_ADC->DR[7] & 0xffc0;
	else
		reg = LPC_ADC->DR[7];

	if (++adc7count >= 512) {
		adc7 = adc7sum >> 9;
		adc7count = 0;
		adc7sum = 0;
	}

	/* restart */

	LPC_ADC->CR &= 0xFFFFFF00;
	LPC_ADC->CR |= (1 << 24) | (1 << 7);
}

void TIMER32_1_IRQHandler(void)
{
	unsigned int reg = LPC_CT32B1->IR;
	int n;

	LPC_CT32B1->IR = reg;

	if (reg & 1) {

		bump++;

		/* auto de-actuate relays */
		for (n = 0; n < sizeof(actuate); n++) {
			if (!actuate[n])
				continue;

			/* opposing side: kill counter and force off */
			actuate[n ^ 1] = 0;
			LPC_GPIO->W1[gpio1_relay[n ^ 1]] = 0;

			actuate[n]--;
			LPC_GPIO->W1[gpio1_relay[n]] = !!actuate[n];
		}
	}
}

void lava_lmp_pin_init(void)
{
	int n = 0;
	int analog = 0;

	rx_state = 0;

	LPC_SYSCON->SYSAHBCLKCTRL |= 1 << 6;

	/* relay control */

	/* all gpio pulldown */
	LPC_IOCON->PIO1_20 = (1 << 3) | (0 << 0); /* RL1 CLR */
	LPC_IOCON->PIO1_21 = (1 << 3) | (0 << 0); /* RL1 SET */
	LPC_IOCON->PIO1_22 = (1 << 3) | (0 << 0); /* RL2 SET */
	LPC_IOCON->PIO1_23 = (1 << 3) | (0 << 0); /* RL2 CLR */

	/* all output 0 (deactuated) */
	LPC_GPIO->CLR[1] = 0xf << 20;

	/* all output mode */
	LPC_GPIO->DIR[1] |= 0xf << 20;

	for (n = 0; n < 4; n++)
		actuate[n] = 0;

	LPC_IOCON->PIO0_8 =  (1 << 3) | (0 << 0);
	LPC_IOCON->PIO0_9 =  (1 << 3) | (0 << 0);
	LPC_IOCON->SWCLK_PIO0_10 = (1 << 3) | (1 << 0);
	LPC_IOCON->TDI_PIO0_11 = (1 << 3) | (1 << 0);
	LPC_IOCON->TMS_PIO0_12 = (1 << 3) | (1 << 0);
	LPC_IOCON->TDO_PIO0_13 = (1 << 3) | (1 << 0);
	LPC_IOCON->TRST_PIO0_14 = (1 << 3) | (1 << 0);
	LPC_IOCON->SWDIO_PIO0_15 = (1 << 3) | (1 << 0);

	LPC_IOCON->PIO0_2 = (1 << 3) | (0 << 0); /* nLED1 */
	LPC_IOCON->PIO0_7 = (1 << 3) | (0 << 0); /* nLED2 */
	LPC_GPIO->CLR[0] = 1 << 2;
	LPC_GPIO->SET[0] = 1 << 7; /* LED2 default off */
	LPC_GPIO->DIR[0] |= (1 << 2) | (1 << 7);

	/* board ID, between 0 and 26 */

	mode =   lava_lmp_gpio_sense(&LPC_IOCON->PIO1_14, 1, 14) +
		(lava_lmp_gpio_sense(&LPC_IOCON->PIO1_15, 1, 15) * 3) +
		(lava_lmp_gpio_sense(&LPC_IOCON->PIO1_16, 1, 16) * 9);

	switch (mode) {
	case BOARDID_SDMUX:
		json_info = json_info_sdmux;
		lmp_json_callback_board = lmp_json_callback_board_sdmux;
		/* mux control and power: inert mode = 00000101 */
		LPC_GPIO->CLR[0] = 0xfa << 8;
		LPC_GPIO->SET[0] = 5 << 8;
		/* both disconnected */
		LPC_GPIO->SET[0] = (0x05 << 8); /* gpio0_17 */
		LPC_GPIO->CLR[0] = 1 << 17 | ((~(0x05 << 8)) & 0xff00);
		/* LSAD0..7 all output */
		LPC_GPIO->DIR[0] |= (1 << 17) | (0xff << 8);
		LPC_IOCON->PIO1_1 =  (1 << 3) | (0 << 0);
		/* LSBD0 DUT_CMD snooping is INPUT */
		LPC_GPIO->DIR[0] &= ~(1 << 16);

		/* off */

		lava_lmp_actuate_relay(RL1_CLR);
		lava_lmp_actuate_relay(RL2_CLR);

		analog = 1;
		break;
	case BOARDID_USB:
		json_info = json_info_usb;
		lmp_json_callback_board = lmp_json_callback_board_usb;
		/* mux control inert mode = 10 */
		LPC_GPIO->CLR[0] = 1 << 8;
		LPC_GPIO->SET[0] = 2 << 8;
		/* LSAD0..1 output */
		LPC_GPIO->DIR[0] |= 3 << 8;
		analog = 1;
		break;
	case BOARDID_HDMI:
		json_info = json_info_hdmi;
		lmp_json_callback_board = lmp_json_callback_board_hdmi;

		LPC_SYSCON->SYSAHBCLKCTRL |= (1<<19) | (1<<23) | (1<<24);
		
		/* deassert scl/sda/hpd forcing */
		LPC_GPIO->SET[0] = 0x0700 << 8;
		/* LSBD0..2 output */
		LPC_GPIO->DIR[0] |= 0x700 << 8;
		/* LSAD0..2 is INPUT */
		LPC_GPIO->DIR[0] &= ~(7 << 8);
		LPC_GPIO->DIR[0] |= 0x10 << 8;
		/* LSB0..3 are outputs */
		LPC_GPIO->DIR[0] |= 0xf << 16;

		/* notice SCL+SDA go through 74lvcx14 inverter... */

		/* i2c snoop - select gpio int sources */
		LPC_SYSCON->PINTSEL[0] = (0 * 24) + 8; /* SCL */
		LPC_SYSCON->PINTSEL[1] = (0 * 24) + 9; /* SDA */

		/* i2c snoop... interrupts on either edge */
		LPC_GPIO_PIN_INT->ISEL &= ~3;
		LPC_GPIO_PIN_INT->IENF = 1; /* SCL rising edge on wire (inv) */
		LPC_GPIO_PIN_INT->IENR = 1; /* SCL falling edge on wire (inv) */

		/* enable rising and falling edges of SCL detection*/
		LPC_GPIO_PIN_INT->FALL = 1 << 0;
		LPC_GPIO_PIN_INT->RISE = 1 << 0;

		/* group interrupt used to detect SDA transition during SCL 1 */
		/* ports 0.8 and 0.9 are interesting for group interrupt */
		LPC_GPIO_GROUP_INT0->PORT_ENA[0] = 3 << 8;

		LPC_IOCON->PIO0_8 = (0 << 3) | (0 << 0);
		LPC_IOCON->PIO0_9 = (0 << 3) | (0 << 0);
		LPC_IOCON->SWCLK_PIO0_10 = (0 << 3) | (1 << 0);

		/* prepare derection of SDA inversion while we are idling hi */
		LPC_GPIO_GROUP_INT0->PORT_POL[0] = 0 << 9 | 0 << 8;

		LPC_GPIO_GROUP_INT0->CTRL = 1 << 1 | 1 << 0;
		LPC_GPIO_GROUP_INT0->CTRL = 1 << 1 | 1 << 0;

		/* set SCL edge change as NMI */
		LPC_SYSCON->NMISRC = 0x80000000 | FLEX_INT0_IRQn;

		analog = 1;
		break;
	case BOARDID_LSGPIO:
		json_info = json_info_lsgpio;
		lmp_json_callback_board = lmp_json_callback_board_lsgpio;
		lava_lmp_ls_bus_mode(0, LS_DIR_IN);
		lava_lmp_ls_bus_mode(1, LS_DIR_IN);
		/* LS Controls are output */
		LPC_GPIO->DIR[1] |= 0xf << 26;
		break;
	case BOARDID_ETH:
		json_info = json_info_eth;
		lmp_json_callback_board = lmp_json_callback_board_eth;

		break;
	case BOARDID_SATA:
		json_info = json_info_sata;
		lmp_json_callback_board = lmp_json_callback_board_sata;
		break;
	}

	if (analog) {
		LPC_SYSCON->PDRUNCFG &= ~(0x1 << 4);
		LPC_SYSCON->SYSAHBCLKCTRL |= 1 << 13;

		/* LSBD7 == AD7 Analogue In */
		LPC_IOCON->PIO0_23 = (0 << 7) | (0 << 3) | (1 << 0);

		  LPC_ADC->CR = ( 0x01 << 0 ) |  /* SEL=1,select channel 0~7 on ADC0 */
			( ( SystemCoreClock / 1100000 - 1 ) << 8 ) |  /* CLKDIV = Fpclk / 1000000 - 1 */ 
			( 0 << 16 ) | 		/* BURST = 0, no BURST, software controlled */
			( 0 << 17 ) |  		/* CLKS = 0, 11 clocks/10 bits */
			( 0 << 24 ) |  		/* START = 0 A/D conversion stops */
			( 0 << 27 );		/* EDGE = 0 (CAP/MAT singal falling,trigger A/D conversion) */

		NVIC_EnableIRQ(ADC_IRQn);
		LPC_ADC->INTEN = 0x080;

		LPC_ADC->CR &= 0xFFFFFF00;
		LPC_ADC->CR |= (1 << 24) | (1 << 7);
	}

	if (mode != BOARDID_SDMUX) {
		lava_lmp_actuate_relay(RL1_SET);
		lava_lmp_actuate_relay(RL2_SET);
	}

	/* sort out the actuation timer */

	LPC_SYSCON->SYSAHBCLKCTRL |= 1 << 9 | 1 << 10;

	LPC_CT32B1->TCR = 2;
	LPC_CT32B1->MR0 = 30000;
	LPC_CT32B1->MCR = 3;
	LPC_CT32B1->TCR = 1;

	NVIC_EnableIRQ(TIMER_32_1_IRQn);
}

void lava_lmp_actuate_relay(int n)
{
	actuate[n] = RELAY_ACTUATION_MS;
}

int lava_lmp_eeprom(unsigned int eep, enum eeprom_dir dir, unsigned char *from, int len)
{
	unsigned int cmd[5], result[4];
	void (*iap)(unsigned int cmd[], unsigned int result[]) =
		(void (*)(unsigned int cmd[], unsigned int result[]))0x1FFF1FF1; 

	cmd[0] = 61 + dir;
	cmd[1] = eep;
	cmd[2] = (unsigned int)from;
	cmd[3] = len;
	cmd[4] = 48000; /* 48MHz / 1000 */
	iap(cmd, result);

	return result[0];
}

void lava_lmp_write_voltage(void)
{
	char str[20];

	dec((adc7 * 6600) >> 16, str);
	usb_queue_string(str);
}

