/* $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15/include/asm-sparc64/pil.h#1 $ */
#ifndef _SPARC64_PIL_H
#define _SPARC64_PIL_H

/* To avoid some locking problems, we hard allocate certain PILs
 * for SMP cross call messages that must do a etrap/rtrap.
 *
 * A cli() does not block the cross call delivery, so when SMP
 * locking is an issue we reschedule the event into a PIL interrupt
 * which is blocked by cli().
 *
 * In fact any XCALL which has to etrap/rtrap has a problem because
 * it is difficult to prevent rtrap from running BH's, and that would
 * need to be done if the XCALL arrived while %pil==15.
 */
#define PIL_SMP_CALL_FUNC	1
#define PIL_SMP_RECEIVE_SIGNAL	2
#define PIL_SMP_CAPTURE		3

#ifndef __ASSEMBLY__
#define PIL_RESERVED(PIL)	((PIL) == PIL_SMP_CALL_FUNC || \
				 (PIL) == PIL_SMP_RECEIVE_SIGNAL || \
				 (PIL) == PIL_SMP_CAPTURE)
#endif

#endif /* !(_SPARC64_PIL_H) */
