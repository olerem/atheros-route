/* Driver for USB Mass Storage compliant devices
 * Transport Functions Header File
 *
 * $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15_2MB/drivers/usb/storage/transport.h#1 $
 *
 * Current development and maintenance by:
 *   (c) 1999, 2000 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
 *
 * This driver is based on the 'USB Mass Storage Class' document. This
 * describes in detail the protocol used to communicate with such
 * devices.  Clearly, the designers had SCSI and ATAPI commands in
 * mind when they created this document.  The commands are all very
 * similar to commands in the SCSI-II and ATAPI specifications.
 *
 * It is important to note that in a number of cases this class
 * exhibits class-specific exemptions from the USB specification.
 * Notably the usage of NAK, STALL and ACK differs from the norm, in
 * that they are used to communicate wait, failed and OK on commands.
 *
 * Also, for certain devices, the interrupt endpoint is used to convey
 * status of a command.
 *
 * Please see http://www.one-eyed-alien.net/~mdharm/linux-usb for more
 * information about this driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _TRANSPORT_H_
#define _TRANSPORT_H_

#include <linux/config.h>
#include <linux/blkdev.h>

/* Protocols */

#define US_PR_CBI	0x00		/* Control/Bulk/Interrupt */
#define US_PR_CB	0x01		/* Control/Bulk w/o interrupt */
#define US_PR_BULK	0x50		/* bulk only */
#ifdef CONFIG_USB_STORAGE_USBAT
#define US_PR_USBAT	0x80		/* SCM-ATAPI bridge */
#endif
#ifdef CONFIG_USB_STORAGE_SDDR09
#define US_PR_EUSB_SDDR09	0x81	/* SCM-SCSI bridge for SDDR-09 */
#endif
#ifdef CONFIG_USB_STORAGE_SDDR55
#define US_PR_SDDR55	0x82		/* SDDR-55 (made up) */
#endif
#define US_PR_DPCM_USB  0xf0		/* Combination CB/SDDR09 */

#ifdef CONFIG_USB_STORAGE_FREECOM
#define US_PR_FREECOM   0xf1		/* Freecom */
#endif

#ifdef CONFIG_USB_STORAGE_DATAFAB
#define US_PR_DATAFAB   0xf2		/* Datafab chipsets */
#endif

#ifdef CONFIG_USB_STORAGE_JUMPSHOT
#define US_PR_JUMPSHOT  0xf3		/* Lexar Jumpshot */
#endif

#define US_PR_DEVICE	0xff		/* Use device's value */

/*
 * Bulk only data structures
 */

/* command block wrapper */
struct bulk_cb_wrap {
	__le32	Signature;		/* contains 'USBC' */
	__u32	Tag;			/* unique per command id */
	__le32	DataTransferLength;	/* size of data */
	__u8	Flags;			/* direction in bit 0 */
	__u8	Lun;			/* LUN normally 0 */
	__u8	Length;			/* of of the CDB */
	__u8	CDB[16];		/* max command */
};

#define US_BULK_CB_WRAP_LEN	31
#define US_BULK_CB_SIGN		0x43425355	/*spells out USBC */
#define US_BULK_FLAG_IN		1
#define US_BULK_FLAG_OUT	0

/* command status wrapper */
struct bulk_cs_wrap {
	__le32	Signature;		/* should = 'USBS' */
	__u32	Tag;			/* same as original command */
	__le32	Residue;		/* amount not transferred */
	__u8	Status;			/* see below */
	__u8	Filler[18];
};

#define US_BULK_CS_WRAP_LEN	13
#define US_BULK_CS_SIGN		0x53425355	/* spells out 'USBS' */
#define US_BULK_STAT_OK		0
#define US_BULK_STAT_FAIL	1
#define US_BULK_STAT_PHASE	2

/* bulk-only class specific requests */
#define US_BULK_RESET_REQUEST	0xff
#define US_BULK_GET_MAX_LUN	0xfe

/*
 * usb_stor_bulk_transfer_xxx() return codes, in order of severity
 */

#define USB_STOR_XFER_GOOD	0	/* good transfer                 */
#define USB_STOR_XFER_SHORT	1	/* transferred less than expected */
#define USB_STOR_XFER_STALLED	2	/* endpoint stalled              */
#define USB_STOR_XFER_LONG	3	/* device tried to send too much */
#define USB_STOR_XFER_ERROR	4	/* transfer died in the middle   */

/*
 * Transport return codes
 */

#define USB_STOR_TRANSPORT_GOOD	   0   /* Transport good, command good	   */
#define USB_STOR_TRANSPORT_FAILED  1   /* Transport good, command failed   */
#define USB_STOR_TRANSPORT_NO_SENSE 2  /* Command failed, no auto-sense    */
#define USB_STOR_TRANSPORT_ERROR   3   /* Transport bad (i.e. device dead) */

/*
 * We used to have USB_STOR_XFER_ABORTED and USB_STOR_TRANSPORT_ABORTED
 * return codes.  But now the transport and low-level transfer routines
 * treat an abort as just another error (-ENOENT for a cancelled URB).
 * It is up to the invoke_transport() function to test for aborts and
 * distinguish them from genuine communication errors.
 */

/*
 * CBI accept device specific command
 */

#define US_CBI_ADSC		0

extern int usb_stor_CBI_transport(struct scsi_cmnd *, struct us_data*);

extern int usb_stor_CB_transport(struct scsi_cmnd *, struct us_data*);
extern int usb_stor_CB_reset(struct us_data*);

extern int usb_stor_Bulk_transport(struct scsi_cmnd *, struct us_data*);
extern int usb_stor_Bulk_max_lun(struct us_data*);
extern int usb_stor_Bulk_reset(struct us_data*);

extern void usb_stor_invoke_transport(struct scsi_cmnd *, struct us_data*);
extern void usb_stor_stop_transport(struct us_data*);

extern int usb_stor_control_msg(struct us_data *us, unsigned int pipe,
		u8 request, u8 requesttype, u16 value, u16 index,
		void *data, u16 size, int timeout);
extern int usb_stor_clear_halt(struct us_data *us, unsigned int pipe);

extern int usb_stor_ctrl_transfer(struct us_data *us, unsigned int pipe,
		u8 request, u8 requesttype, u16 value, u16 index,
		void *data, u16 size);
extern int usb_stor_bulk_transfer_buf(struct us_data *us, unsigned int pipe,
		void *buf, unsigned int length, unsigned int *act_len);
extern int usb_stor_bulk_transfer_sg(struct us_data *us, unsigned int pipe,
		void *buf, unsigned int length, int use_sg, int *residual);

extern int usb_stor_port_reset(struct us_data *us);
#endif
