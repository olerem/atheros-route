/* $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15/drivers/isdn/hardware/eicon/dqueue.h#1 $ */

#ifndef _DIVA_USER_MODE_IDI_DATA_QUEUE_H__
#define _DIVA_USER_MODE_IDI_DATA_QUEUE_H__

#define DIVA_UM_IDI_MAX_MSGS 64

typedef struct _diva_um_idi_data_queue {
	int segments;
	int max_length;
	int read;
	int write;
	int count;
	int segment_pending;
	void *data[DIVA_UM_IDI_MAX_MSGS];
	int length[DIVA_UM_IDI_MAX_MSGS];
} diva_um_idi_data_queue_t;

int diva_data_q_init(diva_um_idi_data_queue_t * q,
		     int max_length, int max_segments);
int diva_data_q_finit(diva_um_idi_data_queue_t * q);
int diva_data_q_get_max_length(const diva_um_idi_data_queue_t * q);
void *diva_data_q_get_segment4write(diva_um_idi_data_queue_t * q);
void diva_data_q_ack_segment4write(diva_um_idi_data_queue_t * q,
				   int length);
const void *diva_data_q_get_segment4read(const diva_um_idi_data_queue_t *
					 q);
int diva_data_q_get_segment_length(const diva_um_idi_data_queue_t * q);
void diva_data_q_ack_segment4read(diva_um_idi_data_queue_t * q);

#endif
