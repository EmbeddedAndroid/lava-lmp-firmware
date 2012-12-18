# Copyright (C) 2012 Linaro Ltd
# Author: Andy Green <andy.green@linaro.org>
# Licensed under LGPL2
#

PROJECT = lava-lmp
CROSS := arm-none-eabi-
CC := $(CROSS)gcc
INCLUDES := -I ../inc
CFLAGS := -mcpu=cortex-m0 -mno-apcs-float \
	  -mthumb \
	  -fomit-frame-pointer \
	  -Wall -Werror -Wstrict-prototypes \
	  -fverbose-asm
LFLAGS := -mthumb -T LPC11U24.ld
LIBS :=
MAIN=$(PROJECT).elf
BIN=$(PROJECT).bin
IMG=$(PROJECT).img

SUBDIRS = src

export

.ONESHELL:
all:
	@for dir in $(SUBDIRS); do
		$(MAKE) -C $$dir
	@done

.ONESHELL:
clean:
	@rm -f $(IMG) src/$(BIN)
	@for dir in $(SUBDIRS); do
		$(MAKE) -C $$dir clean
	@done

