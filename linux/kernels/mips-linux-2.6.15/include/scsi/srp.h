/*
 * Copyright (c) 2005 Cisco Systems.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15/include/scsi/srp.h#1 $
 */

#ifndef SCSI_SRP_H
#define SCSI_SRP_H

/*
 * Structures and constants for the SCSI RDMA Protocol (SRP) as
 * defined by the INCITS T10 committee.  This file was written using
 * draft Revision 16a of the SRP standard.
 */

#include <linux/types.h>

enum {
	SRP_LOGIN_REQ	= 0x00,
	SRP_TSK_MGMT	= 0x01,
	SRP_CMD		= 0x02,
	SRP_I_LOGOUT	= 0x03,
	SRP_LOGIN_RSP	= 0xc0,
	SRP_RSP		= 0xc1,
	SRP_LOGIN_REJ	= 0xc2,
	SRP_T_LOGOUT	= 0x80,
	SRP_CRED_REQ	= 0x81,
	SRP_AER_REQ	= 0x82,
	SRP_CRED_RSP	= 0x41,
	SRP_AER_RSP	= 0x42
};

enum {
	SRP_BUF_FORMAT_DIRECT	= 1 << 1,
	SRP_BUF_FORMAT_INDIRECT	= 1 << 2
};

enum {
	SRP_NO_DATA_DESC	= 0,
	SRP_DATA_DESC_DIRECT	= 1,
	SRP_DATA_DESC_INDIRECT	= 2
};

enum {
	SRP_TSK_ABORT_TASK	= 0x01,
	SRP_TSK_ABORT_TASK_SET	= 0x02,
	SRP_TSK_CLEAR_TASK_SET	= 0x04,
	SRP_TSK_LUN_RESET	= 0x08,
	SRP_TSK_CLEAR_ACA	= 0x40
};

enum srp_login_rej_reason {
	SRP_LOGIN_REJ_UNABLE_ESTABLISH_CHANNEL		= 0x00010000,
	SRP_LOGIN_REJ_INSUFFICIENT_RESOURCES		= 0x00010001,
	SRP_LOGIN_REJ_REQ_IT_IU_LENGTH_TOO_LARGE	= 0x00010002,
	SRP_LOGIN_REJ_UNABLE_ASSOCIATE_CHANNEL		= 0x00010003,
	SRP_LOGIN_REJ_UNSUPPORTED_DESCRIPTOR_FMT	= 0x00010004,
	SRP_LOGIN_REJ_MULTI_CHANNEL_UNSUPPORTED		= 0x00010005,
	SRP_LOGIN_REJ_CHANNEL_LIMIT_REACHED		= 0x00010006
};

struct srp_direct_buf {
	__be64	va;
	__be32	key;
	__be32  len;
};

/*
 * We need the packed attribute because the SRP spec puts the list of
 * descriptors at an offset of 20, which is not aligned to the size
 * of struct srp_direct_buf.
 */
struct srp_indirect_buf {
	struct srp_direct_buf	table_desc;
	__be32			len;
	struct srp_direct_buf	desc_list[0] __attribute__((packed));
};

enum {
	SRP_MULTICHAN_SINGLE = 0,
	SRP_MULTICHAN_MULTI  = 1
};

struct srp_login_req {
	u8	opcode;
	u8	reserved1[7];
	u64	tag;
	__be32	req_it_iu_len;
	u8	reserved2[4];
	__be16	req_buf_fmt;
	u8	req_flags;
	u8	reserved3[5];
	u8	initiator_port_id[16];
	u8	target_port_id[16];
};

struct srp_login_rsp {
	u8	opcode;
	u8	reserved1[3];
	__be32	req_lim_delta;
	u64	tag;
	__be32	max_it_iu_len;
	__be32	max_ti_iu_len;
	__be16	buf_fmt;
	u8	rsp_flags;
	u8	reserved2[25];
};

struct srp_login_rej {
	u8	opcode;
	u8	reserved1[3];
	__be32	reason;
	u64	tag;
	u8	reserved2[8];
	__be16	buf_fmt;
	u8	reserved3[6];
};

struct srp_i_logout {
	u8	opcode;
	u8	reserved[7];
	u64	tag;
};

struct srp_t_logout {
	u8	opcode;
	u8	sol_not;
	u8	reserved[2];
	__be32	reason;
	u64	tag;
};

/*
 * We need the packed attribute because the SRP spec only aligns the
 * 8-byte LUN field to 4 bytes.
 */
struct srp_tsk_mgmt {
	u8	opcode;
	u8	sol_not;
	u8	reserved1[6];
	u64	tag;
	u8	reserved2[4];
	__be64	lun __attribute__((packed));
	u8	reserved3[2];
	u8	tsk_mgmt_func;
	u8	reserved4;
	u64	task_tag;
	u8	reserved5[8];
};

/*
 * We need the packed attribute because the SRP spec only aligns the
 * 8-byte LUN field to 4 bytes.
 */
struct srp_cmd {
	u8	opcode;
	u8	sol_not;
	u8	reserved1[3];
	u8	buf_fmt;
	u8	data_out_desc_cnt;
	u8	data_in_desc_cnt;
	u64	tag;
	u8	reserved2[4];
	__be64	lun __attribute__((packed));
	u8	reserved3;
	u8	task_attr;
	u8	reserved4;
	u8	add_cdb_len;
	u8	cdb[16];
	u8	add_data[0];
};

enum {
	SRP_RSP_FLAG_RSPVALID = 1 << 0,
	SRP_RSP_FLAG_SNSVALID = 1 << 1,
	SRP_RSP_FLAG_DOOVER   = 1 << 2,
	SRP_RSP_FLAG_DOUNDER  = 1 << 3,
	SRP_RSP_FLAG_DIOVER   = 1 << 4,
	SRP_RSP_FLAG_DIUNDER  = 1 << 5
};

struct srp_rsp {
	u8	opcode;
	u8	sol_not;
	u8	reserved1[2];
	__be32	req_lim_delta;
	u64	tag;
	u8	reserved2[2];
	u8	flags;
	u8	status;
	__be32	data_out_res_cnt;
	__be32	data_in_res_cnt;
	__be32	sense_data_len;
	__be32	resp_data_len;
	u8	data[0];
};

#endif /* SCSI_SRP_H */
