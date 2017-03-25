#pragma once

#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <netinet/in.h>

#include "decoder.h"
#include "queue.h"
#include "tssplitter_lite.h"

/* ipc message size */
#define MSGSZ		255

/* used in checksigna.c */
#define MAX_RETRY	(2)

/* type definitions */
typedef struct sock_data {
	int sfd;    /* socket fd */
	struct sockaddr_in addr;
} sock_data;

typedef struct msgbuf {
	long mtype;
	char mtext[MSGSZ];
} message_buf;

typedef struct thread_data {
	int tfd;    /* tuner fd */       //xxx variable

	int wfd;    /* output file fd */ //invariable
	int lnb;    /* LNB voltage */    //invariable
	int msqid;                       //invariable
	time_t start_time;               //invariable

	int recsec;                      //xxx variable

	bool indefinite;                 //invaliable
	bool tune_persistent;            //invaliable

	QUEUE_T *queue;                  //invariable
	sock_data *sock_data;            //invariable
	pthread_t signal_thread;         //invariable
	decoder *decoder;                //invariable
	decoder_options *dopt;           //invariable
	splitter *splitter;              //invariable
} thread_data;

/* TODO: remove these global value... */
extern const char *version;
extern char *bsdev[];
extern char *isdb_t_dev[];

/* prototypes */
int tune(char *channel, thread_data *tdata, int dev_num, unsigned int tsid);
void close_tuner(thread_data *tdata);
void calc_cn(void);

