/* $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15_2MB/include/asm-cris/fasttimer.h#1 $
 * linux/include/asm-cris/fasttimer.h
 *
 * Fast timers for ETRAX100LX
 * This may be useful in other OS than Linux so use 2 space indentation...
 * Copyright (C) 2000, 2002 Axis Communications AB
 */
#include <linux/config.h>
#include <linux/time.h> /* struct timeval */
#include <linux/timex.h>

#ifdef CONFIG_ETRAX_FAST_TIMER

typedef void fast_timer_function_type(unsigned long);

struct fast_timer{ /* Close to timer_list */
  struct fast_timer *next;
  struct fast_timer *prev;
  struct timeval tv_set;
  struct timeval tv_expires;
  unsigned long delay_us;
  fast_timer_function_type *function;
  unsigned long data;
  const char *name;
};

extern struct fast_timer *fast_timer_list;

void start_one_shot_timer(struct fast_timer *t,
                          fast_timer_function_type *function,
                          unsigned long data,
                          unsigned long delay_us,
                          const char *name);

int del_fast_timer(struct fast_timer * t);
/* return 1 if deleted */


void schedule_usleep(unsigned long us);


void fast_timer_init(void);

#endif
