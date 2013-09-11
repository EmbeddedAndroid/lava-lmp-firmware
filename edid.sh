#!/bin/sh

HDMI_LMP=/dev/serial/by-id/usb-Linaro_Ltd_LavaLMP_LL0c000000000777-if00

#
# To program edid:
#
# Wire a HDMI video source to P51 (non-DUT) LMP HDMI socket (just for power)
# Use HDMI-HDMI patch cable between P50 (DUT) socket and Device with EDID
# to reprogram.  Then, eg  -->
#
# ./edid.sh 00ffffffffffff0009d10180455400000815010380351e782eb7d5a456549f270c5054210800810081c08180a9c0b300d1c081c00101023a801871382d40582c4500dd0c1100001e000000ff004232423030363136534c300a20000000fd00324c1e5311000a202020202020000000fc0042656e5120424c323430300a20000f
#
# You may need to do something at the device being reprogrammed to disable
# EEPROM write protect first.

echo -n -e "\x02{\"schema\":\"org.linaro.lmp.hdmi\", \"edid\":\"$1\"}\x04" > $HDMI_LMP
