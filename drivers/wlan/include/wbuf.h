/*
 * Copyright (c) 2005, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _WBUF_H
#define _WBUF_H

/*
 * Definition and API's for OS-independent packet data structure
 */
enum wbuf_type{
    WBUF_TX_DATA = 0,               /* normal tx data frame sent from NDIS */
    WBUF_TX_MGMT,                   /* internally generated management frame */
    WBUF_TX_BEACON,                 /* internally generated beacon frame */
    WBUF_RX,                        /* rx buffer that will be used for DMA */
    WBUF_RX_INTERNAL,               /* rx buffer that won't be used for DMA */
    WBUF_TX_INTERNAL,
#ifndef UMAC
    WBUF_TX_COALESCING,
#endif
#ifdef ATH_SUPPORT_HTC
    WBUF_HTC_CMD,
    WBUF_USB_RX,
    WBUF_USB_TX,    
#endif
    WBUF_TX_CTL,                    /* used to send control frames,
                                     * currently cfend */
    WBUF_MAX_TYPE
};

#define WB_STATUS_OK                0x0000
#define WB_STATUS_TX_ERROR          0x0001

#include <wbuf_private.h>

enum wbuf_exemption_type {
    WBUF_EXEMPT_NO_EXEMPTION,
    WBUF_EXEMPT_ALWAYS,
    WBUF_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE
};

/*
 * OS independent packet data structure. Each OS should define its own wbuf
 * in wbuf_private.h and implement __wbuf_xxx() functions.
 * Other layers should not directly reference the members in wbuf structure.
 */
typedef __wbuf_t wbuf_t;

/*
 * API's for wbuf
 */
wbuf_t wbuf_alloc(osdev_t os_handle, enum wbuf_type, u_int32_t len);
void wbuf_release(osdev_t os_handle, wbuf_t wbuf);

static INLINE u_int8_t *wbuf_header(wbuf_t wbuf) { return __wbuf_header(wbuf); }
static INLINE u_int16_t wbuf_get_pktlen(wbuf_t wbuf) { return __wbuf_get_pktlen(wbuf); }
static INLINE void *wbuf_raw_data(wbuf_t wbuf) { return __wbuf_raw_data(wbuf); }
static INLINE u_int32_t wbuf_get_len(wbuf_t wbuf) { return __wbuf_get_len(wbuf); }

// wbuf_get_datalen_temp *** Should NOT *** be used again in any other part of the code. This is a temporary fix to a semantic problem 
// with the use of wbuf_get_len. wbuf_get_len has been interchangeably used to mean wbuf_get_tailroom, which causes issues for darwin an netbsd, and presumably 
// for windows as well. This macro and its use should be removed as soon as the right fix is implemented.
static INLINE u_int32_t wbuf_get_datalen_temp(wbuf_t wbuf) { return __wbuf_get_datalen_temp(wbuf); }

static INLINE u_int32_t wbuf_get_tailroom(wbuf_t wbuf) { return __wbuf_get_tailroom(wbuf); }

static INLINE int wbuf_get_priority(wbuf_t wbuf) { return __wbuf_get_priority(wbuf); }
static INLINE void wbuf_set_priority(wbuf_t wbuf, int priority) { __wbuf_set_priority(wbuf, priority); }
static INLINE int wbuf_get_tid(wbuf_t wbuf) { return __wbuf_get_tid(wbuf); }
static INLINE void wbuf_set_tid(wbuf_t wbuf, u_int8_t tid) { __wbuf_set_tid(wbuf, tid); }
static INLINE struct ieee80211_node *wbuf_get_node(wbuf_t wbuf) { return __wbuf_get_node(wbuf); }
static INLINE void wbuf_set_node(wbuf_t wbuf, struct ieee80211_node *ni) { __wbuf_set_node(wbuf, ni); }
static INLINE void *wbuf_get_context(wbuf_t wbuf) { return __wbuf_get_context(wbuf); }

#ifdef __CARRIER_PLATFORM__
static INLINE int wbuf_is_encap_done(wbuf_t wbuf) { return __wbuf_is_encap_done(wbuf); }
static INLINE void wbuf_set_encap_done(wbuf_t wbuf) { __wbuf_set_encap_done(wbuf); }
static INLINE void wbuf_clr_encap_done(wbuf_t wbuf) { __wbuf_clr_encap_done(wbuf); }
#endif

static INLINE int wbuf_is_eapol(wbuf_t wbuf) { return __wbuf_is_eapol(wbuf); }
static INLINE void wbuf_set_amsdu(wbuf_t wbuf) { __wbuf_set_amsdu(wbuf); }
static INLINE int wbuf_is_amsdu(wbuf_t wbuf) { return __wbuf_is_amsdu(wbuf); }
static INLINE void wbuf_set_pwrsaveframe(wbuf_t wbuf) { __wbuf_set_pwrsaveframe(wbuf); }
static INLINE int wbuf_is_pwrsaveframe(wbuf_t wbuf) { return __wbuf_is_pwrsaveframe(wbuf); }
static INLINE int wbuf_is_smpsframe(wbuf_t wbuf) { return __wbuf_is_smpsframe(wbuf); }
static INLINE int wbuf_is_fastframe(wbuf_t wbuf) { return __wbuf_is_fastframe(wbuf); }
static INLINE void wbuf_set_fastframe(wbuf_t wbuf) { __wbuf_set_fastframe(wbuf); }
static INLINE int wbuf_is_qosframe(wbuf_t wbuf) { return __wbuf_is_qosframe(wbuf); }
static INLINE void wbuf_set_qosframe(wbuf_t wbuf) { __wbuf_set_qosframe(wbuf); }
#ifdef ATH_SUPPORT_WAPI
#ifdef __linux__
static INLINE int wbuf_is_wai(wbuf_t wbuf) { return __wbuf_is_wai(wbuf); }
#else //other os need implement this function to support WAPI as AP
#define wbuf_is_wai(wbuf) false
#endif
#endif

static INLINE int wbuf_get_exemption_type(wbuf_t wbuf) { return __wbuf_get_exemption_type(wbuf); }
static INLINE void wbuf_set_exemption_type(wbuf_t wbuf, int type) { __wbuf_set_exemption_type(wbuf, type); }

#ifdef ATH_SUPPORT_UAPSD
static INLINE int wbuf_is_uapsd(wbuf_t wbuf) { return __wbuf_is_uapsd(wbuf); }
static INLINE void wbuf_set_uapsd(wbuf_t wbuf) { __wbuf_set_uapsd(wbuf);}
static INLINE void wbuf_clear_uapsd(wbuf_t wbuf) { __wbuf_clear_uapsd(wbuf);}
static INLINE void wbuf_uapsd_update(wbuf_t wbuf) { __wbuf_uapsd_update(wbuf); }
#else
#define wbuf_is_uapsd(wbuf)      false
#define wbuf_set_uapsd(wbuf)
#define wbuf_clear_uapsd(wbuf)
#define wbuf_uapsd_update(wbuf)
#endif

static INLINE int wbuf_is_initimbf(wbuf_t wbuf) { return __wbuf_is_initimbf(wbuf); }

static INLINE void wbuf_complete(wbuf_t wbuf) { __wbuf_complete(wbuf); }

static INLINE int
wbuf_map_sg(osdev_t os_handle, wbuf_t wbuf,  dma_context_t context, void *arg)
{
    return __wbuf_map_sg(os_handle, wbuf, context, arg);
}

static INLINE void
wbuf_unmap_sg(osdev_t os_handle, wbuf_t wbuf, dma_context_t context)
{
    __wbuf_unmap_sg(os_handle, wbuf, context);
}

static INLINE dma_addr_t
wbuf_map_single(osdev_t os_handle, wbuf_t wbuf, int direction, dma_context_t context)
{
    return __wbuf_map_single(os_handle, wbuf, direction, context);
}

static INLINE void
wbuf_unmap_single(osdev_t os_handle, wbuf_t wbuf, int direction, dma_context_t context)
{
    __wbuf_unmap_single(os_handle, wbuf, direction, context);
}

static INLINE void wbuf_init(wbuf_t wbuf, u_int16_t pktlen) { __wbuf_init(wbuf, pktlen); }
static INLINE void wbuf_free(wbuf_t wbuf) { __wbuf_free(wbuf); }

static INLINE u_int8_t *wbuf_push(wbuf_t wbuf, u_int16_t size) { return __wbuf_push(wbuf, size); }
static INLINE u_int16_t wbuf_hdrspace(wbuf_t wbuf) { return __wbuf_hdrspace(wbuf); }
static INLINE void wbuf_trim(wbuf_t wbuf, u_int16_t size) { __wbuf_trim(wbuf, size); }
static INLINE u_int8_t *wbuf_pull(wbuf_t wbuf, u_int16_t size) { return __wbuf_pull(wbuf, size); }
static INLINE int wbuf_append(wbuf_t wbuf, u_int16_t size) { return __wbuf_append(wbuf, size); }
static INLINE u_int8_t *wbuf_get_scatteredbuf_header(wbuf_t wbuf, u_int16_t len) { return __wbuf_get_scatteredbuf_header(wbuf, len); }

static INLINE int wbuf_copydata(wbuf_t wbuf, u_int16_t off, u_int16_t len, void *out_data)
{
    return __wbuf_copydata(wbuf, off, len, out_data);
}

#if ATH_SUPPORT_IQUE || UMAC_SUPPORT_NAWDS != 0
static INLINE wbuf_t wbuf_copy(wbuf_t wbuf) { return __wbuf_copy(wbuf); }
static INLINE int wbuf_is_probing(wbuf_t wbuf) { return __wbuf_is_probing(wbuf); }
static INLINE void wbuf_set_probing(wbuf_t wbuf) { __wbuf_set_probing(wbuf); }
static INLINE void wbuf_clear_probing(wbuf_t wbuf) { __wbuf_clear_probing(wbuf); }
#endif
#if ATH_RXBUF_RECYCLE
static INLINE int wbuf_is_cloned(wbuf_t wbuf) { return __wbuf_is_cloned(wbuf); }
static INLINE void wbuf_set_cloned(wbuf_t wbuf) { __wbuf_set_cloned(wbuf); }
static INLINE void wbuf_clear_cloned(wbuf_t wbuf) { __wbuf_clear_cloned(wbuf); }
#endif
static INLINE wbuf_t wbuf_next(wbuf_t wbuf) { return __wbuf_next(wbuf); }
static INLINE wbuf_t wbuf_next_buf(wbuf_t wbuf) { return __wbuf_next_buf(wbuf); }
static INLINE void  wbuf_set_next(wbuf_t wbuf, wbuf_t next) {  __wbuf_set_next(wbuf, next); }
static INLINE void  wbuf_setnextpkt(wbuf_t wbuf, wbuf_t next) {  __wbuf_setnextpkt(wbuf, next); }
static INLINE void  wbuf_set_age(wbuf_t wbuf, u_int32_t age) { __wbuf_set_age(wbuf,age); }
static INLINE u_int32_t wbuf_get_age(wbuf_t wbuf) { return __wbuf_get_age(wbuf); }
static INLINE wbuf_t wbuf_clone(osdev_t os_handle, wbuf_t wbuf) { return __wbuf_clone(os_handle, wbuf); }
static INLINE wbuf_t wbuf_coalesce(osdev_t os_handle, wbuf_t wbuf) { return __wbuf_coalesce(os_handle, wbuf); }
static INLINE void wbuf_set_complete_handler(wbuf_t wbuf,void *handler, void *arg)
{ 
    __wbuf_set_complete_handler( wbuf,handler,arg);
}

static INLINE void wbuf_get_complete_handler(wbuf_t wbuf,void **phandler, void **parg)
{ 
    __wbuf_get_complete_handler(wbuf,phandler,parg);
}

#ifdef ATH_SUPPORT_HTC
static INLINE wbuf_t wbuf_realloc_hdrspace(wbuf_t wbuf, u_int16_t size) { return __wbuf_realloc_hdrspace(wbuf, size); }
static INLINE int wbuf_cat(wbuf_t dst, wbuf_t src) { return __wbuf_cat(dst, src); }
#endif

#if defined(ATH_SUPPORT_VOWEXT) || defined(ATH_SUPPORT_IQUE) || defined(VOW_LOGLATENCY) || UMAC_SUPPORT_NAWDS != 0
static INLINE u_int8_t wbuf_get_firstxmit(wbuf_t __wbuf) { return __wbuf_get_firstxmit(__wbuf); }
static INLINE u_int32_t wbuf_get_firstxmitts(wbuf_t __wbuf) { return __wbuf_get_firstxmitts(__wbuf); }

static INLINE void wbuf_set_firstxmit(wbuf_t __wbuf, int __val) { __wbuf_set_firstxmit(__wbuf, __val); }
static INLINE void wbuf_set_firstxmitts(wbuf_t __wbuf, u_int32_t __val) { __wbuf_set_firstxmitts(__wbuf, __val); }
static INLINE void wbuf_clear_flags(wbuf_t __wbuf) { __wbuf_clear_flags(__wbuf); }
#endif

#if defined(VOW_TIDSCHED) || defined(ATH_SUPPORT_IQUE) || defined(VOW_LOGLATENCY)
static INLINE void wbuf_set_qin_timestamp(wbuf_t __wbuf, u_int32_t __val) { __wbuf_set_qin_timestamp(__wbuf, __val); }
static INLINE u_int32_t wbuf_get_qin_timestamp(wbuf_t __wbuf) { return __wbuf_get_qin_timestamp(__wbuf); }
#endif

#if UMAC_SUPPORT_RPTPLACEMENT || ATH_SUPPORT_AOW
static INLINE void wbuf_set_netlink_pid(wbuf_t __wbuf, u_int32_t __val) { __wbuf_set_netlink_pid(__wbuf, __val); }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static INLINE void wbuf_set_netlink_dst_pid(wbuf_t __wbuf, u_int32_t __val) { __wbuf_set_netlink_dst_pid(__wbuf, __val); }
#endif
static INLINE void wbuf_set_netlink_dst_group(wbuf_t __wbuf, u_int32_t __val) { __wbuf_set_netlink_dst_group(__wbuf, __val); }
#endif

#if UMAC_SUPPORT_SMARTANTENNA
static INLINE int wbuf_sa_get_antenna(wbuf_t wbuf) { return __wbuf_sa_get_antenna(wbuf); }
static INLINE int wbuf_sa_get_rateidx(wbuf_t wbuf) { return __wbuf_sa_get_rateidx(wbuf); }
static INLINE int wbuf_is_sa_train_packet(wbuf_t wbuf) { return __wbuf_is_sa_train_packet(wbuf); }
static INLINE int wbuf_is_sa_train_lastpacket(wbuf_t wbuf) { return __wbuf_is_sa_train_lastpacket(wbuf); }

static INLINE int wbuf_sa_set_antenna(wbuf_t wbuf, u_int8_t antenna) { return __wbuf_sa_set_antenna(wbuf,antenna); }
static INLINE int wbuf_sa_set_rateidx(wbuf_t wbuf, u_int8_t rateidx) { return __wbuf_sa_set_rateidx(wbuf,rateidx); }
static INLINE int wbuf_sa_set_train_packet(wbuf_t wbuf) { return __wbuf_sa_set_train_packet(wbuf); }
static INLINE int wbuf_sa_set_train_lastpacket(wbuf_t wbuf) { return __wbuf_sa_set_train_lastpacket(wbuf); }
#else
static INLINE int wbuf_is_sa_train_packet(wbuf_t wbuf) { return 0; }
#endif

#endif //_WBUF_H
