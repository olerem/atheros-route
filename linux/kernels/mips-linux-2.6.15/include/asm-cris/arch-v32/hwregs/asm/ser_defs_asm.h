#ifndef __ser_defs_asm_h
#define __ser_defs_asm_h

/*
 * This file is autogenerated from
 *   file:           ../../inst/ser/rtl/ser_regs.r
 *     id:           ser_regs.r,v 1.23 2005/02/08 13:58:35 perz Exp
 *     last modfied: Mon Apr 11 16:09:21 2005
 *
 *   by /n/asic/design/tools/rdesc/src/rdes2c -asm --outfile asm/ser_defs_asm.h ../../inst/ser/rtl/ser_regs.r
 *      id: $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15/include/asm-cris/arch-v32/hwregs/asm/ser_defs_asm.h#1 $
 * Any changes here will be lost.
 *
 * -*- buffer-read-only: t -*-
 */

#ifndef REG_FIELD
#define REG_FIELD( scope, reg, field, value ) \
  REG_FIELD_X_( value, reg_##scope##_##reg##___##field##___lsb )
#define REG_FIELD_X_( value, shift ) ((value) << shift)
#endif

#ifndef REG_STATE
#define REG_STATE( scope, reg, field, symbolic_value ) \
  REG_STATE_X_( regk_##scope##_##symbolic_value, reg_##scope##_##reg##___##field##___lsb )
#define REG_STATE_X_( k, shift ) (k << shift)
#endif

#ifndef REG_MASK
#define REG_MASK( scope, reg, field ) \
  REG_MASK_X_( reg_##scope##_##reg##___##field##___width, reg_##scope##_##reg##___##field##___lsb )
#define REG_MASK_X_( width, lsb ) (((1 << width)-1) << lsb)
#endif

#ifndef REG_LSB
#define REG_LSB( scope, reg, field ) reg_##scope##_##reg##___##field##___lsb
#endif

#ifndef REG_BIT
#define REG_BIT( scope, reg, field ) reg_##scope##_##reg##___##field##___bit
#endif

#ifndef REG_ADDR
#define REG_ADDR( scope, inst, reg ) REG_ADDR_X_(inst, reg_##scope##_##reg##_offset)
#define REG_ADDR_X_( inst, offs ) ((inst) + offs)
#endif

#ifndef REG_ADDR_VECT
#define REG_ADDR_VECT( scope, inst, reg, index ) \
         REG_ADDR_VECT_X_(inst, reg_##scope##_##reg##_offset, index, \
			 STRIDE_##scope##_##reg )
#define REG_ADDR_VECT_X_( inst, offs, index, stride ) \
                          ((inst) + offs + (index) * stride)
#endif

/* Register rw_tr_ctrl, scope ser, type rw */
#define reg_ser_rw_tr_ctrl___base_freq___lsb 0
#define reg_ser_rw_tr_ctrl___base_freq___width 3
#define reg_ser_rw_tr_ctrl___en___lsb 3
#define reg_ser_rw_tr_ctrl___en___width 1
#define reg_ser_rw_tr_ctrl___en___bit 3
#define reg_ser_rw_tr_ctrl___par___lsb 4
#define reg_ser_rw_tr_ctrl___par___width 2
#define reg_ser_rw_tr_ctrl___par_en___lsb 6
#define reg_ser_rw_tr_ctrl___par_en___width 1
#define reg_ser_rw_tr_ctrl___par_en___bit 6
#define reg_ser_rw_tr_ctrl___data_bits___lsb 7
#define reg_ser_rw_tr_ctrl___data_bits___width 1
#define reg_ser_rw_tr_ctrl___data_bits___bit 7
#define reg_ser_rw_tr_ctrl___stop_bits___lsb 8
#define reg_ser_rw_tr_ctrl___stop_bits___width 1
#define reg_ser_rw_tr_ctrl___stop_bits___bit 8
#define reg_ser_rw_tr_ctrl___stop___lsb 9
#define reg_ser_rw_tr_ctrl___stop___width 1
#define reg_ser_rw_tr_ctrl___stop___bit 9
#define reg_ser_rw_tr_ctrl___rts_delay___lsb 10
#define reg_ser_rw_tr_ctrl___rts_delay___width 3
#define reg_ser_rw_tr_ctrl___rts_setup___lsb 13
#define reg_ser_rw_tr_ctrl___rts_setup___width 1
#define reg_ser_rw_tr_ctrl___rts_setup___bit 13
#define reg_ser_rw_tr_ctrl___auto_rts___lsb 14
#define reg_ser_rw_tr_ctrl___auto_rts___width 1
#define reg_ser_rw_tr_ctrl___auto_rts___bit 14
#define reg_ser_rw_tr_ctrl___txd___lsb 15
#define reg_ser_rw_tr_ctrl___txd___width 1
#define reg_ser_rw_tr_ctrl___txd___bit 15
#define reg_ser_rw_tr_ctrl___auto_cts___lsb 16
#define reg_ser_rw_tr_ctrl___auto_cts___width 1
#define reg_ser_rw_tr_ctrl___auto_cts___bit 16
#define reg_ser_rw_tr_ctrl_offset 0

/* Register rw_tr_dma_en, scope ser, type rw */
#define reg_ser_rw_tr_dma_en___en___lsb 0
#define reg_ser_rw_tr_dma_en___en___width 1
#define reg_ser_rw_tr_dma_en___en___bit 0
#define reg_ser_rw_tr_dma_en_offset 4

/* Register rw_rec_ctrl, scope ser, type rw */
#define reg_ser_rw_rec_ctrl___base_freq___lsb 0
#define reg_ser_rw_rec_ctrl___base_freq___width 3
#define reg_ser_rw_rec_ctrl___en___lsb 3
#define reg_ser_rw_rec_ctrl___en___width 1
#define reg_ser_rw_rec_ctrl___en___bit 3
#define reg_ser_rw_rec_ctrl___par___lsb 4
#define reg_ser_rw_rec_ctrl___par___width 2
#define reg_ser_rw_rec_ctrl___par_en___lsb 6
#define reg_ser_rw_rec_ctrl___par_en___width 1
#define reg_ser_rw_rec_ctrl___par_en___bit 6
#define reg_ser_rw_rec_ctrl___data_bits___lsb 7
#define reg_ser_rw_rec_ctrl___data_bits___width 1
#define reg_ser_rw_rec_ctrl___data_bits___bit 7
#define reg_ser_rw_rec_ctrl___dma_mode___lsb 8
#define reg_ser_rw_rec_ctrl___dma_mode___width 1
#define reg_ser_rw_rec_ctrl___dma_mode___bit 8
#define reg_ser_rw_rec_ctrl___dma_err___lsb 9
#define reg_ser_rw_rec_ctrl___dma_err___width 1
#define reg_ser_rw_rec_ctrl___dma_err___bit 9
#define reg_ser_rw_rec_ctrl___sampling___lsb 10
#define reg_ser_rw_rec_ctrl___sampling___width 1
#define reg_ser_rw_rec_ctrl___sampling___bit 10
#define reg_ser_rw_rec_ctrl___timeout___lsb 11
#define reg_ser_rw_rec_ctrl___timeout___width 3
#define reg_ser_rw_rec_ctrl___auto_eop___lsb 14
#define reg_ser_rw_rec_ctrl___auto_eop___width 1
#define reg_ser_rw_rec_ctrl___auto_eop___bit 14
#define reg_ser_rw_rec_ctrl___half_duplex___lsb 15
#define reg_ser_rw_rec_ctrl___half_duplex___width 1
#define reg_ser_rw_rec_ctrl___half_duplex___bit 15
#define reg_ser_rw_rec_ctrl___rts_n___lsb 16
#define reg_ser_rw_rec_ctrl___rts_n___width 1
#define reg_ser_rw_rec_ctrl___rts_n___bit 16
#define reg_ser_rw_rec_ctrl___loopback___lsb 17
#define reg_ser_rw_rec_ctrl___loopback___width 1
#define reg_ser_rw_rec_ctrl___loopback___bit 17
#define reg_ser_rw_rec_ctrl_offset 8

/* Register rw_tr_baud_div, scope ser, type rw */
#define reg_ser_rw_tr_baud_div___div___lsb 0
#define reg_ser_rw_tr_baud_div___div___width 16
#define reg_ser_rw_tr_baud_div_offset 12

/* Register rw_rec_baud_div, scope ser, type rw */
#define reg_ser_rw_rec_baud_div___div___lsb 0
#define reg_ser_rw_rec_baud_div___div___width 16
#define reg_ser_rw_rec_baud_div_offset 16

/* Register rw_xoff, scope ser, type rw */
#define reg_ser_rw_xoff___chr___lsb 0
#define reg_ser_rw_xoff___chr___width 8
#define reg_ser_rw_xoff___automatic___lsb 8
#define reg_ser_rw_xoff___automatic___width 1
#define reg_ser_rw_xoff___automatic___bit 8
#define reg_ser_rw_xoff_offset 20

/* Register rw_xoff_clr, scope ser, type rw */
#define reg_ser_rw_xoff_clr___clr___lsb 0
#define reg_ser_rw_xoff_clr___clr___width 1
#define reg_ser_rw_xoff_clr___clr___bit 0
#define reg_ser_rw_xoff_clr_offset 24

/* Register rw_dout, scope ser, type rw */
#define reg_ser_rw_dout___data___lsb 0
#define reg_ser_rw_dout___data___width 8
#define reg_ser_rw_dout_offset 28

/* Register rs_stat_din, scope ser, type rs */
#define reg_ser_rs_stat_din___data___lsb 0
#define reg_ser_rs_stat_din___data___width 8
#define reg_ser_rs_stat_din___dav___lsb 16
#define reg_ser_rs_stat_din___dav___width 1
#define reg_ser_rs_stat_din___dav___bit 16
#define reg_ser_rs_stat_din___framing_err___lsb 17
#define reg_ser_rs_stat_din___framing_err___width 1
#define reg_ser_rs_stat_din___framing_err___bit 17
#define reg_ser_rs_stat_din___par_err___lsb 18
#define reg_ser_rs_stat_din___par_err___width 1
#define reg_ser_rs_stat_din___par_err___bit 18
#define reg_ser_rs_stat_din___orun___lsb 19
#define reg_ser_rs_stat_din___orun___width 1
#define reg_ser_rs_stat_din___orun___bit 19
#define reg_ser_rs_stat_din___rec_err___lsb 20
#define reg_ser_rs_stat_din___rec_err___width 1
#define reg_ser_rs_stat_din___rec_err___bit 20
#define reg_ser_rs_stat_din___rxd___lsb 21
#define reg_ser_rs_stat_din___rxd___width 1
#define reg_ser_rs_stat_din___rxd___bit 21
#define reg_ser_rs_stat_din___tr_idle___lsb 22
#define reg_ser_rs_stat_din___tr_idle___width 1
#define reg_ser_rs_stat_din___tr_idle___bit 22
#define reg_ser_rs_stat_din___tr_empty___lsb 23
#define reg_ser_rs_stat_din___tr_empty___width 1
#define reg_ser_rs_stat_din___tr_empty___bit 23
#define reg_ser_rs_stat_din___tr_rdy___lsb 24
#define reg_ser_rs_stat_din___tr_rdy___width 1
#define reg_ser_rs_stat_din___tr_rdy___bit 24
#define reg_ser_rs_stat_din___cts_n___lsb 25
#define reg_ser_rs_stat_din___cts_n___width 1
#define reg_ser_rs_stat_din___cts_n___bit 25
#define reg_ser_rs_stat_din___xoff_detect___lsb 26
#define reg_ser_rs_stat_din___xoff_detect___width 1
#define reg_ser_rs_stat_din___xoff_detect___bit 26
#define reg_ser_rs_stat_din___rts_n___lsb 27
#define reg_ser_rs_stat_din___rts_n___width 1
#define reg_ser_rs_stat_din___rts_n___bit 27
#define reg_ser_rs_stat_din___txd___lsb 28
#define reg_ser_rs_stat_din___txd___width 1
#define reg_ser_rs_stat_din___txd___bit 28
#define reg_ser_rs_stat_din_offset 32

/* Register r_stat_din, scope ser, type r */
#define reg_ser_r_stat_din___data___lsb 0
#define reg_ser_r_stat_din___data___width 8
#define reg_ser_r_stat_din___dav___lsb 16
#define reg_ser_r_stat_din___dav___width 1
#define reg_ser_r_stat_din___dav___bit 16
#define reg_ser_r_stat_din___framing_err___lsb 17
#define reg_ser_r_stat_din___framing_err___width 1
#define reg_ser_r_stat_din___framing_err___bit 17
#define reg_ser_r_stat_din___par_err___lsb 18
#define reg_ser_r_stat_din___par_err___width 1
#define reg_ser_r_stat_din___par_err___bit 18
#define reg_ser_r_stat_din___orun___lsb 19
#define reg_ser_r_stat_din___orun___width 1
#define reg_ser_r_stat_din___orun___bit 19
#define reg_ser_r_stat_din___rec_err___lsb 20
#define reg_ser_r_stat_din___rec_err___width 1
#define reg_ser_r_stat_din___rec_err___bit 20
#define reg_ser_r_stat_din___rxd___lsb 21
#define reg_ser_r_stat_din___rxd___width 1
#define reg_ser_r_stat_din___rxd___bit 21
#define reg_ser_r_stat_din___tr_idle___lsb 22
#define reg_ser_r_stat_din___tr_idle___width 1
#define reg_ser_r_stat_din___tr_idle___bit 22
#define reg_ser_r_stat_din___tr_empty___lsb 23
#define reg_ser_r_stat_din___tr_empty___width 1
#define reg_ser_r_stat_din___tr_empty___bit 23
#define reg_ser_r_stat_din___tr_rdy___lsb 24
#define reg_ser_r_stat_din___tr_rdy___width 1
#define reg_ser_r_stat_din___tr_rdy___bit 24
#define reg_ser_r_stat_din___cts_n___lsb 25
#define reg_ser_r_stat_din___cts_n___width 1
#define reg_ser_r_stat_din___cts_n___bit 25
#define reg_ser_r_stat_din___xoff_detect___lsb 26
#define reg_ser_r_stat_din___xoff_detect___width 1
#define reg_ser_r_stat_din___xoff_detect___bit 26
#define reg_ser_r_stat_din___rts_n___lsb 27
#define reg_ser_r_stat_din___rts_n___width 1
#define reg_ser_r_stat_din___rts_n___bit 27
#define reg_ser_r_stat_din___txd___lsb 28
#define reg_ser_r_stat_din___txd___width 1
#define reg_ser_r_stat_din___txd___bit 28
#define reg_ser_r_stat_din_offset 36

/* Register rw_rec_eop, scope ser, type rw */
#define reg_ser_rw_rec_eop___set___lsb 0
#define reg_ser_rw_rec_eop___set___width 1
#define reg_ser_rw_rec_eop___set___bit 0
#define reg_ser_rw_rec_eop_offset 40

/* Register rw_intr_mask, scope ser, type rw */
#define reg_ser_rw_intr_mask___tr_rdy___lsb 0
#define reg_ser_rw_intr_mask___tr_rdy___width 1
#define reg_ser_rw_intr_mask___tr_rdy___bit 0
#define reg_ser_rw_intr_mask___tr_empty___lsb 1
#define reg_ser_rw_intr_mask___tr_empty___width 1
#define reg_ser_rw_intr_mask___tr_empty___bit 1
#define reg_ser_rw_intr_mask___tr_idle___lsb 2
#define reg_ser_rw_intr_mask___tr_idle___width 1
#define reg_ser_rw_intr_mask___tr_idle___bit 2
#define reg_ser_rw_intr_mask___dav___lsb 3
#define reg_ser_rw_intr_mask___dav___width 1
#define reg_ser_rw_intr_mask___dav___bit 3
#define reg_ser_rw_intr_mask_offset 44

/* Register rw_ack_intr, scope ser, type rw */
#define reg_ser_rw_ack_intr___tr_rdy___lsb 0
#define reg_ser_rw_ack_intr___tr_rdy___width 1
#define reg_ser_rw_ack_intr___tr_rdy___bit 0
#define reg_ser_rw_ack_intr___tr_empty___lsb 1
#define reg_ser_rw_ack_intr___tr_empty___width 1
#define reg_ser_rw_ack_intr___tr_empty___bit 1
#define reg_ser_rw_ack_intr___tr_idle___lsb 2
#define reg_ser_rw_ack_intr___tr_idle___width 1
#define reg_ser_rw_ack_intr___tr_idle___bit 2
#define reg_ser_rw_ack_intr___dav___lsb 3
#define reg_ser_rw_ack_intr___dav___width 1
#define reg_ser_rw_ack_intr___dav___bit 3
#define reg_ser_rw_ack_intr_offset 48

/* Register r_intr, scope ser, type r */
#define reg_ser_r_intr___tr_rdy___lsb 0
#define reg_ser_r_intr___tr_rdy___width 1
#define reg_ser_r_intr___tr_rdy___bit 0
#define reg_ser_r_intr___tr_empty___lsb 1
#define reg_ser_r_intr___tr_empty___width 1
#define reg_ser_r_intr___tr_empty___bit 1
#define reg_ser_r_intr___tr_idle___lsb 2
#define reg_ser_r_intr___tr_idle___width 1
#define reg_ser_r_intr___tr_idle___bit 2
#define reg_ser_r_intr___dav___lsb 3
#define reg_ser_r_intr___dav___width 1
#define reg_ser_r_intr___dav___bit 3
#define reg_ser_r_intr_offset 52

/* Register r_masked_intr, scope ser, type r */
#define reg_ser_r_masked_intr___tr_rdy___lsb 0
#define reg_ser_r_masked_intr___tr_rdy___width 1
#define reg_ser_r_masked_intr___tr_rdy___bit 0
#define reg_ser_r_masked_intr___tr_empty___lsb 1
#define reg_ser_r_masked_intr___tr_empty___width 1
#define reg_ser_r_masked_intr___tr_empty___bit 1
#define reg_ser_r_masked_intr___tr_idle___lsb 2
#define reg_ser_r_masked_intr___tr_idle___width 1
#define reg_ser_r_masked_intr___tr_idle___bit 2
#define reg_ser_r_masked_intr___dav___lsb 3
#define reg_ser_r_masked_intr___dav___width 1
#define reg_ser_r_masked_intr___dav___bit 3
#define reg_ser_r_masked_intr_offset 56


/* Constants */
#define regk_ser_active                           0x00000000
#define regk_ser_bits1                            0x00000000
#define regk_ser_bits2                            0x00000001
#define regk_ser_bits7                            0x00000001
#define regk_ser_bits8                            0x00000000
#define regk_ser_del0_5                           0x00000000
#define regk_ser_del1                             0x00000001
#define regk_ser_del1_5                           0x00000002
#define regk_ser_del2                             0x00000003
#define regk_ser_del2_5                           0x00000004
#define regk_ser_del3                             0x00000005
#define regk_ser_del3_5                           0x00000006
#define regk_ser_del4                             0x00000007
#define regk_ser_even                             0x00000000
#define regk_ser_ext                              0x00000001
#define regk_ser_f100                             0x00000007
#define regk_ser_f29_493                          0x00000004
#define regk_ser_f32                              0x00000005
#define regk_ser_f32_768                          0x00000006
#define regk_ser_ignore                           0x00000001
#define regk_ser_inactive                         0x00000001
#define regk_ser_majority                         0x00000001
#define regk_ser_mark                             0x00000002
#define regk_ser_middle                           0x00000000
#define regk_ser_no                               0x00000000
#define regk_ser_odd                              0x00000001
#define regk_ser_off                              0x00000000
#define regk_ser_rw_intr_mask_default             0x00000000
#define regk_ser_rw_rec_baud_div_default          0x00000000
#define regk_ser_rw_rec_ctrl_default              0x00010000
#define regk_ser_rw_tr_baud_div_default           0x00000000
#define regk_ser_rw_tr_ctrl_default               0x00008000
#define regk_ser_rw_tr_dma_en_default             0x00000000
#define regk_ser_rw_xoff_default                  0x00000000
#define regk_ser_space                            0x00000003
#define regk_ser_stop                             0x00000000
#define regk_ser_yes                              0x00000001
#endif /* __ser_defs_asm_h */
