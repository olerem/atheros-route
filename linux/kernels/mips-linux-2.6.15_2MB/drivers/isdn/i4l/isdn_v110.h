/* $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15_2MB/drivers/isdn/i4l/isdn_v110.h#1 $
 *
 * Linux ISDN subsystem, V.110 related functions (linklevel).
 *
 * Copyright by Thomas Pfeiffer (pfeiffer@pds.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef _isdn_v110_h_
#define _isdn_v110_h_

/* 
 * isdn_v110_encode will take raw data and encode it using V.110 
 */
extern struct sk_buff *isdn_v110_encode(isdn_v110_stream *, struct sk_buff *);

/* 
 * isdn_v110_decode receives V.110 coded data from the stream and rebuilds
 * frames from them. The source stream doesn't need to be framed.
 */
extern struct sk_buff *isdn_v110_decode(isdn_v110_stream *, struct sk_buff *);

extern int isdn_v110_stat_callback(int, isdn_ctrl *);
extern void isdn_v110_close(isdn_v110_stream * v);

#endif
