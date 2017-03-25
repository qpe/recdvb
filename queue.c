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
#include <stdbool.h>
#include <stdlib.h>

#include <sys/time.h>

#include "queue.h"

extern bool f_exit;

QUEUE_T * create_queue(size_t size)
{
	QUEUE_T *p_queue;
	size_t memsize = sizeof(QUEUE_T) + size * sizeof(BUFSZ*);

	p_queue = (QUEUE_T*)calloc(memsize, sizeof(char));

	if(p_queue != NULL) {
		p_queue->size = size;
		p_queue->num_avail = size;
		p_queue->num_used = 0;
		pthread_mutex_init(&p_queue->mutex, NULL);
		pthread_cond_init(&p_queue->cond_avail, NULL);
		pthread_cond_init(&p_queue->cond_used, NULL);
	}

	return p_queue;
}

void destroy_queue(QUEUE_T *p_queue)
{
	if(!p_queue) return;

	pthread_mutex_destroy(&p_queue->mutex);
	pthread_cond_destroy(&p_queue->cond_avail);
	pthread_cond_destroy(&p_queue->cond_used);
	free(p_queue);
}

/* enqueue data. this function will block if queue is full. */
void enqueue(QUEUE_T *p_queue, BUFSZ *data)
{
	struct timeval now;
	struct timespec spec;
	int retry_count = 0;

	pthread_mutex_lock(&p_queue->mutex);
	/* entered critical section */

	/* wait while queue is full */
	while(p_queue->num_avail == 0) {
		gettimeofday(&now, NULL);
		spec.tv_sec = now.tv_sec + 1;
		spec.tv_nsec = now.tv_usec * 1000;

		pthread_cond_timedwait(&p_queue->cond_avail,
				       &p_queue->mutex, &spec);
		retry_count++;
		if(retry_count > 60) {
			f_exit = true;
		}
		if(f_exit) {
			pthread_mutex_unlock(&p_queue->mutex);
			return;
		}
	}

	p_queue->buffer[p_queue->in] = data;

	/* move position marker for input to next position */
	p_queue->in++;
	p_queue->in %= p_queue->size;

	/* update counters */
	p_queue->num_avail--;
	p_queue->num_used++;

	/* leaving critical section */
	pthread_mutex_unlock(&p_queue->mutex);
	pthread_cond_signal(&p_queue->cond_used);
}

/* dequeue data. this function will block if queue is empty. */
BUFSZ *dequeue(QUEUE_T *p_queue)
{
	struct timeval now;
	struct timespec spec;
	BUFSZ *buffer;
	int retry_count = 0;

	pthread_mutex_lock(&p_queue->mutex);
	/* entered the critical section*/

	/* wait while queue is empty */
	while(p_queue->num_used == 0) {

		gettimeofday(&now, NULL);
		spec.tv_sec = now.tv_sec + 1;
		spec.tv_nsec = now.tv_usec * 1000;

		pthread_cond_timedwait(&p_queue->cond_used,
				       &p_queue->mutex, &spec);
		retry_count++;
		if(retry_count > 60) {
			f_exit = true;
		}
		if(f_exit) {
			pthread_mutex_unlock(&p_queue->mutex);
			return NULL;
		}
	}

	/* take buffer address */
	buffer = p_queue->buffer[p_queue->out];

	/* move position marker for output to next position */
	p_queue->out++;
	p_queue->out %= p_queue->size;

	/* update counters */
	p_queue->num_avail++;
	p_queue->num_used--;

	/* leaving the critical section */
	pthread_mutex_unlock(&p_queue->mutex);
	pthread_cond_signal(&p_queue->cond_avail);

	return buffer;
}

