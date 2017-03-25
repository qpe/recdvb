#pragma once

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

