/*
 *  Copyright (c) 2008 Atheros Communications Inc.  All rights reserved.
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


#include <ieee80211_txrx_priv.h>

#include "if_upperproto.h"
#include <if_llc.h>
#include <if_athproto.h>
#include <osdep.h>
#include <ieee80211_aow.h>

#ifdef ATH_COALESCING
static wbuf_t ieee80211_tx_coalescing(struct ieee80211_node *ni, wbuf_t wbuf);
#endif

static INLINE int ieee80211_send_wbuf_internal(struct ieee80211vap *vap, wbuf_t wbuf);
static wbuf_t ieee80211_encap_80211( struct ieee80211_node   *ni, wbuf_t  wbuf );

void
ieee80211_update_stats(struct ieee80211vap *vap,
                       wbuf_t wbuf,
                       struct ieee80211_frame *wh,
                       int type, int subtype,
                       struct ieee80211_tx_status *ts)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_phy_stats *phy_stats;
    struct ieee80211_mac_stats *mac_stats;
    int is_mcast;

    is_mcast = IEEE80211_IS_MULTICAST(wh->i_addr1) ? 1 : 0;
    phy_stats = &ic->ic_phy_stats[ic->ic_curmode];
    mac_stats = is_mcast ? &vap->iv_multicast_stats : &vap->iv_unicast_stats;
    
    if (ts->ts_flags == 0) {
        phy_stats->ips_tx_fragments++;
        phy_stats->ips_tx_packets++;
        if (is_mcast)
            phy_stats->ips_tx_multicast++;
        
        if (ts->ts_retries != 0) {
            phy_stats->ips_tx_retries++;
            if (ts->ts_retries > 1)
                phy_stats->ips_tx_multiretries++;
        }
        mac_stats->ims_tx_packets++;
		mac_stats->ims_tx_bytes += wbuf_get_pktlen(wbuf);

        /* update miscellous status */
        if ((wbuf_get_priority(wbuf) == WME_AC_VO) ||
            (wbuf_get_priority(wbuf) == WME_AC_VI)) {
            ic->ic_wme.wme_hipri_traffic++;
        }

        /* 
         * Store timestamp for actual (non-NULL) data frames.
         * This provides other modules such as SCAN and LED with correct 
         * information about the actual data traffic in the system.
         */
        if ((type == IEEE80211_FC0_TYPE_DATA) && IEEE80211_CONTAIN_DATA(subtype)) {
            vap->iv_last_traffic_indication = OS_GET_TIMESTAMP();
        }

        /*
         * If we're in station mode, notify the scan algorithm when a data frame is transmitted successfully.
         */
        if ((vap->iv_opmode == IEEE80211_M_STA) || (vap->iv_opmode == IEEE80211_M_IBSS)) {
            /*
             * Verify it was a directed frame (not broadcast)
             */
            if (IEEE80211_ADDR_EQ(wh->i_addr1, ieee80211_node_get_bssid(vap->iv_bss))) {
                vap->iv_last_ap_frame = OS_GET_TIMESTAMP();

                if (!IEEE80211_ADDR_EQ(wh->i_addr3, IEEE80211_GET_BCAST_ADDR(ic))) {
                    vap->iv_last_directed_frame = OS_GET_TIMESTAMP();
                }
            }
        }
    } else {
        if (ts->ts_flags & IEEE80211_TX_XRETRY)
            phy_stats->ips_tx_xretries++;
        else
            mac_stats->ims_tx_discard++;
    }
}


static INLINE void
ieee80211_release_wbuf_internal(struct ieee80211_node *ni, wbuf_t wbuf, struct ieee80211_tx_status *ts )
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_frame *wh;
    int type, subtype;

    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

    /*
     * Tx post processing for non beacon frames.
     * check if the vap is valid , it could have been deleted.
     */
    if (vap) {
        ieee80211_vap_complete_buf_handler handler;
        void *arg;

        if (!(type == IEEE80211_FC0_TYPE_MGT && subtype == IEEE80211_FC0_SUBTYPE_BEACON)) {
            /*
             * update power management module about wbuf completion
             */
            if ( vap->iv_txrx_event_info.iv_txrx_event_filter & (IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_PS_NULL | 
                         IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_TX | IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_SMPS_ACT)) {
                     ieee80211_vap_txrx_event evt;
                     evt.wh = wh;
                     if ( (vap->iv_txrx_event_info.iv_txrx_event_filter & IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_PS_NULL) &&
                          wbuf_is_pwrsaveframe(wbuf))  {
                         evt.type = IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_PS_NULL;
                         evt.u.status = ts->ts_flags;
                         ieee80211_vap_txrx_deliver_event(vap,&evt);
                     } else if (vap->iv_txrx_event_info.iv_txrx_event_filter & IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_TX) {
                         evt.type = IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_TX;
                         evt.u.status = ts->ts_flags;
                         ieee80211_vap_txrx_deliver_event(vap,&evt);
                     }
                     if ( (vap->iv_txrx_event_info.iv_txrx_event_filter & IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_SMPS_ACT) &&
                          wbuf_is_smpsactframe(wbuf))  {
                         evt.type = IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_SMPS_ACT;
                         evt.u.status = ts->ts_flags;
                         ieee80211_vap_txrx_deliver_event(vap,&evt);
                     }
                 }
            /*
             * beamforming status indicates that a CV update is required
             */
#ifdef ATH_SUPPORT_TxBF
            /* should check status validation and not zero first to avoid EV#82661*/
            if (!(ts->ts_txbfstatus & ~(TxBF_Valid_Status)) && (ts->ts_txbfstatus != 0)){
                if (ni->ni_explicit_compbf || ni->ni_explicit_noncompbf || ni->ni_implicit_bf ) {   // at Bfer state
                    if (ts->ts_txbfstatus & TxBF_Valid_SW_Status) {    // valid sw status.
                        if (ts->ts_txbfstatus & TxBF_STATUS_Sounding_Complete){
                            ni->ni_hw_cv_requested = 0; /* clear flag after sounding sent*/
                            if ( ts->ts_tstamp > ni->ni_cvtstamp){
                                /* report not received yet, expected to receive in TXBF_CV_REPORT_TIMEOUT time*/
                                if (ni->ni_cvretry < TXBF_CV_RETRY_LIMIT ){
                                    OS_CANCEL_TIMER(&ni->ni_report_timer);
                                OS_SET_TIMER(&ni->ni_report_timer, TXBF_CV_REPORT_TIMEOUT);
                                    ni->ni_cvretry ++;
                            }
                            }
                        } else if (ts->ts_txbfstatus & TxBF_STATUS_Sounding_Request){
                            if (ni->ni_sw_cv_timeout){ // reset sw timer
                                OS_CANCEL_TIMER(&(ni->ni_cv_timer));
                                OS_SET_TIMER(&ni->ni_cv_timer, ni->ni_sw_cv_timeout);
                            }
                            ni->ni_bf_update_cv = 1;    // request to update CV cache
                         }
                    } else {
                        if (((ts->ts_flags &  IEEE80211_TX_ERROR) == 0) &&  
                            ((ts->ts_flags & IEEE80211_TX_XRETRY) == 0)) // tx frame ok
                        {
                            if ((ts->ts_txbfstatus & AR_TxBF_Valid_HW_Status)!= AR_Expired){
                                if (ni->ni_hw_cv_requested == 0){
                                    ni->ni_bf_update_cv = 1;    // request to update CV cache for HW status is not AR_Expired.
                                    ni->ni_hw_cv_requested = 1; /* set flag to avoid send sounding repeatedly*/
                                }
                            }
                            else {
                            // if sw cv timeout setting is not equal to zero ,use S/W timer to trigger CV update
                            // otherwise use H/W timer directly.
                                if (ni->ni_sw_cv_timeout) {
                                    if (ni->ni_allow_cv_update) {
                                       ni->ni_allow_cv_update = 0;
                                       ni->ni_bf_update_cv = 1;    // request to update CV cache
                                       OS_SET_TIMER(&ni->ni_cv_timer, ni->ni_sw_cv_timeout);
                                       ni->ni_cvretry = 0;
                                       //IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,"==>%s: request CV update and trigger S/W timer!\n",__func__);
                                    }                  
                                } else {
                                    ni->ni_bf_update_cv = 1;    // request to update CV cache
                                    //IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,"==>%s: txbf status %x,request cv_update!\n",__func__,ts->ts_txbfstatus);
                                }
                            }
                        }
                    }
                }
            }
#endif
            /*
             * update tx statisitics
             */
            ieee80211_update_stats(vap, wbuf, wh, type, subtype, ts);
        }
        if (vap->iv_evtable->wlan_xmit_update_status) {
            vap->iv_evtable->wlan_xmit_update_status(vap->iv_ifp, wbuf, ts);
        }
        /* only management and null data frames have the handler function */
        if (type == IEEE80211_FC0_TYPE_MGT || subtype == IEEE80211_FC0_SUBTYPE_NODATA) {
            wbuf_get_complete_handler(wbuf,(void **)&handler,&arg);
            if (handler) {
                handler(vap,wbuf,arg,wh->i_addr1,wh->i_addr2,wh->i_addr3,ts);
            }
        }
    } else {
        /* PANIC if vap is NULL */
        ASSERT(0);
    }
  
    /* 
     * Removing this printout because under bad channel conditions it results 
     * in continual printouts. If/when there's a new DPRINTF category that's 
     * suitable for this printout, it can be restored and changed from MSG_ANY
     * to the new category, so it won't always print.
     */
    /* 
    if (ts->ts_flags) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, 
                      "%s Tx Status flags 0x%x \n",__func__, ts->ts_flags);
    }
    */


    wbuf_set_status(wbuf, ((ts->ts_flags == 0) ? WB_STATUS_OK : WB_STATUS_TX_ERROR));
    wbuf_complete(wbuf);

}

#ifdef ATH_SUPPORT_QUICK_KICKOUT

void ieee80211_kick_node(struct ieee80211_node *ni)
{
    /* follow what timeout_station() does */
    IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_INACT, ni,
        "station kicked out due to excessive retries (refcnt %u) associd %d\n",
        ieee80211_node_refcnt(ni), IEEE80211_AID(ni->ni_associd));

    if (ni->ni_associd != 0) {
		ni->ni_flags|=IEEE80211_NODE_KICK_OUT_DEAUTH;
    }
    if ( ni->ni_vap->iv_opmode == IEEE80211_M_IBSS || ni->ni_vap->iv_opmode == IEEE80211_M_STA) {
        ieee80211_sta_leave(ni);
    } else {
        ieee80211_node_leave(ni);
    }
}

#endif

void
ieee80211_complete_wbuf(wbuf_t wbuf, struct ieee80211_tx_status *ts )
{
    struct ieee80211_node *ni = wbuf_get_node(wbuf);
    struct ieee80211com *ic = ni->ni_ic;
#if UMAC_SUPPORT_SMARTANTENNA                                        
    int8_t is_trainpkt = 0;
#endif    

    KASSERT((ni != NULL),("ni can not be null"));
    KASSERT((ni->ni_vap != NULL),("vap can not be null"));

#if UMAC_SUPPORT_SMARTANTENNA                                        
    if (wbuf_is_sa_train_packet(wbuf))
    {
        is_trainpkt = 1;
    }
#endif    

    ieee80211_release_wbuf_internal(ni,wbuf,ts);

    /* if not bss node, check the successive tx failed counts */
    if ((ni->ni_vap) && 
        (ni->ni_vap->iv_opmode == IEEE80211_M_HOSTAP) &&
        (ni != ni->ni_vap->iv_bss)) {
        if (ts->ts_flags & IEEE80211_TX_XRETRY) {
#if UMAC_SUPPORT_SMARTANTENNA                                        
         if (is_trainpkt) {
            ni->ni_inact = ni->ni_inact_reload;            
            ni->ni_consecutive_xretries = 0;
         } else {
            ni->ni_consecutive_xretries++;
         }
#else            
            ni->ni_consecutive_xretries++;
#endif            
#ifdef ATH_SUPPORT_QUICK_KICKOUT
            /* if the node is not a NAWDS repeater and failed count reaches 
             * a pre-defined limit, kick out the node 
             */
            if (((ni->ni_flags & IEEE80211_NODE_NAWDS) == 0) &&
                (ni->ni_consecutive_xretries >= ni->ni_vap->iv_sko_th)) {
                if (ni->ni_vap->iv_sko_th != 0) {				
                    ni->ni_consecutive_xretries = 0;
#if UMAC_SUPPORT_SMARTANTENNA                                        
                    /*
                     * In training process there will be lot of consecutive xretries
                     * QUICK_KICKOUT feature is causing STA to disassociate during 
                     * training process.
                     * If no packet is trasmited to the STA and  queueing more and more packets 
                     * will consume all buffers. smart antenna module sholud take care of these
                     * corner cases.
                     */
                    if (ic->ic_get_smartatenna_enable(ic))
                    {
                        if (ni->is_training == 0) {
                            ieee80211_kick_node(ni);
                        }
                    }
                    else
                    {
                        ieee80211_kick_node(ni);
                    }
#else
                    ieee80211_kick_node(ni);
#endif                    
                }
            }
#endif
            /* 
             * Force decrease the inactivity mark count of NAWDS node if 
             * consecutive tx fail count reaches a pre-defined limit.
             * Stop tx to this NAWDS node until it activate again.
             */
            if (ni->ni_flags & IEEE80211_NODE_NAWDS &&
                (ni->ni_consecutive_xretries) >= 5) {
                ni->ni_inact = 1;
            }
        } else {
            if ((ni->ni_flags & IEEE80211_NODE_NAWDS) &&
                (ni->ni_flags & IEEE80211_NODE_HT)) {
                u_int8_t tidno = wbuf_get_tid(wbuf);
                u_int16_t status;
                /* 
                 * NAWDS repeater just brought up so reset the ADDBA status 
                 * so that AGGR could be enabled to enhance throughput
                 * 10 is taken from ADDBA_EXCHANGE_ATTEMPTS
                 */
                ic->ic_addba_status(ni, tidno, &status);
                if (status && (ni->ni_consecutive_xretries) >= 10) {
                    ic->ic_addba_clear(ni);
                }
            }
			
            if (!(ts->ts_flags & IEEE80211_TX_ERROR)) {
                /* node alive so reset the counts */
                ni->ni_consecutive_xretries = 0;
            }
        }
    }
#if ATH_SUPPORT_FLOWMAC_MODULE
    /* if this frame is getting completed due to lack of resources, do not
     * try to wake the queue again.
     */
    if ( (ts->ts_flowmac_flags & IEEE80211_TX_FLOWMAC_DONE) 
            && ni->ni_vap->iv_dev_stopped 
            && ni->ni_vap->iv_flowmac) {
        if (ni->ni_vap->iv_evtable->wlan_pause_queue) {
            ni->ni_vap->iv_evtable->wlan_pause_queue(
                    ni->ni_vap->iv_ifp, 0, ni->ni_vap->iv_flowmac);
            ni->ni_vap->iv_dev_stopped = 0;
            
        }
    }
#endif

#ifdef ATH_SUPPORT_QUICK_KICKOUT
		if ((ni->ni_flags & IEEE80211_NODE_KICK_OUT_DEAUTH) 
                && ieee80211_node_refcnt(ni)==1)
            /* checking node count to one to make sure that no more packets  
               are buffered in hardware queue*/
        {
            struct ieee80211_node *tempni;
            ni->ni_flags &=~ IEEE80211_NODE_KICK_OUT_DEAUTH;
            tempni=ieee80211_tmp_node(ni->ni_vap, ni->ni_macaddr);
            if (tempni != NULL) {
                ieee80211_send_deauth(tempni, IEEE80211_REASON_AUTH_EXPIRE);
                /* claim node immediately */
                ieee80211_free_node(tempni);
            }
        }
#endif

#if  UMAC_SUPPORT_SMARTANTENNA    
    if (wbuf_is_sa_train_lastpacket(wbuf))
    {
        ATH_QUEUE_DELAYED_WORK(ic->ic_smart_workqueue,&ic->smartant_task,HZ/ic->ic_smartantennaparams->schedule_delay);     
    }
#endif    
    ieee80211_free_node(ni);
}

/*
 * to be used by the UMAC internally.
 * for wbufs which do not have a reference yet and have 
 * not been sent down for transmission to ath layer yet and
 * being released from internal buffers.
 */
void
ieee80211_release_wbuf(struct ieee80211_node *ni, wbuf_t wbuf, struct ieee80211_tx_status *ts)
{
    ieee80211_release_wbuf_internal(ni,wbuf,ts );
}


int ieee80211_vap_set_complete_buf_handler(wbuf_t wbuf, 
                   ieee80211_vap_complete_buf_handler handler, void *arg)
{
    wbuf_set_complete_handler(wbuf,(void *) handler,arg);
    return 0;
}


int
ieee80211_classify(struct ieee80211_node *ni, wbuf_t wbuf)
{
    struct ieee80211vap                 *vap = ni->ni_vap;
    struct ieee80211com                 *ic = ni->ni_ic;
    int                                 ac = WME_AC_BE;
    int                                 tid;
#if ATH_SUPPORT_VLAN
    int 								v_wme_ac=0;
    int									v_pri = 0;
#endif


    /*
     * Call wbuf_classify(wbuf) function before the
     * "(ni->ni_flags & IEEE80211_NODE_QOS)" check. The reason is that
     * wbuf_classify() is overloaded with setting EAPOL flag in addition to
     * returning TOS for Maverick and Linux platform, where as for Windows it
     * just returns TOS.
     */
    tid = (wbuf_classify(wbuf) & 0x7);
    ac = TID_TO_WME_AC(tid);

    
    /* default priority */
    if (!(ni->ni_flags & IEEE80211_NODE_QOS)) {
	    wbuf_set_priority(wbuf, WME_AC_BE);
        wbuf_set_tid(wbuf, 0);
        return 0;
    }

#if ATH_SUPPORT_VLAN
	/*
	** If this is a QoS node (set after the above comparison, and there is a
	** VLAN tag associated with the packet, we need to ensure we set the
	** priority correctly for the VLAN
	*/

    if ( adf_net_vlan_tag_present(wbuf) )
    {
        unsigned short tag;
        unsigned short vlanID = adf_net_get_vlan((osdev_t) vap->iv_ifp);

        if ( !adf_net_is_vlan_defined((osdev_t) vap->iv_ifp) )
            return 1;

        if (((tag = adf_net_get_vlan_tag(wbuf)) & VLAN_VID_MASK) != (vlanID & VLAN_VID_MASK))
            return 1;

        v_pri = (tag >> VLAN_PRI_SHIFT) & VLAN_PRI_MASK;
    }
    else
    {
        /*
         * If not a member of a VLAN, check if VLAN type and TCI are present in packet.
         * If so, obtain VLAN priority from TCI.
         * Use for determining 802.1p priority.
         */
        v_pri = wbuf_8021p(wbuf);
        
    }

	/*
	** Determine the VLAN AC
	*/

	v_wme_ac = TID_TO_WME_AC(v_pri);


    /* Choose higher priority of implicit VLAN tag or IP DSCP */
    /* TODO: check this behaviour */
    if (v_wme_ac > ac)
    {
		tid = v_pri;
		ac = v_wme_ac;
    }

#endif

    /* Applying ACM policy */
    if (vap->iv_opmode == IEEE80211_M_STA) {
        int     initial_ac = ac;

        while ((ac != WME_AC_BK)
               && ic->ic_wme.wme_wmeChanParams.cap_wmeParams[ac].wmep_acm
               && !wlan_is_tspecActive(vap)) {
                /* If admission control is mandatory (true) but tspec is not active,
                 * go down to the next lower level that doesn't have acm
                 */
            switch (ac) {
            case WME_AC_VI:
                ac = WME_AC_BE;
                break;
            case WME_AC_VO:
                ac = WME_AC_VI;
                break;
            /*
             * The default case handles ac = WME_AC_BE as well
             * as AC's other than WME_AC_VI & WME_AC_VO. Currently
             * only four AC's (VI, VO, BE, & BK)  are defined.
             * For explicit handling of any other AC's (defined 
             * in future), case statement needs to be expanded.
             */
            default:
                ac = WME_AC_BK;
                break;
            }
        }
        if (initial_ac != ac) {
            /* Recalculate the new TID */
            tid = WME_AC_TO_TID(ac);
        }
    }

    wbuf_set_priority(wbuf, ac);
    wbuf_set_tid(wbuf, tid);
    
    return 0;
}


static INLINE int
ieee80211_send_wbuf_internal(struct ieee80211vap *vap, wbuf_t wbuf)
{
    int retval;
    /*
     * Wake up the H/W first
     */
    if (!ieee80211_vap_ready_is_set(vap)) {
        struct ieee80211_tx_status ts;
        ts.ts_flags = IEEE80211_TX_ERROR; 
        ts.ts_retries=0;
        /* 
         * complete buf will decrement the pending count.
         */
        ieee80211_complete_wbuf(wbuf,&ts);
        return -EIO;
    }


    /* propagate packets to NAWDS repeaters */
    if (ieee80211_nawds_send_wbuf(vap, wbuf) == -1) {
        /* NAWDS node packet but mode is off, drop packet */
        return 0;
    }

    vap->iv_lastdata = OS_GET_TIMESTAMP();
    vap->iv_txrxbytes += wbuf_get_pktlen(wbuf);
    
    ieee80211_sta_power_tx_start(vap->iv_pwrsave_sta);
    /*
     * call back to shim layer to queue it to hardware device.
     */
    retval = vap->iv_evtable->wlan_dev_xmit_queue(vap->iv_ifp, wbuf);

    ieee80211_sta_power_tx_end(vap->iv_pwrsave_sta);

    return retval;
}


/*
 * the main xmit data entry point from OS 
 */
int
wlan_vap_send(wlan_if_t vap, wbuf_t wbuf)
{
    struct ieee80211_node *ni=NULL;
    u_int8_t *daddr;
    int is_data,retval;

    /* 
     * Find the node for the destination so we can do
     * things like power save and fast frames aggregation.
     */
    if (IEEE80211_VAP_IS_SEND_80211_ENABLED(vap)) {
        struct ieee80211_frame *wh;
        int type;
        
        /*
         * WDS mode ?
         */
        wh = (struct ieee80211_frame *)wbuf_header(wbuf);
        daddr = wh->i_addr1;

        /*
         * Vista sometimes sends management frames from the stack,
         * so we need to determine if it's a data frame here.
         */
        type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
        is_data = (type == IEEE80211_FC0_TYPE_DATA) ? 1 : 0;
    } else {
        struct ether_header *eh;
        
        eh = (struct ether_header *)wbuf_header(wbuf);
        daddr = eh->ether_dhost;
        is_data = 1;            /* ethernet frame */


        /*
         * If IQUE is NOT enabled, the ops table is empty and
         * the follow step will be skipped;
         * If IQUE is enabled, and if the packet is a mcast one
         * (and NOT a bcast one), the packet will be converted
         * into ucast packets if the destination in found in the 
         * snoop table, in either Translate way or Tunneling way
         * depending on the mode of mcast enhancement
         */
        if (IEEE80211_IS_MULTICAST(eh->ether_dhost) &&
		    vap->iv_sta_assoc > 0 &&
    		!IEEE80211_IS_BROADCAST(eh->ether_dhost) &&
	    	vap->iv_opmode == IEEE80211_M_HOSTAP &&
            vap->iv_ique_ops.me_convert)
        {  
                /*
                 * if the convert function returns some value larger
                 * than 0, it means that one or more frames have been
                 * transmitted and we are safe to return from here.
                 */
	    		if (vap->iv_ique_ops.me_convert(vap, wbuf) > 0) {
		    	    return 0;
                }
        }
    }
    
    ni = ieee80211_find_txnode(vap, daddr);
    if (ni == NULL) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT,
                          "%s: could not send packet, NI equal to NULL for %s\n", 
                          __func__, ether_sprintf(daddr));
        /* NB: ieee80211_find_txnode does stat+msg */
        goto bad;
    }

    /* calculate priority so driver can find the tx queue */
    if (ieee80211_classify(ni, wbuf)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT,
                          "%s: discard, classification failure (%s)\n",
                          __func__, ether_sprintf(daddr));
        goto bad;
    }

    if (is_data) {
        /* No data frames go out unless we're running. */
        if (!ieee80211_vap_ready_is_set(vap)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT,
                              "%s: ignore data packet, vap is not active\n",
                              __func__);
            goto bad;
        }
	/* function returns 1 if frame is to be fwded
	 * and 0 if the frame can be dropped
	 * when stubbed returns 1
	 */
        if (!ieee80211_apclient_fwd(vap,wbuf)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT,
                          "%s: drop data packet, vap is in sta_fwd\n",
                           __func__);
            goto bad;
        }


#ifdef IEEE80211_WDS
        if (IEEE80211_IS_MULTICAST(daddr)) {
            if ((vap->iv_opmode == IEEE80211_M_WDS) && 
                (!IEEE80211_VAP_IS_STATIC_WDS_ENABLED(vap))) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS,
                                  "%s: drop multicast frame on WDS extender; destined for %s\n", 
                                  __func__, ether_sprintf(daddr));
                wbuf_complete(wbuf);
                return 0;
            }

            if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
                ieee80211_internal_send_dwds_multicast(vap, m);
            }
        }
#endif

        if (ieee80211_node_get_associd(ni) == 0 &&
            vap->iv_opmode != IEEE80211_M_IBSS) {
            /*
             * Destination is not associated; must special
             * case WDS where we point iv_bss at the node
             * for the associated station.
             */
            if(!IEEE80211_IS_TDLS_NODE(ni))
            if (ni != vap->iv_bss ||
                vap->iv_opmode == IEEE80211_M_WDS) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT,
                                  "%s: could not send packet, DA (%s) is not yet associated\n", 
                                  __func__, ether_sprintf(daddr));
                goto bad;
            }
        }

        if (!ieee80211_node_is_authorized(ni) &&
            ni != vap->iv_bss && !wbuf_is_eapol(wbuf)
#ifdef ATH_SUPPORT_WAPI
            && !wbuf_is_wai(wbuf)
#endif
            ) {
            /*
             * Destination is not authenticated
             */
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT,
                              "%s: could not send packet, DA (%s) is not yet authorized\n", 
                              __func__, ether_sprintf(daddr));
            goto bad;
        }
    /*
	 *	Headline block removal: if the state machine is in 
	 *	BLOCKING or PROBING state, transmision of UDP data frames 
	 *	are blocked untill swtiches back to ACTIVE state.
	 */
        if (!IEEE80211_VAP_IS_SEND_80211_ENABLED(vap)) {
            if (vap->iv_ique_ops.hbr_dropblocked) {
                if (vap->iv_ique_ops.hbr_dropblocked(vap, ni, wbuf)) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IQUE,
                                    "%s: packet dropped coz it blocks the headline\n",
                                    __func__);
                    goto bad;
                }
            }
        }
    } /* is_data */
    

    /*
     * XXX When ni is associated with a WDS link then
     * the vap will be the WDS vap but ni_vap will point
     * to the ap vap the station associated to.  Once
     * we handoff the packet to the driver the callback
     * to ieee80211_encap won't be able to tell if the
     * packet should be encapsulated for WDS or not (e.g.
     * multicast frames will not be handled correctly).
     * We hack this by marking the mbuf so ieee80211_encap
     * can do the right thing.
     */
#ifdef IEEE80211_WDS
    if (vap->iv_opmode == IEEE80211_M_WDS)
        wbuf_set_wdsframe(wbuf);
    else
        wbuf_clear_wdsframe(wbuf);
#endif

    wbuf_set_node(wbuf, ni);    /* associate node with wbuf */

    /* EV78142 : initialize completion function to avoid wrongly feteched */
    wbuf_set_complete_handler(wbuf, NULL, NULL);

    if (ieee80211_vap_smps_is_set(vap))
        wbuf_set_smpsframe(wbuf);

    /* notify the sta PM module about xmit queue start to synchronize its network sleep operation */
    ieee80211_sta_power_tx_start(vap->iv_pwrsave_sta);

    /**
     * deliver event to the registered handlers (one of them is PS SM )
     * this needs to be delivered beofore the following logic of queuing the frames
     * if the node is in paused state. this will ensure that the STA Power Save  SM
     * will move to active state while the node is in paused state.
     */
    if (vap->iv_txrx_event_info.iv_txrx_event_filter & IEEE80211_VAP_OUTPUT_EVENT_DATA) {
        ieee80211_vap_txrx_event evt;
        evt.type = IEEE80211_VAP_OUTPUT_EVENT_DATA;
        ieee80211_vap_txrx_deliver_event(vap,&evt);
    }


    /* power-save checks */
    if ((!WME_UAPSD_AC_ISDELIVERYENABLED(wbuf_get_priority(wbuf), ni)) && 
        (ieee80211node_is_paused(ni)) && 
        !ieee80211node_has_flag(ni, IEEE80211_NODE_TEMP)) {
        /*
         * Station in power save mode; pass the frame
         * to the 802.11 layer and continue.  We'll get
         * the frame back when the time is right.
         * XXX lose WDS vap linkage?
         */
        ieee80211node_pause(ni); /* pause it to make sure that no one else unpaused it after the node_is_paused check above, pause operation is ref counted */  
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT,
                          "%s: could not send packet, STA (%s) powersave %d paused %d\n", 
                          __func__, ether_sprintf(daddr), (ni->ni_flags & IEEE80211_NODE_PWR_MGT) ?1 : 0, ieee80211node_is_paused(ni));
        ieee80211_node_saveq_queue(ni, wbuf, (is_data ? IEEE80211_FC0_TYPE_DATA : IEEE80211_FC0_TYPE_MGT));
        ieee80211node_unpause(ni); /* unpause it if we are the last one, the frame will be flushed out */  
        ieee80211_free_node(ni);
        ieee80211_sta_power_tx_end(vap->iv_pwrsave_sta);
        return 0;
    }

    ieee80211_vap_pause_update_xmit_stats(vap,wbuf); /* update the stats for vap pause module */
    retval = ieee80211_send_wbuf_internal(vap, wbuf);
    
    /* notify the sta PM module about xmit queue end to synchronize it its network sleep operation */
    ieee80211_sta_power_tx_end(vap->iv_pwrsave_sta);

    return retval;
bad:   
    if (IEEE80211_IS_MULTICAST(daddr))
        vap->iv_multicast_stats.ims_tx_discard++;
    else
        vap->iv_unicast_stats.ims_tx_discard++;

    if (ni != NULL)
        ieee80211_free_node(ni);

    /* NB: callee's responsibilty to complete the packet */
    wbuf_set_status(wbuf, WB_STATUS_TX_ERROR);
    wbuf_complete(wbuf);
    
    return -EIO;
}

int
ieee80211_send_wbuf(struct ieee80211vap *vap, struct ieee80211_node *ni, wbuf_t wbuf)
{
   /* send the data down to the ath */
   /* deliver event to the registered handlers (one of them is PS SM ) */
    if (vap->iv_txrx_event_info.iv_txrx_event_filter & IEEE80211_VAP_OUTPUT_EVENT_DATA) {
        ieee80211_vap_txrx_event evt;
        evt.type = IEEE80211_VAP_OUTPUT_EVENT_DATA;
        ieee80211_vap_txrx_deliver_event(vap,&evt);
    }
   wbuf_set_node(wbuf, ieee80211_ref_node(ni));
   return ieee80211_send_wbuf_internal(vap,wbuf);
}
/*
 * Encapsulate the frame into 802.11 frame format.
 * the frame has the 802.11 header already.
 */
static wbuf_t 
ieee80211_encap_80211(struct ieee80211_node *ni, wbuf_t wbuf)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_key *key=NULL;
    int key_mapping_key = 0;
    struct ieee80211_frame *wh;
    int type, subtype;
    int useqos=0, use4addr, usecrypto=0;
    int hdrsize, datalen, pad, addlen; /* additional header length we want to append */
    int ac = wbuf_get_priority(wbuf);

#ifdef ATH_COALESCING
    if (!wbuf_is_fastframe(wbuf) && (ic->ic_tx_coalescing == IEEE80211_COALESCING_ENABLED) && (wbuf->wb_type != WBUF_TX_INTERNAL)) {
        wbuf = ieee80211_tx_coalescing(ni, wbuf);
    }
#endif
    
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
    use4addr = ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
                IEEE80211_FC1_DIR_DSTODS) ? 1 : 0;
    if (use4addr)
        hdrsize = sizeof(struct ieee80211_frame_addr4);
    else
        hdrsize = sizeof(struct ieee80211_frame);
    datalen = wbuf_get_pktlen(wbuf) - (hdrsize + sizeof(struct llc));  /* NB: w/o 802.11 header */

    if (!IEEE80211_VAP_IS_SAFEMODE_ENABLED(vap) &&  /* safe mode disabled */
        IEEE80211_VAP_IS_PRIVACY_ENABLED(vap) &&    /* crypto is on */
        (type == IEEE80211_FC0_TYPE_DATA)) {        /* only for data frame */
        /*
         * Find the key that would be used to encrypt the frame if the 
         * frame were to be encrypted. For unicast frame, search the 
         * matching key in the key mapping table first. If not found,
         * used default key. For multicast frame, only use the default key.
         */
        if (vap->iv_opmode == IEEE80211_M_STA ||
            !IEEE80211_IS_MULTICAST(wh->i_addr1) ||
            (vap->iv_opmode == IEEE80211_M_WDS && 
             IEEE80211_VAP_IS_STATIC_WDS_ENABLED(vap))) {
            /* use unicast key */
            key = &ni->ni_ucastkey;
        }
        
        if (key && key->wk_valid) {
            key_mapping_key = 1;
        } else {
            if (vap->iv_def_txkey != IEEE80211_KEYIX_NONE) {
                key = &vap->iv_nw_keys[vap->iv_def_txkey];
                if (!key->wk_valid) {
                    key = NULL;
                }
            } else {
                key = NULL;
            }
        }

        /*
         * Assert our Exemption policy.  We assert it blindly at first, then
         * take the presence/absence of a key into acct.
         *
         * Lookup the ExemptionActionType in the send context info of this frame
         * to determine if we need to encrypt the frame.
         */
        switch (wbuf_get_exemption_type(wbuf)) {
        case WBUF_EXEMPT_NO_EXEMPTION:
            /*
             * We want to encrypt this frame.
             */
            usecrypto = 1;
            break;

        case WBUF_EXEMPT_ALWAYS:
            /*
             * We don't want to encrypt this frame.
             */
            break;

        case WBUF_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE:
            /*
             * We encrypt this frame if and only if a key mapping key is set.
             */
            if (key_mapping_key)
                usecrypto = 1;
            break;

        default:
            ASSERT(0);
            usecrypto = 1;
            break;
        }

        /*
         * If the frame is to be encrypted, but no key is not set, either reject the frame 
         * or clear the WEP bit.
         */
        if (usecrypto && !key) {
            /*
             * If this is a unicast frame or if the BSSPrivacy is on, reject the frame.
             * Otherwise, clear the WEP bit so we will not encrypt the frame. In other words,
             * we'll send multicast frame in clear if multicast key hasn't been setup.
             */
            if (!IEEE80211_IS_MULTICAST(wh->i_addr1))
                goto bad;
            else
                usecrypto = 0; /* XXX: is this right??? */
        }

        if (usecrypto)
            wh->i_fc[1] |= IEEE80211_FC1_WEP;
        else
            wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
    }


    /*
     * XXX: If it's an EAPOL frame:
     * Some 11n APs drop non-QoS frames after ADDBA sequence. For example,
     * bug 31812: Connection failure with Buffalo AMPG144NH. To fix it,
     * seq. number in the same tid space, as requested in ADDBA, need to be
     * used for the EAPOL frames. Therefore, wb_eapol cannot be set.
     * 
     * if (((struct llc *)&wh[1])->llc_snap.ether_type == htobe16(ETHERTYPE_PAE))
     *    wbuf_set_eapol(wbuf);
     */

    /*
     * Figure out additional header length we want to append after the wireless header.
     * - Add Qos Control field if necessary
     *   XXX: EAPOL frames will be encapsulated as QoS frames as well.
     * - Additional QoS control field for OWL WDS workaround
     * - IV will be added in ieee80211_crypto_encap().
     */
    addlen = 0;
    pad = 0;
    if (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_QOS) {
        useqos = 1;
        hdrsize += sizeof(struct ieee80211_qoscntl);
        
          /* For TxBF CV cache update add +HTC field */
#ifdef ATH_SUPPORT_TxBF
        if (ni->ni_bf_update_cv) {
            hdrsize += sizeof(struct ieee80211_htc);
        }
#endif
        
        /*
         * XXX: we assume a QoS frame must come from ieee80211_encap_8023() function,
         * meaning it's already padded. If OS sends a QoS frame (thus without padding),
         * then it'll break.
         */
        if (ic->ic_flags & IEEE80211_F_DATAPAD) {
            pad = roundup(hdrsize, sizeof(u_int32_t)) - hdrsize;
        }
    } else if (type == IEEE80211_FC0_TYPE_DATA &&
               ((ni->ni_flags & IEEE80211_NODE_QOS) ||
                IEEE80211_NODE_USEAMPDU(ni))) {
        useqos = 1;
        addlen += sizeof(struct ieee80211_qoscntl);
        /* For TxBF CV cache update add +HTC field */
#ifdef ATH_SUPPORT_TxBF
        if (ni->ni_bf_update_cv) {
            addlen += sizeof(struct ieee80211_htc);
        }
#endif
    } else if (IEEE80211_VAP_IS_SEND_80211_ENABLED(vap) && 
               type == IEEE80211_FC0_TYPE_DATA && use4addr) 
    {

        /*
         * If a non-QoS 4-addr frame comes from ieee80211_encap_8023() function,
         * then it should be padded. Only need padding non-QoS 4-addr frames
         * if OS sends it with 802.11 header already but without padding.
         */
        addlen = roundup((hdrsize), sizeof(u_int32_t)) - hdrsize;
    }

#if ATH_WDS_WAR_UNENCRYPTED
    /* Owl 2.2 - WDS War - for non-encrypted QoS frames, add extra QoS Ctl field */
    if (use4addr && useqos && !usecrypto && IEEE80211_NODE_USEWDSWAR(ni)) {
        addlen += sizeof(struct ieee80211_qoscntl);
    }
#endif

    if (addlen) {       
        struct ieee80211_frame *wh0;

        if (ic->ic_flags & IEEE80211_F_DATAPAD) {
            /*
             * XXX: if we already have enough padding, then
             * don't need to push in more bytes, otherwise,
             * put in bytes after the original padding.
             */
            /* If addlen > 0, addlen > pad always, as per previous logic */
            addlen = roundup((hdrsize + addlen), sizeof(u_int32_t)) - hdrsize - pad;
        }

        wh0 = wh;
        wh = (struct ieee80211_frame *)wbuf_push(wbuf, addlen);
        if (wh == NULL) {
            goto bad;
        }
        memmove(wh, wh0, hdrsize);
    }
    
    if (useqos) {
        u_int8_t *qos;
        int tid;

        ac = wbuf_get_priority(wbuf);
        tid = wbuf_get_tid(wbuf);

        if (!use4addr)
            qos = ((struct ieee80211_qosframe *)wh)->i_qos;
        else
            qos = ((struct ieee80211_qosframe_addr4 *)wh)->i_qos;

        if ((vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_check_msdu_life) &&
            !(vap->iv_ccx_evtable->wlan_ccx_check_msdu_life(vap->iv_ccx_arg, tid))){
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT, "%s, MSDU Lifetime Exceed discard QOS traffic\n", __func__);
            goto bad;
        }

        qos[0] = tid & IEEE80211_QOS_TID;
        if (ic->ic_wme.wme_wmeChanParams.cap_wmeParams[ac].wmep_noackPolicy)
            qos[0] |= (1 << IEEE80211_QOS_ACKPOLICY_S);
#ifdef ATH_AMSDU
        if (wbuf_is_amsdu(wbuf)) {
            qos[0] |= (1 << IEEE80211_QOS_AMSDU_S) & IEEE80211_QOS_AMSDU;
        }
#endif
        qos[1] = 0;
        wh->i_fc[0] |= IEEE80211_FC0_SUBTYPE_QOS;
        
        /* Fill in the sequence number from the TID sequence space. */
        *(u_int16_t *)&wh->i_seq[0] =
            htole16(ni->ni_txseqs[tid] << IEEE80211_SEQ_SEQ_SHIFT);
        ni->ni_txseqs[tid]++;
        
        if (vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_process_qos) {
            vap->iv_ccx_evtable->wlan_ccx_process_qos(vap->iv_ccx_arg, IEEE80211_TX_QOS_FRAME, tid);
        }

#ifdef ATH_SUPPORT_TxBF
        //IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,"==>%s:CV update\n",__func__);
        if (ni->ni_bf_update_cv) {

            ieee80211_request_cv_update(ic,ni, wbuf, use4addr);
            /* clear flag */
           // ni->ni_bf_update_cv = 0;
        }       
#endif
       
#if ATH_WDS_WAR_UNENCRYPTED
		/* Owl 2.2 WDS WAR - Extra QoS Ctrl field on non-encrypted frames */
		if (use4addr && !usecrypto && IEEE80211_NODE_USEWDSWAR(ni)) {
			struct ieee80211_qosframe_addr4 *qwh4 = (typeof(qwh4))wh;
			qwh4->i_wdswar[0] = qos[0];
			qwh4->i_wdswar[1] = qos[1];
		}
#endif
    } else {
        /* to avoid sw generated frame sequence the same as H/W generated frame,
        * the value lower than min_sw_seq is reserved for HW generated frame */ 
        if ((ni->ni_txseqs[IEEE80211_NON_QOS_SEQ]& IEEE80211_SEQ_MASK) < MIN_SW_SEQ){
            ni->ni_txseqs[IEEE80211_NON_QOS_SEQ] = MIN_SW_SEQ;  
        }
        *(u_int16_t *)wh->i_seq =
            htole16(ni->ni_txseqs[IEEE80211_NON_QOS_SEQ] << IEEE80211_SEQ_SEQ_SHIFT);
        ni->ni_txseqs[IEEE80211_NON_QOS_SEQ]++;
    }

        
    if (!ieee80211_check_and_fragment(vap, wbuf, wh, usecrypto,key,hdrsize)) {
            goto bad;
    }

    /* 
     * Set the PM bit of data frames so that it matches the state the Power  
     * Management module and the AP agreed upon. Only for station UAPSD.
     */
    if (vap->iv_opmode == IEEE80211_M_STA) {
        if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
            if (ieee80211_vap_forced_sleep_is_set(vap)) {
                wh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;
                wbuf_set_pwrsaveframe(wbuf);
            }
        }
    }

/*
 * Update node TX stats for non  smart antenna training packets only 
 */

#if UMAC_SUPPORT_SMARTANTENNA
if (!wbuf_is_sa_train_packet(wbuf))    
{
    IEEE80211_NODE_STAT(ni, tx_data);
    if (IEEE80211_IS_MULTICAST(wh->i_addr1))
        IEEE80211_NODE_STAT(ni, tx_mcast);
    else
        IEEE80211_NODE_STAT(ni, tx_ucast);
    IEEE80211_NODE_STAT_ADD(ni, tx_bytes, datalen);
}
#else
    IEEE80211_NODE_STAT(ni, tx_data);
    if (IEEE80211_IS_MULTICAST(wh->i_addr1))
        IEEE80211_NODE_STAT(ni, tx_mcast);
    else
        IEEE80211_NODE_STAT(ni, tx_ucast);
    IEEE80211_NODE_STAT_ADD(ni, tx_bytes, datalen);
#endif    
    return wbuf;

bad:
    while (wbuf != NULL) {
        wbuf_t wbuf1 = wbuf_next(wbuf);
        IEEE80211_TX_COMPLETE_WITH_ERROR(wbuf);
        wbuf = wbuf1;
    }
    return NULL;
}

/*
 * Encapsulate the frame into 802.11 frame format.
 * If an error is encountered NULL is returned. The caller is required
 * to provide a node reference. 
 */
static INLINE wbuf_t
ieee80211_encap_8023(struct ieee80211_node *ni, wbuf_t wbuf)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
    struct ether_header eh;
    struct ieee80211_frame *wh;
    struct llc *llc;
    int hdrsize, hdrspace, addqos, use4addr, isMulticast;
    int is_amsdu = wbuf_is_amsdu(wbuf);
#ifdef ATH_SUPPORT_TxBF
    int addhtc;
#endif

    /*
     * Copy existing Ethernet header to a safe place.  The
     * rest of the code assumes it's ok to strip it when
     * reorganizing state for the final encapsulation.
     */
    KASSERT(wbuf_get_pktlen(wbuf)>= sizeof(eh), ("no ethernet header!"));
    OS_MEMCPY(&eh, wbuf_header(wbuf), sizeof(struct ether_header));
    addqos = (IEEE80211_NODE_USEAMPDU(ni) || ni->ni_flags & IEEE80211_NODE_QOS);

#ifdef ATH_SUPPORT_TxBF   
    addhtc = (ni->ni_bf_update_cv == 1);

    if (addhtc) {
        hdrsize = sizeof(struct ieee80211_qosframe_htc);
    } else  if (addqos)
#else
    if (addqos)
#endif  
        hdrsize = sizeof(struct ieee80211_qosframe);
    else
        hdrsize = sizeof(struct ieee80211_frame);
    
    isMulticast = (IEEE80211_IS_MULTICAST(eh.ether_dhost)) ? 1 : 0 ;
    use4addr = 0;
    switch (vap->iv_opmode) {
    case IEEE80211_M_HOSTAP:
#ifdef IEEE80211_WDS
        if (wbuf_is_wdsframe(wbuf)) {
            hdrsize += IEEE80211_ADDR_LEN;
            use4addr = 1;
        }
#endif
        if (isMulticast == 0)
		    use4addr = wds_is4addr(vap, eh, ni->ni_macaddr);
        if (ni->ni_flags & IEEE80211_NODE_NAWDS)
            use4addr = 1;
        break;
    case IEEE80211_M_STA:
        /*
         * When forwarding a frame and 4-address support is
         * enabled craft a 4-address frame.
         */
        if ((!IEEE80211_ADDR_EQ(eh.ether_shost, vap->iv_myaddr)) &&
            (IEEE80211_VAP_IS_WDS_ENABLED(vap)))
        {
            use4addr=1;
            isMulticast = IEEE80211_IS_MULTICAST(ni->ni_macaddr);
            /* add a wds entry to the station vap */
            if(IEEE80211_IS_MULTICAST(eh.ether_dhost))
            {
                struct ieee80211_node_table *nt;
                struct ieee80211_node *ni_wds=NULL;
                nt = &ic->ic_sta;
                ni_wds = ieee80211_find_wds_node(nt,eh.ether_shost);
                if(ni_wds)
                {
                    ieee80211_set_wds_node_time(nt,eh.ether_shost);
                    ieee80211_free_node(ni_wds); /* Decr ref count */
                }
                else
                {
                    ieee80211_add_wds_addr(nt, ni, eh.ether_shost,
                    IEEE80211_NODE_F_WDS_BEHIND);
                }
            }
        }
        else
            isMulticast = IEEE80211_IS_MULTICAST(ni->ni_bssid);

        break;
    case IEEE80211_M_WDS:
        hdrsize += IEEE80211_ADDR_LEN;
        use4addr = 1;
        break;
    default:
        break;
    }

    hdrsize = hdrsize + (use4addr ? IEEE80211_ADDR_LEN : 0);
    if (ic->ic_flags & IEEE80211_F_DATAPAD) {
        hdrspace = roundup(hdrsize, sizeof(u_int32_t));
    } else {
        hdrspace = hdrsize;
    }

    if (!is_amsdu && htons(eh.ether_type) >= IEEE8023_MAX_LEN) {
        int useBTEP= (eh.ether_type == htons(ETHERTYPE_AARP)) ||
                      (eh.ether_type == htons(ETHERTYPE_IPX)) ;               
        /* 
         * push the data by
         * required total bytes for 802.11 header (802.11 header + llc - ether header).
         */
        if (wbuf_push(wbuf, (u_int16_t) (hdrspace 
                  + sizeof(struct llc) - sizeof(struct ether_header))) == NULL) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT,
                          "%s:  %s::wbuf_push failed \n",
                          __func__, ether_sprintf(eh.ether_dhost));
            goto bad;
        }

        wh = (struct ieee80211_frame *)wbuf_header(wbuf);
        llc = (struct llc *)((u_int8_t *)wh + hdrspace);
        llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
        llc->llc_control = LLC_UI;
        if (useBTEP) { 
            llc->llc_snap.org_code[0] = BTEP_SNAP_ORGCODE_0; /* 0x0 */
            llc->llc_snap.org_code[1] = BTEP_SNAP_ORGCODE_0; /* 0x0 */
            llc->llc_snap.org_code[2] = BTEP_SNAP_ORGCODE_0; /* 0xf8 */
         } else {
            llc->llc_snap.org_code[0] = RFC1042_SNAP_ORGCODE_0; /* 0x0 */
            llc->llc_snap.org_code[1] = RFC1042_SNAP_ORGCODE_1; /* 0x0 */
            llc->llc_snap.org_code[2] = RFC1042_SNAP_ORGCODE_2; /* 0x0 */
        }
        llc->llc_snap.ether_type = eh.ether_type;
    }
    else {
        /* 
         * push the data by
         * required total bytes for 802.11 header (802.11 header - ether header).
         */
        if (wbuf_push(wbuf, (u_int16_t)(hdrspace-sizeof(struct ether_header))) == NULL) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT,
                          "%s:  %s::wbuf_push failed \n",
                          __func__, ether_sprintf(eh.ether_dhost));
            goto bad;
        }
        wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    }

    wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
    *(u_int16_t *)wh->i_dur = 0;
    /** WDS FIXME */
    /** clean up wds table whenever sending out a packet */

    if((vap->iv_opmode == IEEE80211_M_HOSTAP) && (IEEE80211_VAP_IS_WDS_ENABLED(vap)))
    {
        struct ieee80211_node *ni_wds=NULL;
        u_int32_t wds_age;
        
        ni_wds = ieee80211_find_wds_node(&ic->ic_sta, eh.ether_shost);
        /* Last call increments ref count if !NULL */
        if (ni_wds != NULL) {
            /* if 4 address source pkt reachable through same node as dest 
             * then remove the source addr from wds table
             */
            if(use4addr && (ni_wds == ni))
            {
                ieee80211_remove_wds_addr(&ic->ic_sta, eh.ether_shost, IEEE80211_NODE_F_WDS_REMOTE);
            }
            else if (isMulticast)
            {
                wds_age = ieee80211_find_wds_node_age(&ic->ic_sta, eh.ether_shost);
                if(wds_age > 2000)
                {
                    ieee80211_remove_wds_addr(&ic->ic_sta, eh.ether_shost, IEEE80211_NODE_F_WDS_REMOTE);
                }
            }
            ieee80211_free_node(ni_wds); /* Decr ref count */
        }
    }
    if((vap->iv_opmode == IEEE80211_M_STA) && (IEEE80211_VAP_IS_WDS_ENABLED(vap)))
    {
        struct ieee80211_node *ni_wds=NULL;
        
        ni_wds = ieee80211_find_wds_node(&ic->ic_sta, eh.ether_dhost);
        /* Last call increments ref count if !NULL */
        if (ni_wds != NULL) {
            ieee80211_remove_wds_addr(&ic->ic_sta, eh.ether_dhost,IEEE80211_NODE_F_WDS_BEHIND);
            ieee80211_free_node(ni_wds); /* Decr ref count */
        }
    }
    
    if (use4addr) {
        wh->i_fc[1] = IEEE80211_FC1_DIR_DSTODS;
        IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_macaddr);
        IEEE80211_ADDR_COPY(wh->i_addr2, vap->iv_myaddr);
        IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_dhost);
        IEEE80211_ADDR_COPY(IEEE80211_WH4(wh)->i_addr4, eh.ether_shost);
    } else {
        switch (vap->iv_opmode) {
        case IEEE80211_M_STA:
            /* If its going to AP then set toDS otherwise its a TDLS
               frame and set NoDS */
            if (ni == vap->iv_bss) {
                wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
            } else {
                wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
            }
            IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_macaddr);
            IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
            /* If next hop is the destination, then use bssid as
               addr3 otherwise use the real destination as addr3 */
            if (IEEE80211_ADDR_EQ(ni->ni_macaddr, eh.ether_dhost)){
                IEEE80211_ADDR_COPY(wh->i_addr3, vap->iv_bss->ni_bssid);
            } else {
                IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_dhost);
            }

            break;
        case IEEE80211_M_IBSS:
        case IEEE80211_M_AHDEMO:
            wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
            IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
            IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
            /*
             * NB: always use the bssid from iv_bss as the
             *     neighbor's may be stale after an ibss merge
             */
            IEEE80211_ADDR_COPY(wh->i_addr3, vap->iv_bss->ni_bssid);
            break;
        case IEEE80211_M_HOSTAP:
            wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
            IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
            IEEE80211_ADDR_COPY(wh->i_addr2, ni->ni_bssid);
            IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_shost);
            if (wbuf_is_moredata(wbuf)) {
                    wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
            }
            break;
        case IEEE80211_M_WDS:
            wh->i_fc[1] = IEEE80211_FC1_DIR_DSTODS;
            IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_macaddr);
            IEEE80211_ADDR_COPY(wh->i_addr2, vap->iv_myaddr);
            IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_dhost);
            IEEE80211_ADDR_COPY(IEEE80211_WH4(wh)->i_addr4, eh.ether_shost);
            break;
        case IEEE80211_M_MONITOR:
            goto bad;
        default:
            goto bad;
        }
    }

    if (addqos) {
        /*
         * Just mark the frame as QoS, and QoS control filed will be filled
         * in ieee80211_encap_80211().
         */
        wh->i_fc[0] |= IEEE80211_FC0_SUBTYPE_QOS;
    }

    /*
     * Set per-packet exemption type
     */
    if (eh.ether_type == htons(ETHERTYPE_PAE)) {
        /* 
         * IEEE 802.1X: send EAPOL frames always in the clear.
         * WPA/WPA2: encrypt EAPOL keys when pairwise keys are set.
         */
        if (RSN_AUTH_IS_WPA(rsn) || RSN_AUTH_IS_WPA2(rsn)) {
            wbuf_set_exemption_type(wbuf, WBUF_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE);
        }
        else {
            wbuf_set_exemption_type(wbuf, WBUF_EXEMPT_ALWAYS);
        }
    }
#if ATH_SUPPORT_WAPI
    else if (eh.ether_type == htons(ETHERTYPE_WAI)) {
            wbuf_set_exemption_type(wbuf, WBUF_EXEMPT_ALWAYS);
    }
#endif
    else {
        wbuf_set_exemption_type(wbuf, WBUF_EXEMPT_NO_EXEMPTION);
    }
    
    return ieee80211_encap_80211(ni, wbuf);
    
bad:
    /* complete the failed wbuf here */
    IEEE80211_TX_COMPLETE_WITH_ERROR(wbuf);
    return NULL;
}

wbuf_t
__ieee80211_encap(struct ieee80211_node *ni, wbuf_t wbuf)
{
    struct ieee80211vap *vap = ni->ni_vap;

#ifndef __CARRIER_PLATFORM__
    if (IEEE80211_VAP_IS_SEND_80211_ENABLED(vap)) {
#else
    if (IEEE80211_VAP_IS_SEND_80211_ENABLED(vap) || wbuf_is_encap_done(wbuf)) {
#endif /*__CARRIER_PLATFORM__*/
        struct ieee80211_frame *wh;
        wh = (struct ieee80211_frame *)wbuf_header(wbuf);
        if (wbuf_is_moredata(wbuf)) {
             wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
        }
        return ieee80211_encap_80211(ni, wbuf);
    } else {
        return ieee80211_encap_8023(ni, wbuf);
    }
}

#ifdef ATH_COALESCING
static wbuf_t 
ieee80211_tx_coalescing(struct ieee80211_node *ni, wbuf_t wbuf)
{
    struct ieee80211vap *vap = ni->ni_vap;    
    struct ieee80211com *ic = ni->ni_ic;    
    wbuf_t wbuf_new;

    wbuf_new = wbuf_coalesce(ic->ic_osdev, wbuf);
    if (wbuf_new == NULL) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT,
                          "%s: coalescing failed, stick with the old wbuf.\n",
                          __func__);
        return wbuf;
    }

    wbuf_set_priority(wbuf_new, wbuf_get_priority(wbuf));
    wbuf_set_exemption_type(wbuf_new, wbuf_get_exemption_type(wbuf));
    wbuf_set_node(wbuf_new, ni);

    return wbuf_new;
}

#endif /* ATH_COALESCING */

#ifdef ATH_AMSDU

static int ieee80211_80211frm_amsdu_check(wbuf_t wbuf)
{
    struct ieee80211_frame *wh;
    int iseapol, ismulticast, isip, use4addr;
    u_int16_t hdrsize;
    struct ip_header *ip = NULL;

    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    if (!(wh->i_fc[0] & IEEE80211_FC0_TYPE_DATA)) {
        return 1;
    }

    ismulticast = (IEEE80211_IS_MULTICAST(wh->i_addr1)) ? 1 : 0;
    iseapol = (((struct llc *)&wh[1])->llc_snap.ether_type == htons(ETHERTYPE_PAE)) ? 1 : 0;
    if ((!ismulticast) && (!iseapol))
    {
        isip = (((struct llc *)&wh[1])->llc_snap.ether_type == htons(ETHERTYPE_IP)) ? 1 : 0;
        if (isip)
        {
            use4addr = ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS) ? 1 : 0;
            if (use4addr) {
                hdrsize = sizeof(struct ieee80211_frame_addr4);
            }
            else {
                hdrsize = sizeof(struct ieee80211_frame);
            }
            hdrsize += sizeof(struct llc);
            ip = (struct ip_header *)(wbuf_get_scatteredbuf_header(wbuf, hdrsize));
            if(ip == NULL) {
                return 1;
            }

            if (ip->protocol == IP_PROTO_TCP)
            {
                return 0;
            }
        }
        return 1;
    }

    return (iseapol || ismulticast);

}

int ieee80211_8023frm_amsdu_check(wbuf_t wbuf)
{
    struct ether_header *eh;

    eh = (struct ether_header *)(wbuf_header(wbuf));
    if (eh->ether_type == htons(ETHERTYPE_IP)) {
        const struct ip_header *ip = (struct ip_header *) ((u_int8_t *)eh + sizeof (struct ether_header));
        int len;
        wbuf_t xbuf;
        
        len =  wbuf_get_datalen_temp(wbuf);
        if (len < sizeof(struct ether_header) + sizeof(struct ip_header)) { 
            // IP Header is not in the first buffer, if there is a 2nd buffer, assume it's in there.
            xbuf = wbuf_next_buf(wbuf);

            if (xbuf != NULL) {
                ip = (struct ip_header *)wbuf_raw_data(xbuf);
            }
            else {
                return 1;
            }
        }
        
        if (ip->protocol == IP_PROTO_TCP) {
            return 0;
        }
    }   
    return 1;
}
/*
 * Determine if this frame is not data, or is a multicast or EAPOL.
 * We do not send them in an AMSDU.
 */
int
ieee80211_amsdu_check(struct ieee80211vap *vap, wbuf_t wbuf)
{
    if (wbuf_is_uapsd(wbuf) || wbuf_is_moredata(wbuf)) {
        return 1;
    }
    if (IEEE80211_VAP_IS_SEND_80211_ENABLED(vap)) {
       return ieee80211_80211frm_amsdu_check(wbuf);
    }
    else {
       return ieee80211_8023frm_amsdu_check(wbuf);
    }
}
/*
 * Form AMSDU frames and encapsulate into 802.11 frame format.
 */
static void
ieee80211_80211frm_amsdu_encap(
    wbuf_t amsdu_wbuf,
    wbuf_t wbuf,
    u_int16_t framelen,
    int append_wlan)
{
    struct ieee80211_frame *wh;
    struct ether_header *eh_inter;
    u_int16_t amsdu_len;
    int offset;
    u_int8_t *dest;
    int pad = 0, i, use4addr, hdrsize;
    /* Get WLAN header */
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    use4addr = ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
                IEEE80211_FC1_DIR_DSTODS) ? 1 : 0;
    if (use4addr)
        hdrsize = sizeof(struct ieee80211_frame_addr4);
    else
        hdrsize = sizeof(struct ieee80211_frame);    

    if (append_wlan) {
        /*
         * Copy the original 802.11 header
         */
        wbuf_init(amsdu_wbuf, AMSDU_BUFFER_HEADROOM);
        wbuf_push(amsdu_wbuf, hdrsize);
        OS_MEMCPY(wbuf_header(amsdu_wbuf), wh, hdrsize);
    }
    else {
        /* If the length of former subframe is not round 4, has to pad it here,
         or AP could not parse the following subframes.
         */
        u_int8_t *buf = (u_int8_t *)wbuf_header(amsdu_wbuf);
        pad = (wbuf_get_pktlen(amsdu_wbuf) - hdrsize) % 4;
        
        for (i = 0; i < pad; i++) {
            buf[i + wbuf_get_pktlen(amsdu_wbuf)] = 0;
        }
        wbuf_append(amsdu_wbuf, pad);
    }

    /* Get the start location for this AMSDU */
    offset = wbuf_get_pktlen(amsdu_wbuf);
    
    /* Compute AMSDU length, i.e. header + MSDU */
    amsdu_len = (u_int16_t) sizeof(struct ether_header) + framelen - hdrsize;

    eh_inter = (struct ether_header *)((caddr_t)wbuf_header(amsdu_wbuf)+ offset);
    /* Prepare AMSDU sub-frame header */
    /* ether_type in amsdu is actually the size of subframe */
    eh_inter->ether_type = htons((framelen - hdrsize));
    switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
    case IEEE80211_FC1_DIR_TODS:  /* STA->AP  */
        IEEE80211_ADDR_COPY(eh_inter->ether_dhost, wh->i_addr3);
        IEEE80211_ADDR_COPY(eh_inter->ether_shost, wh->i_addr2);
        break;
    case IEEE80211_FC1_DIR_NODS:  /* STA->STA */
        IEEE80211_ADDR_COPY(eh_inter->ether_dhost, wh->i_addr1);
        IEEE80211_ADDR_COPY(eh_inter->ether_shost, wh->i_addr2);
        break;

    case IEEE80211_FC1_DIR_FROMDS:  /* AP ->STA */
        IEEE80211_ADDR_COPY(eh_inter->ether_dhost, wh->i_addr1);
        IEEE80211_ADDR_COPY(eh_inter->ether_shost, wh->i_addr3);
        break;

    default:
        break;
#if 0
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT, "%s: Wrong direction.\n", __func__);
        wbuf_free(wbuf_new);
        goto bad;
#endif
    }

    /* Fill in the AMSDU payload */
    dest = (u_int8_t *)((u_int8_t *)eh_inter + sizeof(struct ether_header));
    wbuf_copydata(wbuf, hdrsize, wbuf_get_pktlen(wbuf), dest);

    /* Update AMSDU buffer length */
    wbuf_append(amsdu_wbuf, amsdu_len);
}

/*
 * Form AMSDU frames and encapsulate into 80223 frame format.
 */
static void
ieee80211_8023frm_amsdu_encap(
    wbuf_t amsdu_wbuf,
    wbuf_t wbuf,
    u_int16_t framelen,
    int append_wlan)
{
    struct ether_header *eh;
    struct llc *llc;
    struct ether_header *eh_inter;
    u_int16_t amsdu_len;
    int offset;
    u_int8_t *dest, *src;    
    wbuf_t xbuf;
    u_int32_t len_to_copy, len_in_buf, bytes_copied, start_offset;

    /* Get the Ethernet header from the new packet */
    eh = (struct ether_header *)wbuf_header(wbuf);    
    if (append_wlan) {
        /*
         * Copy the original 802.11 header
         */
        OS_MEMCPY(wbuf_header(amsdu_wbuf), eh, sizeof(struct ether_header));
        wbuf_append(amsdu_wbuf, sizeof(struct ether_header));
    }
    /* Get the start location for this AMSDU */
    offset = wbuf_get_pktlen(amsdu_wbuf);

    /* Compute AMSDU length, i.e. header + MSDU */
    amsdu_len = framelen + LLC_SNAPFRAMELEN;

    eh_inter = (struct ether_header *)((u_int8_t *)(wbuf_raw_data(amsdu_wbuf))+offset);

    /* Prepare AMSDU sub-frame header */
    OS_MEMCPY(eh_inter, eh, sizeof(struct ether_header) - sizeof eh->ether_type);
    /* Length field reflects the size of MSDU only */
    eh_inter->ether_type = htons(amsdu_len - sizeof(struct ether_header));

    /* Prepare LLC header */
    llc = (struct llc *)((u_int8_t *)eh_inter + sizeof(struct ether_header));
    llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
    llc->llc_control = LLC_UI;
    llc->llc_snap.org_code[0] = 0;
    llc->llc_snap.org_code[1] = 0;
    llc->llc_snap.org_code[2] = 0;
    llc->llc_snap.ether_type = eh->ether_type;

    /* Fill in the AMSDU payload */
    /* The payload in the original packet could be in multiple wbufs. We need to gather these into a single buffer. */
    /* Note that the ethernet header is already copied and changed to type 1. Need to start at Payload. */
    xbuf = wbuf;
    len_to_copy = wbuf_get_pktlen(wbuf);        
    len_in_buf = wbuf_get_datalen_temp(wbuf);
    bytes_copied = sizeof(struct ether_header);
    start_offset = sizeof(struct ether_header);    
    src = (u_int8_t *)((u_int8_t *)(wbuf_header(wbuf)) + sizeof(struct ether_header));
    dest = (u_int8_t *)((u_int8_t *)llc + sizeof(struct llc));
    while (bytes_copied < len_to_copy) {
        if (start_offset >= len_in_buf) {
            xbuf = wbuf_next_buf(xbuf);
            if (xbuf == NULL) {
                break;
            }
            src = (u_int8_t *)wbuf_header(xbuf);
            len_in_buf =  wbuf_get_datalen_temp(xbuf);
            start_offset = 0;
        }
        OS_MEMCPY(dest, src, len_in_buf);
        dest += len_in_buf;
        start_offset += len_in_buf;
        bytes_copied += len_in_buf;
        src += len_in_buf;
    }
    
    /* Update AMSDU buffer length */
    wbuf_append(amsdu_wbuf, amsdu_len);
}

void ieee80211_amsdu_encap(
    struct ieee80211vap *vap,
    wbuf_t amsdu_wbuf,
    wbuf_t wbuf,
    u_int16_t framelen,
    int append_wlan)
{
    if (IEEE80211_VAP_IS_SEND_80211_ENABLED(vap)) {
        ieee80211_80211frm_amsdu_encap(amsdu_wbuf,wbuf,framelen,append_wlan);
    }
    else {
        ieee80211_8023frm_amsdu_encap(amsdu_wbuf,wbuf,framelen,append_wlan);
    }
}
#endif /* ATH_AMSDU */

static void ieee80211_pwrsave_iter_txq_empty(void *arg, struct ieee80211vap *vap, bool lastvap)
{

    if (vap->iv_txrx_event_info.iv_txrx_event_filter & IEEE80211_VAP_OUTPUT_EVENT_TXQ_EMPTY) {
        ieee80211_vap_txrx_event evt;
        evt.type = IEEE80211_VAP_OUTPUT_EVENT_TXQ_EMPTY;
        ieee80211_vap_txrx_deliver_event(vap,&evt);
    }

}

void
ieee80211_notify_queue_status(struct ieee80211com *ic, u_int16_t qdepth)
{
    int vaps_count=0;
    /* this  need to be fixed to per VAP notification */
    if (qdepth == 0) {
        ieee80211_iterate_vap_list_internal(ic,ieee80211_pwrsave_iter_txq_empty,NULL,vaps_count);
    }
}

bool
ieee80211_is_txq_empty(struct ieee80211com *ic)
{
    return (ic->ic_txq_depth(ic) == 0);
}

bool
wlan_is_txq_empty(wlan_if_t vaphandle)
{
    return ieee80211_is_txq_empty(vaphandle->iv_ic);
}
