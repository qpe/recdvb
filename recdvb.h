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
#ifndef RECDVB_RECDVB_H
#define RECDVB_RECDVB_H

#include <stdbool.h>

#define NUM_BSDEV                        8
#define NUM_ISDB_T_DEV                   8
#define CHTYPE_SATELLITE                 0 /* satellite digital */
#define CHTYPE_GROUND                    1 /* terrestrial digital */
#define MAX_QUEUE                     8192
#define WRITE_SIZE       (1024 * 1024 * 2)

struct recdvb_options {
#ifdef HAVE_LIBARIB25
	bool b25;
	bool strip;
	bool emm;
	int round;
#endif
	int lnb;
	int dev_num;
	unsigned int tsid;
	char *destfile;
	char *channel;
	char *recsecstr;

	int recsec;
	bool use_stdout;
};

#endif

