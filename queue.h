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
#ifndef RECDVB_QUEUE_H
#define RECDVB_QUEUE_H

#include <stdint.h>
#include <pthread.h>

#define MAX_READ_SIZE           (188 * 87) /* 188*87=16356 */

typedef struct _BUFSZ {
	int size;
	uint8_t buffer[MAX_READ_SIZE];
} BUFSZ;

typedef struct _QUEUE_T {
	unsigned int in;           // 次に入れるインデックス
	unsigned int out;          // 次に出すインデックス
	unsigned int size;         // キューのサイズ
	unsigned int num_avail;    // 満タンになると 0 になる
	unsigned int num_used;     // 空っぽになると 0 になる
	pthread_mutex_t mutex;
	pthread_cond_t cond_avail; // データが満タンのときに待つための cond
	pthread_cond_t cond_used;  // データが空のときに待つための cond
	BUFSZ *buffer[1];          // バッファポインタ
} QUEUE_T;

QUEUE_T * create_queue(size_t size);
void destroy_queue(QUEUE_T *p_queue);
void enqueue(QUEUE_T *p_queue, BUFSZ *data);
BUFSZ *dequeue(QUEUE_T *p_queue);

#endif
