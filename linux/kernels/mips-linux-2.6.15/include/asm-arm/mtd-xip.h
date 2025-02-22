/*
 * MTD primitives for XIP support. Architecture specific functions
 *
 * Do not include this file directly. It's included from linux/mtd/xip.h
 * 
 * Author:	Nicolas Pitre
 * Created:	Nov 2, 2004
 * Copyright:	(C) 2004 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15/include/asm-arm/mtd-xip.h#1 $
 */

#ifndef __ARM_MTD_XIP_H__
#define __ARM_MTD_XIP_H__

#include <asm/hardware.h>
#include <asm/arch/mtd-xip.h>

/* fill instruction prefetch */
#define xip_iprefetch() 	do { asm volatile (".rep 8; nop; .endr"); } while (0)

#endif /* __ARM_MTD_XIP_H__ */
