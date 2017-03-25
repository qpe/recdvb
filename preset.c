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
	int lch;
	char *channel;
	unsigned int tsid;
} preset_ch[NUM_PRESET_CH] = {
	{ 101, "bs15_0", 0x40f1 }, { 103, "bs15_1", 0x40f2 },
	{ 141, "bs13_0", 0x40d0 }, { 151, "bs01_0", 0x4010 },
	{ 161, "bs01_1", 0x4011 }, { 171, "bs03_1", 0x4031 },
	{ 181, "bs13_1", 0x40d1 }, { 191, "bs03_0", 0x4030 },
	{ 192, "bs05_0", 0x4450 }, { 193, "bs05_1", 0x4451 },
	{ 200, "bs09_1", 0x4091 }, { 201, "bs07_0", 0x4470 },
	{ 202, "bs07_0", 0x4470 }, { 211, "bs09_0", 0x4490 },
	{ 222, "bs09_2", 0x4092 }, { 231, "bs11_2", 0x46b2 },
	{ 232, "bs11_2", 0x46b2 }, { 233, "bs11_2", 0x46b2 },
	{ 234, "bs19_0", 0x4730 }, { 236, "bs07_1", 0x4671 },
	{ 238, "bs11_0", 0x46b0 }, { 241, "bs11_1", 0x46b1 },
	{ 242, "bs19_1", 0x4731 }, { 243, "bs19_2", 0x4732 },
	{ 244, "bs21_1", 0x4751 }, { 245, "bs21_2", 0x4752 },
	{ 251, "bs23_0", 0x4770 }, { 252, "bs21_0", 0x4750 },
	{ 255, "bs23_1", 0x4771 }, { 256, "bs07_2", 0x4672 },
	{ 258, "bs23_2", 0x4772 }, { 531, "bs11_2", 0x46b2 },
	{ 910, "bs15_1", 0x40f2 }, { 929, "bs15_0", 0x40f1 },
	{ 296, "nd02", 0x6020 }, { 298, "nd02", 0x6020 },
	{ 299, "nd02", 0x6020 }, { 100, "nd04", 0x7040 },
	{ 223, "nd04", 0x7040 }, { 227, "nd04", 0x7040 },
	{ 250, "nd04", 0x7040 }, { 342, "nd04", 0x7040 },
	{ 363, "nd04", 0x7040 }, { 294, "nd06", 0x7060 },
	{ 323, "nd06", 0x7060 }, { 329, "nd06", 0x7060 },
	{ 340, "nd06", 0x7060 }, { 341, "nd06", 0x7060 },
	{ 354, "nd06", 0x7060 }, {  55, "nd08", 0x6080 },
	{ 218, "nd08", 0x6080 }, { 219, "nd08", 0x6080 },
	{ 326, "nd08", 0x6080 }, { 339, "nd08", 0x6080 },
	{ 800, "nd10", 0x60a0 }, { 801, "nd10", 0x60a0 },
	{ 802, "nd10", 0x60a0 }, { 805, "nd10", 0x60a0 },
	{ 254, "nd12", 0x70c0 }, { 325, "nd12", 0x70c0 },
	{ 330, "nd12", 0x70c0 }, { 292, "nd14", 0x70e0 },
	{ 293, "nd14", 0x70e0 }, { 310, "nd14", 0x70e0 },
	{ 290, "nd16", 0x7100 }, { 305, "nd16", 0x7100 },
	{ 311, "nd16", 0x7100 }, { 333, "nd16", 0x7100 },
	{ 343, "nd16", 0x7100 }, { 353, "nd16", 0x7100 },
	{ 240, "nd18", 0x7120 }, { 262, "nd18", 0x7120 },
	{ 314, "nd18", 0x7120 }, { 307, "nd20", 0x7140 },
	{ 308, "nd20", 0x7140 }, { 309, "nd20", 0x7140 },
	{ 161, "nd22", 0x7160 }, { 297, "nd22", 0x7160 },
	{ 312, "nd22", 0x7160 }, { 322, "nd22", 0x7160 },
	{ 331, "nd22", 0x7160 }, { 351, "nd22", 0x7160 },
	{ 257, "nd24", 0x7180 }, { 229, "nd24", 0x7180 },
	{ 300, "nd24", 0x7180 }, { 321, "nd24", 0x7180 },
	{ 350, "nd24", 0x7180 }, { 362, "nd24", 0x7180 }
};

void set_lch(char *lch, char **ppch, char **sid, unsigned int *tsid)
{
	int i, ch;
	ch = atoi(lch);
	for (i = 0; i < NUM_PRESET_CH; i++)
		if (preset_ch[i].lch == ch) break;

	if (i < NUM_PRESET_CH ) {
		*ppch = preset_ch[i].channel;
		if (*tsid == 0) *tsid = preset_ch[i].tsid;
		if (*sid == NULL) *sid = lch;
	}
}

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
