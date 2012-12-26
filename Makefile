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
		@$(MAKE) -C $$dir
		@if [ $$? -ne 0 ] ; then
		@ exit 1
		@fi
	@done
	@M=`mount |grep DISABLD | cut -d' ' -f3- | sed s/\ type\ .*//g`
	@if [ -z "$$M" ] ; then
	@ echo "LPC11U24 not mounted"
	@ exit 1
	@fi
	@ echo copying ./$(IMG) to $$M
	@ M=`echo $$M | sed 's/ /\\ /g'`
	@ dd conv=nocreat,notrunc if=./$(IMG) of="$$M/firmware.bin"
	@ umount "$$M"

.ONESHELL:
clean:
	@rm -f $(IMG) src/$(BIN)
	@for dir in $(SUBDIRS); do
		$(MAKE) -C $$dir clean
	@done

