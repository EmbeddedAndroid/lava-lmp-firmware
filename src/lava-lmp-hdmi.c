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

void lava_lmp_hdmi(unsigned char c)
{
	switch (rx_state) {
	case CMD:
		switch (c) {
		case '?':
			usb_queue_string("lava-lmp-hdmi 1 1.0\n");
			break;
		case 'H':
			rx_state = HPD;
			break;
		case 'V':
			/* todo AD7 */
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

