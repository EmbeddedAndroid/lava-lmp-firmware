/*
 * lava-lmp firmware: lava-lmp.h
 *
 * Copyright (C) 2012 Linaro Ltd
 * Author: Andy Green <andy.green@linaro.org>
 *
 * Licensed under LGPL2
 */

enum sense {
	SENSE_LOW,
	SENSE_FLOAT,
	SENSE_HIGH
};

enum relays {
	RL1_CLR,
	RL1_SET,
	RL2_CLR,
	RL2_SET,
};

enum ls_direction {
	LS_DIR_IN,
	LS_DIR_OUT,
};

enum eeprom_dir {
	EEPROM_WRITE,
	EEPROM_READ
};

#define EEPROM_RESERVED_OFFSET 64
#define USB_SERIAL_NUMBER_CHARS 16
#define USB_SERIAL_NUMBER_CHARS_AUTOSET 4

extern int rx_state;
extern int mode;
extern const char *hex;

extern unsigned char hex_char(const char c);
extern void hex8(unsigned int val, char *buf);
extern void hex4(unsigned int val, char *buf);
extern int dec(unsigned int val, char *buf);
extern int _dec(unsigned int val, char *buf, int nonzero, int d);
extern void hexdump(unsigned char *p, int len);

extern int lava_lmp_eeprom(unsigned int eep, enum eeprom_dir dir, unsigned char *from, int len);

extern void usb_queue_tx(const unsigned char *buf, int len);
extern void usb_queue_string(const char *buf);

extern unsigned char lava_lmp_bus_read(int bus);
extern void lava_lmp_bus_write(int bus, unsigned char byte);
extern void lava_lmp_gpio_bus_mode(int bus, int nInOut);
extern void lava_lmp_ls_bus_mode(int bus, enum ls_direction);
extern volatile int adc7;
extern void lava_lmp_write_voltage(void);

extern void lava_lmp_pin_init(void);
extern void lava_lmp_actuate_relay(int n);
extern void lava_lmp_1ms_tick(void);
extern void (*lava_lmp_rx)(int c);
