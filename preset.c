/*
 * recdvb - record tool for linux DVB driver.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "preset.h"

#define NUM_PRESET_CH 88
static const struct {
	char *channel;
	unsigned int tsid;
} preset_ch[NUM_PRESET_CH] = {
	{ "bs01_0", 0x4010 },
	{ "bs01_1", 0x4011 },
	{ "bs01_2", 0x4012 },

	{ "bs03_0", 0x4030 },
	{ "bs03_1", 0x4031 },
	{ "bs03_2", 0x4632 },

	{ "bs05_0", 0x4450 },
	{ "bs05_1", 0x4451 },

	{ "bs09_0", 0x4090 },
	{ "bs09_1", 0x4091 },
	{ "bs09_2", 0x4092 },

	{ "bs11_0", 0x46b0 },
	{ "bs11_1", 0x46b1 },
	{ "bs11_2", 0x46b2 },

	{ "bs13_0", 0x40d0 },
	{ "bs13_1", 0x40d1 },
	{ "bs13_2", 0x46d2 },

	{ "bs15_0", 0x40f1 },
	{ "bs15_1", 0x40f2 },

	{ "bs19_0", 0x4730 },
	{ "bs19_1", 0x4731 },
	{ "bs19_2", 0x4732 },

	{ "bs21_0", 0x4750 },
	{ "bs21_1", 0x4751 },
	{ "bs21_2", 0x4752 },

	{ "bs23_0", 0x4770 },
	{ "bs23_1", 0x4771 },
	{ "bs23_2", 0x4772 },

	/* nd 54ch (12pattern) */
	{ "nd02", 0x6020 }, { "nd04", 0x7040 },
	{ "nd06", 0x7060 }, { "nd08", 0x6080 },
	{ "nd10", 0x60a0 }, { "nd12", 0x70c0 },
	{ "nd14", 0x70e0 }, { "nd16", 0x7100 },
	{ "nd18", 0x7120 }, { "nd20", 0x7140 },
	{ "nd22", 0x7160 }, { "nd24", 0x7180 }
};

void set_bs_tsid(char *pch, unsigned int *tsid)
{
	int i;
	char channel[8] = {0};
	if (((pch[0] == 'b')|| (pch[0] == 'B')) && ((pch[1] == 's') || (pch[1] == 'S'))) {
		channel[0] = 'b';
		channel[1] = 's';
	} else {
		*tsid = 0;
		return;
	}
	for (i = 2; i < 7 && pch[i] != '\0'; i++) {
		channel[i] = pch[i];
	}
	pch[i] = '\0';

	for (i = 0; i < NUM_PRESET_CH; i++)
		if (strcmp(preset_ch[i].channel, channel) == 0) break;

	if (i == NUM_PRESET_CH) {
		*tsid = 0;
	} else {
		*tsid = preset_ch[i].tsid;
		fprintf(stderr, "tsid = 0x%04x\n", *tsid);
	}
}
