SRCS =  main.c \
	lava-lmp.c \
	serial.c \
	lejp.c \
	json.c \
	lava-lmp-sdmux.c \
	lava-lmp-lsgpio.c \
	lava-lmp-hdmi.c \
	lava-lmp-usb.c \
	lava-lmp-eth.c \
	lava-lmp-sata.c \
	cr_startup_lpc11u.c \
	cdc_desc.c \
	system_LPC11Uxx.c

OBJS = $(SRCS:.c=.o)

.PHONY: depend clean

all:	../$(IMG)

.ONESHELL:
../$(IMG): $(MAIN)
	@E=`grep __bss_end__ mapfile | tr -s ' ' | cut -d' ' -f2`
	@S=`grep _vStackTop mapfile | tr -s ' ' | cut -d' ' -f2`
	@echo "   $$(( $$S - $$E )) bytes stack / remaining RAM"
	@$(CROSS)objcopy -O binary $(MAIN) $(BIN)
#
# LPC firmware is a binary file with a signature in the 8th vector slot
# consisting of a 2-complement checksum of the previous 7 vectors.  This
# copies the BIN file over replacing the 8th vector slot with the computed
# checksum
#
	@rm -f ../$(IMG)
	@dd if=$(BIN) of=../$(IMG) bs=1 count=28 2>/dev/null
	@S="00000000"
	@CS="00000000"
	@for N in 0 4 8 12 16 20 24 ; do 
		A=`hexdump -e "1/4 \"%08X\" \"\n\"" -s$$N -n 4 $(BIN)`
		R=$$(( 0x$$CS + 0x$$A ))
		CS=`printf %08X $$R`
		R=$$(( 0x$$S - 0x$$A ))
		R=$$(( $$R & 0xffffffff ))
		S=`printf %08X $$R`
	@done
	@R=$$(( 0x$$S + 0x$$CS ))
	@R=$$(( $$R & 0xffffffff ))
	@echo S=0x$$S CS=0x$$CS sum = $$R
	@echo -n -e "\x$${S:6:2}\x$${S:4:2}\x$${S:2:2}\x$${S:0:2}" >>../$(IMG)
	@dd if=$(BIN) of=../$(IMG) bs=1 skip=32 seek=32 2>/dev/null
	@S=`stat ../$(IMG) -c "%s"`
	@dd if=/dev/zero of=../$(IMG) bs=1 seek=$$S count=$$(( 32768 - $$S ))

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it

