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
#include <stdbool.h>

#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <libgen.h>
#include <fcntl.h>

#include "reader.h"

#include "mkpath.h"
#include "decoder.h"
#include "recdvbcore.h"

/* maximum write length at once */
#define SIZE_CHANK 1316

/* this function will be reader thread */
void *reader_func(void *p)
{
	thread_data *tdata = (thread_data *)p;
	QUEUE_T *p_queue = tdata->queue;
	struct recdvb_options *opts = tdata->opts;
	int wfd = -1;
#ifdef HAVE_LIBARIB25
	int code;
	int use_b25 = 0;
	decoder *decoder = NULL;
	decoder_options dopt = {
		opts->round,
		opts->strip ? 1 : 0,
		opts->emm ? 1 : 0
	};
	ARIB_STD_B25_BUFFER dbuf;
#endif
	BUFSZ *qbuf;
	ARIB_STD_B25_BUFFER sbuf, buf;

	buf.size = 0;
	buf.data = NULL;

#ifdef HAVE_LIBARIB25
	/* initialize decoder */
	if (opts->b25) {
		decoder = b25_startup(&dopt);
		if (decoder == NULL) {
			tdata->status = READER_EXIT_EINIT_DECODER;
			goto end;
		}
		fprintf(stderr, "Info: B25 startup successfully.\n");
		use_b25 = 1;
	}
#endif

	/* open output file */
	if (opts->use_stdout) {
		wfd = 1; /* stdout */
	} else {
		int status;
		char *path = strdup(opts->destfile);
		char *dir = dirname(path);
		status = mkpath(dir, 0777);
		if (status == -1) {
			tdata->status = READER_EXIT_EMKPATH;
			goto end;
		}
		free(path);

		wfd = open(opts->destfile, O_RDWR | O_CREAT | O_TRUNC, 0666);
		if (wfd < 0) {
			tdata->status = READER_EXIT_EOPEN_DESTFILE;
			goto end;
		}
	}

	while (1) {
		ssize_t wc = 0;
		int file_err = 0;

		if (dequeue(p_queue, &qbuf) != 0) {
			/* no queue timeout */
			tdata->status = READER_EXIT_TIMEOUT;
			break;
		}

		/* normal exit */
		if (qbuf == NULL) {
			break;
		}

		sbuf.data = qbuf->buffer;
		sbuf.size = (int32_t)qbuf->size;

		buf = sbuf; /* default */

#ifdef HAVE_LIBARIB25
		if (use_b25) {
			code = b25_decode(decoder, &sbuf, &dbuf);
			if (code < 0) {
				fprintf(stderr, "Error: b25_decode failed (code=%d).\n", code);
				fprintf(stderr, "       fall back to encrypted recording.\n");
				use_b25 = 0;
			} else {
				buf = dbuf;
			}
		}
#endif

		/* write data to output file */
		int size_remain = buf.size;
		int offset = 0;

		while (size_remain > 0) {
			size_t ws = size_remain < SIZE_CHANK ? (size_t)size_remain : SIZE_CHANK;

			wc = write(wfd, buf.data + offset, ws);
			if (wc < 0) {
				file_err = 1;
				break;
			}
			size_remain -= wc;
			offset += wc;
		}

		free(qbuf);
		qbuf = NULL;
		
		/* count up */
		if (pthread_mutex_lock(&tdata->mutex) == 0) {
			int written = buf.size - size_remain;

			if (written > 0) {
				tdata->w_byte += (uint64_t)written;
			}

			pthread_mutex_unlock(&tdata->mutex);
		}

		/* cannot write file */
		if (file_err) {
			break;
		}

	} /* while (1) */

end:

#ifdef HAVE_LIBARIB25
	if (use_b25) {
		code = b25_finish(decoder, &dbuf);
		if (code < 0) {
			tdata->status = READER_EXIT_EB25FINISH;
		}
	}
#endif

	/* close output file */
	if (wfd > 0 && !opts->use_stdout) {
		fsync(wfd);
		close(wfd);
	}

#ifdef HAVE_LIBARIB25
	/* release decoder */
	if (decoder != NULL) {
		b25_shutdown(decoder);
	}
#endif

	if (pthread_mutex_lock(&tdata->mutex) == 0) {
		tdata->alive = 0;
		pthread_mutex_unlock(&tdata->mutex);
	}

	return NULL;
}

void reader_show_error(enum reader_exit_status s)
{
	switch (s) {
	case READER_EXIT_EINIT_DECODER:
		fprintf(stderr, "Error: Cannot start b25 decoder\n");
		fprintf(stderr, "       Fall back to encrypted recording\n");
		break;
	}
}

