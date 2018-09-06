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
#ifndef RECDVB_RECDVBCORE_H
#define RECDVB_RECDVBCORE_H

#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#include "decoder.h"
#include "queue.h"

/* type definitions */
typedef struct thread_data {
	int tfd;    /* tuner fd */       //xxx variable

	int wfd;    /* output file fd */ //invariable
	int lnb;    /* LNB voltage */    //invariable
	time_t start_time;               //invariable

	int recsec;                      //xxx variable

	bool indefinite;                 //invaliable

	QUEUE_T *queue;                  //invariable
	pthread_t signal_thread;         //invariable
#ifdef HAVE_LIBARIB25
	decoder *decoder;                //invariable
#endif
} thread_data;

/* prototypes */
int tune(char *channel, thread_data *tdata, int dev_num, unsigned int tsid);
void close_tuner(thread_data *tdata);
void calc_cn(void);

#endif

