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
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>

#include "recdvbcore.h"
#include "mkpath.h"
#include "version.h"

extern bool f_exit;

const static char *_voltage[] = {"0V", "11V", "15V"};

static void cleanup()
{
	f_exit = true;
}

/* will be signal handler thread */
static void * process_signals()
{
	sigset_t waitset;
	int sig;

	sigemptyset(&waitset);
	sigaddset(&waitset, SIGINT);
	sigaddset(&waitset, SIGTERM);
	sigaddset(&waitset, SIGUSR1);

	sigwait(&waitset, &sig);

	switch(sig) {
	case SIGINT:
		fprintf(stderr, "\nSIGINT received. cleaning up...\n");
		cleanup();
		break;
	case SIGTERM:
		fprintf(stderr, "\nSIGTERM received. cleaning up...\n");
		cleanup();
		break;
	case SIGUSR1: /* normal exit*/
		cleanup();
		break;
	}

	return NULL; /* dummy */
}

static void init_signal_handlers(pthread_t *signal_thread)
{
	sigset_t blockset;

	sigemptyset(&blockset);
	sigaddset(&blockset, SIGINT);
	sigaddset(&blockset, SIGTERM);
	sigaddset(&blockset, SIGUSR1);

	if(pthread_sigmask(SIG_BLOCK, &blockset, NULL))
		fprintf(stderr, "pthread_sigmask() failed.\n");

	pthread_create(signal_thread, NULL, process_signals, NULL);
}

static void show_usage(char *cmd)
{
	fprintf(stderr, "Usage: \n%s [--dev devicenumber] channel\n", cmd);
	fprintf(stderr, "\n");
}

static void show_options(void)
{
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "--dev N:             Use DVB device /dev/dvb/adapterN\n");
	fprintf(stderr, "--lnb voltage:       Specify LNB voltage (0, 11, 15)\n");
	fprintf(stderr, "--help:              Show this help\n");
	fprintf(stderr, "--version:           Show version\n");
}

int main(int argc, char **argv)
{
	pthread_t signal_thread;
	static thread_data tdata;
	int result;
	int option_index;
	int val;
	struct option long_options[] = {
		{ "help",    0, NULL, 'h'},
		{ "version", 0, NULL, 'v'},
		{ "LNB",     1, NULL, 'n'},
		{ "lnb",     1, NULL, 'n'},
		{ "dev",     1, NULL, 'd'},
		{ 0,         0, NULL,  0 } /* terminate */
	};

	int dev_num = 0;

	while((result = getopt_long(argc, argv, "bhvln:d:", long_options, &option_index)) != -1) {
		switch(result) {
		case 'h':
			fprintf(stderr, "\n");
			show_usage(argv[0]);
			fprintf(stderr, "\n");
			show_options();
			fprintf(stderr, "\n");
			exit(0);
		case 'v':
			fprintf(stderr, "%s %s\n", argv[0], version);
			fprintf(stderr, "signal check utility for DVB tuner.\n");
			exit(0);
		/* following options require argument */
		case 'd':
			dev_num = atoi(optarg);
			break;
		case 'n':
			val = atoi(optarg);
			switch(val) {
			case 11:
				tdata.lnb = 1;
				break;
			case 15:
				tdata.lnb = 2;
				break;
			default:
				tdata.lnb = 0;
				break;
			}
			fprintf(stderr, "LNB = %s\n", _voltage[tdata.lnb]);
			break;
		}
	}

	if(argc - optind < 1) {
		fprintf(stderr, "channel must be specified!\n");
		fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
		return 1;
	}

	/* set tune_persistent flag */
	tdata.tune_persistent = true;

	/* spawn signal handler thread */
	init_signal_handlers(&signal_thread);

	/* tune */
	if(tune(argv[optind], &tdata, dev_num, 0) != 0)
		return 1;

	while(1) {
		if(f_exit)
			break;
		/* show signal strength */
		calc_cn();
		sleep(1);
	}

	/* wait for signal thread */
	pthread_kill(signal_thread, SIGUSR1);
	pthread_join(signal_thread, NULL);

	/* close tuner */
	close_tuner(&tdata);

	return 0;
}
