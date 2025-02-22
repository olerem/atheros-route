/*
 * Copyright (c) 2004 Mellanox Technologies Ltd.  All rights reserved.
 * Copyright (c) 2004 Infinicon Corporation.  All rights reserved.
 * Copyright (c) 2004 Intel Corporation.  All rights reserved.
 * Copyright (c) 2004 Topspin Corporation.  All rights reserved.
 * Copyright (c) 2004 Voltaire Corporation.  All rights reserved.
 * Copyright (c) 2005 Sun Microsystems, Inc. All rights reserved.
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
 * $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15_2MB/include/rdma/ib_verbs.h#1 $
 */

#if !defined(IB_VERBS_H)
#define IB_VERBS_H

#include <linux/types.h>
#include <linux/device.h>

#include <asm/atomic.h>
#include <asm/scatterlist.h>
#include <asm/uaccess.h>

union ib_gid {
	u8	raw[16];
	struct {
		__be64	subnet_prefix;
		__be64	interface_id;
	} global;
};

enum ib_node_type {
	IB_NODE_CA 	= 1,
	IB_NODE_SWITCH,
	IB_NODE_ROUTER
};

enum ib_device_cap_flags {
	IB_DEVICE_RESIZE_MAX_WR		= 1,
	IB_DEVICE_BAD_PKEY_CNTR		= (1<<1),
	IB_DEVICE_BAD_QKEY_CNTR		= (1<<2),
	IB_DEVICE_RAW_MULTI		= (1<<3),
	IB_DEVICE_AUTO_PATH_MIG		= (1<<4),
	IB_DEVICE_CHANGE_PHY_PORT	= (1<<5),
	IB_DEVICE_UD_AV_PORT_ENFORCE	= (1<<6),
	IB_DEVICE_CURR_QP_STATE_MOD	= (1<<7),
	IB_DEVICE_SHUTDOWN_PORT		= (1<<8),
	IB_DEVICE_INIT_TYPE		= (1<<9),
	IB_DEVICE_PORT_ACTIVE_EVENT	= (1<<10),
	IB_DEVICE_SYS_IMAGE_GUID	= (1<<11),
	IB_DEVICE_RC_RNR_NAK_GEN	= (1<<12),
	IB_DEVICE_SRQ_RESIZE		= (1<<13),
	IB_DEVICE_N_NOTIFY_CQ		= (1<<14),
};

enum ib_atomic_cap {
	IB_ATOMIC_NONE,
	IB_ATOMIC_HCA,
	IB_ATOMIC_GLOB
};

struct ib_device_attr {
	u64			fw_ver;
	__be64			node_guid;
	__be64			sys_image_guid;
	u64			max_mr_size;
	u64			page_size_cap;
	u32			vendor_id;
	u32			vendor_part_id;
	u32			hw_ver;
	int			max_qp;
	int			max_qp_wr;
	int			device_cap_flags;
	int			max_sge;
	int			max_sge_rd;
	int			max_cq;
	int			max_cqe;
	int			max_mr;
	int			max_pd;
	int			max_qp_rd_atom;
	int			max_ee_rd_atom;
	int			max_res_rd_atom;
	int			max_qp_init_rd_atom;
	int			max_ee_init_rd_atom;
	enum ib_atomic_cap	atomic_cap;
	int			max_ee;
	int			max_rdd;
	int			max_mw;
	int			max_raw_ipv6_qp;
	int			max_raw_ethy_qp;
	int			max_mcast_grp;
	int			max_mcast_qp_attach;
	int			max_total_mcast_qp_attach;
	int			max_ah;
	int			max_fmr;
	int			max_map_per_fmr;
	int			max_srq;
	int			max_srq_wr;
	int			max_srq_sge;
	u16			max_pkeys;
	u8			local_ca_ack_delay;
};

enum ib_mtu {
	IB_MTU_256  = 1,
	IB_MTU_512  = 2,
	IB_MTU_1024 = 3,
	IB_MTU_2048 = 4,
	IB_MTU_4096 = 5
};

static inline int ib_mtu_enum_to_int(enum ib_mtu mtu)
{
	switch (mtu) {
	case IB_MTU_256:  return  256;
	case IB_MTU_512:  return  512;
	case IB_MTU_1024: return 1024;
	case IB_MTU_2048: return 2048;
	case IB_MTU_4096: return 4096;
	default: 	  return -1;
	}
}

enum ib_port_state {
	IB_PORT_NOP		= 0,
	IB_PORT_DOWN		= 1,
	IB_PORT_INIT		= 2,
	IB_PORT_ARMED		= 3,
	IB_PORT_ACTIVE		= 4,
	IB_PORT_ACTIVE_DEFER	= 5
};

enum ib_port_cap_flags {
	IB_PORT_SM				= 1 <<  1,
	IB_PORT_NOTICE_SUP			= 1 <<  2,
	IB_PORT_TRAP_SUP			= 1 <<  3,
	IB_PORT_OPT_IPD_SUP                     = 1 <<  4,
	IB_PORT_AUTO_MIGR_SUP			= 1 <<  5,
	IB_PORT_SL_MAP_SUP			= 1 <<  6,
	IB_PORT_MKEY_NVRAM			= 1 <<  7,
	IB_PORT_PKEY_NVRAM			= 1 <<  8,
	IB_PORT_LED_INFO_SUP			= 1 <<  9,
	IB_PORT_SM_DISABLED			= 1 << 10,
	IB_PORT_SYS_IMAGE_GUID_SUP		= 1 << 11,
	IB_PORT_PKEY_SW_EXT_PORT_TRAP_SUP	= 1 << 12,
	IB_PORT_CM_SUP				= 1 << 16,
	IB_PORT_SNMP_TUNNEL_SUP			= 1 << 17,
	IB_PORT_REINIT_SUP			= 1 << 18,
	IB_PORT_DEVICE_MGMT_SUP			= 1 << 19,
	IB_PORT_VENDOR_CLASS_SUP		= 1 << 20,
	IB_PORT_DR_NOTICE_SUP			= 1 << 21,
	IB_PORT_CAP_MASK_NOTICE_SUP		= 1 << 22,
	IB_PORT_BOOT_MGMT_SUP			= 1 << 23,
	IB_PORT_LINK_LATENCY_SUP		= 1 << 24,
	IB_PORT_CLIENT_REG_SUP			= 1 << 25
};

enum ib_port_width {
	IB_WIDTH_1X	= 1,
	IB_WIDTH_4X	= 2,
	IB_WIDTH_8X	= 4,
	IB_WIDTH_12X	= 8
};

static inline int ib_width_enum_to_int(enum ib_port_width width)
{
	switch (width) {
	case IB_WIDTH_1X:  return  1;
	case IB_WIDTH_4X:  return  4;
	case IB_WIDTH_8X:  return  8;
	case IB_WIDTH_12X: return 12;
	default: 	  return -1;
	}
}

struct ib_port_attr {
	enum ib_port_state	state;
	enum ib_mtu		max_mtu;
	enum ib_mtu		active_mtu;
	int			gid_tbl_len;
	u32			port_cap_flags;
	u32			max_msg_sz;
	u32			bad_pkey_cntr;
	u32			qkey_viol_cntr;
	u16			pkey_tbl_len;
	u16			lid;
	u16			sm_lid;
	u8			lmc;
	u8			max_vl_num;
	u8			sm_sl;
	u8			subnet_timeout;
	u8			init_type_reply;
	u8			active_width;
	u8			active_speed;
	u8                      phys_state;
};

enum ib_device_modify_flags {
	IB_DEVICE_MODIFY_SYS_IMAGE_GUID	= 1
};

struct ib_device_modify {
	u64	sys_image_guid;
};

enum ib_port_modify_flags {
	IB_PORT_SHUTDOWN		= 1,
	IB_PORT_INIT_TYPE		= (1<<2),
	IB_PORT_RESET_QKEY_CNTR		= (1<<3)
};

struct ib_port_modify {
	u32	set_port_cap_mask;
	u32	clr_port_cap_mask;
	u8	init_type;
};

enum ib_event_type {
	IB_EVENT_CQ_ERR,
	IB_EVENT_QP_FATAL,
	IB_EVENT_QP_REQ_ERR,
	IB_EVENT_QP_ACCESS_ERR,
	IB_EVENT_COMM_EST,
	IB_EVENT_SQ_DRAINED,
	IB_EVENT_PATH_MIG,
	IB_EVENT_PATH_MIG_ERR,
	IB_EVENT_DEVICE_FATAL,
	IB_EVENT_PORT_ACTIVE,
	IB_EVENT_PORT_ERR,
	IB_EVENT_LID_CHANGE,
	IB_EVENT_PKEY_CHANGE,
	IB_EVENT_SM_CHANGE,
	IB_EVENT_SRQ_ERR,
	IB_EVENT_SRQ_LIMIT_REACHED,
	IB_EVENT_QP_LAST_WQE_REACHED
};

struct ib_event {
	struct ib_device	*device;
	union {
		struct ib_cq	*cq;
		struct ib_qp	*qp;
		struct ib_srq	*srq;
		u8		port_num;
	} element;
	enum ib_event_type	event;
};

struct ib_event_handler {
	struct ib_device *device;
	void            (*handler)(struct ib_event_handler *, struct ib_event *);
	struct list_head  list;
};

#define INIT_IB_EVENT_HANDLER(_ptr, _device, _handler)		\
	do {							\
		(_ptr)->device  = _device;			\
		(_ptr)->handler = _handler;			\
		INIT_LIST_HEAD(&(_ptr)->list);			\
	} while (0)

struct ib_global_route {
	union ib_gid	dgid;
	u32		flow_label;
	u8		sgid_index;
	u8		hop_limit;
	u8		traffic_class;
};

struct ib_grh {
	__be32		version_tclass_flow;
	__be16		paylen;
	u8		next_hdr;
	u8		hop_limit;
	union ib_gid	sgid;
	union ib_gid	dgid;
};

enum {
	IB_MULTICAST_QPN = 0xffffff
};

#define IB_LID_PERMISSIVE	__constant_htons(0xFFFF)

enum ib_ah_flags {
	IB_AH_GRH	= 1
};

struct ib_ah_attr {
	struct ib_global_route	grh;
	u16			dlid;
	u8			sl;
	u8			src_path_bits;
	u8			static_rate;
	u8			ah_flags;
	u8			port_num;
};

enum ib_wc_status {
	IB_WC_SUCCESS,
	IB_WC_LOC_LEN_ERR,
	IB_WC_LOC_QP_OP_ERR,
	IB_WC_LOC_EEC_OP_ERR,
	IB_WC_LOC_PROT_ERR,
	IB_WC_WR_FLUSH_ERR,
	IB_WC_MW_BIND_ERR,
	IB_WC_BAD_RESP_ERR,
	IB_WC_LOC_ACCESS_ERR,
	IB_WC_REM_INV_REQ_ERR,
	IB_WC_REM_ACCESS_ERR,
	IB_WC_REM_OP_ERR,
	IB_WC_RETRY_EXC_ERR,
	IB_WC_RNR_RETRY_EXC_ERR,
	IB_WC_LOC_RDD_VIOL_ERR,
	IB_WC_REM_INV_RD_REQ_ERR,
	IB_WC_REM_ABORT_ERR,
	IB_WC_INV_EECN_ERR,
	IB_WC_INV_EEC_STATE_ERR,
	IB_WC_FATAL_ERR,
	IB_WC_RESP_TIMEOUT_ERR,
	IB_WC_GENERAL_ERR
};

enum ib_wc_opcode {
	IB_WC_SEND,
	IB_WC_RDMA_WRITE,
	IB_WC_RDMA_READ,
	IB_WC_COMP_SWAP,
	IB_WC_FETCH_ADD,
	IB_WC_BIND_MW,
/*
 * Set value of IB_WC_RECV so consumers can test if a completion is a
 * receive by testing (opcode & IB_WC_RECV).
 */
	IB_WC_RECV			= 1 << 7,
	IB_WC_RECV_RDMA_WITH_IMM
};

enum ib_wc_flags {
	IB_WC_GRH		= 1,
	IB_WC_WITH_IMM		= (1<<1)
};

struct ib_wc {
	u64			wr_id;
	enum ib_wc_status	status;
	enum ib_wc_opcode	opcode;
	u32			vendor_err;
	u32			byte_len;
	__be32			imm_data;
	u32			qp_num;
	u32			src_qp;
	int			wc_flags;
	u16			pkey_index;
	u16			slid;
	u8			sl;
	u8			dlid_path_bits;
	u8			port_num;	/* valid only for DR SMPs on switches */
};

enum ib_cq_notify {
	IB_CQ_SOLICITED,
	IB_CQ_NEXT_COMP
};

enum ib_srq_attr_mask {
	IB_SRQ_MAX_WR	= 1 << 0,
	IB_SRQ_LIMIT	= 1 << 1,
};

struct ib_srq_attr {
	u32	max_wr;
	u32	max_sge;
	u32	srq_limit;
};

struct ib_srq_init_attr {
	void		      (*event_handler)(struct ib_event *, void *);
	void		       *srq_context;
	struct ib_srq_attr	attr;
};

struct ib_qp_cap {
	u32	max_send_wr;
	u32	max_recv_wr;
	u32	max_send_sge;
	u32	max_recv_sge;
	u32	max_inline_data;
};

enum ib_sig_type {
	IB_SIGNAL_ALL_WR,
	IB_SIGNAL_REQ_WR
};

enum ib_qp_type {
	/*
	 * IB_QPT_SMI and IB_QPT_GSI have to be the first two entries
	 * here (and in that order) since the MAD layer uses them as
	 * indices into a 2-entry table.
	 */
	IB_QPT_SMI,
	IB_QPT_GSI,

	IB_QPT_RC,
	IB_QPT_UC,
	IB_QPT_UD,
	IB_QPT_RAW_IPV6,
	IB_QPT_RAW_ETY
};

struct ib_qp_init_attr {
	void                  (*event_handler)(struct ib_event *, void *);
	void		       *qp_context;
	struct ib_cq	       *send_cq;
	struct ib_cq	       *recv_cq;
	struct ib_srq	       *srq;
	struct ib_qp_cap	cap;
	enum ib_sig_type	sq_sig_type;
	enum ib_qp_type		qp_type;
	u8			port_num; /* special QP types only */
};

enum ib_rnr_timeout {
	IB_RNR_TIMER_655_36 =  0,
	IB_RNR_TIMER_000_01 =  1,
	IB_RNR_TIMER_000_02 =  2,
	IB_RNR_TIMER_000_03 =  3,
	IB_RNR_TIMER_000_04 =  4,
	IB_RNR_TIMER_000_06 =  5,
	IB_RNR_TIMER_000_08 =  6,
	IB_RNR_TIMER_000_12 =  7,
	IB_RNR_TIMER_000_16 =  8,
	IB_RNR_TIMER_000_24 =  9,
	IB_RNR_TIMER_000_32 = 10,
	IB_RNR_TIMER_000_48 = 11,
	IB_RNR_TIMER_000_64 = 12,
	IB_RNR_TIMER_000_96 = 13,
	IB_RNR_TIMER_001_28 = 14,
	IB_RNR_TIMER_001_92 = 15,
	IB_RNR_TIMER_002_56 = 16,
	IB_RNR_TIMER_003_84 = 17,
	IB_RNR_TIMER_005_12 = 18,
	IB_RNR_TIMER_007_68 = 19,
	IB_RNR_TIMER_010_24 = 20,
	IB_RNR_TIMER_015_36 = 21,
	IB_RNR_TIMER_020_48 = 22,
	IB_RNR_TIMER_030_72 = 23,
	IB_RNR_TIMER_040_96 = 24,
	IB_RNR_TIMER_061_44 = 25,
	IB_RNR_TIMER_081_92 = 26,
	IB_RNR_TIMER_122_88 = 27,
	IB_RNR_TIMER_163_84 = 28,
	IB_RNR_TIMER_245_76 = 29,
	IB_RNR_TIMER_327_68 = 30,
	IB_RNR_TIMER_491_52 = 31
};

enum ib_qp_attr_mask {
	IB_QP_STATE			= 1,
	IB_QP_CUR_STATE			= (1<<1),
	IB_QP_EN_SQD_ASYNC_NOTIFY	= (1<<2),
	IB_QP_ACCESS_FLAGS		= (1<<3),
	IB_QP_PKEY_INDEX		= (1<<4),
	IB_QP_PORT			= (1<<5),
	IB_QP_QKEY			= (1<<6),
	IB_QP_AV			= (1<<7),
	IB_QP_PATH_MTU			= (1<<8),
	IB_QP_TIMEOUT			= (1<<9),
	IB_QP_RETRY_CNT			= (1<<10),
	IB_QP_RNR_RETRY			= (1<<11),
	IB_QP_RQ_PSN			= (1<<12),
	IB_QP_MAX_QP_RD_ATOMIC		= (1<<13),
	IB_QP_ALT_PATH			= (1<<14),
	IB_QP_MIN_RNR_TIMER		= (1<<15),
	IB_QP_SQ_PSN			= (1<<16),
	IB_QP_MAX_DEST_RD_ATOMIC	= (1<<17),
	IB_QP_PATH_MIG_STATE		= (1<<18),
	IB_QP_CAP			= (1<<19),
	IB_QP_DEST_QPN			= (1<<20)
};

enum ib_qp_state {
	IB_QPS_RESET,
	IB_QPS_INIT,
	IB_QPS_RTR,
	IB_QPS_RTS,
	IB_QPS_SQD,
	IB_QPS_SQE,
	IB_QPS_ERR
};

enum ib_mig_state {
	IB_MIG_MIGRATED,
	IB_MIG_REARM,
	IB_MIG_ARMED
};

struct ib_qp_attr {
	enum ib_qp_state	qp_state;
	enum ib_qp_state	cur_qp_state;
	enum ib_mtu		path_mtu;
	enum ib_mig_state	path_mig_state;
	u32			qkey;
	u32			rq_psn;
	u32			sq_psn;
	u32			dest_qp_num;
	int			qp_access_flags;
	struct ib_qp_cap	cap;
	struct ib_ah_attr	ah_attr;
	struct ib_ah_attr	alt_ah_attr;
	u16			pkey_index;
	u16			alt_pkey_index;
	u8			en_sqd_async_notify;
	u8			sq_draining;
	u8			max_rd_atomic;
	u8			max_dest_rd_atomic;
	u8			min_rnr_timer;
	u8			port_num;
	u8			timeout;
	u8			retry_cnt;
	u8			rnr_retry;
	u8			alt_port_num;
	u8			alt_timeout;
};

enum ib_wr_opcode {
	IB_WR_RDMA_WRITE,
	IB_WR_RDMA_WRITE_WITH_IMM,
	IB_WR_SEND,
	IB_WR_SEND_WITH_IMM,
	IB_WR_RDMA_READ,
	IB_WR_ATOMIC_CMP_AND_SWP,
	IB_WR_ATOMIC_FETCH_AND_ADD
};

enum ib_send_flags {
	IB_SEND_FENCE		= 1,
	IB_SEND_SIGNALED	= (1<<1),
	IB_SEND_SOLICITED	= (1<<2),
	IB_SEND_INLINE		= (1<<3)
};

struct ib_sge {
	u64	addr;
	u32	length;
	u32	lkey;
};

struct ib_send_wr {
	struct ib_send_wr      *next;
	u64			wr_id;
	struct ib_sge	       *sg_list;
	int			num_sge;
	enum ib_wr_opcode	opcode;
	int			send_flags;
	__be32			imm_data;
	union {
		struct {
			u64	remote_addr;
			u32	rkey;
		} rdma;
		struct {
			u64	remote_addr;
			u64	compare_add;
			u64	swap;
			u32	rkey;
		} atomic;
		struct {
			struct ib_ah *ah;
			u32	remote_qpn;
			u32	remote_qkey;
			u16	pkey_index; /* valid for GSI only */
			u8	port_num;   /* valid for DR SMPs on switch only */
		} ud;
	} wr;
};

struct ib_recv_wr {
	struct ib_recv_wr      *next;
	u64			wr_id;
	struct ib_sge	       *sg_list;
	int			num_sge;
};

enum ib_access_flags {
	IB_ACCESS_LOCAL_WRITE	= 1,
	IB_ACCESS_REMOTE_WRITE	= (1<<1),
	IB_ACCESS_REMOTE_READ	= (1<<2),
	IB_ACCESS_REMOTE_ATOMIC	= (1<<3),
	IB_ACCESS_MW_BIND	= (1<<4)
};

struct ib_phys_buf {
	u64      addr;
	u64      size;
};

struct ib_mr_attr {
	struct ib_pd	*pd;
	u64		device_virt_addr;
	u64		size;
	int		mr_access_flags;
	u32		lkey;
	u32		rkey;
};

enum ib_mr_rereg_flags {
	IB_MR_REREG_TRANS	= 1,
	IB_MR_REREG_PD		= (1<<1),
	IB_MR_REREG_ACCESS	= (1<<2)
};

struct ib_mw_bind {
	struct ib_mr   *mr;
	u64		wr_id;
	u64		addr;
	u32		length;
	int		send_flags;
	int		mw_access_flags;
};

struct ib_fmr_attr {
	int	max_pages;
	int	max_maps;
	u8	page_size;
};

struct ib_ucontext {
	struct ib_device       *device;
	struct list_head	pd_list;
	struct list_head	mr_list;
	struct list_head	mw_list;
	struct list_head	cq_list;
	struct list_head	qp_list;
	struct list_head	srq_list;
	struct list_head	ah_list;
};

struct ib_uobject {
	u64			user_handle;	/* handle given to us by userspace */
	struct ib_ucontext     *context;	/* associated user context */
	struct list_head	list;		/* link to context's list */
	u32			id;		/* index into kernel idr */
};

struct ib_umem {
	unsigned long		user_base;
	unsigned long		virt_base;
	size_t			length;
	int			offset;
	int			page_size;
	int                     writable;
	struct list_head	chunk_list;
};

struct ib_umem_chunk {
	struct list_head	list;
	int                     nents;
	int                     nmap;
	struct scatterlist      page_list[0];
};

struct ib_udata {
	void __user *inbuf;
	void __user *outbuf;
	size_t       inlen;
	size_t       outlen;
};

#define IB_UMEM_MAX_PAGE_CHUNK						\
	((PAGE_SIZE - offsetof(struct ib_umem_chunk, page_list)) /	\
	 ((void *) &((struct ib_umem_chunk *) 0)->page_list[1] -	\
	  (void *) &((struct ib_umem_chunk *) 0)->page_list[0]))

struct ib_umem_object {
	struct ib_uobject	uobject;
	struct ib_umem		umem;
};

struct ib_pd {
	struct ib_device       *device;
	struct ib_uobject      *uobject;
	atomic_t          	usecnt; /* count all resources */
};

struct ib_ah {
	struct ib_device	*device;
	struct ib_pd		*pd;
	struct ib_uobject	*uobject;
};

typedef void (*ib_comp_handler)(struct ib_cq *cq, void *cq_context);

struct ib_cq {
	struct ib_device       *device;
	struct ib_uobject      *uobject;
	ib_comp_handler   	comp_handler;
	void                  (*event_handler)(struct ib_event *, void *);
	void *            	cq_context;
	int               	cqe;
	atomic_t          	usecnt; /* count number of work queues */
};

struct ib_srq {
	struct ib_device       *device;
	struct ib_pd	       *pd;
	struct ib_uobject      *uobject;
	void		      (*event_handler)(struct ib_event *, void *);
	void		       *srq_context;
	atomic_t		usecnt;
};

struct ib_qp {
	struct ib_device       *device;
	struct ib_pd	       *pd;
	struct ib_cq	       *send_cq;
	struct ib_cq	       *recv_cq;
	struct ib_srq	       *srq;
	struct ib_uobject      *uobject;
	void                  (*event_handler)(struct ib_event *, void *);
	void		       *qp_context;
	u32			qp_num;
	enum ib_qp_type		qp_type;
};

struct ib_mr {
	struct ib_device  *device;
	struct ib_pd	  *pd;
	struct ib_uobject *uobject;
	u32		   lkey;
	u32		   rkey;
	atomic_t	   usecnt; /* count number of MWs */
};

struct ib_mw {
	struct ib_device	*device;
	struct ib_pd		*pd;
	struct ib_uobject	*uobject;
	u32			rkey;
};

struct ib_fmr {
	struct ib_device	*device;
	struct ib_pd		*pd;
	struct list_head	list;
	u32			lkey;
	u32			rkey;
};

struct ib_mad;
struct ib_grh;

enum ib_process_mad_flags {
	IB_MAD_IGNORE_MKEY	= 1,
	IB_MAD_IGNORE_BKEY	= 2,
	IB_MAD_IGNORE_ALL	= IB_MAD_IGNORE_MKEY | IB_MAD_IGNORE_BKEY
};

enum ib_mad_result {
	IB_MAD_RESULT_FAILURE  = 0,      /* (!SUCCESS is the important flag) */
	IB_MAD_RESULT_SUCCESS  = 1 << 0, /* MAD was successfully processed   */
	IB_MAD_RESULT_REPLY    = 1 << 1, /* Reply packet needs to be sent    */
	IB_MAD_RESULT_CONSUMED = 1 << 2  /* Packet consumed: stop processing */
};

#define IB_DEVICE_NAME_MAX 64

struct ib_cache {
	rwlock_t                lock;
	struct ib_event_handler event_handler;
	struct ib_pkey_cache  **pkey_cache;
	struct ib_gid_cache   **gid_cache;
};

struct ib_device {
	struct device                *dma_device;

	char                          name[IB_DEVICE_NAME_MAX];

	struct list_head              event_handler_list;
	spinlock_t                    event_handler_lock;

	struct list_head              core_list;
	struct list_head              client_data_list;
	spinlock_t                    client_data_lock;

	struct ib_cache               cache;

	u32                           flags;

	int		           (*query_device)(struct ib_device *device,
						   struct ib_device_attr *device_attr);
	int		           (*query_port)(struct ib_device *device,
						 u8 port_num,
						 struct ib_port_attr *port_attr);
	int		           (*query_gid)(struct ib_device *device,
						u8 port_num, int index,
						union ib_gid *gid);
	int		           (*query_pkey)(struct ib_device *device,
						 u8 port_num, u16 index, u16 *pkey);
	int		           (*modify_device)(struct ib_device *device,
						    int device_modify_mask,
						    struct ib_device_modify *device_modify);
	int		           (*modify_port)(struct ib_device *device,
						  u8 port_num, int port_modify_mask,
						  struct ib_port_modify *port_modify);
	struct ib_ucontext *       (*alloc_ucontext)(struct ib_device *device,
						     struct ib_udata *udata);
	int                        (*dealloc_ucontext)(struct ib_ucontext *context);
	int                        (*mmap)(struct ib_ucontext *context,
					   struct vm_area_struct *vma);
	struct ib_pd *             (*alloc_pd)(struct ib_device *device,
					       struct ib_ucontext *context,
					       struct ib_udata *udata);
	int                        (*dealloc_pd)(struct ib_pd *pd);
	struct ib_ah *             (*create_ah)(struct ib_pd *pd,
						struct ib_ah_attr *ah_attr);
	int                        (*modify_ah)(struct ib_ah *ah,
						struct ib_ah_attr *ah_attr);
	int                        (*query_ah)(struct ib_ah *ah,
					       struct ib_ah_attr *ah_attr);
	int                        (*destroy_ah)(struct ib_ah *ah);
	struct ib_srq *            (*create_srq)(struct ib_pd *pd,
						 struct ib_srq_init_attr *srq_init_attr,
						 struct ib_udata *udata);
	int                        (*modify_srq)(struct ib_srq *srq,
						 struct ib_srq_attr *srq_attr,
						 enum ib_srq_attr_mask srq_attr_mask);
	int                        (*query_srq)(struct ib_srq *srq,
						struct ib_srq_attr *srq_attr);
	int                        (*destroy_srq)(struct ib_srq *srq);
	int                        (*post_srq_recv)(struct ib_srq *srq,
						    struct ib_recv_wr *recv_wr,
						    struct ib_recv_wr **bad_recv_wr);
	struct ib_qp *             (*create_qp)(struct ib_pd *pd,
						struct ib_qp_init_attr *qp_init_attr,
						struct ib_udata *udata);
	int                        (*modify_qp)(struct ib_qp *qp,
						struct ib_qp_attr *qp_attr,
						int qp_attr_mask);
	int                        (*query_qp)(struct ib_qp *qp,
					       struct ib_qp_attr *qp_attr,
					       int qp_attr_mask,
					       struct ib_qp_init_attr *qp_init_attr);
	int                        (*destroy_qp)(struct ib_qp *qp);
	int                        (*post_send)(struct ib_qp *qp,
						struct ib_send_wr *send_wr,
						struct ib_send_wr **bad_send_wr);
	int                        (*post_recv)(struct ib_qp *qp,
						struct ib_recv_wr *recv_wr,
						struct ib_recv_wr **bad_recv_wr);
	struct ib_cq *             (*create_cq)(struct ib_device *device, int cqe,
						struct ib_ucontext *context,
						struct ib_udata *udata);
	int                        (*destroy_cq)(struct ib_cq *cq);
	int                        (*resize_cq)(struct ib_cq *cq, int cqe);
	int                        (*poll_cq)(struct ib_cq *cq, int num_entries,
					      struct ib_wc *wc);
	int                        (*peek_cq)(struct ib_cq *cq, int wc_cnt);
	int                        (*req_notify_cq)(struct ib_cq *cq,
						    enum ib_cq_notify cq_notify);
	int                        (*req_ncomp_notif)(struct ib_cq *cq,
						      int wc_cnt);
	struct ib_mr *             (*get_dma_mr)(struct ib_pd *pd,
						 int mr_access_flags);
	struct ib_mr *             (*reg_phys_mr)(struct ib_pd *pd,
						  struct ib_phys_buf *phys_buf_array,
						  int num_phys_buf,
						  int mr_access_flags,
						  u64 *iova_start);
	struct ib_mr *             (*reg_user_mr)(struct ib_pd *pd,
						  struct ib_umem *region,
						  int mr_access_flags,
						  struct ib_udata *udata);
	int                        (*query_mr)(struct ib_mr *mr,
					       struct ib_mr_attr *mr_attr);
	int                        (*dereg_mr)(struct ib_mr *mr);
	int                        (*rereg_phys_mr)(struct ib_mr *mr,
						    int mr_rereg_mask,
						    struct ib_pd *pd,
						    struct ib_phys_buf *phys_buf_array,
						    int num_phys_buf,
						    int mr_access_flags,
						    u64 *iova_start);
	struct ib_mw *             (*alloc_mw)(struct ib_pd *pd);
	int                        (*bind_mw)(struct ib_qp *qp,
					      struct ib_mw *mw,
					      struct ib_mw_bind *mw_bind);
	int                        (*dealloc_mw)(struct ib_mw *mw);
	struct ib_fmr *	           (*alloc_fmr)(struct ib_pd *pd,
						int mr_access_flags,
						struct ib_fmr_attr *fmr_attr);
	int		           (*map_phys_fmr)(struct ib_fmr *fmr,
						   u64 *page_list, int list_len,
						   u64 iova);
	int		           (*unmap_fmr)(struct list_head *fmr_list);
	int		           (*dealloc_fmr)(struct ib_fmr *fmr);
	int                        (*attach_mcast)(struct ib_qp *qp,
						   union ib_gid *gid,
						   u16 lid);
	int                        (*detach_mcast)(struct ib_qp *qp,
						   union ib_gid *gid,
						   u16 lid);
	int                        (*process_mad)(struct ib_device *device,
						  int process_mad_flags,
						  u8 port_num,
						  struct ib_wc *in_wc,
						  struct ib_grh *in_grh,
						  struct ib_mad *in_mad,
						  struct ib_mad *out_mad);

	struct module               *owner;
	struct class_device          class_dev;
	struct kobject               ports_parent;
	struct list_head             port_list;

	enum {
		IB_DEV_UNINITIALIZED,
		IB_DEV_REGISTERED,
		IB_DEV_UNREGISTERED
	}                            reg_state;

	u64			     uverbs_cmd_mask;
	int			     uverbs_abi_ver;

	u8                           node_type;
	u8                           phys_port_cnt;
};

struct ib_client {
	char  *name;
	void (*add)   (struct ib_device *);
	void (*remove)(struct ib_device *);

	struct list_head list;
};

struct ib_device *ib_alloc_device(size_t size);
void ib_dealloc_device(struct ib_device *device);

int ib_register_device   (struct ib_device *device);
void ib_unregister_device(struct ib_device *device);

int ib_register_client   (struct ib_client *client);
void ib_unregister_client(struct ib_client *client);

void *ib_get_client_data(struct ib_device *device, struct ib_client *client);
void  ib_set_client_data(struct ib_device *device, struct ib_client *client,
			 void *data);

static inline int ib_copy_from_udata(void *dest, struct ib_udata *udata, size_t len)
{
	return copy_from_user(dest, udata->inbuf, len) ? -EFAULT : 0;
}

static inline int ib_copy_to_udata(struct ib_udata *udata, void *src, size_t len)
{
	return copy_to_user(udata->outbuf, src, len) ? -EFAULT : 0;
}

int ib_register_event_handler  (struct ib_event_handler *event_handler);
int ib_unregister_event_handler(struct ib_event_handler *event_handler);
void ib_dispatch_event(struct ib_event *event);

int ib_query_device(struct ib_device *device,
		    struct ib_device_attr *device_attr);

int ib_query_port(struct ib_device *device,
		  u8 port_num, struct ib_port_attr *port_attr);

int ib_query_gid(struct ib_device *device,
		 u8 port_num, int index, union ib_gid *gid);

int ib_query_pkey(struct ib_device *device,
		  u8 port_num, u16 index, u16 *pkey);

int ib_modify_device(struct ib_device *device,
		     int device_modify_mask,
		     struct ib_device_modify *device_modify);

int ib_modify_port(struct ib_device *device,
		   u8 port_num, int port_modify_mask,
		   struct ib_port_modify *port_modify);

/**
 * ib_alloc_pd - Allocates an unused protection domain.
 * @device: The device on which to allocate the protection domain.
 *
 * A protection domain object provides an association between QPs, shared
 * receive queues, address handles, memory regions, and memory windows.
 */
struct ib_pd *ib_alloc_pd(struct ib_device *device);

/**
 * ib_dealloc_pd - Deallocates a protection domain.
 * @pd: The protection domain to deallocate.
 */
int ib_dealloc_pd(struct ib_pd *pd);

/**
 * ib_create_ah - Creates an address handle for the given address vector.
 * @pd: The protection domain associated with the address handle.
 * @ah_attr: The attributes of the address vector.
 *
 * The address handle is used to reference a local or global destination
 * in all UD QP post sends.
 */
struct ib_ah *ib_create_ah(struct ib_pd *pd, struct ib_ah_attr *ah_attr);

/**
 * ib_create_ah_from_wc - Creates an address handle associated with the
 *   sender of the specified work completion.
 * @pd: The protection domain associated with the address handle.
 * @wc: Work completion information associated with a received message.
 * @grh: References the received global route header.  This parameter is
 *   ignored unless the work completion indicates that the GRH is valid.
 * @port_num: The outbound port number to associate with the address.
 *
 * The address handle is used to reference a local or global destination
 * in all UD QP post sends.
 */
struct ib_ah *ib_create_ah_from_wc(struct ib_pd *pd, struct ib_wc *wc,
				   struct ib_grh *grh, u8 port_num);

/**
 * ib_modify_ah - Modifies the address vector associated with an address
 *   handle.
 * @ah: The address handle to modify.
 * @ah_attr: The new address vector attributes to associate with the
 *   address handle.
 */
int ib_modify_ah(struct ib_ah *ah, struct ib_ah_attr *ah_attr);

/**
 * ib_query_ah - Queries the address vector associated with an address
 *   handle.
 * @ah: The address handle to query.
 * @ah_attr: The address vector attributes associated with the address
 *   handle.
 */
int ib_query_ah(struct ib_ah *ah, struct ib_ah_attr *ah_attr);

/**
 * ib_destroy_ah - Destroys an address handle.
 * @ah: The address handle to destroy.
 */
int ib_destroy_ah(struct ib_ah *ah);

/**
 * ib_create_srq - Creates a SRQ associated with the specified protection
 *   domain.
 * @pd: The protection domain associated with the SRQ.
 * @srq_init_attr: A list of initial attributes required to create the SRQ.
 *
 * srq_attr->max_wr and srq_attr->max_sge are read the determine the
 * requested size of the SRQ, and set to the actual values allocated
 * on return.  If ib_create_srq() succeeds, then max_wr and max_sge
 * will always be at least as large as the requested values.
 */
struct ib_srq *ib_create_srq(struct ib_pd *pd,
			     struct ib_srq_init_attr *srq_init_attr);

/**
 * ib_modify_srq - Modifies the attributes for the specified SRQ.
 * @srq: The SRQ to modify.
 * @srq_attr: On input, specifies the SRQ attributes to modify.  On output,
 *   the current values of selected SRQ attributes are returned.
 * @srq_attr_mask: A bit-mask used to specify which attributes of the SRQ
 *   are being modified.
 *
 * The mask may contain IB_SRQ_MAX_WR to resize the SRQ and/or
 * IB_SRQ_LIMIT to set the SRQ's limit and request notification when
 * the number of receives queued drops below the limit.
 */
int ib_modify_srq(struct ib_srq *srq,
		  struct ib_srq_attr *srq_attr,
		  enum ib_srq_attr_mask srq_attr_mask);

/**
 * ib_query_srq - Returns the attribute list and current values for the
 *   specified SRQ.
 * @srq: The SRQ to query.
 * @srq_attr: The attributes of the specified SRQ.
 */
int ib_query_srq(struct ib_srq *srq,
		 struct ib_srq_attr *srq_attr);

/**
 * ib_destroy_srq - Destroys the specified SRQ.
 * @srq: The SRQ to destroy.
 */
int ib_destroy_srq(struct ib_srq *srq);

/**
 * ib_post_srq_recv - Posts a list of work requests to the specified SRQ.
 * @srq: The SRQ to post the work request on.
 * @recv_wr: A list of work requests to post on the receive queue.
 * @bad_recv_wr: On an immediate failure, this parameter will reference
 *   the work request that failed to be posted on the QP.
 */
static inline int ib_post_srq_recv(struct ib_srq *srq,
				   struct ib_recv_wr *recv_wr,
				   struct ib_recv_wr **bad_recv_wr)
{
	return srq->device->post_srq_recv(srq, recv_wr, bad_recv_wr);
}

/**
 * ib_create_qp - Creates a QP associated with the specified protection
 *   domain.
 * @pd: The protection domain associated with the QP.
 * @qp_init_attr: A list of initial attributes required to create the QP.
 */
struct ib_qp *ib_create_qp(struct ib_pd *pd,
			   struct ib_qp_init_attr *qp_init_attr);

/**
 * ib_modify_qp - Modifies the attributes for the specified QP and then
 *   transitions the QP to the given state.
 * @qp: The QP to modify.
 * @qp_attr: On input, specifies the QP attributes to modify.  On output,
 *   the current values of selected QP attributes are returned.
 * @qp_attr_mask: A bit-mask used to specify which attributes of the QP
 *   are being modified.
 */
int ib_modify_qp(struct ib_qp *qp,
		 struct ib_qp_attr *qp_attr,
		 int qp_attr_mask);

/**
 * ib_query_qp - Returns the attribute list and current values for the
 *   specified QP.
 * @qp: The QP to query.
 * @qp_attr: The attributes of the specified QP.
 * @qp_attr_mask: A bit-mask used to select specific attributes to query.
 * @qp_init_attr: Additional attributes of the selected QP.
 *
 * The qp_attr_mask may be used to limit the query to gathering only the
 * selected attributes.
 */
int ib_query_qp(struct ib_qp *qp,
		struct ib_qp_attr *qp_attr,
		int qp_attr_mask,
		struct ib_qp_init_attr *qp_init_attr);

/**
 * ib_destroy_qp - Destroys the specified QP.
 * @qp: The QP to destroy.
 */
int ib_destroy_qp(struct ib_qp *qp);

/**
 * ib_post_send - Posts a list of work requests to the send queue of
 *   the specified QP.
 * @qp: The QP to post the work request on.
 * @send_wr: A list of work requests to post on the send queue.
 * @bad_send_wr: On an immediate failure, this parameter will reference
 *   the work request that failed to be posted on the QP.
 */
static inline int ib_post_send(struct ib_qp *qp,
			       struct ib_send_wr *send_wr,
			       struct ib_send_wr **bad_send_wr)
{
	return qp->device->post_send(qp, send_wr, bad_send_wr);
}

/**
 * ib_post_recv - Posts a list of work requests to the receive queue of
 *   the specified QP.
 * @qp: The QP to post the work request on.
 * @recv_wr: A list of work requests to post on the receive queue.
 * @bad_recv_wr: On an immediate failure, this parameter will reference
 *   the work request that failed to be posted on the QP.
 */
static inline int ib_post_recv(struct ib_qp *qp,
			       struct ib_recv_wr *recv_wr,
			       struct ib_recv_wr **bad_recv_wr)
{
	return qp->device->post_recv(qp, recv_wr, bad_recv_wr);
}

/**
 * ib_create_cq - Creates a CQ on the specified device.
 * @device: The device on which to create the CQ.
 * @comp_handler: A user-specified callback that is invoked when a
 *   completion event occurs on the CQ.
 * @event_handler: A user-specified callback that is invoked when an
 *   asynchronous event not associated with a completion occurs on the CQ.
 * @cq_context: Context associated with the CQ returned to the user via
 *   the associated completion and event handlers.
 * @cqe: The minimum size of the CQ.
 *
 * Users can examine the cq structure to determine the actual CQ size.
 */
struct ib_cq *ib_create_cq(struct ib_device *device,
			   ib_comp_handler comp_handler,
			   void (*event_handler)(struct ib_event *, void *),
			   void *cq_context, int cqe);

/**
 * ib_resize_cq - Modifies the capacity of the CQ.
 * @cq: The CQ to resize.
 * @cqe: The minimum size of the CQ.
 *
 * Users can examine the cq structure to determine the actual CQ size.
 */
int ib_resize_cq(struct ib_cq *cq, int cqe);

/**
 * ib_destroy_cq - Destroys the specified CQ.
 * @cq: The CQ to destroy.
 */
int ib_destroy_cq(struct ib_cq *cq);

/**
 * ib_poll_cq - poll a CQ for completion(s)
 * @cq:the CQ being polled
 * @num_entries:maximum number of completions to return
 * @wc:array of at least @num_entries &struct ib_wc where completions
 *   will be returned
 *
 * Poll a CQ for (possibly multiple) completions.  If the return value
 * is < 0, an error occurred.  If the return value is >= 0, it is the
 * number of completions returned.  If the return value is
 * non-negative and < num_entries, then the CQ was emptied.
 */
static inline int ib_poll_cq(struct ib_cq *cq, int num_entries,
			     struct ib_wc *wc)
{
	return cq->device->poll_cq(cq, num_entries, wc);
}

/**
 * ib_peek_cq - Returns the number of unreaped completions currently
 *   on the specified CQ.
 * @cq: The CQ to peek.
 * @wc_cnt: A minimum number of unreaped completions to check for.
 *
 * If the number of unreaped completions is greater than or equal to wc_cnt,
 * this function returns wc_cnt, otherwise, it returns the actual number of
 * unreaped completions.
 */
int ib_peek_cq(struct ib_cq *cq, int wc_cnt);

/**
 * ib_req_notify_cq - Request completion notification on a CQ.
 * @cq: The CQ to generate an event for.
 * @cq_notify: If set to %IB_CQ_SOLICITED, completion notification will
 *   occur on the next solicited event. If set to %IB_CQ_NEXT_COMP,
 *   notification will occur on the next completion.
 */
static inline int ib_req_notify_cq(struct ib_cq *cq,
				   enum ib_cq_notify cq_notify)
{
	return cq->device->req_notify_cq(cq, cq_notify);
}

/**
 * ib_req_ncomp_notif - Request completion notification when there are
 *   at least the specified number of unreaped completions on the CQ.
 * @cq: The CQ to generate an event for.
 * @wc_cnt: The number of unreaped completions that should be on the
 *   CQ before an event is generated.
 */
static inline int ib_req_ncomp_notif(struct ib_cq *cq, int wc_cnt)
{
	return cq->device->req_ncomp_notif ?
		cq->device->req_ncomp_notif(cq, wc_cnt) :
		-ENOSYS;
}

/**
 * ib_get_dma_mr - Returns a memory region for system memory that is
 *   usable for DMA.
 * @pd: The protection domain associated with the memory region.
 * @mr_access_flags: Specifies the memory access rights.
 */
struct ib_mr *ib_get_dma_mr(struct ib_pd *pd, int mr_access_flags);

/**
 * ib_reg_phys_mr - Prepares a virtually addressed memory region for use
 *   by an HCA.
 * @pd: The protection domain associated assigned to the registered region.
 * @phys_buf_array: Specifies a list of physical buffers to use in the
 *   memory region.
 * @num_phys_buf: Specifies the size of the phys_buf_array.
 * @mr_access_flags: Specifies the memory access rights.
 * @iova_start: The offset of the region's starting I/O virtual address.
 */
struct ib_mr *ib_reg_phys_mr(struct ib_pd *pd,
			     struct ib_phys_buf *phys_buf_array,
			     int num_phys_buf,
			     int mr_access_flags,
			     u64 *iova_start);

/**
 * ib_rereg_phys_mr - Modifies the attributes of an existing memory region.
 *   Conceptually, this call performs the functions deregister memory region
 *   followed by register physical memory region.  Where possible,
 *   resources are reused instead of deallocated and reallocated.
 * @mr: The memory region to modify.
 * @mr_rereg_mask: A bit-mask used to indicate which of the following
 *   properties of the memory region are being modified.
 * @pd: If %IB_MR_REREG_PD is set in mr_rereg_mask, this field specifies
 *   the new protection domain to associated with the memory region,
 *   otherwise, this parameter is ignored.
 * @phys_buf_array: If %IB_MR_REREG_TRANS is set in mr_rereg_mask, this
 *   field specifies a list of physical buffers to use in the new
 *   translation, otherwise, this parameter is ignored.
 * @num_phys_buf: If %IB_MR_REREG_TRANS is set in mr_rereg_mask, this
 *   field specifies the size of the phys_buf_array, otherwise, this
 *   parameter is ignored.
 * @mr_access_flags: If %IB_MR_REREG_ACCESS is set in mr_rereg_mask, this
 *   field specifies the new memory access rights, otherwise, this
 *   parameter is ignored.
 * @iova_start: The offset of the region's starting I/O virtual address.
 */
int ib_rereg_phys_mr(struct ib_mr *mr,
		     int mr_rereg_mask,
		     struct ib_pd *pd,
		     struct ib_phys_buf *phys_buf_array,
		     int num_phys_buf,
		     int mr_access_flags,
		     u64 *iova_start);

/**
 * ib_query_mr - Retrieves information about a specific memory region.
 * @mr: The memory region to retrieve information about.
 * @mr_attr: The attributes of the specified memory region.
 */
int ib_query_mr(struct ib_mr *mr, struct ib_mr_attr *mr_attr);

/**
 * ib_dereg_mr - Deregisters a memory region and removes it from the
 *   HCA translation table.
 * @mr: The memory region to deregister.
 */
int ib_dereg_mr(struct ib_mr *mr);

/**
 * ib_alloc_mw - Allocates a memory window.
 * @pd: The protection domain associated with the memory window.
 */
struct ib_mw *ib_alloc_mw(struct ib_pd *pd);

/**
 * ib_bind_mw - Posts a work request to the send queue of the specified
 *   QP, which binds the memory window to the given address range and
 *   remote access attributes.
 * @qp: QP to post the bind work request on.
 * @mw: The memory window to bind.
 * @mw_bind: Specifies information about the memory window, including
 *   its address range, remote access rights, and associated memory region.
 */
static inline int ib_bind_mw(struct ib_qp *qp,
			     struct ib_mw *mw,
			     struct ib_mw_bind *mw_bind)
{
	/* XXX reference counting in corresponding MR? */
	return mw->device->bind_mw ?
		mw->device->bind_mw(qp, mw, mw_bind) :
		-ENOSYS;
}

/**
 * ib_dealloc_mw - Deallocates a memory window.
 * @mw: The memory window to deallocate.
 */
int ib_dealloc_mw(struct ib_mw *mw);

/**
 * ib_alloc_fmr - Allocates a unmapped fast memory region.
 * @pd: The protection domain associated with the unmapped region.
 * @mr_access_flags: Specifies the memory access rights.
 * @fmr_attr: Attributes of the unmapped region.
 *
 * A fast memory region must be mapped before it can be used as part of
 * a work request.
 */
struct ib_fmr *ib_alloc_fmr(struct ib_pd *pd,
			    int mr_access_flags,
			    struct ib_fmr_attr *fmr_attr);

/**
 * ib_map_phys_fmr - Maps a list of physical pages to a fast memory region.
 * @fmr: The fast memory region to associate with the pages.
 * @page_list: An array of physical pages to map to the fast memory region.
 * @list_len: The number of pages in page_list.
 * @iova: The I/O virtual address to use with the mapped region.
 */
static inline int ib_map_phys_fmr(struct ib_fmr *fmr,
				  u64 *page_list, int list_len,
				  u64 iova)
{
	return fmr->device->map_phys_fmr(fmr, page_list, list_len, iova);
}

/**
 * ib_unmap_fmr - Removes the mapping from a list of fast memory regions.
 * @fmr_list: A linked list of fast memory regions to unmap.
 */
int ib_unmap_fmr(struct list_head *fmr_list);

/**
 * ib_dealloc_fmr - Deallocates a fast memory region.
 * @fmr: The fast memory region to deallocate.
 */
int ib_dealloc_fmr(struct ib_fmr *fmr);

/**
 * ib_attach_mcast - Attaches the specified QP to a multicast group.
 * @qp: QP to attach to the multicast group.  The QP must be type
 *   IB_QPT_UD.
 * @gid: Multicast group GID.
 * @lid: Multicast group LID in host byte order.
 *
 * In order to send and receive multicast packets, subnet
 * administration must have created the multicast group and configured
 * the fabric appropriately.  The port associated with the specified
 * QP must also be a member of the multicast group.
 */
int ib_attach_mcast(struct ib_qp *qp, union ib_gid *gid, u16 lid);

/**
 * ib_detach_mcast - Detaches the specified QP from a multicast group.
 * @qp: QP to detach from the multicast group.
 * @gid: Multicast group GID.
 * @lid: Multicast group LID in host byte order.
 */
int ib_detach_mcast(struct ib_qp *qp, union ib_gid *gid, u16 lid);

#endif /* IB_VERBS_H */
