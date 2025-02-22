#ifndef __dma_defs_h
#define __dma_defs_h

/*
 * This file is autogenerated from
 *   file:           ../../inst/dma/inst/dma_common/rtl/dma_regdes.r
 *     id:           dma_regdes.r,v 1.39 2005/02/10 14:07:23 janb Exp
 *     last modfied: Mon Apr 11 16:06:51 2005
 *
 *   by /n/asic/design/tools/rdesc/src/rdes2c --outfile dma_defs.h ../../inst/dma/inst/dma_common/rtl/dma_regdes.r
 *      id: $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15_2MB/include/asm-cris/arch-v32/hwregs/dma_defs.h#1 $
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

/* C-code for register scope dma */

/* Register rw_data, scope dma, type rw */
typedef unsigned int reg_dma_rw_data;
#define REG_RD_ADDR_dma_rw_data 0
#define REG_WR_ADDR_dma_rw_data 0

/* Register rw_data_next, scope dma, type rw */
typedef unsigned int reg_dma_rw_data_next;
#define REG_RD_ADDR_dma_rw_data_next 4
#define REG_WR_ADDR_dma_rw_data_next 4

/* Register rw_data_buf, scope dma, type rw */
typedef unsigned int reg_dma_rw_data_buf;
#define REG_RD_ADDR_dma_rw_data_buf 8
#define REG_WR_ADDR_dma_rw_data_buf 8

/* Register rw_data_ctrl, scope dma, type rw */
typedef struct {
  unsigned int eol     : 1;
  unsigned int dummy1  : 2;
  unsigned int out_eop : 1;
  unsigned int intr    : 1;
  unsigned int wait    : 1;
  unsigned int dummy2  : 26;
} reg_dma_rw_data_ctrl;
#define REG_RD_ADDR_dma_rw_data_ctrl 12
#define REG_WR_ADDR_dma_rw_data_ctrl 12

/* Register rw_data_stat, scope dma, type rw */
typedef struct {
  unsigned int dummy1 : 3;
  unsigned int in_eop : 1;
  unsigned int dummy2 : 28;
} reg_dma_rw_data_stat;
#define REG_RD_ADDR_dma_rw_data_stat 16
#define REG_WR_ADDR_dma_rw_data_stat 16

/* Register rw_data_md, scope dma, type rw */
typedef struct {
  unsigned int md : 16;
  unsigned int dummy1 : 16;
} reg_dma_rw_data_md;
#define REG_RD_ADDR_dma_rw_data_md 20
#define REG_WR_ADDR_dma_rw_data_md 20

/* Register rw_data_md_s, scope dma, type rw */
typedef struct {
  unsigned int md_s : 16;
  unsigned int dummy1 : 16;
} reg_dma_rw_data_md_s;
#define REG_RD_ADDR_dma_rw_data_md_s 24
#define REG_WR_ADDR_dma_rw_data_md_s 24

/* Register rw_data_after, scope dma, type rw */
typedef unsigned int reg_dma_rw_data_after;
#define REG_RD_ADDR_dma_rw_data_after 28
#define REG_WR_ADDR_dma_rw_data_after 28

/* Register rw_ctxt, scope dma, type rw */
typedef unsigned int reg_dma_rw_ctxt;
#define REG_RD_ADDR_dma_rw_ctxt 32
#define REG_WR_ADDR_dma_rw_ctxt 32

/* Register rw_ctxt_next, scope dma, type rw */
typedef unsigned int reg_dma_rw_ctxt_next;
#define REG_RD_ADDR_dma_rw_ctxt_next 36
#define REG_WR_ADDR_dma_rw_ctxt_next 36

/* Register rw_ctxt_ctrl, scope dma, type rw */
typedef struct {
  unsigned int eol        : 1;
  unsigned int dummy1     : 3;
  unsigned int intr       : 1;
  unsigned int dummy2     : 1;
  unsigned int store_mode : 1;
  unsigned int en         : 1;
  unsigned int dummy3     : 24;
} reg_dma_rw_ctxt_ctrl;
#define REG_RD_ADDR_dma_rw_ctxt_ctrl 40
#define REG_WR_ADDR_dma_rw_ctxt_ctrl 40

/* Register rw_ctxt_stat, scope dma, type rw */
typedef struct {
  unsigned int dummy1 : 7;
  unsigned int dis : 1;
  unsigned int dummy2 : 24;
} reg_dma_rw_ctxt_stat;
#define REG_RD_ADDR_dma_rw_ctxt_stat 44
#define REG_WR_ADDR_dma_rw_ctxt_stat 44

/* Register rw_ctxt_md0, scope dma, type rw */
typedef struct {
  unsigned int md0 : 16;
  unsigned int dummy1 : 16;
} reg_dma_rw_ctxt_md0;
#define REG_RD_ADDR_dma_rw_ctxt_md0 48
#define REG_WR_ADDR_dma_rw_ctxt_md0 48

/* Register rw_ctxt_md0_s, scope dma, type rw */
typedef struct {
  unsigned int md0_s : 16;
  unsigned int dummy1 : 16;
} reg_dma_rw_ctxt_md0_s;
#define REG_RD_ADDR_dma_rw_ctxt_md0_s 52
#define REG_WR_ADDR_dma_rw_ctxt_md0_s 52

/* Register rw_ctxt_md1, scope dma, type rw */
typedef unsigned int reg_dma_rw_ctxt_md1;
#define REG_RD_ADDR_dma_rw_ctxt_md1 56
#define REG_WR_ADDR_dma_rw_ctxt_md1 56

/* Register rw_ctxt_md1_s, scope dma, type rw */
typedef unsigned int reg_dma_rw_ctxt_md1_s;
#define REG_RD_ADDR_dma_rw_ctxt_md1_s 60
#define REG_WR_ADDR_dma_rw_ctxt_md1_s 60

/* Register rw_ctxt_md2, scope dma, type rw */
typedef unsigned int reg_dma_rw_ctxt_md2;
#define REG_RD_ADDR_dma_rw_ctxt_md2 64
#define REG_WR_ADDR_dma_rw_ctxt_md2 64

/* Register rw_ctxt_md2_s, scope dma, type rw */
typedef unsigned int reg_dma_rw_ctxt_md2_s;
#define REG_RD_ADDR_dma_rw_ctxt_md2_s 68
#define REG_WR_ADDR_dma_rw_ctxt_md2_s 68

/* Register rw_ctxt_md3, scope dma, type rw */
typedef unsigned int reg_dma_rw_ctxt_md3;
#define REG_RD_ADDR_dma_rw_ctxt_md3 72
#define REG_WR_ADDR_dma_rw_ctxt_md3 72

/* Register rw_ctxt_md3_s, scope dma, type rw */
typedef unsigned int reg_dma_rw_ctxt_md3_s;
#define REG_RD_ADDR_dma_rw_ctxt_md3_s 76
#define REG_WR_ADDR_dma_rw_ctxt_md3_s 76

/* Register rw_ctxt_md4, scope dma, type rw */
typedef unsigned int reg_dma_rw_ctxt_md4;
#define REG_RD_ADDR_dma_rw_ctxt_md4 80
#define REG_WR_ADDR_dma_rw_ctxt_md4 80

/* Register rw_ctxt_md4_s, scope dma, type rw */
typedef unsigned int reg_dma_rw_ctxt_md4_s;
#define REG_RD_ADDR_dma_rw_ctxt_md4_s 84
#define REG_WR_ADDR_dma_rw_ctxt_md4_s 84

/* Register rw_saved_data, scope dma, type rw */
typedef unsigned int reg_dma_rw_saved_data;
#define REG_RD_ADDR_dma_rw_saved_data 88
#define REG_WR_ADDR_dma_rw_saved_data 88

/* Register rw_saved_data_buf, scope dma, type rw */
typedef unsigned int reg_dma_rw_saved_data_buf;
#define REG_RD_ADDR_dma_rw_saved_data_buf 92
#define REG_WR_ADDR_dma_rw_saved_data_buf 92

/* Register rw_group, scope dma, type rw */
typedef unsigned int reg_dma_rw_group;
#define REG_RD_ADDR_dma_rw_group 96
#define REG_WR_ADDR_dma_rw_group 96

/* Register rw_group_next, scope dma, type rw */
typedef unsigned int reg_dma_rw_group_next;
#define REG_RD_ADDR_dma_rw_group_next 100
#define REG_WR_ADDR_dma_rw_group_next 100

/* Register rw_group_ctrl, scope dma, type rw */
typedef struct {
  unsigned int eol  : 1;
  unsigned int tol  : 1;
  unsigned int bol  : 1;
  unsigned int dummy1 : 1;
  unsigned int intr : 1;
  unsigned int dummy2 : 2;
  unsigned int en   : 1;
  unsigned int dummy3 : 24;
} reg_dma_rw_group_ctrl;
#define REG_RD_ADDR_dma_rw_group_ctrl 104
#define REG_WR_ADDR_dma_rw_group_ctrl 104

/* Register rw_group_stat, scope dma, type rw */
typedef struct {
  unsigned int dummy1 : 7;
  unsigned int dis : 1;
  unsigned int dummy2 : 24;
} reg_dma_rw_group_stat;
#define REG_RD_ADDR_dma_rw_group_stat 108
#define REG_WR_ADDR_dma_rw_group_stat 108

/* Register rw_group_md, scope dma, type rw */
typedef struct {
  unsigned int md : 16;
  unsigned int dummy1 : 16;
} reg_dma_rw_group_md;
#define REG_RD_ADDR_dma_rw_group_md 112
#define REG_WR_ADDR_dma_rw_group_md 112

/* Register rw_group_md_s, scope dma, type rw */
typedef struct {
  unsigned int md_s : 16;
  unsigned int dummy1 : 16;
} reg_dma_rw_group_md_s;
#define REG_RD_ADDR_dma_rw_group_md_s 116
#define REG_WR_ADDR_dma_rw_group_md_s 116

/* Register rw_group_up, scope dma, type rw */
typedef unsigned int reg_dma_rw_group_up;
#define REG_RD_ADDR_dma_rw_group_up 120
#define REG_WR_ADDR_dma_rw_group_up 120

/* Register rw_group_down, scope dma, type rw */
typedef unsigned int reg_dma_rw_group_down;
#define REG_RD_ADDR_dma_rw_group_down 124
#define REG_WR_ADDR_dma_rw_group_down 124

/* Register rw_cmd, scope dma, type rw */
typedef struct {
  unsigned int cont_data : 1;
  unsigned int dummy1    : 31;
} reg_dma_rw_cmd;
#define REG_RD_ADDR_dma_rw_cmd 128
#define REG_WR_ADDR_dma_rw_cmd 128

/* Register rw_cfg, scope dma, type rw */
typedef struct {
  unsigned int en   : 1;
  unsigned int stop : 1;
  unsigned int dummy1 : 30;
} reg_dma_rw_cfg;
#define REG_RD_ADDR_dma_rw_cfg 132
#define REG_WR_ADDR_dma_rw_cfg 132

/* Register rw_stat, scope dma, type rw */
typedef struct {
  unsigned int mode           : 5;
  unsigned int list_state     : 3;
  unsigned int stream_cmd_src : 8;
  unsigned int dummy1         : 8;
  unsigned int buf            : 8;
} reg_dma_rw_stat;
#define REG_RD_ADDR_dma_rw_stat 136
#define REG_WR_ADDR_dma_rw_stat 136

/* Register rw_intr_mask, scope dma, type rw */
typedef struct {
  unsigned int group      : 1;
  unsigned int ctxt       : 1;
  unsigned int data       : 1;
  unsigned int in_eop     : 1;
  unsigned int stream_cmd : 1;
  unsigned int dummy1     : 27;
} reg_dma_rw_intr_mask;
#define REG_RD_ADDR_dma_rw_intr_mask 140
#define REG_WR_ADDR_dma_rw_intr_mask 140

/* Register rw_ack_intr, scope dma, type rw */
typedef struct {
  unsigned int group      : 1;
  unsigned int ctxt       : 1;
  unsigned int data       : 1;
  unsigned int in_eop     : 1;
  unsigned int stream_cmd : 1;
  unsigned int dummy1     : 27;
} reg_dma_rw_ack_intr;
#define REG_RD_ADDR_dma_rw_ack_intr 144
#define REG_WR_ADDR_dma_rw_ack_intr 144

/* Register r_intr, scope dma, type r */
typedef struct {
  unsigned int group      : 1;
  unsigned int ctxt       : 1;
  unsigned int data       : 1;
  unsigned int in_eop     : 1;
  unsigned int stream_cmd : 1;
  unsigned int dummy1     : 27;
} reg_dma_r_intr;
#define REG_RD_ADDR_dma_r_intr 148

/* Register r_masked_intr, scope dma, type r */
typedef struct {
  unsigned int group      : 1;
  unsigned int ctxt       : 1;
  unsigned int data       : 1;
  unsigned int in_eop     : 1;
  unsigned int stream_cmd : 1;
  unsigned int dummy1     : 27;
} reg_dma_r_masked_intr;
#define REG_RD_ADDR_dma_r_masked_intr 152

/* Register rw_stream_cmd, scope dma, type rw */
typedef struct {
  unsigned int cmd  : 10;
  unsigned int dummy1 : 6;
  unsigned int n    : 8;
  unsigned int dummy2 : 7;
  unsigned int busy : 1;
} reg_dma_rw_stream_cmd;
#define REG_RD_ADDR_dma_rw_stream_cmd 156
#define REG_WR_ADDR_dma_rw_stream_cmd 156


/* Constants */
enum {
  regk_dma_ack_pkt                         = 0x00000100,
  regk_dma_anytime                         = 0x00000001,
  regk_dma_array                           = 0x00000008,
  regk_dma_burst                           = 0x00000020,
  regk_dma_client                          = 0x00000002,
  regk_dma_copy_next                       = 0x00000010,
  regk_dma_copy_up                         = 0x00000020,
  regk_dma_data_at_eol                     = 0x00000001,
  regk_dma_dis_c                           = 0x00000010,
  regk_dma_dis_g                           = 0x00000020,
  regk_dma_idle                            = 0x00000001,
  regk_dma_intern                          = 0x00000004,
  regk_dma_load_c                          = 0x00000200,
  regk_dma_load_c_n                        = 0x00000280,
  regk_dma_load_c_next                     = 0x00000240,
  regk_dma_load_d                          = 0x00000140,
  regk_dma_load_g                          = 0x00000300,
  regk_dma_load_g_down                     = 0x000003c0,
  regk_dma_load_g_next                     = 0x00000340,
  regk_dma_load_g_up                       = 0x00000380,
  regk_dma_next_en                         = 0x00000010,
  regk_dma_next_pkt                        = 0x00000010,
  regk_dma_no                              = 0x00000000,
  regk_dma_only_at_wait                    = 0x00000000,
  regk_dma_restore                         = 0x00000020,
  regk_dma_rst                             = 0x00000001,
  regk_dma_running                         = 0x00000004,
  regk_dma_rw_cfg_default                  = 0x00000000,
  regk_dma_rw_cmd_default                  = 0x00000000,
  regk_dma_rw_intr_mask_default            = 0x00000000,
  regk_dma_rw_stat_default                 = 0x00000101,
  regk_dma_rw_stream_cmd_default           = 0x00000000,
  regk_dma_save_down                       = 0x00000020,
  regk_dma_save_up                         = 0x00000020,
  regk_dma_set_reg                         = 0x00000050,
  regk_dma_set_w_size1                     = 0x00000190,
  regk_dma_set_w_size2                     = 0x000001a0,
  regk_dma_set_w_size4                     = 0x000001c0,
  regk_dma_stopped                         = 0x00000002,
  regk_dma_store_c                         = 0x00000002,
  regk_dma_store_descr                     = 0x00000000,
  regk_dma_store_g                         = 0x00000004,
  regk_dma_store_md                        = 0x00000001,
  regk_dma_sw                              = 0x00000008,
  regk_dma_update_down                     = 0x00000020,
  regk_dma_yes                             = 0x00000001
};
#endif /* __dma_defs_h */
