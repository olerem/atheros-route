/* $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15/include/asm-cris/sigcontext.h#1 $ */

#ifndef _ASM_CRIS_SIGCONTEXT_H
#define _ASM_CRIS_SIGCONTEXT_H

#include <asm/ptrace.h>

/* This struct is saved by setup_frame in signal.c, to keep the current context while
   a signal handler is executed. It's restored by sys_sigreturn.
   
   To keep things simple, we use pt_regs here even though normally you just specify
   the list of regs to save. Then we can use copy_from_user on the entire regs instead
   of a bunch of get_user's as well...

*/

struct sigcontext {
	struct pt_regs regs;  /* needs to be first */
	unsigned long oldmask;
	unsigned long usp;    /* usp before stacking this gunk on it */
};

#endif

