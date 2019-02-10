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
#include <errno.h>

#include <sys/poll.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

#include "queue.h"

#include "recdvbcore.h"

#define ISDBTYPE_ISDBT 0
#define ISDBTYPE_ISDBS 1

static int fefd = 0;
static int dmxfd = 0;
static int lnb = 0;
static int isdbtype = 0;

static int set_lnb_off(const thread_data *tdata)
{
	if (tdata->lnb == 0) return 0;
	if (lnb == 0) return 0;
	if (fefd <= 0) return 0;

	if (ioctl(fefd, FE_SET_VOLTAGE, SEC_VOLTAGE_OFF) == -1) {
		perror("ioctl FE_SET_VOLTAGE failed.\n");
		return 1;
	}
	lnb = 0;
	return 0;
}

static int is_isdb()
{
	struct dtv_properties props;
	struct dtv_property prop[1];

	prop[0].cmd = DTV_ENUM_DELSYS;
	props.num = 1;
	props.props = prop;

	if (ioctl(fefd, FE_GET_PROPERTY, &props) != 0)
	{
		fprintf(stderr, "Error: ioctl FE_GET_PROPERTY failed. (errno=%d)\n", errno);
		return -1;
	}

	for (__u32 i = 0; i < prop[0].u.buffer.len; ++i)
	{
		if (prop[0].u.buffer.data[i] == SYS_ISDBT)
		{
			return ISDBTYPE_ISDBT;
		}

		if (prop[0].u.buffer.data[i] == SYS_ISDBS)
		{
			return ISDBTYPE_ISDBS;
		}
	}
	return -1;
}

void close_tuner(thread_data *tdata)
{
	if (fefd > 0) {
		set_lnb_off(tdata);
		close(fefd);
		fefd = 0;
	}
	if (dmxfd > 0) {
		close(dmxfd);
		dmxfd = 0;
	}
	if (tdata->tfd == -1)
		return;

	close(tdata->tfd);
	tdata->tfd = -1;
}

void show_frontend_info(void)
{
	struct dtv_property prop[4];
	struct dtv_properties props;
	prop[0].cmd = DTV_STAT_CNR;
	prop[1].cmd = DTV_STAT_ERROR_BLOCK_COUNT;
	prop[2].cmd = DTV_STAT_TOTAL_BLOCK_COUNT;
	prop[3].cmd = DTV_STAT_SIGNAL_STRENGTH;
	props.props = prop;
	props.num = 4;
	if (ioctl(fefd, FE_GET_PROPERTY, &props) == 0) {

		fprintf(stderr, "Frontend info:");

		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j == 0 || j < prop[i].u.st.len; ++j) {
				switch (i) {
				case 0:
					fprintf(stderr, " CNR[%d]=", j);
					break;
				case 1:
					fprintf(stderr, " ErrorBlock[%d]=", j);
					break;
				case 2:
					fprintf(stderr, " TotalBlock[%d]=", j);
					break;
				case 3:
					fprintf(stderr, " Signal[%d]=", j);
					break;
				}

				switch (prop[i].u.st.stat[j].scale) {
				case FE_SCALE_COUNTER:
					fprintf(stderr, "%llu", prop[i].u.st.stat[j].uvalue);
					break;
				case FE_SCALE_RELATIVE:
					fprintf(stderr, "%lf", (double)prop[i].u.st.stat[j].uvalue / 655.35);
					break;
				case FE_SCALE_DECIBEL:
					fprintf(stderr, "%lf", (double)prop[i].u.st.stat[j].svalue / 1000);
					break;
				case FE_SCALE_NOT_AVAILABLE:
					fprintf(stderr, "N/A");
					break;
				}
			}
		}
		fprintf(stderr, "\n");
	}
}

static int open_tuner(int dev_num, struct dvb_frontend_info *fe_info)
{
	char device[32] = {0};

	if (fefd == 0) {
		sprintf(device, "/dev/dvb/adapter%d/frontend0", dev_num);
		fefd = open(device, O_RDWR);
		if (fefd < 0) {
			fprintf(stderr, "cannot open frontend device\n");
			fefd = 0;
			return 1;
		}
		fprintf(stderr, "device = %s\n", device);
	}

	if ((ioctl(fefd, FE_GET_INFO, fe_info) < 0)) {
		fprintf(stderr, "FE_GET_INFO failed\n");
		return 1;
	}

	isdbtype = is_isdb();

	if ((isdbtype != ISDBTYPE_ISDBT) && (isdbtype != ISDBTYPE_ISDBS)) {
		fprintf(stderr, "Error: tuner type is not ISDB-T/ISDB-S.\n");
		return 1;
	}

	return 0;
}

static int set_ofdm_frequency(const char *channel, struct dtv_property *prop)
{
	uint32_t fe_freq;

	prop->cmd = DTV_FREQUENCY;

	if ((fe_freq = (uint32_t)atoi(channel)) == 0) {
		fprintf(stderr, "channel is not number\n");
		return 1;
	}

	prop->u.data = (fe_freq * 6000 + 395143) * 1000;
	fprintf(stderr,"tuning to %d kHz\n",prop->u.data / 1000);

	return 0;
}

static int set_qpsk_frequency(const char *channel, struct dtv_property *prop)
{
	uint32_t fe_freq;

	prop->cmd = DTV_FREQUENCY;

	if (((channel[0] == 'b') || (channel[0] == 'B')) &&
	    ((channel[1] == 's') || (channel[1] == 'S'))) {
		if ((fe_freq = (uint32_t)atoi(channel + 2)) == 0) {
			fprintf(stderr, "channel is not BSnn\n\tnn=numeric\n");
			return 1;
		}
		prop->u.data = fe_freq * 19180 + 1030300;
	} else if (((channel[0] == 'n')||(channel[0] == 'N')) &&
		   ((channel[1] == 'd')||(channel[1] == 'D'))) {
		if ((fe_freq = (uint32_t)atoi(channel + 2)) == 0) {
			fprintf(stderr, "channel is not NDnn\n\tnn=numeric\n");
			return 1;
		}
		prop->u.data = fe_freq * 20000 + 1573000;
	} else {
		fprintf(stderr, "channel is invalid\n");
		return 1;
	}

	fprintf(stderr,"tuning to %d MHz\n",prop->u.data / 1000);
	return 0;
}

int tune(char *channel, thread_data *tdata, int dev_num, unsigned int tsid)
{
	struct dtv_property prop[4];
	struct dtv_properties props;
	struct dvb_frontend_info fe_info;
	int rc, i;
	struct dmx_pes_filter_params filter;
	struct dvb_frontend_event event;
	struct pollfd pfd[1];
	char device[32];

	/* open tuner */
	rc = open_tuner(dev_num, &fe_info);
	if (rc != 0) return 1;

	fprintf(stderr,"Using DVB card \"%s\"\n",fe_info.name);

	/* specify command */
	props.num = 0;
	if (isdbtype == ISDBTYPE_ISDBT) {
		/* ISDB-T */
		tdata->lnb = 0; /* lnb is unavailable */
		rc = set_ofdm_frequency(channel, &prop[props.num]);
		if (rc != 0) return 1;
		props.num++;
	} else {
		/* ISDB-S */
		if (lnb == 0) {
			lnb = 1;
			prop[props.num].cmd = DTV_VOLTAGE;
			switch (tdata->lnb) {
			case 2:
				prop[props.num].u.data = SEC_VOLTAGE_18;
				break;
			case 1:
				prop[props.num].u.data = SEC_VOLTAGE_13;
				break;
			default:
				prop[props.num].u.data = SEC_VOLTAGE_OFF;
				break;
			}
			props.num++;
		}
		rc = set_qpsk_frequency(channel, &prop[props.num]);
		if (rc != 0) return 1;
		props.num++;
	}
#ifdef DTV_STREAM_ID
	prop[props.num].cmd = DTV_STREAM_ID;
#else
	prop[props.num].cmd = DTV_ISDBS_TS_ID;
#endif
	prop[props.num].u.data = tsid;
	props.num++;

	prop[props.num].cmd = DTV_TUNE;
	props.num++;

	props.props = prop;

	if (ioctl(fefd, FE_SET_PROPERTY, &props) == -1) {
		perror("ioctl FE_SET_PROPERTY failed.\n");
		return 1;
	}

	pfd[0].fd = fefd;
	pfd[0].events = POLLIN;
	event.status = 0;
	fprintf(stderr,"polling");
	for (i = 0; i < 5; i++) {
		if ((event.status & FE_TIMEDOUT) != 0) break;
		if ((event.status & FE_HAS_LOCK) != 0) break;

		fprintf(stderr, ".");
		if (!poll(pfd, 1, 5000)) continue;
		if (!(pfd[0].revents & POLLIN)) continue;
		  if ((rc = ioctl(fefd, FE_GET_EVENT, &event)) < 0) {
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

	if (dmxfd == 0) {
		sprintf(device, "/dev/dvb/adapter%d/demux0", dev_num);
		if ((dmxfd = open(device, O_RDWR)) < 0) {
			dmxfd = 0;
			fprintf(stderr, "cannot open demux device\n");
			return 1;
		}
	}

	// 0x2000 pass all pids
	filter.pid = 0x2000;
	filter.input = DMX_IN_FRONTEND;
	filter.output = DMX_OUT_TS_TAP;
	filter.pes_type = DMX_PES_VIDEO;
	filter.flags = DMX_IMMEDIATE_START;
	if (ioctl(dmxfd, DMX_SET_PES_FILTER, &filter) == -1) {
		fprintf(stderr,"FILTER %i: ", filter.pid);
		perror("ioctl DMX_SET_PES_FILTER");
		close(dmxfd);
		dmxfd = 0;
		return 1;
	}

	if (tdata->tfd < 0) {
		sprintf(device, "/dev/dvb/adapter%d/dvr0", dev_num);
		if ((tdata->tfd = open(device, O_RDONLY)) < 0) {
			fprintf(stderr, "cannot open dvr device\n");
			close(dmxfd);
			dmxfd = 0;
			return 1;
		}
	}

	/* show signal strength */
	show_frontend_info();

	return 0; /* success */
}

