#ifndef __intr_vect_defs_h
#define __intr_vect_defs_h

/*
 * This file is autogenerated from
 *   file:           ../../inst/intr_vect/rtl/guinness/ivmask.config.r
 *     id:           ivmask.config.r,v 1.4 2005/02/15 16:05:38 stefans Exp
 *     last modfied: Mon Apr 11 16:08:03 2005
 *
 *   by /n/asic/design/tools/rdesc/src/rdes2c --outfile intr_vect_defs.h ../../inst/intr_vect/rtl/guinness/ivmask.config.r
 *      id: $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15/include/asm-cris/arch-v32/hwregs/intr_vect_defs.h#1 $
 * Any changes here will be lost.
 *
 * -*- buffer-read-only: t -*-
 */
/* Main access macros */
#ifndef REG_RD
#define REG_RD( scope, inst, reg ) \
  REG_READ( reg_##scope##_##reg, \
            (inst) + REG_RD_ADDR_##scope##_##reg )
#endif

#ifndef REG_WR
#define REG_WR( scope, inst, reg, val ) \
  REG_WRITE( reg_##scope##_##reg, \
             (inst) + REG_WR_ADDR_##scope##_##reg, (val) )
#endif

#ifndef REG_RD_VECT
#define REG_RD_VECT( scope, inst, reg, index ) \
  REG_READ( reg_##scope##_##reg, \
            (inst) + REG_RD_ADDR_##scope##_##reg + \
	    (index) * STRIDE_##scope##_##reg )
#endif

#ifndef REG_WR_VECT
#define REG_WR_VECT( scope, inst, reg, index, val ) \
  REG_WRITE( reg_##scope##_##reg, \
             (inst) + REG_WR_ADDR_##scope##_##reg + \
	     (index) * STRIDE_##scope##_##reg, (val) )
#endif

#ifndef REG_RD_INT
#define REG_RD_INT( scope, inst, reg ) \
  REG_READ( int, (inst) + REG_RD_ADDR_##scope##_##reg )
#endif

#ifndef REG_WR_INT
#define REG_WR_INT( scope, inst, reg, val ) \
  REG_WRITE( int, (inst) + REG_WR_ADDR_##scope##_##reg, (val) )
#endif

#ifndef REG_RD_INT_VECT
#define REG_RD_INT_VECT( scope, inst, reg, index ) \
  REG_READ( int, (inst) + REG_RD_ADDR_##scope##_##reg + \
	    (index) * STRIDE_##scope##_##reg )
#endif

#ifndef REG_WR_INT_VECT
#define REG_WR_INT_VECT( scope, inst, reg, index, val ) \
  REG_WRITE( int, (inst) + REG_WR_ADDR_##scope##_##reg + \
	     (index) * STRIDE_##scope##_##reg, (val) )
#endif

#ifndef REG_TYPE_CONV
#define REG_TYPE_CONV( type, orgtype, val ) \
  ( { union { orgtype o; type n; } r; r.o = val; r.n; } )
#endif

#ifndef reg_page_size
#define reg_page_size 8192
#endif

#ifndef REG_ADDR
#define REG_ADDR( scope, inst, reg ) \
  ( (inst) + REG_RD_ADDR_##scope##_##reg )
#endif

#ifndef REG_ADDR_VECT
#define REG_ADDR_VECT( scope, inst, reg, index ) \
  ( (inst) + REG_RD_ADDR_##scope##_##reg + \
    (index) * STRIDE_##scope##_##reg )
#endif

/* C-code for register scope intr_vect */

/* Register rw_mask, scope intr_vect, type rw */
typedef struct {
  unsigned int memarb  : 1;
  unsigned int gen_io  : 1;
  unsigned int iop0    : 1;
  unsigned int iop1    : 1;
  unsigned int iop2    : 1;
  unsigned int iop3    : 1;
  unsigned int dma0    : 1;
  unsigned int dma1    : 1;
  unsigned int dma2    : 1;
  unsigned int dma3    : 1;
  unsigned int dma4    : 1;
  unsigned int dma5    : 1;
  unsigned int dma6    : 1;
  unsigned int dma7    : 1;
  unsigned int dma8    : 1;
  unsigned int dma9    : 1;
  unsigned int ata     : 1;
  unsigned int sser0   : 1;
  unsigned int sser1   : 1;
  unsigned int ser0    : 1;
  unsigned int ser1    : 1;
  unsigned int ser2    : 1;
  unsigned int ser3    : 1;
  unsigned int p21     : 1;
  unsigned int eth0    : 1;
  unsigned int eth1    : 1;
  unsigned int timer   : 1;
  unsigned int bif_arb : 1;
  unsigned int bif_dma : 1;
  unsigned int ext     : 1;
  unsigned int dummy1  : 2;
} reg_intr_vect_rw_mask;
#define REG_RD_ADDR_intr_vect_rw_mask 0
#define REG_WR_ADDR_intr_vect_rw_mask 0

/* Register r_vect, scope intr_vect, type r */
typedef struct {
  unsigned int memarb  : 1;
  unsigned int gen_io  : 1;
  unsigned int iop0    : 1;
  unsigned int iop1    : 1;
  unsigned int iop2    : 1;
  unsigned int iop3    : 1;
  unsigned int dma0    : 1;
  unsigned int dma1    : 1;
  unsigned int dma2    : 1;
  unsigned int dma3    : 1;
  unsigned int dma4    : 1;
  unsigned int dma5    : 1;
  unsigned int dma6    : 1;
  unsigned int dma7    : 1;
  unsigned int dma8    : 1;
  unsigned int dma9    : 1;
  unsigned int ata     : 1;
  unsigned int sser0   : 1;
  unsigned int sser1   : 1;
  unsigned int ser0    : 1;
  unsigned int ser1    : 1;
  unsigned int ser2    : 1;
  unsigned int ser3    : 1;
  unsigned int p21     : 1;
  unsigned int eth0    : 1;
  unsigned int eth1    : 1;
  unsigned int timer   : 1;
  unsigned int bif_arb : 1;
  unsigned int bif_dma : 1;
  unsigned int ext     : 1;
  unsigned int dummy1  : 2;
} reg_intr_vect_r_vect;
#define REG_RD_ADDR_intr_vect_r_vect 4

/* Register r_masked_vect, scope intr_vect, type r */
typedef struct {
  unsigned int memarb  : 1;
  unsigned int gen_io  : 1;
  unsigned int iop0    : 1;
  unsigned int iop1    : 1;
  unsigned int iop2    : 1;
  unsigned int iop3    : 1;
  unsigned int dma0    : 1;
  unsigned int dma1    : 1;
  unsigned int dma2    : 1;
  unsigned int dma3    : 1;
  unsigned int dma4    : 1;
  unsigned int dma5    : 1;
  unsigned int dma6    : 1;
  unsigned int dma7    : 1;
  unsigned int dma8    : 1;
  unsigned int dma9    : 1;
  unsigned int ata     : 1;
  unsigned int sser0   : 1;
  unsigned int sser1   : 1;
  unsigned int ser0    : 1;
  unsigned int ser1    : 1;
  unsigned int ser2    : 1;
  unsigned int ser3    : 1;
  unsigned int p21     : 1;
  unsigned int eth0    : 1;
  unsigned int eth1    : 1;
  unsigned int timer   : 1;
  unsigned int bif_arb : 1;
  unsigned int bif_dma : 1;
  unsigned int ext     : 1;
  unsigned int dummy1  : 2;
} reg_intr_vect_r_masked_vect;
#define REG_RD_ADDR_intr_vect_r_masked_vect 8

/* Register r_nmi, scope intr_vect, type r */
typedef struct {
  unsigned int ext      : 1;
  unsigned int watchdog : 1;
  unsigned int dummy1   : 30;
} reg_intr_vect_r_nmi;
#define REG_RD_ADDR_intr_vect_r_nmi 12

/* Register r_guru, scope intr_vect, type r */
typedef struct {
  unsigned int jtag : 1;
  unsigned int dummy1 : 31;
} reg_intr_vect_r_guru;
#define REG_RD_ADDR_intr_vect_r_guru 16

/* Register rw_ipi, scope intr_vect, type rw */
typedef struct
{
  unsigned int vector;
} reg_intr_vect_rw_ipi;
#define REG_RD_ADDR_intr_vect_rw_ipi 20
#define REG_WR_ADDR_intr_vect_rw_ipi 20

/* Constants */
enum {
  regk_intr_vect_off                       = 0x00000000,
  regk_intr_vect_on                        = 0x00000001,
  regk_intr_vect_rw_mask_default           = 0x00000000
};
#endif /* __intr_vect_defs_h */
