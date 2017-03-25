#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/ipc.h>
#include <sys/msg.h>

#include <getopt.h>

#include "recdvbcore.h"
#include "time.h"

#define MSGSZ     255

extern bool f_exit;

static void show_usage(char *cmd)
{
	fprintf(stderr, "Usage: \n%s --pid pid [--channel channel] [--sid SID1,SID2] [--tsid TSID] [--extend time_to_extend] [--time recording_time]\n", cmd);
	fprintf(stderr, "\n");
}

static void show_options()
{
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "--pid:               Process id of recdvb to control\n");
	fprintf(stderr, "--channel:           Tune to specified channel\n");
	fprintf(stderr, "--sid SID1,SID2,...: Specify SID number in CSV format (101,102,...)\n");
	fprintf(stderr, "--tsid TSID:         Specify TSID in decimal or hex, hex begins '0x'\n");
	fprintf(stderr, "--extend:            Extend recording time\n");
	fprintf(stderr, "--time:              Set total recording time\n");
	fprintf(stderr, "--help:              Show this help\n");
	fprintf(stderr, "--version:           Show version\n");
}

int main(int argc, char **argv)
{
	int msqid;
	int msgflg = IPC_CREAT | 0666;
	key_t key = 0;
	int recsec = 0, extsec=0;
	char *channel = NULL;
	message_buf sbuf;
	size_t buf_length;
	char *sid_list = NULL;

	int result;
	int option_index;
	unsigned int tsid = 0;
	struct option long_options[] = {
		{ "pid",     1, NULL, 'p'},
		{ "channel", 1, NULL, 'c'},
		{ "sid",     1, NULL, 'i'},
		{ "tsid",    1, NULL, 's'},
		{ "extend",  1, NULL, 'e'},
		{ "time",    1, NULL, 't'},
		{ "help",    0, NULL, 'h'},
		{ "version", 0, NULL, 'v'},
		{ 0,         0, NULL, 0}    /* terminate */
	};

	while((result = getopt_long(argc, argv, "p:c:i:s:e:t:hvl", long_options, &option_index)) != -1) {
		switch(result) {
		case 'h':
			fprintf(stderr, "\n");
			show_usage(argv[0]);
			fprintf(stderr, "\n");
			show_options();
			fprintf(stderr, "\n");
			exit(0);
			break;
		case 'v':
			fprintf(stderr, "%s %s\n", argv[0], version);
			fprintf(stderr, "control command for recdvb.\n");
			exit(0);
			break;
		/* following options require argument */
		case 'p':
			key = (key_t)atoi(optarg);
			fprintf(stderr, "Pid = %d\n", key);
			break;
		case 'c':
			channel = optarg;
			fprintf(stderr, "Channel = %s\n", channel);
			break;
		case 'e':
			parse_time(optarg, &extsec);
			fprintf(stderr, "Extend %d sec\n", extsec);
			break;
		case 't':
			parse_time(optarg, &recsec);
			fprintf(stderr, "Total recording time = %d sec\n", recsec);
			break;
		case 'i':
			sid_list = optarg;
			fprintf(stderr, "Service ID = %s\n", sid_list);
			break;
		case 's':
			tsid = atoi(optarg);
			if(strlen(optarg) > 2)
				if((optarg[0] == '0') && ((optarg[1] == 'X') || (optarg[1] == 'x')))
					sscanf(optarg + 2, "%x", &tsid);
			fprintf(stderr, "tsid = 0x%x\n", tsid);
			break;
		}
	}

	if(!key) {
		fprintf(stderr, "Some required parameters are missing!\n");
		fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
		exit(1);
	}

	if ((msqid = msgget(key, msgflg )) < 0) {
		perror("msgget");
		exit(1);
	}

	sbuf.mtype = 1;
	sprintf(sbuf.mtext, "ch=%s t=%d e=%d sid=%s tsid=%d", channel, recsec, extsec, sid_list, tsid);

	buf_length = strlen(sbuf.mtext) + 1 ;

	if (msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT) < 0) {
		perror("msgsnd");
		exit(1);
	}

	exit(0);
}
