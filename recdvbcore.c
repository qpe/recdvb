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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

// #include "queue.h"

#include "recdvbcore.h"

#define ISDBTYPE_ISDBT 0
#define ISDBTYPE_ISDBS 1

#define DEVNAME_BUFFER 32

static int get_isdbtype(int fefd)
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

static int set_isdb_t_frequency(const char *channel, struct dtv_property *prop)
{
	uint32_t fe_freq;

	prop->cmd = DTV_FREQUENCY;

	if ((fe_freq = (uint32_t)atoi(channel)) == 0) {
		fprintf(stderr, "Error: channel is not number\n");
		return 1;
	}

	prop->u.data = (fe_freq * 6000 + 395143) * 1000;
	fprintf(stderr, "Info: Tuning to %d kHz\n", prop->u.data / 1000);

	return 0;
}

static int set_isdb_s_frequency(const char *channel, struct dtv_property *prop)
{
	uint32_t fe_freq;

	prop->cmd = DTV_FREQUENCY;

	if (((channel[0] == 'b') || (channel[0] == 'B')) &&
	    ((channel[1] == 's') || (channel[1] == 'S'))) {
		if ((fe_freq = (uint32_t)atoi(channel + 2)) == 0) {
			fprintf(stderr, "Error: channel is not BSnn (nn=numeric)\n");
			return 1;
		}
		prop->u.data = fe_freq * 19180 + 1030300;
	} else if (((channel[0] == 'n')||(channel[0] == 'N')) &&
		   ((channel[1] == 'd')||(channel[1] == 'D'))) {
		if ((fe_freq = (uint32_t)atoi(channel + 2)) == 0) {
			fprintf(stderr, "Error: channel is not NDnn (nn=numeric)\n");
			return 1;
		}
		prop->u.data = fe_freq * 20000 + 1573000;
	} else {
		fprintf(stderr, "Error: channel is invalid\n");
		return 1;
	}

	fprintf(stderr, "Info: Tuning to %d MHz\n", prop->u.data / 1000);
	return 0;
}

static int frontend_show_info(int fefd)
{
	struct dvb_frontend_info fe_info;

	if (ioctl(fefd, FE_GET_INFO, &fe_info) != 0)
	{
		fprintf(stderr, "Error: FE_GET_INFO failed. (errno=%d)\n", errno);
		return -1;
	}

	/* show info */
	fprintf(stderr, "Info: DVB card name \"%s\".\n", fe_info.name);
	fprintf(stderr, "      Freq min %u, max %u.\n", fe_info.frequency_min, fe_info.frequency_max);
	fprintf(stderr, "      Freq stepsize %u, tolerance %u.\n", fe_info.frequency_stepsize, fe_info.frequency_tolerance);
	fprintf(stderr, "      Symbol Rate min %u, max %u, tolerance %u.\n", fe_info.symbol_rate_min, fe_info.symbol_rate_max, fe_info.symbol_rate_tolerance);
	fprintf(stderr, "      fe_cups 0x%x.\n", fe_info.caps);
	
	return 0;
}

int open_frontend(int dev_num)
{
	int fefd;
	int isdbtype;
	char device[DEVNAME_BUFFER] = {0};

	sprintf(device, "/dev/dvb/adapter%d/frontend0", dev_num);
	fefd = open(device, O_RDWR);
	if (fefd < 0) {
		fprintf(stderr, "Error: Cannot open dvb frontend. (errno=%d)\n", errno);
		return -1;
	}
	
	fprintf(stderr, "Info: DVB frontend = %s\n", device);

	/* check isdb type */
	isdbtype = get_isdbtype(fefd);

	if ((isdbtype != ISDBTYPE_ISDBT) && (isdbtype != ISDBTYPE_ISDBS)) {
		fprintf(stderr, "Error: tuner type is not ISDB-T/ISDB-S.\n");
		close(fefd);
		return -1;
	}
	fprintf(stderr, "Info: Tuner type is %s\n", isdbtype == ISDBTYPE_ISDBT ? "ISDB-T" : "ISDB-S");

	if (frontend_show_info(fefd) != 0) {
		close(fefd);
		return -1;
	}

	return fefd;
}

int frontend_tune(int fefd, char *channel, unsigned int tsid, int lnb)
{
	int isdbtype;
	struct dtv_property prop[4];
	struct dtv_properties props;

	/* specify command */
	props.num = 0;
	props.props = prop;

	isdbtype = get_isdbtype(fefd);

	if (isdbtype == ISDBTYPE_ISDBT) {
		/* frequency */
		if (set_isdb_t_frequency(channel, &prop[props.num]) != 0) {
			return -1;
		}
		props.num++;
	} else if (isdbtype == ISDBTYPE_ISDBS) {
		/* LNB */
		prop[props.num].cmd = DTV_VOLTAGE;
		if (lnb == 15) {
			prop[props.num].u.data = SEC_VOLTAGE_18;
		} else if (lnb == 11) {
			prop[props.num].u.data = SEC_VOLTAGE_13;
		} else {
			prop[props.num].u.data = SEC_VOLTAGE_OFF;
		}
		props.num++;

		/* frequency */
		if (set_isdb_s_frequency(channel, &prop[props.num]) != 0) {
			return -1;
		}
		props.num++;

		/* TSID */
		prop[props.num].cmd = DTV_STREAM_ID;
		prop[props.num].u.data = tsid;
		props.num++;
	}

	prop[props.num].cmd = DTV_TUNE;
	props.num++;


	if (ioctl(fefd, FE_SET_PROPERTY, &props) == -1) {
		fprintf(stderr, "Error: FE_SET_PROPERTY failed. (errno=%d)\n", errno);
		return -1;
	}

	return 0;
}

void frontend_show_stats(int fefd)
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

int frontend_locked(int fefd)
{
	unsigned int status;
	
	if (ioctl(fefd, FE_READ_STATUS, &status) == -1) {
		return -1;
	}
	if (status & FE_HAS_LOCK) {
		return 0;
	}

	return 1;
}

void frontend_show_frequency(int fefd)
{
	struct dtv_properties props;
	struct dtv_property prop[1];

	prop[0].cmd = DTV_FREQUENCY;
	props.num = 1;
	props.props = prop;

	if (ioctl(fefd, FE_GET_PROPERTY, &props) != 0)
	{
		return;
	}

	if (get_isdbtype(fefd) == ISDBTYPE_ISDBT) {
		fprintf(stderr, "Info: Tuned %d KHz.\n", prop[0].u.data / 1000);
	} else {
		fprintf(stderr, "Info: Tuned %d MHz.\n", prop[0].u.data / 1000);
	}

	return;
}

int open_demux(int dev_num)
{
	int dmxfd = -1;
	char device[DEVNAME_BUFFER] = {0};

	sprintf(device, "/dev/dvb/adapter%d/demux0", dev_num);
	if ((dmxfd = open(device, O_RDWR)) < 0) {
		fprintf(stderr, "Error: Cannot open demux device. (errno=%d)\n", errno);
		return -1;
	}

	return dmxfd;
}

int demux_start(int dmxfd)
{
	struct dmx_pes_filter_params filter;

	// 0x2000 pass all pids
	filter.pid = 0x2000;
	filter.input = DMX_IN_FRONTEND;
	filter.output = DMX_OUT_TS_TAP;
	filter.pes_type = DMX_PES_VIDEO;
	filter.flags = DMX_IMMEDIATE_START;
	if (ioctl(dmxfd, DMX_SET_PES_FILTER, &filter) == -1) {
		fprintf(stderr,"Error: DMX_SET_PES_FILTER failed. (errno=%d)\n", errno);
		return -1;
	}

	return 0;
}

int open_dvr(int dev_num)
{
	int dvrfd = -1;
	char device[DEVNAME_BUFFER] = {0};

	sprintf(device, "/dev/dvb/adapter%d/dvr0", dev_num);
	if ((dvrfd = open(device, O_RDONLY | O_NONBLOCK)) < 0) {
		fprintf(stderr, "Error: Cannot open dvr device. (errno=%d)\n", errno);
		return -1;
	}

	return dvrfd;
}

