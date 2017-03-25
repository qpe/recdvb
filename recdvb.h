#pragma once

#include <stdint.h>
#include <pthread.h>   /* pthread_* */

#define NUM_BSDEV                        8
#define NUM_ISDB_T_DEV                   8
#define CHTYPE_SATELLITE                 0 /* satellite digital */
#define CHTYPE_GROUND                    1 /* terrestrial digital */
#define MAX_QUEUE                     8192
#define MAX_READ_SIZE           (188 * 87) /* 188*87=16356 */
#define WRITE_SIZE       (1024 * 1024 * 2)

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

