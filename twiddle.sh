#!/bin/sh

LMP=/dev/serial/by-id/usb-Linaro_Ltd_LavaLMP_LL09123000000123-if00

stty -F $LMP -isig  -icanon -iexten -echo -xcase -echoe -echok -echonl -echoctl -echoke -inlcr -ignbrk -ignpar -igncr -icrnl -imaxbel -ixon -ixoff -ixany -iuclc -onlcr -opost -olcuc -ocrnl -onlret min 1 time 0 eof 1 ispeed 115200 ospeed 115200 -cstopb -crtscts -parenb cread clocal cs8

# need to do this to make it work
echo -n -e "\x02{\"schema\":\"org.linaro.lmp.info\"}\x04" > $LMP

while [ 1 ] ; do
	echo -n -e "\x02{\"schema\":\"org.linaro.lmp.lsgpio\", \"modes\":[{\"name\":\"a-dir\",\"option\": \"out\"},{\"name\":\"a-data\",\"option\":\"aa\"}]}\x04" > $LMP
	echo -n -e "\x02{\"schema\":\"org.linaro.lmp.lsgpio\", \"modes\":[{\"name\":\"a-dir\",\"option\": \"out\"},{\"name\":\"a-data\",\"option\":\"55\"}]}\x04" > $LMP

done

