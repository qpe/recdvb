#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

#include "version.h"
#include "queue.h"

#include "recdvbcore.h"

#define ISDB_T_NODE_LIMIT 24        // 32:ARIB limit 24:program maximum
#define ISDB_T_SLOT_LIMIT 8

/* globals */
bool f_exit = false;
char bs_channel_buf[8];

static int fefd = 0;
static int dmxfd = 0;

void close_tuner(thread_data *tdata)
{
	if(fefd > 0){
		close(fefd);
		fefd = 0;
	}
	if(dmxfd > 0){
		close(dmxfd);
		dmxfd = 0;
	}
	if(tdata->tfd == -1)
		return;

	close(tdata->tfd);
	tdata->tfd = -1;
}

void calc_cn(void)
{
	  int strength = 0;
	  ioctl(fefd, FE_READ_SNR, &strength);
	  fprintf(stderr, "SNR: %d\n", strength);
}

/*
 * success -> return 0
 */
static int open_tuner(int dev_num, struct dvb_frontend_info *fe_info)
{
	char device[32] = {0};

	if(fefd == 0){
		sprintf(device, "/dev/dvb/adapter%d/frontend0", dev_num);
		fefd = open(device, O_RDWR);
		if(fefd < 0) {
			fprintf(stderr, "cannot open frontend device\n");
			fefd = 0;
			return 1;
		}
		fprintf(stderr, "device = %s\n", device);
	}

	if ( (ioctl(fefd,FE_GET_INFO, fe_info) < 0)){
		fprintf(stderr, "FE_GET_INFO failed\n");
		return 1;
	}

	if((fe_info->type != FE_OFDM) &&
	   (fe_info->type != FE_QPSK)){
		fprintf(stderr, "type is not supported\n");
		return 1;
	}

	return 0;
}

/* from checksignal.c */
int tune(char *channel, thread_data *tdata, int dev_num, unsigned int tsid)
{
	struct dtv_property prop[3];
	struct dtv_properties props;
	struct dvb_frontend_info fe_info;
	int fe_freq;
	int rc, i;
	struct dmx_pes_filter_params filter;
	struct dvb_frontend_event event;
	struct pollfd pfd[1];
	char device[32];

	/* open tuner */
	rc = open_tuner(dev_num, &fe_info);
	if(rc != 0) return 1;

	fprintf(stderr,"Using DVB card \"%s\"\n",fe_info.name);
	prop[0].cmd = DTV_FREQUENCY;
	if(fe_info.type == FE_OFDM){
		if( (fe_freq = atoi(channel)) == 0){
			fprintf(stderr, "channel is not number\n");
			return 1;
		}
		prop[0].u.data = (fe_freq * 6000 + 395143) * 1000;
		fprintf(stderr,"tuning to %d kHz\n",prop[0].u.data / 1000);
	} else {
		/* FE_QPSK */
		if( ((channel[0] == 'b') || (channel[0] == 'B')) &&
		    ((channel[1] == 's') || (channel[1] == 'S')) ){
			if( (fe_freq = atoi(channel+2)) == 0){
				fprintf(stderr, "channel is not BSnn\n\tnn=numeric\n");
				return 1;
			}
			/* BS */
			prop[0].u.data = fe_freq * 19180 + 1030300;
		} else if( ((channel[0] == 'n')||(channel[0] == 'N')) &&
			   ((channel[1] == 'd')||(channel[1] == 'D')) ){
			if( (fe_freq = atoi(channel + 2)) == 0){
				fprintf(stderr, "channel is not NDnn\n\tnn=numeric\n");
				return 1;
			}
			/* ND */
			prop[0].u.data = fe_freq * 20000 + 1573000;
		} else {
			fprintf(stderr, "channel is invalid\n");
			return 1;
		}
		fprintf(stderr,"tuning to %d MHz\n",prop[0].u.data / 1000);
	}
#ifdef DTV_STREAM_ID
	prop[1].cmd = DTV_STREAM_ID;
#else
	prop[1].cmd = DTV_ISDBS_TS_ID;
#endif
	prop[1].u.data = tsid;
	prop[2].cmd = DTV_TUNE;

	props.props = prop;
	props.num = 3;

	if (ioctl(fefd, FE_SET_PROPERTY, &props) == -1) {
		perror("ioctl FE_SET_PROPERTY failed.\n");
		return 1;
	}

	pfd[0].fd = fefd;
	pfd[0].events = POLLIN;
	event.status=0;
	fprintf(stderr,"polling");
	for (i = 0; i < 5; i++) {
		if ((event.status & FE_TIMEDOUT) != 0) break;
		if ((event.status & FE_HAS_LOCK) != 0) break;

		fprintf(stderr, ".");
		if (!poll(pfd,1,5000)) continue;
		if (!(pfd[0].revents & POLLIN)) continue;
		  if ((rc = ioctl(fefd, FE_GET_EVENT, &event)) < 0){
		  	if (errno != EOVERFLOW) {
				perror("ioctl FE_GET_EVENT");
				fprintf(stderr,"status = %d\n", rc);
				fprintf(stderr,"errno = %d\n", errno);
				return -1;
			} else {
				fprintf(stderr, "\n");
				fprintf(stderr, "Overflow error, trying again ");
				fprintf(stderr, "(status = %d, errno = %d)", rc, errno);
			}
		}
	}

	if ((event.status & FE_HAS_LOCK) == 0) {
		fprintf(stderr, "\nCannot lock to the signal on the given channel\n");
		return 1;
	}

	fprintf(stderr, "ok\n");

	if(dmxfd == 0){
		sprintf(device, "/dev/dvb/adapter%d/demux0", dev_num);
		if((dmxfd = open(device, O_RDWR)) < 0){
			dmxfd = 0;
			fprintf(stderr, "cannot open demux device\n");
			return 1;
		}
	}

	filter.pid = 0x2000;
	filter.input = DMX_IN_FRONTEND;
	filter.output = DMX_OUT_TS_TAP;
//	filter.pes_type = DMX_PES_OTHER;
	filter.pes_type = DMX_PES_VIDEO;
	filter.flags = DMX_IMMEDIATE_START;
	if (ioctl(dmxfd, DMX_SET_PES_FILTER, &filter) == -1) {
		fprintf(stderr,"FILTER %i: ", filter.pid);
		perror("ioctl DMX_SET_PES_FILTER");
		close(dmxfd);
		dmxfd = 0;
		return 1;
	}

	if(tdata->tfd < 0){
		sprintf(device, "/dev/dvb/adapter%d/dvr0", dev_num);
		if((tdata->tfd = open(device, O_RDONLY)) < 0){
//		if((tdata->tfd = open(device, O_RDONLY|O_NONBLOCK)) < 0){
			fprintf(stderr, "cannot open dvr device\n");
			close(dmxfd);
			dmxfd = 0;
			return 1;
		}
	}

	if(!tdata->tune_persistent) {
		/* show signal strength */
		calc_cn();
	}

	return 0; /* success */
}
