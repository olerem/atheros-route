/* $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15/include/asm-sparc/scatterlist.h#1 $ */
#ifndef _SPARC_SCATTERLIST_H
#define _SPARC_SCATTERLIST_H

#include <linux/types.h>

struct scatterlist {
	struct page *page;
	unsigned int offset;

	unsigned int length;

	__u32 dvma_address; /* A place to hang host-specific addresses at. */
	__u32 dvma_length;
};

#define sg_dma_address(sg) ((sg)->dvma_address)
#define sg_dma_len(sg)     ((sg)->dvma_length)

#define ISA_DMA_THRESHOLD (~0UL)

#endif /* !(_SPARC_SCATTERLIST_H) */
