/* $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15/arch/sparc/prom/segment.c#1 $
 * segment.c:  Prom routine to map segments in other contexts before
 *             a standalone is completely mapped.  This is for sun4 and
 *             sun4c architectures only.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/openprom.h>
#include <asm/oplib.h>

extern void restore_current(void);

/* Set physical segment 'segment' at virtual address 'vaddr' in
 * context 'ctx'.
 */
void
prom_putsegment(int ctx, unsigned long vaddr, int segment)
{
	unsigned long flags;
	spin_lock_irqsave(&prom_lock, flags);
	(*(romvec->pv_setctxt))(ctx, (char *) vaddr, segment);
	restore_current();
	spin_unlock_irqrestore(&prom_lock, flags);
	return;
}
