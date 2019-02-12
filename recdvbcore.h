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

/* frontend */
int open_frontend(int dev_num);
int frontend_tune(int fefd, char *channel, unsigned int tsid, int lnb);
void frontend_show_stats(int fefd);
int frontend_locked(int fefd);
void frontend_show_frequency(int fefd);

/* demux */
int open_demux(int dev_num);
int demux_start(int dmxfd);

/* dvr */
int open_dvr(int dev_num);

#endif

