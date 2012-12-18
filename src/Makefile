SRCS =  main.c \
	lava-lmp.c \
	lava-lmp-sdmux.c \
	lava-lmp-lsgpio.c \
	lava-lmp-hdmi.c \
	lava-lmp-usb.c \
	cr_startup_lpc11u.c \
	cdc_desc.c \
	system_LPC11Uxx.c

OBJS = $(SRCS:.c=.o)

.PHONY: depend clean

all:	../$(IMG)

.ONESHELL:
../$(IMG): $(MAIN)
	@$(CROSS)objcopy -O binary $(MAIN) $(BIN)
#
# LPC firmware is a binary file with a signature in the 8th vector slot
# consisting of a 2-complement checksum of the previous 7 vectors.  This
# copies the BIN file over replacing the 8th vector slot with the computed
# checksum
#
	@rm -f ../$(IMG)
	@dd if=$(BIN) of=../$(IMG) bs=1 count=28 2>/dev/null
	@S="FFFFFFFF"
	@for N in 0 4 8 12 16 20 24 ; do 
		A=`hexdump -e "1/4 \"%08X\" \"\n\"" -s$$N -n 4 $(BIN)`
		R=$$(( 0x$$S - 0x$$A ))
		S=`printf %08X $$R`
	@done
	@echo -n -e "\x$${S:6:2}\x$${S:4:2}\x$${S:2:2}\x$${S:0:2}" >>../$(IMG)
	@dd if=$(BIN) of=../$(IMG) bs=1 skip=32 seek=32 2>/dev/null


$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it

