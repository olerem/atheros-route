#ifndef __strmux_defs_h
#define __strmux_defs_h

/*
 * This file is autogenerated from
 *   file:           ../../inst/strmux/rtl/guinness/strmux_regs.r
 *     id:           strmux_regs.r,v 1.10 2005/02/10 10:10:46 perz Exp 
 *     last modfied: Mon Apr 11 16:09:43 2005
 * 
 *   by /n/asic/design/tools/rdesc/src/rdes2c --outfile strmux_defs.h ../../inst/strmux/rtl/guinness/strmux_regs.r
 *      id: $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.31/arch/cris/include/arch-v32/mach-fs/mach/hwregs/strmux_defs.h#1 $
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

/* C-code for register scope strmux */

/* Register rw_cfg, scope strmux, type rw */
typedef struct {
  unsigned int dma0 : 3;
  unsigned int dma1 : 3;
  unsigned int dma2 : 3;
  unsigned int dma3 : 3;
  unsigned int dma4 : 3;
  unsigned int dma5 : 3;
  unsigned int dma6 : 3;
  unsigned int dma7 : 3;
  unsigned int dma8 : 3;
  unsigned int dma9 : 3;
  unsigned int dummy1 : 2;
} reg_strmux_rw_cfg;
#define REG_RD_ADDR_strmux_rw_cfg 0
#define REG_WR_ADDR_strmux_rw_cfg 0


/* Constants */
enum {
  regk_strmux_ata                          = 0x00000003,
  regk_strmux_eth0                         = 0x00000001,
  regk_strmux_eth1                         = 0x00000004,
  regk_strmux_ext0                         = 0x00000001,
  regk_strmux_ext1                         = 0x00000001,
  regk_strmux_ext2                         = 0x00000001,
  regk_strmux_ext3                         = 0x00000001,
  regk_strmux_iop0                         = 0x00000002,
  regk_strmux_iop1                         = 0x00000001,
  regk_strmux_off                          = 0x00000000,
  regk_strmux_p21                          = 0x00000004,
  regk_strmux_rw_cfg_default               = 0x00000000,
  regk_strmux_ser0                         = 0x00000002,
  regk_strmux_ser1                         = 0x00000002,
  regk_strmux_ser2                         = 0x00000004,
  regk_strmux_ser3                         = 0x00000003,
  regk_strmux_sser0                        = 0x00000003,
  regk_strmux_sser1                        = 0x00000003,
  regk_strmux_strcop                       = 0x00000002
};
#endif /* __strmux_defs_h */
