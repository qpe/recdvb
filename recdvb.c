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
#include <signal.h>
#include <stdbool.h>

#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <pthread.h>
#include <unistd.h>

#include "recdvbcore.h"
#include "mkpath.h"
#include "time.h"
#include "queue.h"
#include "reader.h"
#include "preset.h"
#include "version.h"

#include "recdvb.h"

/* globals */
bool f_exit = false;

/*
 * for options
 */

static const char short_options[] = "br:smn:d:hvi:t:c";
static const struct option long_options[] = {
#ifdef HAVE_LIBARIB25
	{ "b25",       0, NULL, 'b'},
	{ "B25",       0, NULL, 'b'},
	{ "round",     1, NULL, 'r'},
	{ "strip",     0, NULL, 's'},
	{ "emm",       0, NULL, 'm'},
	{ "EMM",       0, NULL, 'm'},
#endif
	{ "LNB",       1, NULL, 'n'},
	{ "lnb",       1, NULL, 'n'},
	{ "dev",       1, NULL, 'd'},
	{ "help",      0, NULL, 'h'},
	{ "version",   0, NULL, 'v'},
	{ "tsid",      1, NULL, 't'},
	{ 0,           0, NULL,  0 } /* terminate */
};

static const char options_desc[] =
"Options:\n"
#ifdef HAVE_LIBARIB25
"  -b, --b25:               Decrypt using BCAS card\n"
"    -r, --round N:         Specify round number\n"
"    -s, --strip:           Strip null stream\n"
"    -m, --EMM:             Instruct EMM operation\n"
#endif
"  -d, --dev N:             Use DVB device /dev/dvb/adapterN\n"
"  -n, --lnb voltage:       Specify LNB voltage (0, 11, 15)\n"
"  -t, --tsid TSID:         Specify TSID in decimal or hex, hex begins '0x'\n"
"  -h, --help:              Show this help\n"
"  -v, --version:           Show version\n";

static void show_usage(char *cmd)
{
	fprintf(stderr, "Usage: \n%s "
#ifdef HAVE_LIBARIB25
		"[--b25 [--round N] [--strip] [--EMM]] "
#endif
		"[--dev devicenumber] "
		"[--lnb voltage] "
		"[--tsid TSID] "
		"channel rectime destfile\n", cmd);
	fprintf(stderr, "\n");
	fprintf(stderr, "Remarks:\n");
	fprintf(stderr, "if channel begins with 'bs##' or 'nd##', "
			"means BS/CS channel, '##' is numeric.\n");
	fprintf(stderr, "if rectime  is '-', records indefinitely.\n");
	fprintf(stderr, "if destfile is '-', stdout is used for output.\n");
}

static void show_options(void)
{
	fprintf(stderr, options_desc);
}

static void show_help(char **argv)
{
	fprintf(stderr, "\n");
	show_usage(argv[0]);
	fprintf(stderr, "\n");
	show_options();
	fprintf(stderr, "\n");
}

static void show_version(char **argv)
{
	fprintf(stderr, "%s %s\n", argv[0], version);
	fprintf(stderr, "recorder command for DVB tuner.\n");
}

static int parse_options(struct recdvb_options *opts, int argc, char **argv)
{
	int rc;
	int option_index;
	bool help = false;
	bool version = false;
	bool validation = true;

	/* set defaults */
	opts->lnb = 0;
	opts->dev_num = 0;
	opts->tsid = 0;
	opts->destfile = NULL;
	opts->channel = NULL;
	opts->recsecstr = NULL;
	opts->recsec = 0;
	opts->use_stdout = false;

	/* get option args */
	while ((rc = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (rc) {
#ifdef HAVE_LIBARIB25
		case 'b':
			opts->b25 = true;
			break;
		case 's':
			opts->strip = true;
			break;
		case 'm':
			opts->emm = true;
			break;
		case 'r':
			opts->round = atoi(optarg);
			break;
#endif
		case 'h':
			help = true;
			break;
		case 'v':
			version = true;
			break;
		/* following options require argument */
		case 'n':
			opts->lnb = atoi(optarg);
			break;
		case 'd':
			opts->dev_num = atoi(optarg);
			break;
		case 't':
			opts->tsid = (unsigned int)atoi(optarg);
			/* hex */
			if (strlen(optarg) > 2 && optarg[0] == '0' && (optarg[1] == 'X' || optarg[1] == 'x')) {
				sscanf(optarg + 2, "%x", &opts->tsid);
			}
			break;
		}
	}

	if (argc - optind < 3) {
		fprintf(stderr, "Error: Some required parameters are missing!\n");
		fprintf(stderr, "       Try '%s --help' for more information.\n", argv[0]);
		return -1;
	}

	/* get no option args */
	opts->channel = argv[optind];
	opts->recsecstr = argv[optind + 1];
	opts->destfile = argv[optind + 2];

	if (help) {
		show_help(argv);
		return 1;
	}

	if (version) {
		show_version(argv);
		return 1;
	}
	
	// check options
	if (opts->destfile && !strcmp("-", opts->destfile)) {
		opts->use_stdout = true;
	}

	switch (opts->lnb) {
	case 0:
	case 11:
	case 15:
		break;
	default:
		fprintf(stderr, "Error: Specified LNB value is not valid.\n");
		validation = false;
	}

	if (opts->tsid == 0) {
		/* update tsid when channel is BS */
		set_bs_tsid(opts->channel, &opts->tsid);
	}

	if (parse_time(opts->recsecstr, &opts->recsec) != 0) {
		fprintf(stderr, "Error: Failed to parse recsec.\n");
		validation = false;
	}

	if (!validation) {
		return -1;
	}

	return 0;
}

void show_user_input(struct recdvb_options *opts)
{
	fprintf(stderr, "Info: Specified options:\n");
	fprintf(stderr, "      Channel: %s\n", opts->channel);
	fprintf(stderr, "      Destination file: %s\n", opts->destfile);
	if (opts->recsec == -1) {
		fprintf(stderr, "      Record seconds: indefinite\n");
	} else {
		fprintf(stderr, "      Record seconds: %d\n", opts->recsec);
	}
	fprintf(stderr, "      Device Number: %d\n", opts->dev_num);
	fprintf(stderr, "      TSID: %d\n", opts->dev_num);
	fprintf(stderr, "      LNB: %dV\n", opts->lnb);
#ifdef HAVE_LIBARIB25
	fprintf(stderr, "      B25 decode: %s\n", opts->b25 ? "enable" : "disable");
	if (opts->b25) {
		fprintf(stderr, "          strip: %s\n", opts->strip ? "enable" : "disable");
		fprintf(stderr, "          emm: %s\n", opts->strip ? "enable" : "disable");
		fprintf(stderr, "          round: %d\n", opts->round);
	}
#endif
}

static void cleanup(thread_data *tdata)
{
	f_exit = true;

	pthread_cond_signal(&tdata->queue->cond_avail);
	pthread_cond_signal(&tdata->queue->cond_used);
}

/* will be signal handler thread */
static void * process_signals(void *t)
{
	sigset_t waitset;
	int sig;
	thread_data *tdata = (thread_data *)t;

	sigemptyset(&waitset);
	sigaddset(&waitset, SIGPIPE);
	sigaddset(&waitset, SIGINT);
	sigaddset(&waitset, SIGTERM);
	sigaddset(&waitset, SIGUSR1);
	sigaddset(&waitset, SIGUSR2);

	sigwait(&waitset, &sig);

	switch (sig) {
	case SIGPIPE:
		fprintf(stderr, "\nSIGPIPE received. cleaning up...\n");
		cleanup(tdata);
		break;
	case SIGINT:
		fprintf(stderr, "\nSIGINT received. cleaning up...\n");
		cleanup(tdata);
		break;
	case SIGTERM:
		fprintf(stderr, "\nSIGTERM received. cleaning up...\n");
		cleanup(tdata);
		break;
	case SIGUSR1: /* normal exit*/
		cleanup(tdata);
		break;
	case SIGUSR2: /* error */
		fprintf(stderr, "Detected an error. cleaning up...\n");
		cleanup(tdata);
		break;
	}

	return NULL; /* dummy */
}

static void init_signal_handlers(pthread_t *signal_thread, thread_data *tdata)
{
	sigset_t blockset;

	sigemptyset(&blockset);
	sigaddset(&blockset, SIGPIPE);
	sigaddset(&blockset, SIGINT);
	sigaddset(&blockset, SIGTERM);
	sigaddset(&blockset, SIGUSR1);
	sigaddset(&blockset, SIGUSR2);

	if (pthread_sigmask(SIG_BLOCK, &blockset, NULL)) {
		fprintf(stderr, "pthread_sigmask() failed.\n");
	}

	pthread_create(signal_thread, NULL, process_signals, tdata);
}

int main(int argc, char **argv)
{
	int rc;
	time_t cur_time;
	pthread_t signal_thread;
	pthread_t reader_thread;
	QUEUE_T *p_queue = create_queue(MAX_QUEUE);
	BUFSZ   *bufptr;
	static thread_data tdata;
	struct recdvb_options opts;

	/* default value */
	tdata.lnb = 0;
	tdata.tfd = -1;

#ifdef HAVE_LIBARIB25
	decoder *decoder = NULL;
	decoder_options dopt = {
		4,  /* round */
		0,  /* strip */
		0   /* emm */
	};
#endif

	rc = parse_options(&opts, argc, argv);
	if (rc == -1) {
		return 1; // exit with failure.
	}
	if (rc == 1) {
		return 0; // exit successfully.
	}

	show_user_input(&opts);

	/* open input tuner */
	if (tune(opts.channel, &tdata, opts.dev_num, opts.tsid) != 0) {
		fprintf(stderr, "Error: tuning failed.\n");
	}

	/* open output file */
	if (opts.use_stdout) {
		tdata.wfd = 1; /* stdout */
	} else {
		int status;
		char *path = strdup(opts.destfile);
		char *dir = dirname(path);
		status = mkpath(dir, 0777);
		if (status == -1) {
			fprintf(stderr, "Error: mkpath failed.\n");
		}
		free(path);

		tdata.wfd = open(opts.destfile, (O_RDWR | O_CREAT | O_TRUNC), 0666);
		if (tdata.wfd < 0) {
			fprintf(stderr, "Error: Cannot open output file.\n");
			return 1;
		}
	}

#ifdef HAVE_LIBARIB25
	/* initialize decoder */
	if (opts.b25) {
		decoder = b25_startup(&dopt);
		if (!decoder) {
			fprintf(stderr, "Error: Cannot start b25 decoder\n");
			fprintf(stderr, "       Fall back to encrypted recording\n");
			opts.b25 = false;
		}
	}
#endif

	/* prepare thread data */
	tdata.queue = p_queue;
#ifdef HAVE_LIBARIB25
	tdata.decoder = decoder;
#endif
	/* spawn signal handler thread */
	init_signal_handlers(&signal_thread, &tdata);

	/* spawn reader thread */
	tdata.signal_thread = signal_thread;
	pthread_create(&reader_thread, NULL, reader_func, &tdata);

	fprintf(stderr, "Info: Recording...\n");

	time(&tdata.start_time);

	/* read from tuner */
	while (1) {
		if (f_exit) break;

		time(&cur_time);
		bufptr = malloc(sizeof(BUFSZ));
		if (!bufptr) {
			f_exit = true;
			break;
		}
		bufptr->size = read(tdata.tfd, bufptr->buffer, MAX_READ_SIZE);
		if (bufptr->size <= 0) {
			if ((cur_time - tdata.start_time) >= opts.recsec && opts.recsec != -1) {
				f_exit = true;
				enqueue(p_queue, NULL);
				break;
			} else {
				free(bufptr);
				continue;
			}
		}
		enqueue(p_queue, bufptr);

		/* stop recording */
		time(&cur_time);
		if ((cur_time - tdata.start_time) >= opts.recsec && opts.recsec != -1) {
			break;
		}
	}

	pthread_kill(signal_thread, SIGUSR1);

	/* wait for threads */
	pthread_join(reader_thread, NULL);
	pthread_join(signal_thread, NULL);

	/* close tuner */
	close_tuner(&tdata);

	/* release queue */
	destroy_queue(p_queue);

	/* close output file */
	if (!opts.use_stdout) {
		fsync(tdata.wfd);
		close(tdata.wfd);
	}

#ifdef HAVE_LIBARIB25
	/* release decoder */
	if (opts.b25) {
		b25_shutdown(decoder);
	}
#endif

	return 0;
}

