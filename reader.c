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
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <pthread.h>

#include "queue.h"
#include "recdvbcore.h"

#include "reader.h"

/* maximum write length at once */
#define SIZE_CHANK 1316

extern bool f_exit;

/* this function will be reader thread */
void *reader_func(void *p)
{
	thread_data *tdata = (thread_data *)p;
	QUEUE_T *p_queue = tdata->queue;
	int wfd = tdata->wfd;
#ifdef HAVE_LIBARIB25
	int code;
	decoder *dec = tdata->decoder;
	bool use_b25 = dec ? true : false;
	ARIB_STD_B25_BUFFER dbuf;
#endif
	pthread_t signal_thread = tdata->signal_thread;
	BUFSZ *qbuf;
	ARIB_STD_B25_BUFFER sbuf, buf;

	buf.size = 0;
	buf.data = NULL;

	if (wfd == -1)
		return NULL;

	while (1) {
		ssize_t wc = 0;
		int file_err = 0;
		qbuf = dequeue(p_queue);
		/* no entry in the queue */
		if (qbuf == NULL) {
			break;
		}

		sbuf.data = qbuf->buffer;
		sbuf.size = (int32_t)qbuf->size;

		buf = sbuf; /* default */
#ifdef HAVE_LIBARIB25
		if (use_b25) {
			code = b25_decode(dec, &sbuf, &dbuf);
			if (code < 0) {
				fprintf(stderr, "b25_decode failed (code=%d).", code);
				fprintf(stderr, " fall back to encrypted recording.\n");
				use_b25 = false;
			}
			else
				buf = dbuf;
		}
#endif

		/* write data to output file */
		int size_remain = buf.size;
		int offset = 0;

		while (size_remain > 0) {
			size_t ws = size_remain < SIZE_CHANK ? (size_t)size_remain : SIZE_CHANK;

			wc = write(wfd, buf.data + offset, ws);
			if (wc < 0) {
				perror("write");
				file_err = 1;
				pthread_kill(signal_thread,
					errno == EPIPE ? SIGPIPE : SIGUSR2);
				break;
			}
			size_remain -= wc;
			offset += wc;
		}

		free(qbuf);
		qbuf = NULL;

		/* normal exit */
		if ((f_exit && !p_queue->num_used) || file_err) {

			buf = sbuf; /* default */

#ifdef HAVE_LIBARIB25
			if (use_b25) {
				code = b25_finish(dec, &dbuf);
				if(code < 0)
					fprintf(stderr, "b25_finish failed\n");
				else
					buf = dbuf;
			}
#endif

			if (!file_err) {
				wc = write(wfd, buf.data, (size_t)buf.size);
				if (wc < 0) {
					perror("write");
					file_err = 1;
					pthread_kill(signal_thread, errno == EPIPE ? SIGPIPE : SIGUSR2);
				}
			}

			break;
		}
	} /* while (1) */

	time_t cur_time;
	time(&cur_time);
	fprintf(stderr, "Recorded %dsec\n", (int)(cur_time - tdata->start_time));

	return NULL;
}

