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

#define RELAY_ACTUATION_MS 20

enum board_id {
	BOARDID_SDMUX = (SENSE_FLOAT * 9) + (SENSE_LOW * 3) + SENSE_FLOAT,
	BOARDID_LSGPIO = (SENSE_FLOAT * 9) + (SENSE_LOW * 3) + SENSE_LOW,
	BOARDID_HDMI = (SENSE_FLOAT * 9) + (SENSE_FLOAT * 3) + SENSE_LOW,
	BOARDID_USB = (SENSE_LOW * 9) + (SENSE_FLOAT * 3) + SENSE_FLOAT,
};

void (*lava_lmp_rx)(unsigned char c);

extern void lava_lmp_sdmux(unsigned char c);
extern void lava_lmp_lsgpio(unsigned char c);
extern void lava_lmp_hdmi(unsigned char c);
extern void lava_lmp_usb(unsigned char c);

static int mode;
static volatile unsigned char actuate[4];

const char *hex = "0123456789ABCDEF";

/* shared by all the implementations for their rx sm, 0 at init */
int rx_state;

static unsigned char gpio1_relay[] = {
	[RL1_CLR] = 20,
	[RL1_SET] = 21,
	[RL2_CLR] = 23,
	[RL2_SET] = 22,
};


unsigned char hex_char(const char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return 10 + c - 'a';
	if (c >= 'A' && c <= 'F')
		return 10 + c - 'A';

	return 0x10;
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

	/* map floating to 1 (1 is illegal) */
	if (n == 3)
		n = 1;

	return n;
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

void lava_lmp_ls_bus_mode(int bus, enum ls_direction nInOut)
{
	int n = 26;

	if (bus)
		n +=2;

	/* kill any driving from level shifter */
	LPC_GPIO->SET[1] = 2 << n;

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

void lava_lmp_pin_init(void)
{
	int n = 0;

	rx_state = 0;

	/* relay control */

	/* all gpio pulldown */
	LPC_IOCON->PIO1_20 = (1 << 3) | (0 << 0); /* RL1 CLR */
	LPC_IOCON->PIO1_21 = (1 << 3) | (0 << 0); /* RL1 SET */
	LPC_IOCON->PIO0_22 = (1 << 3) | (0 << 0); /* RL2 SET */
	LPC_IOCON->PIO0_23 = (1 << 3) | (0 << 0); /* RL2 CLR */

	/* all output 0 (deactuated) */
	LPC_GPIO->CLR[1] = 0xf << 20;

	/* all output mode */
	LPC_GPIO->DIR[1] |= 0xf << 20;

	for (n = 0; n < 4; n++)
		actuate[n] = 0;

	/* board ID, between 0 and 26 */

	mode =   lava_lmp_gpio_sense(&LPC_IOCON->PIO1_14, 1, 14) +
		(lava_lmp_gpio_sense(&LPC_IOCON->PIO1_15, 1, 15) * 3) +
		(lava_lmp_gpio_sense(&LPC_IOCON->PIO1_16, 1, 16) * 9);

	switch (mode) {
	case BOARDID_SDMUX:
		lava_lmp_rx = lava_lmp_sdmux;
		/* mux control and power: inert mode = 00000101 */
		LPC_GPIO->CLR[0] = 0xfa << 8;
		LPC_GPIO->SET[0] = 5 << 8;
		/* LSAD0..7 all output */
		LPC_GPIO->DIR[0] |= 0xff << 8;
		/* LSBD0 DUT_CMD snooping is INPUT */
		LPC_GPIO->DIR[0] &= ~(1 << 16);
		/* LSBD7 == AD7 Analogue In */
		LPC_IOCON->PIO0_23 = (0 << 7) | (0 << 3) | (1 << 0);
		break;
	case BOARDID_USB:
		lava_lmp_rx = lava_lmp_usb;
		/* mux control inert mode = 10 */
		LPC_GPIO->CLR[0] = 1 << 8;
		LPC_GPIO->SET[0] = 2 << 8;
		/* LSAD0..1 output */
		LPC_GPIO->DIR[0] |= 3 << 8;
		/* LSBD7 == AD7 Analogue In */
		LPC_IOCON->PIO0_23 = (0 << 7) | (0 << 3) | (1 << 0);
		break;
	case BOARDID_HDMI:
		lava_lmp_rx = lava_lmp_hdmi;
		/* deassert scl/sda/hpd forcing */
		LPC_GPIO->SET[0] = 0x0700 << 8;
		/* LSBD0..2 output */
		LPC_GPIO->DIR[0] |= 0x700 << 8;
		/* LSAD0..2 is INPUT */
		LPC_GPIO->DIR[0] &= ~(7 << 8);
		/* LSBD7 == AD7 Analogue In */
		LPC_IOCON->PIO0_23 = (0 << 7) | (0 << 3) | (1 << 0);
		break;
	case BOARDID_LSGPIO:
		lava_lmp_rx = lava_lmp_lsgpio;
		lava_lmp_ls_bus_mode(0, LS_DIR_IN);
		lava_lmp_ls_bus_mode(1, LS_DIR_IN);
		/* LS Controls are output */
		LPC_GPIO->DIR[1] |= 0xf << 26;
		break;
	}

	/* everything that has relays is OK with them SET by default */
	lava_lmp_actuate_relay(RL1_SET);
	lava_lmp_actuate_relay(RL2_SET);
}

void lava_lmp_actuate_relay(int n)
{
	actuate[n] = RELAY_ACTUATION_MS;
}

void lava_lmp_1ms_tick(void)
{
	int n;

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


