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

#include <errno.h>
#include <getopt.h>
#include <unistd.h>

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/signalfd.h>

#include "recdvb.h"

#include "recdvbcore.h"
#include "time.h"
#include "queue.h"
#include "reader.h"
#include "preset.h"

#define NEVENTS 32
#define TUNE_TIMEOUT 5
#define READ_TIMEOUT 5

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
"Common options:\n"
"  -d, --dev N:             Use DVB device /dev/dvb/adapterN\n"
"  -h, --help:              Show this help\n"
"  -v, --version:           Show version\n"
#ifdef HAVE_LIBARIB25
"\n"
"B25 options:\n"
"  -b, --b25:               Decrypt using BCAS card\n"
"    -r, --round N:         Specify round number\n"
"    -s, --strip:           Strip null stream\n"
"    -m, --EMM:             Instruct EMM operation\n"
#endif
"\n"
"ISDB-S options:\n"
"  -n, --lnb voltage:       Specify LNB voltage (0, 11, 15, default is 0)\n"
"  -t, --tsid TSID:         Specify TSID in decimal or hex, hex begins '0x'\n";

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
	fprintf(stderr, "%s %s\n", argv[0], PACKAGE_VERSION);
	fprintf(stderr, "recorder command for DVB tuner.\n");
}

static int parse_options(struct recdvb_options *opts, int argc, char **argv)
{
	int rc;
	int option_index;
	bool help = false;
	bool version = false;
	bool validation = true;

	char *endptr = NULL;
	char *lnbstr = NULL;
	char *tsidstr = NULL;
	char *recsecstr = NULL;
	char *dev_numstr = NULL;
#ifdef HAVE_LIBARIB25
	char *roundstr = NULL;
#endif

	/* set defaults */
	opts->lnb = 0;
	opts->dev_num = 0;
	opts->tsid = 0;
	opts->destfile = NULL;
	opts->channel = NULL;
	opts->recsec = 0;
	opts->use_stdout = false;
#ifdef HAVE_LIBARIB25
	opts->b25 = false;
	opts->strip = false;
	opts->emm = false;
	opts->round = 4;
#endif

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
			roundstr = optarg;
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
			lnbstr = optarg;
			break;
		case 'd':
			dev_numstr = optarg;
			break;
		case 't':
			tsidstr = optarg;
			break;
		}
	}

	if (help) {
		show_help(argv);
		return 1;
	}

	if (version) {
		show_version(argv);
		return 1;
	}

	if (argc - optind < 3) {
		fprintf(stderr, "Error: Some required parameters are missing!\n");
		fprintf(stderr, "       Try '%s --help' for more information.\n", argv[0]);
		return -1;
	}

	/* get no option args */
	opts->channel = argv[optind];
	recsecstr = argv[optind + 1];
	opts->destfile = argv[optind + 2];
	
	/* check options */
#ifdef HAVE_LIBARIB25
	if (opts->b25 && roundstr) {
		opts->round = (int)strtol(roundstr, &endptr, 10);
		if (*endptr != '\0') {
			fprintf(stderr, "Error: Parse b25 round number failed.\n");
			validation = false;
		}
	}
#endif

	if (lnbstr) {
		opts->lnb = (int)strtol(lnbstr, &endptr, 10);
		if (*endptr != '\0') {
			fprintf(stderr, "Error: Parse LNB number failed.\n");
			validation = false;
		} else {
			switch (opts->lnb) {
			case 0:
			case 11:
			case 15:
				break;
			default:
				fprintf(stderr, "Error: Specified LNB value is not valid.\n");
				validation = false;
			}
		}
	}

	if (dev_numstr) {
		opts->dev_num = (int)strtol(dev_numstr, &endptr, 10);
		if (*endptr != '\0') {
			fprintf(stderr, "Error: Parse device number failed.\n");
			validation = false;
		} else if (opts->dev_num < 0) {
			fprintf(stderr, "Error: device number must be positive.\n");
			validation = false;
		}
	}

	if (tsidstr) {
		opts->tsid = (unsigned int)strtoul(tsidstr, &endptr, 0);
		if (*endptr != '\0') {
			fprintf(stderr, "Error: Parse TSID number failed.\n");
			validation = false;
		}
	}

	if (opts->tsid == 0) {
		/* update tsid when channel is BS */
		set_bs_tsid(opts->channel, &(opts->tsid));
	}

	if (parse_time(recsecstr, &opts->recsec) != 0) {
		fprintf(stderr, "Error: Failed to parse recsec.\n");
		validation = false;
	}

	if (opts->destfile && !strcmp("-", opts->destfile)) {
		opts->use_stdout = true;
	}

	if (!validation) {
		return -1;
	}

	return 0;
}

static void show_user_input(struct recdvb_options *opts)
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
	fprintf(stderr, "      TSID: 0x%x\n", opts->tsid);
	fprintf(stderr, "      LNB: %dV\n", opts->lnb);
#ifdef HAVE_LIBARIB25
	fprintf(stderr, "      B25 decode: %s\n", opts->b25 ? "enable" : "disable");
	if (opts->b25) {
		fprintf(stderr, "          strip: %s\n", opts->strip ? "enable" : "disable");
		fprintf(stderr, "          emm: %s\n", opts->emm ? "enable" : "disable");
		fprintf(stderr, "          round: %d\n", opts->round);
	}
#endif
}

static uint64_t diff_timespec(struct timespec *a, struct timespec *b)
{
	// make sure a >= b
	if(a->tv_sec < b->tv_sec || (a->tv_sec == b->tv_sec && a->tv_nsec < b->tv_nsec)) {
		struct timespec *c = a;
		a = b;
		b = c;
	}
	uint64_t d = a->tv_sec - b->tv_sec;
	d *= 1000;
	d += (a->tv_nsec - b->tv_nsec) / 1000000;
	return d;
}

int main(int argc, char **argv)
{
	int i, rc;
	int fefd = -1;
	int dmxfd = -1;
	int dvrfd = -1;
	int f_exit = 0;
	int tuned = 0;
	uint64_t r_byte = 0, p_r_byte = 0;
	uint64_t o_byte = 0;
	int notune_count = 0;
	int noread_count = 0;

	struct timespec cur_time = {0}, start_time = {0}, read_time = {0};
	BUFSZ   *bufptr;
	static struct recdvb_options opts;

	/* for epoll */
	int epfd = -1;
	int nfds = 0;
	struct epoll_event ev, evs[NEVENTS];

	/* for signalfd */
	int sfd = -1;
	sigset_t mask;

	/* for timerfd */
	int tfd = -1;
	struct itimerspec interval = {{1, 0}, {1, 0}};

	/* for thread */
	pthread_t reader_thread;
	static thread_data tdata = {
	};
	QUEUE_T *p_queue = create_queue(MAX_QUEUE);

	/* default value */

	rc = parse_options(&opts, argc, argv);
	if (rc == -1) {
		return 1; // exit with failure.
	}
	if (rc == 1) {
		return 0; // exit successfully.
	}

	show_user_input(&opts);

	/* create epoll event fd */
	epfd = epoll_create(NEVENTS);
	if (epfd == -1)
	{
		fprintf(stderr, "Error: failed to call epoll_create. (errno=%d)\n", errno);
		goto end;
	}

	/* create signal fd */
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGPIPE);
	sigaddset(&mask, SIGTERM);
	sigprocmask(SIG_BLOCK, &mask, NULL);
	sfd = signalfd(-1, &mask, 0);
	if (sfd == -1) {
		fprintf(stderr, "Error: cannot create signalfd. (errno=%d)\n", errno);
		goto end;
	}

	/* add epoll event source: signal */
	ev.data.fd = sfd;
	ev.events = EPOLLIN;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev) == -1) {
		fprintf(stderr, "Error: cannot add source signal fd to epoll. (errno=%d)\n", errno);
		goto end;
	}
	
	/* create timer fd */
	tfd = timerfd_create(CLOCK_REALTIME, 0);
	if (tfd == -1) {
		fprintf(stderr, "Error: cannot create timerfd. (errno=%d)\n", errno);
		goto end;
	}

	/* set timer */
	if (timerfd_settime(tfd, 0, &interval, NULL) == -1) {
		fprintf(stderr, "Error: cannot set interval time. (errno=%d)\n", errno);
		goto end;
	}

	/* add epoll event source: timer */
	ev.data.fd = tfd;
	ev.events = EPOLLIN;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, tfd, &ev) == -1) {
		fprintf(stderr, "Error: cannot add source timer fd to epoll. (errno=%d)\n", errno);
		goto end;
	}

	/* prepare thread data */
	tdata.opts = &opts;
	tdata.alive = 1;
	tdata.queue = p_queue;
	tdata.status = READER_EXIT_NOERROR;
	tdata.w_byte = 0;
	pthread_mutex_init(&tdata.mutex, NULL);

	/* spawn reader thread */
	pthread_create(&reader_thread, NULL, reader_func, &tdata);

	/* open frontend */
	fefd = open_frontend(opts.dev_num);
	if (fefd == -1) {
		goto end;
	}

	/* add epoll event source: dvb frontend */
	ev.data.fd = fefd;
	ev.events = EPOLLIN;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fefd, &ev) == -1) {
		fprintf(stderr, "Error: Cannot add source dvb frontend fd to epoll. (errno=%d)\n", errno);
		goto end;
	}

	/* tune */
	if(frontend_tune(fefd, opts.channel, opts.tsid, opts.lnb) != 0) {
		goto end;
	}

	/* open dvb demux */
	dmxfd = open_demux(opts.dev_num);
	if (dmxfd == -1) {
		goto end;
	}

	/* open dvb dvr */
	dvrfd = open_dvr(opts.dev_num);
	if (dvrfd == -1) {
		goto end;
	}

	/* add epoll event source: dvb dvr */
	ev.data.fd = dvrfd;
	ev.events = EPOLLIN;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, dvrfd, &ev) == -1) {
		fprintf(stderr, "Error: Cannot add source dvb dvr fd to epoll. (errno=%d)\n", errno);
		goto end;
	}

	clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);

	/* event loop */
	while (!f_exit) {

		/* check thread alive */
		if (pthread_mutex_trylock(&tdata.mutex) == 0) {
			if (tdata.alive == 0) {
				f_exit = 1;
			}
			pthread_mutex_unlock(&tdata.mutex);

			if (f_exit) {
				fprintf(stderr, "Info: reader thread finished.\n");
				break;
			}
		}
		
		/* wait events */
		nfds = epoll_wait(epfd, evs, NEVENTS, -1);
		if (nfds < 0) {
			fprintf(stderr, "Error: epoll_wait failed. (errno=%d)\n", errno);
			break;
		} else if (nfds == 0) {
			continue;
		}

		/* stop recording */
		clock_gettime(CLOCK_MONOTONIC_RAW, &cur_time);
		if (opts.recsec != -1 && diff_timespec(&cur_time, &start_time) / 1000 >= opts.recsec) {
			break;
		}

		/* event proc */
		for (i = 0; i < nfds; ++i) {

			if (evs[i].data.fd == sfd) {
				/* signal */
				struct signalfd_siginfo info = {0};
				read(sfd, &info, sizeof(info));

				fprintf(stderr, "\nInfo: Catch signal.\n");
				f_exit = 1;
				break;
			} else if (evs[i].data.fd == tfd) {
				/* timer */
				uint64_t exp;
				read(tfd, &exp, sizeof(uint64_t));

				if (tuned == 0) {
					/* check timeout */
					notune_count++;
					if (notune_count < TUNE_TIMEOUT) {
						continue;
					}
					f_exit = 1;
					fprintf(stderr, "Error: Tune timeout.\n");
					break;
				} else {
					uint64_t w_byte = 0;
					/* show stats */
					frontend_show_stats(fefd);
					if (pthread_mutex_trylock(&tdata.mutex) == 0) {
						w_byte = tdata.w_byte;
						pthread_mutex_unlock(&tdata.mutex);
					}
					fprintf(stderr, "      Read %lubyte, Write %lubyte, Overrun %lubyte\n", r_byte, w_byte, o_byte);

					/* check timeout */
					if (p_r_byte == r_byte) {
						noread_count++;
						if (noread_count < READ_TIMEOUT) {
							continue;
						}
						f_exit = 1;
						fprintf(stderr, "Error: Read timeout.\n");
						break;
					} else {
						noread_count = 0;
						p_r_byte = r_byte;
					}
				}
			} else if (evs[i].data.fd == fefd) {
				/* frontend */
				if (tuned != 0) {
					continue;
				}
				if (frontend_locked(fefd) != 0) {
					continue;
				}
				
				tuned = 1;

				/* demux start */
				if (demux_start(dmxfd) != 0) {
					f_exit = 1;
					fprintf(stderr, "Error: Cannot start demux.\n");
					break;
				}

				/* remove from epoll */
				if (epoll_ctl(epfd, EPOLL_CTL_DEL, fefd, NULL) != 0) {
					f_exit = 1;
					fprintf(stderr, "Error: Remove fd from epoll failed. (errno=%d)\n", errno);
					break;
				}

				/* show current frequency */
				frontend_show_frequency(fefd);

			} else if (evs[i].data.fd == dvrfd) {
				/* dvr */

				/* make sure event is EPOLLIN */
				if (!(evs[i].events & EPOLLIN)) {
					continue;
				}

				/* allocate memory for read data from dvr */
				bufptr = malloc(sizeof(BUFSZ));
				if (!bufptr) {
					f_exit = 1;
					fprintf(stderr, "Error: Cannot allocate buffer memory.\n");
					break;
				}

				/* read dvr */
				bufptr->size = read(dvrfd, bufptr->buffer, MAX_READ_SIZE);
				if (bufptr->size <= 0) {
					free(bufptr);
					continue;
				}

				/* insert data to ring buffer */
				if (enqueue(p_queue, bufptr) != 0) {
					/* queue is full, dropped */
					free(bufptr);
					o_byte += bufptr->size;
				}

				/* set first read time */
				if (r_byte == 0) {
					clock_gettime(CLOCK_MONOTONIC_RAW, &read_time);
				}

				/* count up total read size */
				r_byte += bufptr->size;
			}
		}
	} /* while (!f_exit) */

	/* show record time info */
	fprintf(stderr, "Info: Elapsed time %.2lfsec\n", diff_timespec(&cur_time, &start_time) / 1000.0);
	if (read_time.tv_sec > 0 || read_time.tv_nsec) {
		/* tuning successful. */
		fprintf(stderr, "      (Tuning %.2lfsec)\n", diff_timespec(&read_time, &start_time) / 1000.0);
	}

end:

	/* tell exit to thread. */
	while (enqueue(p_queue, NULL) != 0) {
		/* when enqueue timeout, check thread alive */
		int thread_alive = 1;
		if (pthread_mutex_trylock(&tdata.mutex) == 0) {
			if (tdata.alive == 0) {
				thread_alive = 0;
			}
			pthread_mutex_unlock(&tdata.mutex);

			if (thread_alive == 0) {
				break;
			}
		}
	}

	/* close epoll */
	if (epfd != -1) {
		close(epfd);
	}

	/* close dvr/dmx/frontend anyway */
	if (dvrfd != -1) {
		close(dvrfd);
	}

	if (dmxfd != -1) {
		close(dmxfd);
	}

	if (fefd != -1) {
		close(fefd);
	}
	
	/* close timerfd */
	if (tfd != -1) {
		close(tfd);
	}

	/* close signalfd */
	if (sfd != -1) {
		close(sfd);
	}

	/* wait for threads */
	pthread_join(reader_thread, NULL);

	/* release queue */
	destroy_queue(p_queue);

	/* show status */
	fprintf(stderr, "Info: Read %lubyte, Write %lubyte, Overrun %lubyte\n", r_byte, tdata.w_byte, o_byte);

	return 0;
}

