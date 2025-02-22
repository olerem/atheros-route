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

#include "ath_internal.h"
#include "ath_aow.h"
#ifndef REMOVE_PKT_LOG
#include "pktlog.h"
extern struct ath_pktlog_funcs *g_pktlog_funcs;
#endif

#if ATH_SUPPORT_VOWEXT
#include "ratectrl11n.h"

#if ATH_SUPPORT_IQUE
static u_int8_t min_qdepth_per_ac[WME_NUM_AC] = { 2, 2, 1, 1};
#endif

#endif


/*
 * To be included by ath_xmit.c so that  we can inlise some of the functions
 * for performance reasons.
 */
#if ATH_SUPPORT_HT

#define ADDBA_EXCHANGE_ATTEMPTS     10
#define ADDBA_TIMEOUT               200 /* 200 milliseconds */

#if ATH_SUPPORT_VOWEXT
/* Typical MPDU Length, used for all the rate control computations */
#define MPDU_LENGTH                 1544
#endif

extern const u_int32_t bits_per_symbol[][2];


static void
ath_tid_cleanup(struct ath_softc *sc, struct ath_txq *txq, struct ath_atx_tid *tid);

#ifdef VOW_TIDSCHED
void
ath_wrr_schedule(struct ath_softc *sc);
static INLINE int 
is_tid_in_sctidqueue(struct ath_softc *sc,  struct ath_atx_tid *tid);
#endif

void
ath_txq_schedule(struct ath_softc *sc, struct ath_txq *txq);

#ifdef VOW_TIDSCHED
INLINE void ath_tx_queue_tid(struct ath_softc *sc, struct ath_txq *txq, struct ath_atx_tid *tid);
#else
INLINE void ath_tx_queue_tid(struct ath_txq *txq, struct ath_atx_tid *tid);
#endif
extern u_int32_t
ath_pkt_duration(struct ath_softc *sc, u_int8_t rix, struct ath_buf *bf,
                 int width, int half_gi, HAL_BOOL shortPreamble);

#ifdef VOW_TIDSCHED
extern void ath_tid_cqw(struct ath_softc *sc, struct ath_atx_tid *tid);
#endif

#ifdef VOW_LOGLATENCY
static inline void
vow_loglatency(struct ath_softc *sc, struct ath_buf *bf, u_int32_t currts)
{
    struct ath_stats *stats = (struct ath_stats *) &sc->sc_stats;

    u_int32_t qin_ts;
    u_int32_t lapsed_rd;
    u_int32_t lapsed_qd;
    u_int32_t lapsed_td;
    u_int8_t  bin_rd; 
    u_int8_t  bin_qd; 
    u_int8_t  ac;
    u_int8_t firstxmit;
    u_int32_t firstxmitts; 

    firstxmit = wbuf_get_firstxmit(bf->bf_mpdu);
    firstxmitts = wbuf_get_firstxmitts(bf->bf_mpdu);
    qin_ts = wbuf_get_qin_timestamp(bf->bf_mpdu);
    ac = wbuf_get_priority(bf->bf_mpdu);

    if(!firstxmit) {
      lapsed_rd = (currts>=firstxmitts) ? (currts - firstxmitts) : 
                                          ((0xffffffff - firstxmitts) + currts);
      lapsed_td = (currts>=qin_ts) ? (currts - qin_ts) : 
                                     ((0xffffffff - qin_ts) + currts);
      lapsed_qd = lapsed_td - lapsed_rd;

      bin_rd = (lapsed_rd>>10);
      bin_qd = (lapsed_qd>>10);
      if(bin_rd>(ATH_STATS_LATENCY_BINS-1)) bin_rd=ATH_STATS_LATENCY_BINS-1;
      if(bin_qd>(ATH_STATS_LATENCY_BINS-1)) bin_qd=ATH_STATS_LATENCY_BINS-1;

      stats->ast_queue_delay[0][bin_qd]++;
      stats->ast_queue_delay[ac+1][bin_qd]++;
      stats->ast_retry_delay[0][bin_rd]++;
      stats->ast_retry_delay[ac+1][bin_rd]++;
    }
}
#endif

#ifdef VOW_TIDSCHED
/*
 * Check if TID is in sc->tid_q 
 */
static INLINE int 
is_tid_in_sctidqueue(struct ath_softc *sc,  struct ath_atx_tid *tid) {
    struct ath_atx_tid *tmp_tid;
    int found = 0;
    TAILQ_FOREACH(tmp_tid, &sc->tid_q, wrr_tid_qelem) {

    if(tmp_tid == tid) {
        found = 1;
        break;
        }
    }
  
    return(found);
}
#endif

/*
 * Check if an ADDBA is required.
 */
int
ath_aggr_check(ath_dev_t dev, ath_node_t node, u_int8_t tidno)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = ATH_NODE(node);
    struct ath_atx_tid *tid;

#ifdef ATH_RIFS
    if (!sc->sc_txaggr && !sc->sc_txrifs)
#else
    if (!sc->sc_txaggr)
#endif
        return 0;
    
    /* ADDBA exchange must be completed before sending aggregates */
    tid = ATH_AN_2_TID(an, tidno);

    if (tid->cleanup_inprogress)
        return 0;
    
    if (!tid->addba_exchangecomplete) {
        if (!tid->addba_exchangeinprogress &&
            (tid->addba_exchangeattempts < ADDBA_EXCHANGE_ATTEMPTS)) {
            tid->addba_exchangeattempts++;
            return 1;
        }
    }
    return 0;
}

/*
 * ADDBA request timer - timeout
 */
static int
ath_addba_timer(void *arg)
{
    ath_atx_tid_t *tid = (ath_atx_tid_t *)arg;
    struct ath_softc *sc = tid->an->an_sc;

    ATH_PS_WAKEUP(sc);

    if (sc->sc_scanning) {
        // If we are scanning, reschedule the addba timer expiry - we 
        // should not flush the tid at this point.
        DPRINTF(sc, ATH_DEBUG_XMIT,
                "%s: Scanning : Addba Timer Expired, but rescheduled \n", __func__);
        ATH_PS_SLEEP(sc);
        return 0;
    }
    if (cmpxchg(&tid->addba_exchangeinprogress, 1, 0) == 1) {
        /* ADDBA exchange timed out, schedule pending frames */
        ath_vap_pause_txq_use_inc(sc);
        ATH_TX_RESUME_TID(sc, tid);
        ath_vap_pause_txq_use_dec(sc);
    }

    ATH_PS_SLEEP(sc);

    return 1;   /* don't re-arm itself */
}

/*
 * Setup ADDBA request
 */
void
ath_addba_requestsetup(
    ath_dev_t dev, ath_node_t node,
    u_int8_t tidno,
    struct ieee80211_ba_parameterset *baparamset,
    u_int16_t *batimeout,
    struct ieee80211_ba_seqctrl *basequencectrl,
    u_int16_t buffersize
    )
{
    struct ath_node *an = ATH_NODE(node);
    ath_atx_tid_t *tid = ATH_AN_2_TID(an, tidno);
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    /* make sure the value of amsdusupported can only be 1 or 0 */
    baparamset->amsdusupported = sc->sc_txamsdu? IEEE80211_BA_AMSDU_SUPPORTED:0;
    baparamset->bapolicy       = IEEE80211_BA_POLICY_IMMEDIATE;
    baparamset->tid            = tidno;
    baparamset->buffersize     = buffersize;
    *batimeout                 = 0;
    basequencectrl->fragnum    = 0;
    basequencectrl->startseqnum = tid->seq_start;

    /* Start ADDBA request timer */
    if (cmpxchg(&tid->addba_exchangeinprogress, 0, 1) == 0) {
        if (!ath_timer_is_active(&tid->addba_requesttimer))
            ath_start_timer(&tid->addba_requesttimer);
    }

    ATH_TX_PAUSE_TID(ATH_DEV_TO_SC(dev), tid);
}

/*
 * Process ADDBA response
 */
void
ath_addba_responseprocess(
    ath_dev_t dev, ath_node_t node,
    u_int16_t statuscode,
    struct ieee80211_ba_parameterset *baparamset,
    u_int16_t batimeout
    )
{
    struct ath_node *an = ATH_NODE(node);
    u_int16_t tidno = baparamset->tid;
    ath_atx_tid_t *tid = ATH_AN_2_TID(an, tidno);
    int resume = 1;

    /* Stop ADDBA request timer */
    if (cmpxchg(&tid->addba_exchangeinprogress, 1, 0) == 1) {
        ath_cancel_timer(&tid->addba_requesttimer, CANCEL_NO_SLEEP);
    } else {
        resume = 0;
    }

    tid->addba_exchangestatuscode = statuscode;

    if (statuscode == IEEE80211_STATUS_SUCCESS) {
        /* Enable aggregation! */
        tid->addba_exchangecomplete = 1;

        /* adjust transmitter's BAW size according to ADDBA response */
        tid->baw_size = MIN(baparamset->buffersize, tid->baw_size);

        /*
         * For nodes that have negotiated for a lower BA window, lets attempt
         * to fill the entire BA with a single aggregate.
         */
        if (tid->baw_size < WME_MAX_BA) {
            tid->min_depth = 1;
        }

        /* This field indicates whether the receiver accepts AMSDUs
         * carried in QoS data AMPDU under this BlockAck aggrement.
         */
        tid->addba_amsdusupported = baparamset->amsdusupported;

        ath_wmi_aggr_enable(dev, an, tidno, 1);

        if (resume) {
            struct ath_softc *sc = ATH_DEV_TO_SC(dev);
            ath_vap_pause_txq_use_inc(sc);
            ATH_TX_RESUME_TID(sc, tid);
            ath_vap_pause_txq_use_dec(sc);
        }
    } else {
        /* ADDBA exchange failed, schedule pending frames */
        if (resume) {
            struct ath_softc *sc = ATH_DEV_TO_SC(dev);
            ath_vap_pause_txq_use_inc(sc);
            ATH_TX_RESUME_TID(sc, tid);
            ath_vap_pause_txq_use_dec(sc);
        }
    }
}

/*
 * Return status of ADDBA request
 */
u_int16_t
ath_addba_status(ath_dev_t dev, ath_node_t node, u_int8_t tidno)
{
    struct ath_node *an = ATH_NODE(node);
    struct ath_atx_tid *tid = ATH_AN_2_TID(an, tidno);
    u_int16_t status;

    /*
     * Report the ADDBA response status code.  Return a special status to indicate
     * that either ADDBA was not initiated, or the response has not been received yet.
     */
    if ((tid->addba_exchangestatuscode == IEEE80211_STATUS_SUCCESS) &&
        !tid->addba_exchangecomplete) {
        status = 0xFFFF;
    } else {
        status = tid->addba_exchangestatuscode;
    }

    return status;
}

/*
 * Clear ADDBA for all tids in this node
 */
void
ath_addba_clear(ath_dev_t dev, ath_node_t node)
{
    struct ath_node *an = ATH_NODE(node);
    int i;
    struct ath_atx_tid *tid;
    struct ath_arx_tid *rxtid;
#define    N(a)    (sizeof (a) / sizeof (a[0]))

    for (i = 0; i < N(an->an_tx_tid); i++) {
        tid = &an->an_tx_tid[i];
        if (tid->addba_exchangecomplete) {
            tid->addba_exchangecomplete = 0;
            tid->addba_exchangeattempts = 0;
            tid->addba_exchangestatuscode = IEEE80211_STATUS_UNSPECIFIED;
            tid->paused = AH_FALSE;

            ath_wmi_aggr_enable(dev, an, tid->tidno, 0);
        }
    }

    for (i = 0; i < N(an->an_rx_tid); i++) {
        rxtid = &an->an_rx_tid[i];
        if (rxtid->addba_exchangecomplete)
            rxtid->addba_exchangecomplete = 0;
    }
#undef N
}

void
ath_addba_cancel_timers(ath_dev_t dev, ath_node_t node)
{
    struct ath_node *an = ATH_NODE(node);
    int tidno;
    ath_atx_tid_t *tid;

    for (tidno = 0; tidno < WME_NUM_TID; tidno++) {
        tid = ATH_AN_2_TID(an, tidno);
        if (cmpxchg(&tid->addba_exchangeinprogress, 1, 0) == 1) {
            ath_cancel_timer(&tid->addba_requesttimer, CANCEL_NO_SLEEP);
        }
    }
}

/*
 * Add a sub-frame to block ack window
 */
static void
ath_tx_addto_baw(struct ath_softc *sc, struct ath_atx_tid *tid, struct ath_buf *bf)
{
    int index, cindex;

    if (bf->bf_isretried) {
        __11nstats(sc, tx_bawretries);
        return;
    }

    __11nstats(sc, tx_bawnorm);


    index  = ATH_BA_INDEX(tid->seq_start, bf->bf_seqno);
    cindex = (tid->baw_head + index) & (ATH_TID_MAX_BUFS - 1);
#if AR_DEBUG
    /* This is debug code for bug 34797. */
    if (TX_BUF_BITMAP_IS_SET(tid->tx_buf_bitmap, cindex)) {
        char loc_panic_buf[256];
        snprintf(loc_panic_buf, sizeof(loc_panic_buf),
            "tidno %d cix %d btail %d bhead %d "
            "seq_start %d bf_seqno %d seq_next %d",
            tid->tidno, cindex, tid->baw_tail, tid->baw_head,
            tid->seq_start, bf->bf_seqno, tid->seq_next);
        ASSERT_MSG(!TX_BUF_BITMAP_IS_SET(tid->tx_buf_bitmap, cindex), loc_panic_buf);
    }
#else
    ASSERT(!TX_BUF_BITMAP_IS_SET(tid->tx_buf_bitmap, cindex));
#endif
    TX_BUF_BITMAP_SET(tid->tx_buf_bitmap, cindex);

    if (index >= ((tid->baw_tail - tid->baw_head) & (ATH_TID_MAX_BUFS - 1))) {
        __11nstats(sc, tx_bawadv);
        tid->baw_tail = cindex;
        INCR(tid->baw_tail, ATH_TID_MAX_BUFS);
    }
}

/*
 * Update block ack window
 */
void
ath_tx_update_baw(struct ath_softc *sc, struct ath_atx_tid *tid, int seqno)
{
    int index, cindex;

    __11nstats(sc, tx_bawupdates);

    index  = ATH_BA_INDEX(tid->seq_start, seqno);
    cindex = (tid->baw_head + index) & (ATH_TID_MAX_BUFS - 1);

    TX_BUF_BITMAP_CLR(tid->tx_buf_bitmap, cindex);

    while (tid->baw_head != tid->baw_tail &&
           !TX_BUF_BITMAP_IS_SET(tid->tx_buf_bitmap, tid->baw_head)) {
        __11nstats(sc, tx_bawupdtadv);
        INCR(tid->seq_start, IEEE80211_SEQ_MAX);
        INCR(tid->baw_head, ATH_TID_MAX_BUFS);
    }
}

static void
ath_bar_tx(struct ath_softc *sc, struct ath_node *an, struct ath_atx_tid *tid)
{
    __11nstats(sc, tx_bars);

    /* pause TID until BAR completes */
    ATH_TX_PAUSE_TID(sc, tid);

    if (sc->sc_ieee_ops->send_bar) {
        if (sc->sc_ieee_ops->send_bar(an->an_node, tid->tidno, tid->seq_start)) {
            /* resume tid if send bar failed. */
            ATH_TX_RESUME_TID(sc, tid);
        }
    }
}

void
ath_tx_complete_bar(struct ath_softc *sc, struct ath_buf *bf, ath_bufhead *bf_q, int txok)
{
    struct ath_node *an = bf->bf_node;
    struct ath_atx_tid *tid = ATH_AN_2_TID(an, bf->bf_tidno);

    ATH_TX_RESUME_TID(sc, tid);

#ifdef ATH_SUPPORT_TxBF
    ath_tx_complete_buf(sc, bf, bf_q, txok, 0, 0);
#else
    ath_tx_complete_buf(sc, bf, bf_q, txok);
#endif
}

/*
 * pause a tid
 */
void
ath_tx_pause_tid(struct ath_softc *sc, struct ath_atx_tid *tid)
{
    struct ath_txq *txq = &sc->sc_txq[tid->ac->qnum];

    ATH_TXQ_LOCK(txq);   

    tid->paused++;

    __11nstats(sc, tx_tidpaused);

    ATH_TXQ_UNLOCK(txq);
}

/*
 * resume a tid and schedule aggregate
 */
void
ath_tx_resume_tid(struct ath_softc *sc, struct ath_atx_tid *tid)
{
    struct ath_txq *txq = &sc->sc_txq[tid->ac->qnum];

#ifdef ATH_SWRETRY
    /*if STA leave and receive the power save of do not do the tid->pause--*/
    if ((tid->an->an_flags & ATH_NODE_LEAVE)!=ATH_NODE_LEAVE)
    {
#endif	
    /* recv pspoll when sta is not in ps mode */
        ASSERT(tid->paused > 0);

        ATH_TXQ_LOCK(txq);

        tid->paused--;
        __11nstats(sc, tx_tidresumed);

        if (tid->paused > 0) {
            ATH_TXQ_UNLOCK(txq);
            return;
        }
#ifdef ATH_SWRETRY		
    }
    else
        ATH_TXQ_LOCK(txq);
#endif	

    if (TAILQ_EMPTY(&tid->buf_q)) {
        ATH_TXQ_UNLOCK(txq);
        return;
    }

    /*
     * Add this TID to scheduler and try to send out aggregates
     */
#ifdef VOW_TIDSCHED
    ath_tx_queue_tid(sc, txq, tid);
#else
    ath_tx_queue_tid(txq, tid);
#endif
    ath_txq_schedule(sc, txq);
    ATH_TXQ_UNLOCK(txq);
}

/*
 * Performs transmit side cleanup when TID changes from aggregated to
 * unaggregated.
 * - Pause the TID and mark cleanup in progress
 * - Stop ADDBA timer if it's armed.
 * - Discard all retry frames from the s/w queue.
 */
void
ath_tx_aggr_teardown(struct ath_softc *sc, struct ath_node *an, u_int8_t tidno)
{
    struct ath_atx_tid *tid = ATH_AN_2_TID(an, tidno);
    struct ath_txq *txq = &sc->sc_txq[tid->ac->qnum];
    struct ath_buf *bf;
    ath_bufhead bf_head;

    if ( tid->cleanup_inprogress ) /* cleanup is in progress */
        return;

    if (!tid->addba_exchangecomplete) {
        /* Clear the addba_exchangeattempts
         * we might have exceeded the exchageattempts limit
         */
        tid->addba_exchangeattempts = 0;
        tid->addba_exchangestatuscode = IEEE80211_STATUS_UNSPECIFIED;
        return;
    }

    /* TID must be paused first */
    ATH_TX_PAUSE_TID(sc, tid);
    
    /* stop ADDBA request timer (if ADDBA in progress) */
    if (cmpxchg(&tid->addba_exchangeinprogress, 1, 0) == 1)
        ath_cancel_timer(&tid->addba_requesttimer, CANCEL_NO_SLEEP);

    /*
     * drop all software retried frames and mark this TID
     */
    ATH_TXQ_LOCK(txq);
    while (!TAILQ_EMPTY(&tid->buf_q)) {
        bf = TAILQ_FIRST(&tid->buf_q);
        if (!bf->bf_isretried) {
            /*
             * NB: it's based on the assumption that
             * software retried frame will always stay
             * at the head of software queue.
             */
            break;
        }

        TAILQ_REMOVE_HEAD_UNTIL(&tid->buf_q, &bf_head, bf->bf_lastfrm, bf_list);
        ath_tx_update_baw(sc, tid, bf->bf_seqno);

        /* complete this sub-frame */
#ifdef ATH_SUPPORT_TxBF
        ath_tx_complete_buf(sc, bf, &bf_head, 0, 0, 0);
#else
        ath_tx_complete_buf(sc, bf, &bf_head, 0);
#endif
    }


    if (tid->baw_head != tid->baw_tail) {
        ATH_TXQ_UNLOCK(txq);
        tid->cleanup_inprogress = AH_TRUE;
    } else {
        tid->addba_exchangecomplete = 0;
        tid->addba_exchangeattempts = 0;
        tid->addba_exchangestatuscode = IEEE80211_STATUS_UNSPECIFIED;

        ATH_TXQ_UNLOCK(txq);

        ath_wmi_aggr_enable((ath_dev_t) sc, an, tidno, 0);

        ath_vap_pause_txq_use_inc(sc);
        ATH_TX_RESUME_TID(sc, tid);
        ath_vap_pause_txq_use_dec(sc);
    }


}

/*
 * Tear down either tx or rx aggregation. This is usually called
 * before protocol layer sends a DELBA.
 */
void
ath_aggr_teardown(ath_dev_t dev, ath_node_t node, u_int8_t tidno, u_int8_t initiator)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = ATH_NODE(node);

    if (initiator)
        ath_tx_aggr_teardown(sc, an, tidno);
    else
        ath_rx_aggr_teardown(sc, an, tidno);
}

/*
 * Function to send a normal HT (non-AMPDU) frame
 * NB: must be called with txq lock held
 */
int
ath_tx_send_normal(struct ath_softc *sc, struct ath_txq *txq, struct ath_atx_tid *tid,
        ath_bufhead *bf_head, ieee80211_tx_control_t *txctl)
{
    struct ath_buf *bf;
    int isProbe;
    u_int8_t ac = TID_TO_WME_AC(tid->tidno);

#ifdef ATH_SWRETRY
    /*
     * When tid is paused, but
     * - tid is empty, and
     * - pspoll_pending is on, and
     * - HWQ is empty
     * in this case, we send the frame to HW directly.
     */
    bool pspoll_bypass;
#endif

    bf = TAILQ_FIRST(bf_head);
    bf->bf_seqno = txctl->seqno;

    __11nstats(sc, tx_pkts);

#ifdef ATH_SWRETRY
	pspoll_bypass = TAILQ_EMPTY(&tid->buf_q)
							 && tid->paused
							 && tid->an->an_pspoll_pending
							 && !txq->axq_depth;

    if (!pspoll_bypass
        && (!TAILQ_EMPTY(&tid->buf_q) || tid->paused || txq->axq_depth))
#else
    if (!TAILQ_EMPTY(&tid->buf_q) || tid->paused || txq->axq_depth)
#endif
    {
#ifdef ATH_SWRETRY
        // if tid is paused and it is not a data, set an_pspoll_pending as AH_TRUE to send it out.
        if (tid->paused && !tid->an->an_pspoll_pending && !bf->bf_isdata)
        {
            ATH_NODE_SWRETRY_TXBUF_LOCK(tid->an);
            tid->an->an_pspoll_pending = AH_TRUE;
            ATH_NODE_SWRETRY_TXBUF_UNLOCK(tid->an);			
        }
#endif

        /*
         * Add this frame to software queue for scheduling later.
         */
        __11nstats(sc, tx_queue);
        TAILQ_CONCAT(&tid->buf_q, bf_head, bf_list);

#ifdef VOW_TIDSCHED
        ath_tx_queue_tid(sc, txq, tid);
#else
        ath_tx_queue_tid(txq, tid);
#endif

#if ATH_SWRETRY
        if ((tid->paused && tid->an->an_pspoll_pending && !txq->axq_depth) ||
            (!txq->axq_depth && !tid->paused))
#else
        if (!txq->axq_depth && !tid->paused)
#endif
        {
            ath_txq_schedule(sc, txq);
        }

		
#if ATH_SWRETRY		
        //if current STA power save is on and UMAC don't have any power save data
        //set the tim bit on.
        //add a condition here for check an_pspoll_pending due to if and an_pspoll_pending set TURE, it will direcen send 
        //out one packet.
        if (!tid->an->an_pspoll_pending && !tid->an->an_tim_set && (tid->an->an_flags & ATH_NODE_PWRSAVE) 
           && ((sc->sc_ieee_ops->get_pwrsaveq_len(tid->an->an_node)==0)))
        {
            ATH_NODE_SWRETRY_TXBUF_LOCK(tid->an);
            sc->sc_ieee_ops->set_tim(tid->an->an_node,1);
            tid->an->an_tim_set = AH_TRUE;
            ATH_NODE_SWRETRY_TXBUF_UNLOCK(tid->an);
        }		
#endif		
        return 0;
    }

    bf->bf_isampdu = 0; /* regular HT frame */

    if (txctl->ht) {
        /* update starting sequence number for subsequent ADDBA request */
        INCR(tid->seq_start, IEEE80211_SEQ_MAX);
    }

    sc->sc_log_rcfind = 1;
    if( txctl->isdata && !txctl->use_minrate && !txctl->ismcast) {
#if UMAC_SUPPORT_SMARTANTENNA
    if (wbuf_is_sa_train_packet(bf->bf_mpdu))
    {
        ath_rate_fixedrate(sc, tid->an, bf->bf_shpreamble, wbuf_sa_get_rateidx(bf->bf_mpdu),
                           wbuf_sa_get_antenna(bf->bf_mpdu), ATH_RC_PROBE_ALLOWED, ac,
                           bf->bf_rcs,&isProbe, AH_FALSE, bf->bf_flags);
    }
    else
    {
        ath_rate_findrate(sc, tid->an, bf->bf_shpreamble, bf->bf_frmlen,
                      ATH_11N_TXMAXTRY, ATH_RC_PROBE_ALLOWED,
                      ac, bf->bf_rcs, &isProbe, AH_FALSE,bf->bf_flags);
    }

#else        
    ath_rate_findrate(sc, tid->an, bf->bf_shpreamble, bf->bf_frmlen,
                      ATH_11N_TXMAXTRY, ATH_RC_PROBE_ALLOWED,
                      ac, bf->bf_rcs, &isProbe, AH_FALSE,bf->bf_flags);
#endif        
    }

#if ATH_SWRETRY
    if (pspoll_bypass)
    {
    	ATH_NODE_SWRETRY_TXBUF_LOCK(tid->an);
        tid->an->an_pspoll_pending = AH_FALSE;
		ATH_NODE_SWRETRY_TXBUF_UNLOCK(tid->an);
    }
#endif

    /* Queue to h/w directly */
    __11nstats(sc, tx_minqdepth);
    bf->bf_nframes = 1;
    bf->bf_lastbf = bf->bf_lastfrm; /* one single frame */
    ath_buf_set_rate(sc, bf);
    return ath_tx_txqaddbuf(sc, txq, bf_head);
}

/*
 * Function to send an A-MPDU
 * NB: must be called with txq lock held
 */
int
ath_tx_send_ampdu(struct ath_softc *sc, struct ath_txq *txq, struct ath_atx_tid *tid, 
                  ath_bufhead *bf_head, ieee80211_tx_control_t *txctl)
{
    struct ath_buf *bf;
    int isProbe;
    u_int8_t ac = TID_TO_WME_AC(tid->tidno);
#ifdef ATH_SWRETRY
    /*
     * When tid is paused, but
     * - tid is empty, and
     * - pspoll_pending is on, and
     * - HWQ is empty
     * in this case, we send the frame to HW directly.
     */
    bool pspoll_bypass;
#endif

    bf = TAILQ_FIRST(bf_head);
    bf->bf_seqno = txctl->seqno; /* save seqno and tidno in buffer */
    bf->bf_tidno = txctl->tidno;

    __11nstats(sc, tx_pkts);

    /*
     * Do not queue to h/w when any of the following conditions is true:
     * - there are pending frames in software queue
     * - the TID is currently paused for ADDBA/BAR request
     * - seqno is not within block-ack window
     * - h/w queue depth exceeds low water mark
     */
#ifdef ATH_SWRETRY
	pspoll_bypass = TAILQ_EMPTY(&tid->buf_q)
                         && tid->paused
                         && tid->an->an_pspoll_pending
                         && !txq->axq_depth;
	 if (!pspoll_bypass
		 && (!TAILQ_EMPTY(&tid->buf_q) || (tid->paused && !pspoll_bypass)
		 || txq->axq_depth
		 || !BAW_WITHIN(tid->seq_start, tid->baw_size, bf->bf_seqno)))
#else
    if (!TAILQ_EMPTY(&tid->buf_q) || tid->paused ||
        txq->axq_depth ||
        !BAW_WITHIN(tid->seq_start, tid->baw_size, bf->bf_seqno))
#endif
    {
#ifdef ATH_SWRETRY
        // if tid is paused and it is not a data, set an_pspoll_pending as AH_TRUE to send it out.
        if (tid->paused && !tid->an->an_pspoll_pending && !bf->bf_isdata)
        {
            ATH_NODE_SWRETRY_TXBUF_LOCK(tid->an);
            tid->an->an_pspoll_pending = AH_TRUE;
            ATH_NODE_SWRETRY_TXBUF_UNLOCK(tid->an);			
        }
#endif

        /*
         * Add this frame to software queue for scheduling later
         * for aggregation.
         */
        __11nstats(sc, tx_queue);
        TAILQ_CONCAT(&tid->buf_q, bf_head, bf_list);

#ifdef VOW_TIDSCHED
        ath_tx_queue_tid(sc, txq, tid);
#else
        ath_tx_queue_tid(txq, tid);
#endif

#if ATH_SWRETRY
        if ((tid->paused && tid->an->an_pspoll_pending && !txq->axq_depth) ||
            (!txq->axq_depth && !tid->paused))
#else
        if (!txq->axq_depth && !tid->paused)
#endif
        {
            ath_txq_schedule(sc, txq);
        }

#ifdef ATH_SWRETRY
        //if current STA power save is on and UMAC don't have any power save data
        //set the tim bit on.
        //add a condition here for check an_pspoll_pending due to if and an_pspoll_pending set TURE, it will direcen send 
        //out one packet.
        if (!tid->an->an_pspoll_pending && !tid->an->an_tim_set && (tid->an->an_flags & ATH_NODE_PWRSAVE) 
             && ((sc->sc_ieee_ops->get_pwrsaveq_len(tid->an->an_node)==0)))
        {
            ATH_NODE_SWRETRY_TXBUF_LOCK(tid->an);
            sc->sc_ieee_ops->set_tim(tid->an->an_node,1);
            tid->an->an_tim_set = AH_TRUE;
            ATH_NODE_SWRETRY_TXBUF_UNLOCK(tid->an);
        }		
#endif
        return 0;
    }

    bf->bf_isampdu = 1;

    sc->sc_log_rcfind = 1;
    if(likely(!txctl->use_minrate)) {
#if UMAC_SUPPORT_SMARTANTENNA 
        if (wbuf_is_sa_train_packet(bf->bf_mpdu)) {
            ath_rate_fixedrate(sc, tid->an, bf->bf_shpreamble, wbuf_sa_get_rateidx(bf->bf_mpdu),
                           wbuf_sa_get_antenna(bf->bf_mpdu), ATH_RC_PROBE_ALLOWED, ac,
                           bf->bf_rcs,&isProbe, AH_FALSE, bf->bf_flags);
        } else {
            ath_rate_findrate(sc, tid->an, bf->bf_shpreamble, bf->bf_frmlen,
                          ATH_11N_TXMAXTRY, ATH_RC_PROBE_ALLOWED,
                          ac, bf->bf_rcs, &isProbe, AH_FALSE,bf->bf_flags);
        }
#else
        ath_rate_findrate(sc, tid->an, bf->bf_shpreamble, bf->bf_frmlen,
                      ATH_11N_TXMAXTRY, ATH_RC_PROBE_ALLOWED,
                      ac, bf->bf_rcs, &isProbe, AH_FALSE,bf->bf_flags);
#endif    
    }
    /* Add sub-frame to BAW */
    ath_tx_addto_baw(sc, tid, bf);

    /* Queue to h/w without aggregation */
    __11nstats(sc, tx_minqdepth);
    bf->bf_nframes = 1;
    bf->bf_lastbf = bf->bf_lastfrm; /* one single frame */
    ath_buf_set_rate(sc, bf);
    if (ath_tx_txqaddbuf(sc, txq, bf_head) != 0) {	
        ath_tx_update_baw(sc, tid, bf->bf_seqno);		
#ifdef ATH_SUPPORT_TxBF
        ath_tx_complete_buf(sc, bf, bf_head, 0, 0, 0);
#else
        ath_tx_complete_buf(sc, bf, bf_head, 0);
#endif
    }
    return 0;
}

/*
 * looks up the rate
 * returns aggr limit based on lowest of the rates
 */
static u_int32_t
ath_lookup_rate(struct ath_softc *sc, struct ath_node *an, struct ath_buf *bf, int mimoburst, int *legacy)
{
    int                     i, prate = 0;
    u_int32_t               max4msframelen, frame_length;
    u_int16_t               aggr_limit;
    const HAL_RATE_TABLE    *rt = sc->sc_currates;
    u_int16_t               maxampdu;
    u_int8_t                ac;

#if ATH_SUPPORT_VOWEXT
    struct atheros_node     *oan = ATH_NODE_ATHEROS(an);
    TX_RATE_CTRL            *pRc = NULL;
    int                     n_head_fail=0, n_tail_fail=0, n_aggr_size=0;
#endif
    /*
     * Log the rate lookup.
     */
    sc->sc_log_rcfind = 1;
    ac = TID_TO_WME_AC(bf->bf_tidno);
#if ATH_SUPPORT_VOWEXT
#if ATH_SUPPORT_IQUE
    if (ac <= WME_AC_BK) pRc = (TX_RATE_CTRL *)(&oan->txRateCtrl);
    else pRc = (TX_RATE_CTRL *)(&oan->txRateCtrlViVo);
#else
	pRc = (TX_RATE_CTRL *)(&oan->txRateCtrl);
#endif
#endif
    *legacy = 0;
#if UMAC_SUPPORT_SMARTANTENNA 
    if (wbuf_is_sa_train_packet(bf->bf_mpdu))
    {
        ath_rate_fixedrate(sc, an, AH_TRUE, wbuf_sa_get_rateidx(bf->bf_mpdu),
                           wbuf_sa_get_antenna(bf->bf_mpdu), ATH_RC_PROBE_ALLOWED, ac,
                           bf->bf_rcs,&prate, AH_FALSE, bf->bf_flags);
    }
    else
    {
        /* XXX: we know shortPreamble and pktlen is not used for 11n rate control */
        ath_rate_findrate(sc, an, AH_TRUE, 0, ATH_11N_TXMAXTRY, ATH_RC_PROBE_ALLOWED,
                      ac, bf->bf_rcs, &prate, AH_FALSE,bf->bf_flags);
    }
#else
    /* XXX: we know shortPreamble and pktlen is not used for 11n rate control */
     ath_rate_findrate(sc, an, AH_TRUE, 0, ATH_11N_TXMAXTRY, ATH_RC_PROBE_ALLOWED,
                      ac, bf->bf_rcs, &prate, AH_FALSE,bf->bf_flags);
#endif     



    /*
     * Find the lowest frame length among the rate series that will have a
     * 4ms transmit duration.
     * TODO - TXOP limit needs to be considered.
     */
    max4msframelen = IEEE80211_AMPDU_LIMIT_MAX;

    for (i = 0; i < 4; i++) {
        if (bf->bf_rcs[i].tries) {
            frame_length = bf->bf_rcs[i].max4msframelen;

            if (rt->info[bf->bf_rcs[i].rix].phy != IEEE80211_T_HT) {
                *legacy = 1;
                break;
            }

#ifdef ATH_BT_COEX
            if (sc->sc_hasbtcoex) {
                ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_AGGR_FRAME_LEN, &frame_length);
            }
#endif
            max4msframelen = MIN(max4msframelen, frame_length);

            if (mimoburst)
                break;
        }
    }

    /*
     * limit aggregate size by the minimum rate if rate selected is
     * not a probe rate, if rate selected is a probe rate then
     * avoid aggregation of this packet.
     */
    if (prate || *legacy)
        return 0;

#if ATH_SUPPORT_IQUE
    if (sc->sc_ac_params[ac].aggrsize_scaling) {
        u_int16_t vi_depth = 0, vo_depth = 0;

        if (ATH_TXQ_SETUP(sc, WME_AC_VO)) {
            vo_depth = sc->sc_txq[WME_AC_VO].axq_depth;
        }

        if(ac == WME_AC_VI)
        {
            struct ath_txq *txq = &sc->sc_txq[bf->bf_qnum];

            /* In case of VI, apply aggregate size scaling only when 
             *  multi vi traffic is going or if vo traffic is going on
             */
            if(TAILQ_FIRST(&txq->axq_acq) || vo_depth) {
                max4msframelen = max4msframelen >> (sc->sc_ac_params[ac].aggrsize_scaling);
            }
        }
        else
        {
            if (ATH_TXQ_SETUP(sc, WME_AC_VI)) {
                vi_depth = sc->sc_txq[WME_AC_VI].axq_depth;
            }

            if (vi_depth > 0 || vo_depth > 0)
                max4msframelen = max4msframelen >> (sc->sc_ac_params[ac].aggrsize_scaling);
        }
    }
#endif
    aggr_limit = MIN(max4msframelen, sc->sc_config.ampdu_limit);

    /*
     * h/w can accept aggregates upto 16 bit lengths (65535). The IE, however
     * can hold upto 65536, which shows up here as zero. Ignore 65536 since we 
     * are constrained by hw.
     */
    maxampdu = an->an_aggr.tx.maxampdu;
    if (maxampdu)
        aggr_limit = MIN(aggr_limit, maxampdu);

#if ATH_SUPPORT_VOWEXT
    /* RCA */
    if ( (ATH_IS_VOWEXT_RCA_ENABLED(sc)) && ( (WME_AC_VI == ac) || (WME_AC_VO == ac)) ){
        aggr_limit = MIN(aggr_limit, ( (pRc->aggrLimit)*MPDU_LENGTH ));
    }
    else if ( ATH_IS_VOWEXT_AGGRSIZE_ENABLED(sc)) {
        n_head_fail = pRc->nHeadFail ? (pRc->nHeadFail) : 1;
        n_tail_fail = pRc->nTailFail;
        n_aggr_size = pRc->nAggrSize;
        
        if(an->throttle) { 
            an->throttle--;
        }
        if(((n_tail_fail >= MAX((sc->agthresh * n_head_fail)>>2,4)) ||
                ((n_head_fail+n_tail_fail)*2 >= n_aggr_size)) && n_aggr_size >= 8) {
            if((n_aggr_size>>1)) {
                
                
                aggr_limit = MIN(aggr_limit,
                                MAX((n_aggr_size >> 1) * (4 + 1540),sc->agtb_blim));
                
            }
            an->throttle = 10;
        }
    } 
#endif
    return aggr_limit;
}

/*
 * returns the number of delimiters to be added to
 * meet the minimum required mpdudensity.
 * caller should make sure that the rate is  HT rate .
 */
static INLINE int
ath_compute_num_delims(struct ath_softc *sc, struct ath_buf *bf, u_int16_t frmlen, int is_first_subfrm)
{
    const HAL_RATE_TABLE    *rt = sc->sc_currates;
    u_int32_t               nsymbits, nsymbols;
    int                     width, half_gi;
    int                     ndelim = 0, mindelim;
    u_int16_t               minlen;
    u_int8_t                rc, flags, rix;
    u_int32_t               mpdudensity;
    u_int32_t               extradelim = 0;

    
    if (!sc->sc_ent_min_pkt_size_enable) {
        /* Select standard number of delimiters based on frame length alone */
        if (frmlen < ATH_AGGR_MINPLEN) {
            u_int16_t delta = ATH_AGGR_MINPLEN - frmlen;
            ndelim = delta >> 2;
            /* round up if needed */
            if (delta & (ATH_AGGR_DELIM_SZ - 1)) {
                ndelim++;
            }
        }

        /*
         * If encryption enabled, hardware requires some more padding between
         * subframes.
         * TODO - this could be improved to be dependent on the rate.
         *      The hardware can keep up at lower rates, but not higher rates
         *      See bug 20205.
         */
        switch (bf->bf_keytype) {
            case HAL_KEY_TYPE_AES:
#if ATH_SUPPORT_WAPI
            case HAL_KEY_TYPE_WAPI:
#endif
                /*
                 * No delims for encryption on Osprey or later. See EVA 79217.
                 */
                ath_hal_getcapability(sc->sc_ah, HAL_CAP_EXTRADELIMWAR, 0, &extradelim);
		        if (extradelim)
                    ndelim += ATH_AGGR_ENCRYPTDELIM;
                break;
            case HAL_KEY_TYPE_WEP:
            case HAL_KEY_TYPE_TKIP:
                ndelim += ATH_NODE(bf->bf_node)->an_aggr.tx.weptkipdelim;
                break;
            default:
                break;
        }

        /*
         * WAR for EV 77658 - Add delimiters to first sub-frame when using
         * RTS/CTS with aggregation and non-enterprise Osprey.
         *
         * Bug fixed in AR9580/Peacock and later.
         */
        if (is_first_subfrm && sc->sc_ent_rtscts_delim_war) {
            ndelim = MAX(ndelim, AH_FIRST_DESC_NDELIMS);
        }
    }

    /*
     * Convert desired mpdu density from microeconds to bytes based
     * on highest rate in rate series (i.e. first rate) to determine
     * required minimum length for subframe. Take into account
     * whether high rate is 20 or 40Mhz and half or full GI.
     */
    mpdudensity = ATH_NODE(bf->bf_node)->an_aggr.tx.mpdudensity;

    /*
     * If there is no mpdu density restriction, no further calculation
     * is needed.
     */
    if (mpdudensity == 0) {
        return ndelim;
    }

    rix = bf->bf_rcs[0].rix;
    flags = bf->bf_rcs[0].flags;
    rc = rt->info[rix].rateCode;
    width = (flags & ATH_RC_CW40_FLAG) ? 1 : 0;
    half_gi = (flags & ATH_RC_SGI_FLAG) ? 1 : 0;

    if (half_gi) {
        nsymbols=NUM_SYMBOLS_PER_USEC_HALFGI(mpdudensity);
    } else {
        nsymbols=NUM_SYMBOLS_PER_USEC(mpdudensity);
    }

    if (nsymbols == 0) {
        nsymbols = 1;
    }

    nsymbits = bits_per_symbol[HT_RC_2_MCS(rc)][width];
    minlen = (nsymbols * nsymbits) / BITS_PER_BYTE;

    /* Is frame shorter than required minimum length? */
    if (frmlen < minlen) {
        /* Get the minimum number of delimiters required. */
        mindelim = (minlen - frmlen) / ATH_AGGR_DELIM_SZ;
        ndelim = MAX(mindelim, ndelim);
    }

    return ndelim;
}

/*
 * For aggregation from software buffer queue.
 * NB: must be called with txq lock held
 */

static ATH_AGGR_STATUS
ath_tx_form_aggr(struct ath_softc *sc, struct ath_atx_tid *tid, 
    ath_bufhead *bf_q, struct ath_buf **bf_last, 
    struct aggr_rifs_param *param, int mimoburst, int *prev_frames)
{
    struct ath_buf *bf, *tbf, *bf_first, *bf_prev = NULL;
    ath_bufhead bf_head;
    int rl = 0, nframes = 0, ndelim;
    u_int16_t aggr_limit = 0, al = 0, bpad = 0, al_delta, h_baw;
    ATH_AGGR_STATUS status = ATH_AGGR_DONE;
    u_int32_t aggr_limit_with_rts = sc->sc_rtsaggrlimit;
    int prev_al = 0, is_ms_rate = 0, legacy = 0;
    int is_ap = (sc->sc_opmode == HAL_M_HOSTAP);
    struct ath_node *an = NULL;
#ifdef ATH_SUPPORT_TxBF
    u_int8_t    is_prev_sounding=0;
#endif
    
#if defined(ATH_SUPPORT_VOWEXT) || ATH_SUPPORT_IQUE_EXT || defined(VOW_LOGLATENCY)
    u_int32_t currts = 0;
    int currts_set = 0;
#endif

#if defined(ATH_SUPPORT_VOWEXT)
    u_int32_t lapsed;
#endif

#if defined(ATH_SUPPORT_VOWEXT) || ATH_SUPPORT_IQUE_EXT
    u_int8_t firstxmit;
    u_int32_t firstxmitts;
#endif

#if UMAC_SUPPORT_SMARTANTENNA
    u_int8_t aggr_antenna = 0;
#endif    

#ifdef ATH_RIFS
    if (sc->sc_txrifs) {
        rl = param->param_rl;
        aggr_limit = param->param_max_len;
    }
#endif

    /*
     * For nodes that have negotiated for a lower BA window, lets attempt
     * to fill the entire BA with a single aggregate.
     */
    h_baw = (tid->baw_size < WME_MAX_BA) ? tid->baw_size : tid->baw_size/2;

    bf_first = TAILQ_FIRST(&tid->buf_q);
    an = tid->an;
#if UMAC_SUPPORT_SMARTANTENNA
    if(wbuf_is_sa_train_packet(bf_first->bf_mpdu)) {    
    aggr_antenna = wbuf_sa_get_antenna(bf_first->bf_mpdu); // fist aggr smart antenna
    } 
#endif    

    /*
     * Get the RTS aggr limit.
     */
    if (mimoburst) {
        DPRINTF(sc, ATH_DEBUG_PWR_SAVE, "%s:forming aggregate for burst\n",
            __func__);
        if (aggr_limit_with_rts) {
            prev_al = aggr_limit_with_rts;
        }
    }

#if defined(ATH_SUPPORT_VOWEXT)
    if( (!ATH_IS_VOWEXT_RCA_ENABLED(sc)) && ATH_IS_VOWEXT_AGGRSIZE_ENABLED(sc)) {
        currts_set = 1;
    }
#endif

#if ATH_SUPPORT_IQUE_EXT
    if(sc->sc_retry_duration || sc->total_delay_timeout) {
        currts_set = 1;
    }
#endif

#if defined(VOW_LOGLATENCY)
    if(sc->loglatency) {
        currts_set = 1;
    }
#endif

#if defined(ATH_SUPPORT_VOWEXT) || ATH_SUPPORT_IQUE_EXT || defined(VOW_LOGLATENCY)
     if(currts_set) {
        currts = ath_hal_gettsf32(sc->sc_ah);
     }
     else {
        currts = 0;
     }
#endif

    do {
        bf = TAILQ_FIRST(&tid->buf_q);
#ifdef ATH_SUPPORT_TxBF       
#define MS(_v, _f)  (((_v) & _f) >> _f##_S)

        //DPRINTF(sc, ATH_DEBUG_ANY,"==>%s:  bf_flags %x ,nframe %d\n",__func__,bf->bf_flags,nframes );

        // it should set sounding frame as first frame of aggr frame , otherwise the sounding frame 
        // related setting in descriptor will not work.
        // 
        if (MS(bf->bf_flags,HAL_TXDESC_TXBF_SOUND)!=0) { // sounding frame ??   
            if (nframes!=0) {// first frame of aggr frame
                break;
            } else {
                is_prev_sounding=1;
            }
        }
        if ((nframes==1)& (is_prev_sounding)){
            break;              // force sounding frame as single framees
        }
#endif

#if UMAC_SUPPORT_SMARTANTENNA
if(wbuf_is_sa_train_packet(bf->bf_mpdu))
{
    /*
     * Limit aggregation for each antenna i.e in general if we send 1 packet to each antenna
     * we can form a full aggr with all packets where antenna number are different, 
     * PER calcualtion will be wrong so form aggr with antenna as a boundry.
     */
   if (aggr_antenna != wbuf_sa_get_antenna(bf->bf_mpdu))
   {
      status = ATH_AGGR_LIMITED;
      break;
   }
}
#endif    
        /*
         * do not step over block-ack window
         */
        if (!BAW_WITHIN(tid->seq_start, tid->baw_size, bf->bf_seqno)) {
            status = ATH_AGGR_BAW_CLOSED;
            DPRINTF(sc, ATH_DEBUG_PWR_SAVE, "%s baw closed seqstart %d baw_size %d bf_seqno %d\n",
                    __func__,tid->seq_start, tid->baw_size, bf->bf_seqno);
            if(bf == bf_first)
            {
                if(tid->baw_head == tid->baw_tail)
                {
                    tid->seq_start=bf->bf_seqno;
                    DPRINTF(sc, ATH_DEBUG_PWR_SAVE, "%s WAR kick in send bar",
                            __func__);
                    ath_bar_tx(sc, bf->bf_node, tid);                   
                }
            }			
            break;
        }

        /*
         * Treat minrate packets specially:
         * For these packets, bf->bf_useminrate is set.
         * Case I: minrate pkt is the first packet
         *  a) Deque from TID q
         *  b) Add to bf_q (for the caller to parse & send)
         *  c) Update tid->seq_start (for next ADDBA)
         *  d) Return with ATH_AGGR_DONE
         * Case II : minrate pkt is somewhere in the middle
         *  a) Break out of loop
         *  b) Return with ATH_AGGR_DONE
         * In Case II, basically let the next call to this function
         * take care of the minrate packet, since it will now end up
         * at the Q head.
         */
        if (unlikely(bf->bf_useminrate)) {

            status = ATH_AGGR_DONE;

            if (bf_first == bf) { /* Case I, per comments */
                bf->bf_next = NULL;
                ath_hal_setdesclink(sc->sc_ah, bf->bf_desc, 0);
                TAILQ_REMOVE_HEAD_UNTIL(&tid->buf_q, &bf_head, bf, bf_list);
                tbf = TAILQ_FIRST(&bf_head);
                tbf->bf_isampdu = 1;
                TAILQ_CONCAT(bf_q, &bf_head, bf_list);
                ASSERT(nframes == 0);
                nframes++;
                ath_tx_addto_baw(sc, tid, bf);
                break;
            } else { /* Case II, per comments */
                break;
            }
        }

        if (!rl) {
            aggr_limit = ath_lookup_rate(sc, tid->an, bf, mimoburst, &legacy);
            rl = 1;
 
            sc->sc_lastdatarix = bf->bf_rcs[0].rix;
            sc->sc_lastrixflags = bf->bf_rcs[0].flags;

            /*
             * Is rate multi stream
             */
            is_ms_rate = (bf->bf_rcs[0].flags & (ATH_RC_DS_FLAG | ATH_RC_TS_FLAG))? 1 : 0;

#if defined(ATH_SUPPORT_VOWEXT) || ATH_SUPPORT_IQUE_EXT || defined(VOW_LOGLATENCY)
            firstxmit = wbuf_get_firstxmit(bf->bf_mpdu);
            
            if(firstxmit) {
                wbuf_set_firstxmitts(bf->bf_mpdu, currts);
                wbuf_set_firstxmit(bf->bf_mpdu, 0);
            }

            firstxmitts = wbuf_get_firstxmitts(bf->bf_mpdu);
#endif

#if ATH_SUPPORT_VOWEXT 
       if ((ATH_IS_VOWEXT_RCA_ENABLED(sc)) || (ATH_IS_VOWEXT_AGGRSIZE_ENABLED(sc)) ) {
           if (!ATH_IS_VOWEXT_RCA_ENABLED(sc)) {
                   lapsed = (currts >= firstxmitts) ? (currts - firstxmitts) : 
                                           ((0xffffffff - firstxmitts) + currts);
                   if(lapsed >= sc->agtb_tlim) { 
                       an->throttle = 10;
                   }
               }
               if (an->throttle ) {
                   an->min_depth = 1;
                   bf->bf_rcs[0].tries = bf->bf_rcs[1].tries = bf->bf_rcs[2].tries = bf->bf_rcs[3].tries = 1;
               } else {
#if ATH_SUPPORT_IQUE
                   int vi_depth = 0;
                   if ( TID_TO_WME_AC(tid->tidno) < WME_AC_VI ){
                       vi_depth = sc->sc_txq[WME_AC_VI].axq_depth;

                       if (vi_depth > 0){ 
                           an->min_depth = min_qdepth_per_ac[WME_AC_VI];
                       } else {
                           an->min_depth = min_qdepth_per_ac[TID_TO_WME_AC(tid->tidno)];
                       }
                   } else {
                       an->min_depth = min_qdepth_per_ac[TID_TO_WME_AC(tid->tidno)];
                   }
#else
                   an->min_depth = ATH_AGGR_MIN_QDEPTH;
#endif
               }
           }
#endif

#ifdef ATH_RIFS
            if (sc->sc_txrifs) {
                param->param_rcs[0] = bf->bf_rcs[0];
                param->param_rcs[1] = bf->bf_rcs[1];
                param->param_rcs[2] = bf->bf_rcs[2];
                param->param_rcs[3] = bf->bf_rcs[3];
                param->param_max_len = aggr_limit;
                param->param_rl = rl;
            }
#endif
        }
#if defined(ATH_SUPPORT_VOWEXT) || ATH_SUPPORT_IQUE_EXT || defined(VOW_LOGLATENCY)
        else {
            if(wbuf_get_firstxmit(bf->bf_mpdu)) {
              wbuf_set_firstxmitts(bf->bf_mpdu, currts);
              wbuf_set_firstxmit(bf->bf_mpdu, 0);
            }
        }
#endif

        /*
         * do not exceed aggregation limit
         */
        al_delta = ATH_AGGR_DELIM_SZ + bf->bf_frmlen;

        /*
         * Check whether the packets are preceded with RTS in the case of
         * dynamic SM/MIMO power save. RTS needs to be applied in multi stream
         * rates only.
         */
        if (is_ap && !mimoburst && is_ms_rate) {
            an = bf_first->bf_node;

            if (an->an_smmode == ATH_SM_PWRSAV_DYNAMIC) {
                if (nframes &&
                (aggr_limit_with_rts < (al + bpad + al_delta))) {
                    status = ATH_AGGR_8K_LIMITED;
                    *prev_frames = nframes;
                    DPRINTF(sc, ATH_DEBUG_PWR_SAVE, "%s "
                        "AGGR_8K_LIMITED %d frames "
                        " al %d bpad %d al_delta %d\n", __func__,
                        nframes, al, bpad, al_delta);
                    break;
                }
            }
        }

        if (nframes && (aggr_limit < (al + bpad + al_delta + prev_al))) {
            status = ATH_AGGR_LIMITED;
            break;
        }

        /*
         * do not exceed subframe limit
         */
#ifdef ATH_RIFS
        if ((nframes +*prev_frames) >= MIN(h_baw, sc->sc_config.ampdu_subframes)
        || (sc->sc_txrifs && ((nframes + *prev_frames) >= 
                param->param_max_frames)))
#else
        if ((nframes + *prev_frames) >= 
        MIN(h_baw, sc->sc_config.ampdu_subframes))
#endif
        {
            status = ATH_AGGR_LIMITED;
            break;
        }

        /*
         * add padding for previous frame to aggregation length
         */
        al += bpad + al_delta;

        /*
         * Get the delimiters needed to meet the MPDU density for this node.
         */
        ndelim = ath_compute_num_delims(sc, bf_first, bf->bf_frmlen, 
                    (nframes == 0));

        /* When the receiver has issue with block ack generation, make
         * sure we add extra two delimiters, provided, only when the other
         * end point require this war.
         */
        if (sc->sc_ieee_ops->node_get_extradelimwar &&
            sc->sc_ieee_ops->node_get_extradelimwar(an->an_node)) {
            ndelim = MAX(ndelim , 2);
        }

        bpad = PADBYTES(al_delta) + (ndelim << 2);

        bf->bf_next = NULL;
        ath_hal_setdesclink(sc->sc_ah, bf->bf_lastfrm->bf_desc, 0);

        /*
         * this packet is part of an aggregate
         * - remove all descriptors belonging to this frame from software queue
         * - add it to block ack window
         * - set up descriptors for aggregation
         */
        TAILQ_REMOVE_HEAD_UNTIL(&tid->buf_q, &bf_head, bf->bf_lastfrm, bf_list);
        tbf = TAILQ_FIRST(&bf_head);
        tbf->bf_isampdu = 1;
        ath_tx_addto_baw(sc, tid, bf);

#ifdef ATH_SWRETRY
        /*
         * this frame was swr eligible, but now become part of aggr
         * in this case we reset the swr related flags
         */
        if (bf->bf_isswretry) {
            struct ath_txq *txq = &sc->sc_txq[tid->ac->qnum];
            struct ieee80211_frame * wh;
            wh = (struct ieee80211_frame *)wbuf_header(bf->bf_mpdu);
            
            ATH_TXBUF_SWRETRY_RESET(bf);
            
            bf->bf_isampdu = 1;

            DPRINTF(sc, ATH_DEBUG_SWR, "%s: AGGR NOW dst=%s SeqCtrl=0x%02X%02X qnum=%d swr_num_eligible_frms=%d\n",
                    __func__, ether_sprintf(wh->i_addr1), wh->i_seq[0], wh->i_seq[1],
                    txq->axq_qnum, (an->an_swretry_info[txq->axq_qnum]).swr_num_eligible_frms);
        }
#endif

        TAILQ_FOREACH(tbf, &bf_head, bf_list) {
            ath_hal_set11n_aggr_middle(sc->sc_ah, tbf->bf_desc, ndelim);
        }

#ifdef ATH_RIFS
        if (sc->sc_txrifs) {
            ATH_SET_TX_SET_NOACK_POLICY(sc, ATH_MPDU_2_QOS_WH(bf->bf_mpdu));
            TAILQ_FOREACH(tbf, &bf_head, bf_list) {
                ath_hal_set11n_aggr_rifs_burst(sc->sc_ah, tbf->bf_desc);
            }
        }
#endif

        /*
         * link buffers of this frame to the aggregate
         */
        TAILQ_CONCAT(bf_q, &bf_head, bf_list);
        nframes ++;

        if (bf_prev) {
            bf_prev->bf_next = bf;
            ath_hal_setdesclink(sc->sc_ah, bf_prev->bf_lastfrm->bf_desc, bf->bf_daddr);
        }
        bf_prev = bf;

        /* 
         * In case of aggregation, if any of the MPDU has RTS/CTS enabled
         * we enable the RTS/CTS for the whole A-MPDU.
         */
        bf_first->bf_flags |= bf->bf_flags & (HAL_TXDESC_RTSENA | HAL_TXDESC_CTSENA);

#if AGGR_NOSHORT
        /*
         * terminate aggregation on a small packet boundary
         */
        if (bf->bf_frmlen < ATH_AGGR_MINPLEN) {
            status = ATH_AGGR_SHORTPKT;
            break;
        }
#endif
    } while (!TAILQ_EMPTY(&tid->buf_q));

    bf_first->bf_al = al;
    bf_first->bf_nframes = nframes;
    *bf_last = bf_prev;

#if ATH_SUPPORT_VOWEXT /* RCA */
#if ATH_SUPPORT_IQUE
    if ( ATH_IS_VOWEXT_RCA_ENABLED(sc) && (nframes <= 3)&& ( (WME_AC_VI == TID_TO_WME_AC(tid->tidno)) || (WME_AC_VO == TID_TO_WME_AC(tid->tidno)) ) \
            && (!legacy)){ 
        bf_first->bf_rcs[0].tries = 1;
        bf_first->bf_rcs[1].tries = 2;
	bf_first->bf_rcs[2].tries = 2;
        bf_first->bf_rcs[3].tries = 4;
        an->min_depth = min_qdepth_per_ac[TID_TO_WME_AC(tid->tidno)];
#ifdef ATH_RIFS
        if (sc->sc_txrifs) {
            param->param_rcs[0].tries = bf_first->bf_rcs[0].tries;
            param->param_rcs[1].tries = bf_first->bf_rcs[1].tries;
            param->param_rcs[2].tries = bf_first->bf_rcs[2].tries;
            param->param_rcs[3].tries = bf_first->bf_rcs[3].tries;
        }
#endif
    }
#endif
#endif

    return status;
}

#ifdef ATH_RIFS

/*
 * ath_tx_get_rifsframe ()
 *
 * Purpose:
 *  - Get a buffer from the tid q. If we don't exceed the length limit,
 *    or overstep the BAW add this to the list of frames in the RIFS burst.
 * Locks:
 *  - none
 *
 */

static INLINE ATH_AGGR_STATUS
ath_tx_get_rifsframe(struct ath_softc *sc, struct ath_atx_tid *tid,
                     ath_bufhead *bf_q, struct ath_buf **bf_last,
                     struct aggr_rifs_param *param, int nframes)
{
    struct ath_buf *bf, *tbf;
    int legacy;
    ATH_AGGR_STATUS status = ATH_AGGR_DONE;
    ath_bufhead bf_head;

    bf = TAILQ_FIRST(&tid->buf_q);

    /*
     * do not step over block-ack window
     */
    if (!BAW_WITHIN(tid->seq_start, tid->baw_size, bf->bf_seqno)) {
        status = ATH_AGGR_BAW_CLOSED;
    } else {
        if (!param->param_rl) {
            param->param_max_len = ath_lookup_rate(sc, tid->an, bf, 0, &legacy);
            param->param_rcs[0] = bf->bf_rcs[0];
            param->param_rcs[1] = bf->bf_rcs[1];
            param->param_rcs[2] = bf->bf_rcs[2];
            param->param_rcs[3] = bf->bf_rcs[3];         
            param->param_rl = 1;
        }

        /*
         * do not exceed length limit
         */
        param->param_al += bf->bf_frmlen;
        if (nframes && (param->param_al > param->param_max_len)) {
            return ATH_AGGR_LIMITED;
        }
 
        /*
         * this packet is part of an RIFS burst
         * - remove all descriptors belonging to this frame from software queue
         * - add it to block ack window
         */
        TAILQ_REMOVE_HEAD_UNTIL(&tid->buf_q, &bf_head,
                                bf->bf_lastfrm, bf_list);
        tbf = TAILQ_FIRST(&bf_head);
        tbf->bf_isampdu = 1;

        ATH_SET_TX_SET_NOACK_POLICY(sc, ATH_MPDU_2_QOS_WH(bf->bf_mpdu));

        ath_tx_addto_baw(sc, tid, bf);
        *bf_last = bf;

        TAILQ_CONCAT(bf_q, &bf_head, bf_list);
        bf->bf_nframes = 1;
    }
    return status;
}


/*
 * ath_tx_form_rifsburst ()
 *
 * Purpose:
 *  - Form a RIFS burst of aggregates or singles.
 * Locks:
 *  - none
 *
 */

static ATH_AGGR_STATUS
ath_tx_form_rifsburst(struct ath_softc *sc, struct ath_atx_tid *tid,
                      ath_bufhead *bf_q, struct ath_buf **bf_last_frame)
{
    int    h_baw, nframes = 0, nrifsubframes = 0;
    int    nframes_rifs_limit, nframes_aggr_limit;
    ATH_AGGR_STATUS status = ATH_AGGR_DONE;
    ath_bufhead bf_head;
    struct ath_buf *bf_last_sub, *bf_first = NULL, *bf_last = NULL,
                   *tbf, *bf_prev = NULL;
    struct ath_rc_series rcs[4];
    struct aggr_rifs_param param = {0, 0, 0, 0, NULL};
    int prev_frames = 0;

    /*
     * For nodes that have negotiated for a lower BA window, lets attempt
     * to fill the entire BA with a single aggregate.
     */
    h_baw = (tid->baw_size < WME_MAX_BA) ? tid->baw_size : tid->baw_size/2;

    nframes_rifs_limit  = MIN(h_baw, sc->sc_config.ampdu_subframes);
    nframes_aggr_limit  = MIN(h_baw, sc->sc_config.ampdu_subframes/sc->sc_config.rifs_ampdu_div);
    param.param_rcs = rcs;

    do {
        TAILQ_INIT(&bf_head);

        if (sc->sc_txaggr) {
            /* Set the max # of frames we want in the aggregate in param */
            param.param_max_frames = MIN(nframes_aggr_limit,
                                         nframes_rifs_limit - nframes);
            /* Form the aggregate */
            status = ath_tx_form_aggr(sc, tid, &bf_head, &bf_last_sub,
                                      &param, 0, &prev_frames);
        } else {
            /* Get a single frame from the tid q */
            status = ath_tx_get_rifsframe(sc, tid, &bf_head, &bf_last_sub,
                                          &param, nframes);

            if (ATH_AGGR_LIMITED == status)
                break;
        }

        /*
         * no frames picked up to be bursted
         */
        if (TAILQ_EMPTY(&bf_head))
            break;

        bf_first = TAILQ_FIRST(&bf_head);
        bf_last = TAILQ_LAST(&bf_head, ath_bufhead_s);

        bf_first->bf_lastbf = bf_last;
        bf_last_sub->bf_next = NULL;
        ath_hal_setdesclink(ah, bf_last->bf_desc, 0);

        /*
         * Copy the rate series acquired from the first lookup into every
         * aggregate/single.
         */
        bf_first->bf_rcs[0] = rcs[0];
        bf_first->bf_rcs[1] = rcs[1];
        bf_first->bf_rcs[2] = rcs[2];
        bf_first->bf_rcs[3] = rcs[3];

        ath_buf_set_rate(sc, bf_first);

        /* Handle the single vs aggregate case when aggregation is enabled */
        if (sc->sc_txaggr) {
            if (bf_first->bf_nframes == 1) {
                TAILQ_FOREACH(tbf, &bf_head, bf_list) {
                    ath_hal_clr11n_aggr(sc->sc_ah, tbf->bf_desc);
                }
            } else {
                bf_first->bf_isaggr = 1;
                ath_hal_set11n_aggr_first(sc->sc_ah, bf_first->bf_desc,
                                          bf_first->bf_al);
                tbf = bf_last_sub;
                do {
                    ath_hal_set11n_aggr_last(sc->sc_ah, tbf->bf_desc);
                    tbf = TAILQ_NEXT(tbf, bf_list);
                } while (tbf != NULL);
            }
        }

        /*
         * Set More RIFS in every descriptor of the last frame in the
         * aggregate or in the lone frame in the non-aggregate case.
         */
        tbf = bf_last_sub;
        do {
            ath_hal_set11n_rifs_burst_middle(sc->sc_ah, tbf->bf_desc);
            tbf = TAILQ_NEXT(tbf, bf_list);
        } while (tbf != NULL);

        TAILQ_CONCAT(bf_q, &bf_head, bf_list);

        /* Link this sub-frame with the prior frames of the RIFS burst */
        if (bf_prev) {
            bf_prev->bf_next = bf_first;
            ath_hal_setdesclink(ah, bf_prev->bf_lastfrm->bf_desc, bf_first->bf_daddr);
        }
        bf_prev = bf_last_sub;

        /*
         * Count every aggregate or single frame as an element within the
         * RIFS burst.
         */
        nrifsubframes++;
        nframes += bf_first->bf_nframes;

    } while (!TAILQ_EMPTY(&tid->buf_q) && (nframes < nframes_rifs_limit) &&
             status != ATH_AGGR_BAW_CLOSED);

    /*
     * no frames picked up to be bursted
     */
    if (TAILQ_EMPTY(bf_q))
        return status;

    bf_first = TAILQ_FIRST(bf_q);
    bf_last_sub = *bf_last_frame = bf_prev;

    bf_first->bf_rifsburst_elem = ATH_RIFS_SUBFRAME_FIRST;
    bf_last_sub->bf_rifsburst_elem = ATH_RIFS_SUBFRAME_LAST;

    /* Clear the More RIFS bit on every descriptor of the last sub-frame */
    tbf = bf_last_sub;
    do {
        ath_hal_set11n_rifs_burst_last(sc->sc_ah, tbf->bf_desc);
        tbf = TAILQ_NEXT(tbf, bf_list);
    } while (tbf != NULL);

    bf_first->bf_nframes = nframes;
    bf_first->bf_nrifsubframes = nrifsubframes;

    return status;
}


/*
 * ath_rifsburst_bar_buf_alloc ()
 *
 * Purpose:
 *  - allocates wbuf
 *  - remove an ath_buf from the free buffer list
 * Locks:
 *  - acquires sc_txbuflock while manipulating the free buffer list
 *
 */
#if ATH_TX_BUF_FLOW_CNTL
static struct ath_buf *
ath_rifsburst_bar_buf_alloc(struct ath_softc *sc, u_int32_t *buf_used)
#else
static struct ath_buf *
ath_rifsburst_bar_buf_alloc(struct ath_softc *sc)
#endif
{
    struct ath_buf *bf = NULL;
    wbuf_t wbuf;
    size_t wbuf_len = sizeof(struct ieee80211_frame_bar);

    /* XXX: allocating with type beacon since it provides single map/unmap */
    wbuf = wbuf_alloc(sc->sc_osdev, WBUF_TX_BEACON, wbuf_len);
    if (NULL == wbuf)
        return bf;

    ATH_TXBUF_LOCK(sc);

    if (!TAILQ_EMPTY(&sc->sc_txbuf)) {
        bf = TAILQ_FIRST(&sc->sc_txbuf);
#if ATH_TX_BUF_FLOW_CNTL
        if (bf) {
	        (*buf_used)++;
            sc->sc_txbuf_free--;
        }
#endif
        TAILQ_REMOVE(&sc->sc_txbuf, bf, bf_list);
    }

    ATH_TXBUF_UNLOCK(sc);

    if (NULL == bf) {
        wbuf_complete(wbuf);
        return bf;
    }

    ATH_TXBUF_RESET(bf, sc->sc_num_txmaps);

    __11nstats(sc, txrifs_bar_alloc);
    bf->bf_mpdu = wbuf;
    bf->bf_frmlen = wbuf_len;

    return bf;
}


/*
 * ath_rifsburst_bar_buf_free ()
 *
 * Purpose:
 *  - unmap the wbuf associated with this ath_buf
 *  - free the wbuf associated with this ath_buf
 * Locks:
 *  -none
 *
 */

static void
ath_rifsburst_bar_buf_free(struct ath_softc *sc, struct ath_buf *bf)
{
    wbuf_t wbuf = (wbuf_t)bf->bf_mpdu;

    __11nstats(sc, txrifs_bar_freed);
    wbuf_unmap_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                      OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
    wbuf_complete(wbuf);
}


/*
 * ath_rifsburst_bar_tx ()
 *
 * Purpose:
 *  - Setup the BAR frame for transmit
 * Locks:
 *  - none
 *
 */

static void
ath_rifsburst_bar_tx(struct ath_softc *sc, ath_atx_tid_t *tid,
                     struct ath_buf *bf, struct ath_buf *bf_last)
{
    wbuf_t                      wbuf = bf->bf_mpdu;
    struct ieee80211_frame_bar  *bar;
    void                        *ds;
    u_int8_t                    minrate = 0x0b;
    HAL_11N_RATE_SERIES         series[4] = {{ 0 }};
    int                         i;
    u_int                       txpower;
    u_int32_t                   smartAntenna = 0;

    bar = (struct ieee80211_frame_bar *) wbuf_header(wbuf);

    /* Inherit frame header fields from the last frame of the RIFS burst */
    OS_MEMCPY(bar, wbuf_header(bf_last->bf_mpdu), bf->bf_frmlen);

    /*
     * form the bar frame
     */
    bar->i_fc[1]  = IEEE80211_FC1_DIR_NODS;
    bar->i_fc[0]  = IEEE80211_FC0_VERSION_0 |
                    IEEE80211_FC0_TYPE_CTL  |
                    IEEE80211_FC0_SUBTYPE_BAR;
    bar->i_ctl    = (tid->tidno << IEEE80211_BAR_CTL_TID_S) |
                                   IEEE80211_BAR_CTL_COMBA;
    bar->i_seq    = htole16(tid->seq_start << IEEE80211_SEQ_SEQ_SHIFT);

    ATH_TXBUF_RESET(bf, sc->sc_num_txmaps);

    bf->bf_seqno  = tid->seq_start;
    bf->bf_tidno = bf_last->bf_tidno;
    bf->bf_node = bf_last->bf_node;
    bf->bf_isaggr  = 0;
    bf->bf_isbar  = bf->bf_isampdu = 0;
    bf->bf_lastbf = bf;
    bf->bf_rifsburst_elem = ATH_RIFS_BAR;
    ath_hal_setdesclink(ah, bf->bf_desc, 0);
    bf->bf_next = NULL;

    /*
     * setup buf
     */
    bf->bf_buf_addr[0] = wbuf_map_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                                     OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));

    /*
     * setup desc
     */
    ds = bf->bf_desc;

    ath_hal_set11n_txdesc(sc->sc_ah, ds
                          , bf->bf_frmlen + IEEE80211_CRC_LEN  /* frame length */
                          , HAL_PKT_TYPE_NORMAL                /* Atheros packet type */
                          , MIN(txctl->txpower, 60)            /* txpower */
                          , HAL_TXKEYIX_INVALID                /* key cache index */
                          , HAL_KEY_TYPE_CLEAR                 /* key type */
                          , HAL_TXDESC_INTREQ
                          | HAL_TXDESC_CLRDMASK /* flags */
                        );

    bf->bf_buf_len[0] = bf->bf_frmlen;
    ath_hal_filltxdesc(ah, ds
                       , bf->bf_buf_addr                        /* buffer address */
                       , bf->bf_buf_len                         /* buffer length */
                       , 0                                      /* descriptor id */
                       , bf->bf_qnum                            /* QCU number */
                       , HAL_KEY_TYPE_CLEAR                     /* key type */
                       , AH_TRUE                                /* first segment */
                       , AH_TRUE                                /* last segment */
                       , ds                                     /* first descriptor */
                       );

   txpower = IS_CHAN_2GHZ(&sc->sc_curchan) ?
       sc->sc_config.txpowlimit2G :
       sc->sc_config.txpowlimit5G;
    for (i = 0; i < 4; i++) {
        series[i].Tries = bf_last->bf_rcs[i].tries;
        if (series[i].Tries) {
            series[i].Rate = minrate;
            series[i].RateIndex = bf->bf_rcs[i].rix;
            series[i].TxPowerCap = txpower;
            series[i].ChSel = ath_txchainmask_reduction(sc,
                    sc->sc_tx_chainmask, series[i].Rate);
        }
    }

#if UMAC_SUPPORT_SMARTANTENNA
    if(sc->sc_smartant_enable)
    {
        /* same default antenna will be used for all rate series */
        smartAntenna = (sc->sc_defant) |(sc->sc_defant << 8)| (sc->sc_defant << 16) | (sc->sc_defant << 24); 
    }
    else    
    {
        smartAntenna = SMARTANT_INVALID; /* if smart antenna is not enabled */
    }
#else
    smartAntenna = SMARTANT_INVALID;
#endif    

    ath_hal_set11n_ratescenario(sc->sc_ah, ds, ds, 0, 0, 0,
                                series, 4, 0, smartAntenna);
}

/*
 * process pending frames possibly doing RIFS bursting
 * NB: must be called with txq lock held
 */

static void
ath_tx_sched_rifs(struct ath_softc *sc, struct ath_txq *txq, ath_atx_tid_t *tid)
{
    struct ath_buf *bf, *tbf, *bf_last, *bf_last_rifs;
    struct ath_buf *bar_bf = NULL;
    ATH_AGGR_STATUS status;
    ath_bufhead bf_q;
#ifdef ATH_SWRETRY
    struct ath_node *an = tid->an;
#endif

    /* Loop through TID buffer queue looking for frames to schedule */
    do {

        if ((sc->sc_enhanceddmasupport) && (txq->axq_depth >= HAL_TXFIFO_DEPTH)) {
            /* Reached the MAX FIFO DEPTH - do not add any more buffer to the HW */
            break;
        }

#ifdef ATH_SWRETRY
        /*
         * If the txq is in filtering status (previous frame, either ampdu or non-ampdu, fails due to XRETRY)
         *   - if there is still swr eligible frame in txq, do not schedule tid
         */
        if (sc->sc_enhanceddmasupport) {
            if ((an->an_swretry_info[txq->axq_qnum]).swr_state_filtering) {
                if ((an->an_swretry_info[txq->axq_qnum]).swr_num_eligible_frms) {
                    break;
                }
            }
        }
#endif

    /* If TID queue is empty, break */
        if (TAILQ_EMPTY(&tid->buf_q))
            break;

        /* If RIFS is configured and no BAR allocated, allocate a BAR */
        if (sc->sc_txrifs && (NULL == bar_bf)) {
#if ATH_TX_BUF_FLOW_CNTL
            bar_bf = ath_rifsburst_bar_buf_alloc(sc, &txq->axq_num_buf_used);
#else
			bar_bf = ath_rifsburst_bar_buf_alloc(sc);
#endif
            /* Return if there is no buf available for BAR */
            if (bar_bf == NULL) {
                return;
            }
        }

        TAILQ_INIT(&bf_q);

        /*
         * Insert into bf_q, one of the following:
         * 1. a RIFS burst
         * 2. a RIFS burst of aggregates
         */
        status = ath_tx_form_rifsburst(sc, tid, &bf_q, &bf_last_rifs);

        /*
         * no frames picked up to be bursted;
         * block-ack window is not open
         */
        if (TAILQ_EMPTY(&bf_q))
            break;

        bf = TAILQ_FIRST(&bf_q);
        bf_last = TAILQ_LAST(&bf_q, ath_bufhead_s);
        bf->bf_lastbf = bf_last;

        /*
         * if only one RIFS sub-frame, send
         */
        if (bf->bf_nrifsubframes == 1) {
            /* Clear No Ack policy set in Qos that is done blindly in
             * ath_tx_form_rifsburst().
             */
            tbf = TAILQ_FIRST(&bf_q);
            do {
                ATH_SET_TX_CLR_NOACK_POLICY(sc,
                                            ATH_MPDU_2_QOS_WH(tbf->bf_mpdu));
                tbf = tbf->bf_next;
            } while (tbf != NULL);

            TAILQ_FOREACH(tbf, &bf_q, bf_list) {
                bf->bf_rifsburst_elem = ATH_RIFS_NONE;
                ath_hal_clr11n_rifs_burst(sc->sc_ah, tbf->bf_desc);
            }

            
            if (ath_tx_txqaddbuf(sc, txq, &bf_q) != 0) {
#ifdef ATH_SUPPORT_TxBF
                ath_tx_complete_buf(sc, bf, &bf_q, 0, 0, 0);
#else
                ath_tx_complete_buf(sc, bf, &bf_q, 0);
#endif
            }
        } else {
            /* If we have allocated a BAR, set it up leveraging information from
             * the last frame in bf_q, bf_last_rifs.
             */
            if (bar_bf && (bf->bf_nrifsubframes > 1)) {
                /* Setup the BAR */
                ath_rifsburst_bar_tx(sc, tid, bar_bf, bf_last_rifs);
                /* Set rifslast: used for rate completion, last ds in RIFS burst */
                bf->bf_rifslast = bf_last;
                /* Link the BAR to the RIFS burst */
                TAILQ_INSERT_TAIL(&bf_q, bar_bf, bf_list);
                ath_hal_setdesclink(ah, bf_last->bf_desc, bar_bf->bf_daddr);
                bf_last_rifs->bf_next = bar_bf;
                bf->bf_lastbf = bar_bf;
                /* Set BAR to NULL as to avoid freeing it below */
                bar_bf = NULL;
            }

            /*
             * queue burst to hardware
             */
            if (ath_tx_txqaddbuf(sc, txq, &bf_q) != 0) {
                printk("--- %s[%d] --- HW Q is full ---\n", __func__, __LINE__);
            }
        }
    } while (txq->axq_depth < ATH_AGGR_MIN_QDEPTH &&
             status != ATH_AGGR_BAW_CLOSED);

    /*
     * Result of allocating a BAR and not tx'ing it.
     */
    if (bar_bf) {
        ath_rifsburst_bar_buf_free(sc, bar_bf);
        ATH_TXBUF_LOCK(sc);
        TAILQ_INSERT_TAIL(&sc->sc_txbuf, bar_bf, bf_list);
#if ATH_TX_BUF_FLOW_CNTL
        txq->axq_num_buf_used--;
        sc->sc_txbuf_free++;
#endif
        ATH_TXBUF_UNLOCK(sc);
#if ATH_SUPPORT_FLOWMAC_MODULE
        if (sc->sc_osnetif_flowcntrl) {
            ath_netif_wake_queue(sc);
        }
#endif
    }
}
#endif

static INLINE u_int32_t
ath_buf_get_ba_period(struct ath_softc *sc, struct ath_buf *bf)
{
    u_int32_t    ba_period;
    u_int8_t    rix, cix;
    const HAL_RATE_TABLE *rt;

    rt = sc->sc_currates;

    /*
     * The second aggregate has only one rate, so use it to get
     * the control rate.
     */
    rix = bf->bf_rcs[0].rix;
    cix = rt->info[rix].controlRate;

    switch (rt->info[cix].rateCode) {
    default:
    case 0x0b: /* 6 Mb OFDM */
        ba_period = 68;
        break;
    case 0x0a: /* 12 Mb OFDM */
        ba_period = 44;
        break;
    case 0x09: /* 24 Mb OFDM */
        ba_period = 32;
        break;
    }

    return (ba_period);
}

/*
 * Do MIMO Bursting. Form two aggregates and hand-over to the hardware.
 */
static void
ath_tx_sched_mimoburst(struct ath_softc *sc, struct ath_txq *txq, 
    ath_atx_tid_t *tid, struct aggr_rifs_param *param, 
    ath_bufhead *bf_qfirst, int *prev_frames)
{
    int burst_duration = 0;
    struct ath_buf *bf_firstaggr, *bf, *bf_last, *tbf, *bf_lastaggr = NULL;
    ath_bufhead bf_q;
#ifdef ATH_SWRETRY
    struct ath_node *an = tid->an;
#endif

    bf_firstaggr = TAILQ_FIRST(bf_qfirst);
    TAILQ_INIT(&bf_q);

    do {
        if ((sc->sc_enhanceddmasupport) && (txq->axq_depth >= HAL_TXFIFO_DEPTH)) {
            /* Reached the MAX FIFO DEPTH - do not add any more buffer to the HW */
            break;
        }

#ifdef ATH_SWRETRY
        /*
         * If the txq is in filtering status (previous frame, either ampdu or non-ampdu, fails due to XRETRY)
         *   - if there is still swr eligible frame in txq, do not shcedule tid
         */
        if (sc->sc_enhanceddmasupport) {
            if ((an->an_swretry_info[txq->axq_qnum]).swr_state_filtering) {
                if ((an->an_swretry_info[txq->axq_qnum]).swr_num_eligible_frms) {
                    break;
                }
            }
        }
#endif

        if (TAILQ_EMPTY(&tid->buf_q))
            break;

        ath_tx_form_aggr(sc, tid, &bf_q, &bf_lastaggr, param, 1, prev_frames);

        /*
         * no frames picked up to be aggregated; block-ack window is not open
         */
        if (TAILQ_EMPTY(&bf_q))
            break;

        bf = TAILQ_FIRST(&bf_q);
        bf_last = TAILQ_LAST(&bf_q, ath_bufhead_s);
        bf->bf_lastbf = bf_last;

        /*
         * if only one frame, send as non-aggregate
         */
        if (bf->bf_nframes == 1) {
            ASSERT(bf->bf_lastfrm == bf_last);

            bf->bf_isaggr = 0;
            /*
             * clear aggr bits for every descriptor
             * XXX TODO: is there a way to optimize it?
             */
            TAILQ_FOREACH(tbf, &bf_q, bf_list) {
                ath_hal_clr11n_aggr(sc->sc_ah, tbf->bf_desc);
            }
        } else {
            /*
             * setup first desc with rate and aggr info
             */
            bf->bf_isaggr  = 1;
            ath_hal_set11n_aggr_first(sc->sc_ah, bf->bf_desc, bf->bf_al);

            /*
             * anchor last frame of aggregate correctly
             */
            ASSERT(bf_lastaggr);
            ASSERT(bf_lastaggr->bf_lastfrm == bf_last);
            tbf = bf_lastaggr;
            do {
                ath_hal_set11n_aggr_last(sc->sc_ah, tbf->bf_desc);
                tbf = TAILQ_NEXT(tbf, bf_list);
            } while (tbf != NULL);

            txq->axq_aggr_depth++;
        }

        /* Tag the second frame in the burst */
        bf->bf_aggrburst = 1;

        /*
         * 1 try, 1 rate
         */
        bf->bf_rcs[0].tries = 1;
        bf->bf_rcs[1].tries = bf->bf_rcs[2].tries = 
            bf->bf_rcs[3].tries = 0;
        bf->bf_rcs[1].rix = bf->bf_rcs[2].rix = bf->bf_rcs[3].rix = 0;

        /*
         * Remove RTS enable flags if present.
         */
        bf->bf_flags &= ~(HAL_TXDESC_RTSENA);

        ath_buf_set_rate(sc, bf);
        /*
         * Burst duration is calculated based on the duration of second aggr.
         */
        burst_duration = ath_pkt_duration(sc, bf->bf_rcs[0].rix, bf,
            (bf->bf_rcs[0].flags & ATH_RC_CW40_FLAG) != 0,
            (bf->bf_rcs[0].flags & ATH_RC_SGI_FLAG), bf->bf_shpreamble);

        /*
         * Protect the bar and SIFSs for the second aggr.
         */
#define    OFDM_SIFS_TIME    16
        burst_duration += ath_buf_get_ba_period(sc, bf) +
            2 * OFDM_SIFS_TIME;

        /*
         * Set the burst duration and vmf required in the first aggr.
         */
        ath_hal_set11n_burstduration(sc->sc_ah, bf_firstaggr->bf_desc, 
            burst_duration);
        ath_hal_set11n_virtualmorefrag(sc->sc_ah, \
                bf_firstaggr->bf_desc,1);
        bf->bf_aggrburst = 0;

    } while (0);

    /*
     * queue both aggregates to hardware
     */
    DPRINTF(sc, ATH_DEBUG_PWR_SAVE, "%s Queuing both the aggrs\n",
        __func__);

    if (ath_tx_txqaddbuf(sc, txq, bf_qfirst) != 0) {
        printk("--- %s[%d] --- HW Q Full, Could not add Aggr to HW -- Not being Freed !! LEAK !! \n", __func__, __LINE__);
    }
    if (!TAILQ_EMPTY(&bf_q)) {
        if (ath_tx_txqaddbuf(sc, txq, &bf_q) != 0) {
            printk("--- %s[%d] --- HW Q Full, Could not add Aggr to HW -- Not being Freed !! LEAK !! \n", __func__, __LINE__);
        }
    }
}

/*
 * process pending HT single or legacy frames
 * NB: must be called with txq lock held
 */
INLINE void
ath_tx_sched_normal(struct ath_softc *sc, struct ath_txq *txq, ath_atx_tid_t *tid)
{
    struct ath_buf *bf, *tbf, *bf_last;
    u_int16_t status = 0;
    int rl = 0, legacy = 0;
    struct ath_rc_series rcs[4];
    ath_bufhead bf_q;
#ifdef ATH_SWRETRY
    struct ath_node *an = tid->an;
#endif

    OS_MEMZERO(rcs, sizeof(rcs));

    do {

        if ((sc->sc_enhanceddmasupport) && (txq->axq_depth >= HAL_TXFIFO_DEPTH)) {
            /* Reached the MAX FIFO DEPTH - do not add any more buffer to the HW */
            break;
        }

#ifdef ATH_SWRETRY
        /*
         * If the txq is in filtering status (previous frame, either ampdu or non-ampdu, fails due to XRETRY)
         *   - if there is still swr eligible frame in txq, do not shcedule tid
         */
        if (sc->sc_enhanceddmasupport) {
            if ((an->an_swretry_info[txq->axq_qnum]).swr_state_filtering) {
                if ((an->an_swretry_info[txq->axq_qnum]).swr_num_eligible_frms) {
                    DPRINTF(sc, ATH_DEBUG_SWR, "%s: HWQ %d in filtering status but still more frame needed to be filtered\n",
                            __func__, txq->axq_qnum);
                    break;
                }
            }
        }   
#endif

        if (TAILQ_EMPTY(&tid->buf_q))
            break;

        TAILQ_INIT(&bf_q);

        bf = TAILQ_FIRST(&tid->buf_q);

        /* 
         * Perform only one rate lookup, even if we are queuing multiple frames.
         */ 
        if (bf->bf_isdata && !bf->bf_useminrate && !bf->bf_ismcast) {
            if (!rl) {
                status = ath_lookup_rate(sc, tid->an, bf, 0, &legacy);
                rl = 1;

                rcs[0] = bf->bf_rcs[0];
                rcs[1] = bf->bf_rcs[1];
                rcs[2] = bf->bf_rcs[2];
                rcs[3] = bf->bf_rcs[3];
            } else {
                bf->bf_rcs[0] = rcs[0];
                bf->bf_rcs[1] = rcs[1];
                bf->bf_rcs[2] = rcs[2];
                bf->bf_rcs[3] = rcs[3];
            }
        }

        bf->bf_next = NULL;
        ath_hal_setdesclink(sc->sc_ah, bf->bf_lastfrm->bf_desc, 0);

        /*
         * this packet will be send as a single
         * - remove all descriptors belonging to this frame from software queue
         */
        TAILQ_REMOVE_HEAD_UNTIL(&tid->buf_q, &bf_q, bf->bf_lastfrm, bf_list);

        bf = TAILQ_FIRST(&bf_q);
        bf_last = TAILQ_LAST(&bf_q, ath_bufhead_s);
        bf->bf_lastbf = bf_last;

        if (bf->bf_ht) {
            if (bf->bf_isampdu) {
                /* It is possible that previous ampdu fails and 
                 * now scheduled as legacy frame
                 */
                int index, cindex;
                index  = ATH_BA_INDEX(tid->seq_start, bf->bf_seqno);
                cindex = (tid->baw_head + index) & (ATH_TID_MAX_BUFS - 1);
                if (TX_BUF_BITMAP_IS_SET(tid->tx_buf_bitmap, cindex) && bf->bf_isampdu) {
                    ath_tx_update_baw(sc, tid, bf->bf_seqno);
                } else {
                    /* These AMPDUs are in TID but never scheduled.
                     * seq_next already incremented in __ath_tx_prepare.
                     */
                    INCR(tid->seq_start, IEEE80211_SEQ_MAX);
                }
            } else {
                /* update starting sequence number for subsequent ADDBA request */
                INCR(tid->seq_start, IEEE80211_SEQ_MAX);
            }
        }

        if (legacy)
            __11nstats(sc, tx_legacy);
        else
            __11nstats(sc, txunaggr_single);

        ASSERT(bf->bf_lastfrm == bf_last);

        bf->bf_isaggr = bf->bf_isampdu = 0;
        /*
         * clear aggr bits for every descriptor
         * XXX TODO: is there a way to optimize it?
         */
        TAILQ_FOREACH(tbf, &bf_q, bf_list) {
            ath_hal_clr11n_aggr(sc->sc_ah, tbf->bf_desc);
        }

#ifdef ATH_SWRETRY
        if ((tid->an->an_flags & ATH_NODE_PWRSAVE) && tid->an->an_pspoll_pending) {
            if (ath_exist_pendingfrm_tidq((ath_dev_t)sc, (ath_node_t)tid->an) == 0 ||
                sc->sc_ieee_ops->get_pwrsaveq_len(tid->an->an_node)) {
                struct ieee80211_frame  *wh = (struct ieee80211_frame *)wbuf_header(bf->bf_mpdu);
                wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
            }
        }
#endif
        ath_buf_set_rate(sc, bf);
        if (ath_tx_txqaddbuf(sc, txq, &bf_q) != 0) {
            printk("ath_tx_sched_normal: txqadd failed for single\n");
#ifdef ATH_SUPPORT_TxBF
                ath_tx_complete_buf(sc, bf, &bf_q, 0, 0, 0);
#else
                ath_tx_complete_buf(sc, bf, &bf_q, 0);
#endif
        }
#ifdef ATH_SWRETRY
        if (tid->an->an_tim_set==AH_TRUE)
    	{
            if ((tid->an->an_flags & ATH_NODE_PWRSAVE) && (sc->sc_ieee_ops->get_pwrsaveq_len(tid->an->an_node)==0) && (ath_exist_pendingfrm_tidq((ath_dev_t) sc,tid->an)!=0))
            {
                ATH_NODE_SWRETRY_TXBUF_LOCK(tid->an);
                sc->sc_ieee_ops->set_tim(tid->an->an_node,0);
                tid->an->an_tim_set=AH_FALSE;	
                ATH_NODE_SWRETRY_TXBUF_UNLOCK(tid->an);		
            }
            else if (!(tid->an->an_flags & ATH_NODE_PWRSAVE) && (sc->sc_ieee_ops->get_pwrsaveq_len(tid->an->an_node)==0))
            {
                ATH_NODE_SWRETRY_TXBUF_LOCK(tid->an);
                sc->sc_ieee_ops->set_tim(tid->an->an_node,0);
                tid->an->an_tim_set=AH_FALSE;			
                ATH_NODE_SWRETRY_TXBUF_UNLOCK(tid->an);
	    }
	}
        /* send only one frame upon pspoll */
        if ((tid->an->an_flags & ATH_NODE_PWRSAVE) && tid->an->an_pspoll_pending) {
            ATH_NODE_SWRETRY_TXBUF_LOCK(tid->an);
            tid->an->an_pspoll_pending = AH_FALSE;
            ATH_NODE_SWRETRY_TXBUF_UNLOCK(tid->an);
            break;
        }
#endif
    } while (txq->axq_depth < ATH_SINGLES_MIN_QDEPTH && (!atomic_read(&sc->sc_in_reset)));

}
/*
 * process pending frames possibly doing a-mpdu aggregation
 * NB: must be called with txq lock held
 */
static INLINE void
ath_tx_sched_aggr(struct ath_softc *sc, struct ath_txq *txq, ath_atx_tid_t *tid)
{
    struct ath_buf *bf, *tbf, *bf_last, *bf_lastaggr = NULL;
    ATH_AGGR_STATUS status;
    ath_bufhead bf_q;
    struct aggr_rifs_param param = {0, 0, 0, 0, NULL};
    int prev_frames = 0;
    /* For out-of-BAW error condition handling */
    struct ath_buf *bft;
    ath_bufhead bf_headt;
    
#if ATH_SUPPORT_VOWEXT
    struct ath_node *an = (struct ath_node *) tid->an;
    u_int8_t min_depth = (ATH_IS_VOWEXT_AGGRSIZE_ENABLED(sc) || ATH_IS_VOWEXT_RCA_ENABLED(sc)) ? an->min_depth : tid->min_depth;
#else
    u_int8_t min_depth = tid->min_depth;
#ifdef ATH_SWRETRY
    struct ath_node *an = (struct ath_node *) tid->an;
#endif
#endif

    u_int8_t condition = 0;

    do {

        if ((sc->sc_enhanceddmasupport) && (txq->axq_depth >= HAL_TXFIFO_DEPTH)) {
            /* Reached the MAX FIFO DEPTH - do not add any more buffer to the HW */
            break;
        }

#ifdef ATH_SWRETRY
        /*
         * If the txq is in filtering status (previous frame, either ampdu or non-ampdu, fails due to XRETRY)
         *   - if there is still swr eligible frame in txq, do not shcedule tid
         */
        if (sc->sc_enhanceddmasupport) {
            if ((an->an_swretry_info[txq->axq_qnum]).swr_state_filtering) {
                if ((an->an_swretry_info[txq->axq_qnum]).swr_num_eligible_frms) {
                    DPRINTF(sc, ATH_DEBUG_SWR, "%s: HWQ %d in filtering status but still more frame needed to be filtered\n",
                            __func__, txq->axq_qnum);
                    break;
                }
            }
        }
#endif

        if (TAILQ_EMPTY(&tid->buf_q))
            break;

        TAILQ_INIT(&bf_q);

        status = ath_tx_form_aggr(sc, tid, &bf_q, &bf_lastaggr, &param,
        0, &prev_frames);
        /*
	if (status == ATH_AGGR_BAW_CLOSED) {
	    bft = TAILQ_FIRST(&tid->buf_q);
	    printk("%s: tid_start %d\n", __func__, tid->seq_start);
	    if (bft)
		printk("bft->bf_seqno = %d\n", bft->bf_seqno);
	}
        */

        /*
         * Error condition: BA window start sequence number has advanced
         * beyond the first frame in the TID queue.
         * Software should not get into this state and will not recover
         * from this condition.
         * The condition checks if the frame at the head of the queue is within the BAW.
         * The frame at the head of the queue should never be beyond BAW.
         * The condition also checks that form_aggr return BAW_CLOSED, and
         * no frames were picked up for aggregation.
         * The queue will be stuck, so drop the frame.
         */
        if (!TAILQ_EMPTY(&tid->buf_q)) {
            bft = TAILQ_FIRST(&tid->buf_q);
            if ((status == ATH_AGGR_BAW_CLOSED) && (TAILQ_EMPTY(&bf_q)) &&
                (!BAW_WITHIN(tid->seq_start, tid->baw_size, bft->bf_seqno)) &&
                /* in front of BAW */
                (!BAW_WITHIN(tid->seq_start, IEEE80211_SEQ_MAX/2, bft->bf_seqno))) {
                printk("%s: Recovering from error condition: Flushing TID queue until head \
                       is within the window\n", __func__);
                printk("%s: TID[%d] seq_start:%d seq_next:%d Head:%d Tail:%d BAW size:%d seqno:%d\n",
                        __func__, tid->tidno, tid->seq_start, tid->seq_next, tid->baw_head,
                        tid->baw_tail, tid->baw_size, bft->bf_seqno);

                /*
                 * Drop one frame, subsequent schedules will drain the queue 
                 * until the condition no longer persists.
                 */
                TAILQ_REMOVE_HEAD_UNTIL(&tid->buf_q, &bf_headt, bft->bf_lastfrm, bf_list);
#ifdef ATH_SUPPORT_TxBF
                ath_tx_complete_buf(sc, bft, &bf_headt, 0, 0, 0);
#else
                ath_tx_complete_buf(sc, bft, &bf_headt, 0);
#endif
            }
        }

        /*
         * no frames picked up to be aggregated; block-ack window is not open
         */
        if (TAILQ_EMPTY(&bf_q))
            break;

        bf = TAILQ_FIRST(&bf_q);
        bf_last = TAILQ_LAST(&bf_q, ath_bufhead_s);
        bf->bf_lastbf = bf_last;

        /*
         * if only one frame, send as non-aggregate
         */
        if (bf->bf_nframes == 1) {
            __11nstats(sc, txaggr_single);
            ASSERT(bf->bf_lastfrm == bf_last);

            bf->bf_isaggr = 0;
            /*
             * clear aggr bits for every descriptor
             * XXX TODO: is there a way to optimize it?
             */
            TAILQ_FOREACH(tbf, &bf_q, bf_list) {
                ath_hal_clr11n_aggr(sc->sc_ah, tbf->bf_desc);
            }

#ifdef ATH_SWRETRY
        if ((tid->an->an_flags & ATH_NODE_PWRSAVE) && tid->an->an_pspoll_pending) {
            if (ath_exist_pendingfrm_tidq((ath_dev_t)sc, (ath_node_t)tid->an) == 0 ||
                sc->sc_ieee_ops->get_pwrsaveq_len(tid->an->an_node)) {
                struct ieee80211_frame  *wh = (struct ieee80211_frame *)wbuf_header(bf->bf_mpdu);
                wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
            }
        }
#endif

            ath_buf_set_rate(sc, bf);
            if (ath_tx_txqaddbuf(sc, txq, &bf_q) != 0) {
                ath_tx_update_baw(sc, tid, bf->bf_seqno);
#ifdef ATH_SUPPORT_TxBF
                ath_tx_complete_buf(sc, bf, &bf_q, 0, 0, 0);
#else
                ath_tx_complete_buf(sc, bf, &bf_q, 0);
#endif
                printk("ath_tx_sched_aggr: txqadd failed for single\n");
            }
        } else {
            /*
             * setup first desc with rate and aggr info
             */
            bf->bf_isaggr  = 1;
            ath_hal_set11n_aggr_first(sc->sc_ah, bf->bf_desc, bf->bf_al);
            ath_buf_set_rate(sc, bf);

            __11nstats(sc,tx_aggregates);
            __11nstatsn(sc,tx_aggr_frames,bf->bf_nframes);

            /*
             * anchor last frame of aggregate correctly
             */
            ASSERT(bf_lastaggr);
            ASSERT(bf_lastaggr->bf_lastfrm == bf_last);
            tbf = bf_lastaggr;
            do {
                ath_hal_set11n_aggr_last(sc->sc_ah, tbf->bf_desc);
                tbf = TAILQ_NEXT(tbf, bf_list);
            } while (tbf != NULL);

            txq->axq_aggr_depth++;

            if (status == ATH_AGGR_8K_LIMITED) {
                /*
                 * Mimo Burst, Queue aggregate in pairs
                 */
                ath_tx_sched_mimoburst(sc, txq, tid, &param, &bf_q, &prev_frames);
                __11nstats(sc, txaggr_mimo);
            } else {
                /*
                 * Normal aggregate, queue to hardware
                 */
                if (ath_tx_txqaddbuf(sc, txq, &bf_q) != 0) {
                    printk("ath_tx_sched_aggr: txqadd failed for aggregate\n");
                }
            }
        }

#ifdef ATH_SWRETRY
        if (tid->an->an_tim_set==AH_TRUE)
    	{
            if ((tid->an->an_flags & ATH_NODE_PWRSAVE) && (sc->sc_ieee_ops->get_pwrsaveq_len(tid->an->an_node)==0) && (ath_exist_pendingfrm_tidq((ath_dev_t) sc,tid->an)!=0))
            {
                ATH_NODE_SWRETRY_TXBUF_LOCK(tid->an);
                sc->sc_ieee_ops->set_tim(tid->an->an_node,0);
                tid->an->an_tim_set=AH_FALSE;	
                ATH_NODE_SWRETRY_TXBUF_UNLOCK(tid->an);		
            }
            else if (!(tid->an->an_flags & ATH_NODE_PWRSAVE) && (sc->sc_ieee_ops->get_pwrsaveq_len(tid->an->an_node)==0))
            {
                ATH_NODE_SWRETRY_TXBUF_LOCK(tid->an);
                sc->sc_ieee_ops->set_tim(tid->an->an_node,0);
                tid->an->an_tim_set=AH_FALSE;			
                ATH_NODE_SWRETRY_TXBUF_UNLOCK(tid->an);
	    }
	}

        /* send only one frame upon pspoll */
        if ((tid->an->an_flags & ATH_NODE_PWRSAVE) && tid->an->an_pspoll_pending) {
            ATH_NODE_SWRETRY_TXBUF_LOCK(tid->an);
            tid->an->an_pspoll_pending = AH_FALSE;
            ATH_NODE_SWRETRY_TXBUF_UNLOCK(tid->an);
            break;
        }
#endif

        condition = (txq->axq_depth  < min_depth ) && (status != ATH_AGGR_BAW_CLOSED);

    } while ( condition && (!atomic_read(&sc->sc_in_reset)));
}

#ifdef VOW_TIDSCHED
void
ath_wrr_schedule(struct ath_softc *sc)
{
    struct ath_atx_tid *tid;
    struct ath_atx_ac *ac;
    struct ath_txq *txq;
    TAILQ_HEAD(,ath_atx_tid)paused_tid_q;
    TAILQ_HEAD(,ath_atx_tid)paused_sc_tid_q;
    TAILQ_INIT(&paused_tid_q);
    TAILQ_INIT(&paused_sc_tid_q);

    /*
     * process a single tid per destination
     */
    do {
        tid = TAILQ_FIRST(&sc->tid_q);
        if (tid == NULL) {
            /* nothing to schedule */
            __11nstats(sc, tx_schednone);
            return;
        }
        ac = tid->ac;
        if (ac == NULL) {
            /* nothing to schedule */
            __11nstats(sc, tx_schednone);
            return;
        }

        txq = &sc->sc_txq[tid->ac->qnum];
        ATH_TXQ_LOCK(txq);

        TAILQ_REMOVE(&ac->tid_q, tid, tid_qelem);
        if(tid->ac->qnum<HAL_NUM_DATA_QUEUES) {
          TAILQ_REMOVE(&sc->tid_q, tid, wrr_tid_qelem);
        }
        tid->sched = AH_FALSE;

        if (tid->paused) {   /* check next tid to keep h/w busy */
            TAILQ_INSERT_TAIL(&paused_tid_q, tid, tid_qelem);
            TAILQ_INSERT_TAIL(&paused_sc_tid_q, tid, wrr_tid_qelem);
            tid->sched = AH_TRUE;
            ATH_TXQ_UNLOCK(txq);
            continue;
	    }

        /*
         * schedule rifs or aggregation for this tid
         */
#ifdef ATH_RIFS
        if (sc->sc_txrifs)
            ath_tx_sched_rifs(sc, txq, tid);
        else
#endif
        if (!(tid->an->an_smmode == ATH_SM_PWRSAV_DYNAMIC) ||
            ((txq->axq_depth % 2) == 0)) {
            if(ath_aggr_query(tid)) {
              ath_tx_sched_aggr(sc, txq, tid);
            } else {
             /* In ath_tx_send_normal() or ath_tx_sched_normal() we INCR(
              * tid->seq_start) before queueing to h/w. While it is safe to do
              * so from send_normal(), it might not be from sched_normal(). The
              * later may queue to h/w even if already something in there in the
              * h/w queue, i.e. !txq->axq_depth. If this something happens to be
              * an AMPDU (after an HT STA sends DELBA), tid->seq_start could be
              * lagging behind. Adding the baw_head == baw_tail check here makes
              * sure that no APMDUs of this tid are queued to h/w, so that
              * seq_start can be safely incremented in sched_normal(). */
              if (tid->baw_head == tid->baw_tail ||
                  !txq->axq_depth) { //The check for axq_depth is precautionary
                  ath_tx_sched_normal(sc, txq, tid);
              } else {
                  TAILQ_INSERT_TAIL(&paused_tid_q, tid, tid_qelem);
                  TAILQ_INSERT_TAIL(&paused_sc_tid_q, tid, wrr_tid_qelem);
                  tid->sched = AH_TRUE;
                  ATH_TXQ_UNLOCK(txq);
                  continue;
              }
            }
        }

        /*
         * add tid to round-robin queue if more frames are pending for the tid
         */
        if (!TAILQ_EMPTY(&tid->buf_q)) {
            ath_tx_queue_tid(sc, txq, tid);
        }

        /* only schedule one TID at a time */
        ATH_TXQ_UNLOCK(txq);
        break;
    } while (!TAILQ_EMPTY(&sc->tid_q));

    if (!TAILQ_EMPTY(&paused_tid_q)) {
        TAILQ_INSERTQ_HEAD(&ac->tid_q, &paused_tid_q, tid_qelem);
        TAILQ_INSERTQ_HEAD(&sc->tid_q, &paused_sc_tid_q, wrr_tid_qelem);
    }
}
#endif

#if ATH_SUPPORT_VOWEXT
static INLINE struct ath_atx_ac *
vsp_schedule(struct ath_softc *sc, struct ath_txq *txq, struct ath_atx_ac *first_schd_ac)
{
    struct ath_atx_ac *ac;

	/*
     * get the first node/ac pair on the queue
     */
    ac = TAILQ_FIRST(&txq->axq_acq);
    if (ac == NULL) {
        /* nothing to schedule */
        return NULL;
    }
    
    if (ac == first_schd_ac) {
        /* ac already scheduled */
        return NULL;
    }

	/* Apply the scheduling penality (if there) in case vsp enabled & TXQ is Video queue */
	if((sc->vsp_enable) && (ac->qnum == ath_tx_get_qnum(sc, HAL_TX_QUEUE_DATA, HAL_WME_AC_VI)))
	{
		struct ath_atx_ac *firstac = NULL;

        TAILQ_REMOVE(&txq->axq_acq, ac, ac_qelem);

		if(TAILQ_EMPTY(&txq->axq_acq))
		{
			/* Q is having only one AC so, schedule it irrespective of schedule penality count */
			ac->sched = AH_FALSE;
		}
		else
		{
			firstac = ac;
			do
			{
				if(ac->max_sch_penality)
				{
					if(ac->sch_penality_cnt)
					{	/* This ac is not suitable for current scheduling, reduce the penality counter and re queue this ac */
						ac->sch_penality_cnt--;
						TAILQ_INSERT_TAIL(&txq->axq_acq, ac, ac_qelem);
					}
					else
					{
						/* This ac is suitable for schediling now, reset the scheduling penality for next scheduling */
						ac->sch_penality_cnt = ac->max_sch_penality;
						break;
					}
				}
				else
				{
					/* Got an ac suitable for scheduling */
					break;
				}
                
                /* Get the first ac/node pair */
                ac = TAILQ_FIRST(&txq->axq_acq);
                if( ac == first_schd_ac){
                    return NULL;
                }
				TAILQ_REMOVE(&txq->axq_acq, ac, ac_qelem);  /* no need to check for ac==null */
			}
			while (ac != firstac);

            ac->sched = AH_FALSE;
		}
	}
	else
	{
		TAILQ_REMOVE(&txq->axq_acq, ac, ac_qelem);
		ac->sched = AH_FALSE;
	}

	return ac;
}
#endif

/*
 * Tx scheduling logic
 * NB: must be called with txq lock held
 */
void
ath_txq_schedule(struct ath_softc *sc, struct ath_txq *txq)
{
    struct ath_atx_ac *ac;
    struct ath_atx_tid *tid = NULL;
    TAILQ_HEAD(,ath_atx_tid)paused_tid_q;
#ifdef VOW_TIDSCHED
    TAILQ_HEAD(,ath_atx_tid)paused_sc_tid_q;
#endif
#if ATH_SUPPORT_VOWEXT
    struct ath_atx_ac *first_schd_ac = NULL;
    int if_first_ac = 1;

#if ATH_SUPPORT_IQUE
    /* to protect priority inversion between VI and BE stream,
     * whenever there are packets in VI HW queue, limit the BE/BK
     * HW queue depth to video queue depth.
     */
    if ( ATH_IS_VOWEXT_RCA_ENABLED(sc) ){
        if ( (txq->axq_depth >= min_qdepth_per_ac[WME_AC_VI]) && 
                ((sc->sc_haltype2q[HAL_WME_AC_BE] == txq->axq_qnum) || 
            (sc->sc_haltype2q[HAL_WME_AC_BK] == txq->axq_qnum)) ){
            if ( sc->sc_txq[WME_AC_VI].axq_depth > 0){
               return; 
            }
        }
    }
#endif

    do {

#endif

        TAILQ_INIT(&paused_tid_q);
#ifdef VOW_TIDSCHED
        TAILQ_INIT(&paused_sc_tid_q);
#endif


#if ATH_SUPPORT_VOWEXT
        ac = vsp_schedule(sc, txq, first_schd_ac);
        if (ac == NULL) {
            /* nothing to schedule */
            
            if( first_schd_ac == NULL ){
                __11nstats(sc, tx_schednone);
            }
            return;
        }
        if (if_first_ac) {
            first_schd_ac = ac; 
            if_first_ac = 0;
        }
#else
        /*
         * get the first node/ac pair on the queue
         */
        ac = TAILQ_FIRST(&txq->axq_acq);
        if (ac == NULL) {
            /* nothing to schedule */
            __11nstats(sc, tx_schednone);
            return;
        }

        TAILQ_REMOVE(&txq->axq_acq, ac, ac_qelem);
        ac->sched = AH_FALSE;
#endif

        /*
         * process a single tid per destination
         */
        while ( !TAILQ_EMPTY(&ac->tid_q) ){

            tid = TAILQ_FIRST(&ac->tid_q);
            TAILQ_REMOVE(&ac->tid_q, tid, tid_qelem);
#ifdef VOW_TIDSCHED
	        if(tid->ac->qnum<HAL_NUM_DATA_QUEUES) {
                TAILQ_REMOVE(&sc->tid_q, tid, wrr_tid_qelem);
            }
#endif
            tid->sched = AH_FALSE;

            /* Upon PS Poll, skip here and schedule tid when paused */
#ifdef ATH_SWRETRY			
            if (tid->paused && !tid->an->an_pspoll_pending) {   /* check next tid to keep h/w busy */
#else
            if (tid->paused) {	/* check next tid to keep h/w busy */
#endif
                TAILQ_INSERT_TAIL(&paused_tid_q, tid, tid_qelem);
#ifdef VOW_TIDSCHED
	            if(tid->ac->qnum<HAL_NUM_DATA_QUEUES) {
                    TAILQ_INSERT_TAIL(&paused_sc_tid_q, tid, wrr_tid_qelem);
                }
#endif
                tid->sched = AH_TRUE;
                continue;
            }

            /*
             * schedule rifs or aggregation for this tid
             */
#if ATH_RIFS
            if (sc->sc_txrifs)
                ath_tx_sched_rifs(sc, txq, tid);
            else
#endif
                if (!(tid->an->an_smmode == ATH_SM_PWRSAV_DYNAMIC) ||
                        ((txq->axq_depth % 2) == 0)) {
                    if (ath_aggr_query(tid)) {
#ifdef VOW_DEBUG
                        printk("RRSA: %d\n", tid->ac->qnum);
#endif
                        ath_tx_sched_aggr(sc, txq, tid);
                    } else {
                        if (tid->baw_head == tid->baw_tail ||
                            txq->axq_depth == 0) {
#ifdef VOW_DEBUG
                            printk("RRSN: %d\n", tid->ac->qnum);
#endif
                            ath_tx_sched_normal(sc, txq, tid);
                        } else {
                            TAILQ_INSERT_TAIL(&paused_tid_q, tid, tid_qelem);
#ifdef VOW_TIDSCHED
                            if (tid->ac->qnum<HAL_NUM_DATA_QUEUES) {
                                TAILQ_INSERT_TAIL(&paused_sc_tid_q, tid, wrr_tid_qelem);
                            }
#endif
                            tid->sched = AH_TRUE;
                            continue;
                        }
                    }
                }

            /*
             * add tid to round-robin queue if more frames are pending for the tid
             */
#ifdef VOW_TIDSCHED
            if (!TAILQ_EMPTY(&tid->buf_q))
                ath_tx_queue_tid(sc, txq, tid);
#else
            if (!TAILQ_EMPTY(&tid->buf_q))
                ath_tx_queue_tid(txq, tid);
#endif
            /* only schedule one TID at a time */
            break;
        }

        if (tid == NULL) {
            /* nothing to schedule for this ac */
            __11nstats(sc, tx_schednone);
         }

        if (!TAILQ_EMPTY(&paused_tid_q)) {
            TAILQ_INSERTQ_HEAD(&ac->tid_q, &paused_tid_q, tid_qelem);
        }
#ifdef VOW_TIDSCHED
        if (!TAILQ_EMPTY(&paused_sc_tid_q)) {
            TAILQ_INSERTQ_HEAD(&sc->tid_q, &paused_sc_tid_q, wrr_tid_qelem);
        }
#endif

        /*
         * schedule AC if more TIDs need processing
         */
        if (!TAILQ_EMPTY(&ac->tid_q)) {
            /*
             * add dest ac to txq if not already added
             */
            if (ac->sched == AH_FALSE) {
                ac->sched = AH_TRUE;
                TAILQ_INSERT_TAIL(&txq->axq_acq, ac, ac_qelem);
            }
        }
#if ATH_SUPPORT_VOWEXT
        else if ( ac == first_schd_ac ){
            if_first_ac = 1;
        }
        if ( (!ATH_IS_VOWEXT_RCA_ENABLED(sc)) || ((sc->sc_haltype2q[HAL_WME_AC_VI] != txq->axq_qnum) && 
                    (sc->sc_haltype2q[HAL_WME_AC_VO] != txq->axq_qnum)) || 
                (txq->axq_depth >= ATH_AGGR_MIN_QDEPTH) ){
            break; 
        }

    } while ( !TAILQ_EMPTY(&txq->axq_acq) );
#endif
}

static INLINE void
ath_tx_set_retry(struct ath_softc *sc, struct ath_buf *bf)
{
    wbuf_t wbuf;
    struct ieee80211_frame *wh;

    __11nstats(sc, tx_retries);

    bf->bf_isretried = 1;
    bf->bf_retries ++;

    wbuf = bf->bf_mpdu;
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    wh->i_fc[1] |= IEEE80211_FC1_RETRY;
}


/*
 * Compute the number of bad frames
 */
int
ath_tx_num_badfrms(struct ath_softc *sc, struct ath_buf *bf, struct ath_tx_status *ts, int txok)
{
    struct ath_node *an = bf->bf_node;
    int isnodegone = (an->an_flags & ATH_NODE_CLEAN);
    struct ath_buf *bf_last = bf->bf_lastbf;
    u_int16_t seq_st = 0;
    u_int32_t ba[WME_BA_BMP_SIZE >> 5];
    int ba_index, nframes;
    int nbad = 0;
    int isaggr = 0;
    
#ifdef ATH_RIFS
    int isrifs = 0;
#endif

#if ATH_SUPPORT_VOWEXT
    u_int32_t   babmp = 0;
    u_int8_t    idx = 0;
    u_int8_t    n_head_fail = 0;
    u_int8_t    n_tail_fail= 0;
    u_int32_t   retval;
#endif

#if VOW_LOGLATENCY
    u_int32_t   currts = 0;
#endif
#if ATH_SUPPORT_VOW_DCS
    void *ds = bf->bf_lastbf->bf_desc;
#define MIN_ACK_RSSI_THRESH 10
#endif
    nframes = bf->bf_nframes;

    if (isnodegone || bf_last->bf_isswaborted)
        return 0;

    isaggr = bf->bf_isaggr;
#ifdef ATH_RIFS
    isrifs = (ATH_RIFS_SUBFRAME_FIRST == bf->bf_rifsburst_elem) ? 1:0;

    if (isaggr || isrifs)
#else
    if (isaggr)
#endif
    {
        seq_st = ATH_DS_BA_SEQ(ts);
        OS_MEMCPY(ba, ATH_DS_BA_BITMAP(ts), WME_BA_BMP_SIZE >> 3);
    }

#if VOW_LOGLATENCY
    if(sc->loglatency) {
        currts = ath_hal_gettsf32(sc->sc_ah);
    }
    else {
        currts = 0;
    }
#endif

#ifdef ATH_RIFS
    while (bf && bf->bf_rifsburst_elem != ATH_RIFS_BAR)
#else
    while (bf)
#endif
    {
        ba_index = ATH_BA_INDEX(seq_st, bf->bf_seqno);
#ifdef ATH_RIFS
        if (!txok || ((isaggr || isrifs) && && !ATH_BA_ISSET(ba, ba_index)))
#else
        if (!txok || (isaggr && !ATH_BA_ISSET(ba, ba_index)))
#endif
        {
            nbad++;
#if ATH_SUPPORT_VOWEXT
            if ( ATH_IS_VOWEXT_AGGRSIZE_ENABLED(sc) || ATH_IS_VOWEXT_RCA_ENABLED(sc))
                babmp = babmp << 1;
#endif
        }
        else {
#ifdef ATH_SUPPORT_VOWEXT
          if (ATH_IS_VOWEXT_AGGRSIZE_ENABLED(sc) || ATH_IS_VOWEXT_RCA_ENABLED(sc)) {
            babmp = ((babmp << 1)|0x1);
          }
#endif
#ifdef VOW_LOGLATENCY
          if(sc->loglatency) {
            vow_loglatency(sc, bf, currts);
          }
#endif
#if ATH_SUPPORT_VOW_DCS
          sc->sc_dcs_params.bytecnttx += bf->bf_frmlen;
#endif

        }
        bf = bf->bf_next;
    }
#if ATH_SUPPORT_VOW_DCS
    if (txok){
        sc->sc_dcs_params.last_ack_rssi = ts->ts_rssi;
    }
    if (sc->sc_dcs_params.last_ack_rssi > MIN_ACK_RSSI_THRESH){
        /* when the RSSI is very low, then don't include the failure time in wastedtxtime */
        sc->sc_dcs_params.wastedtxtime +=  ath_hal_txcalcairtime(sc->sc_ah, ds, ts, 
                AH_TRUE, nbad, nframes);
    }
#endif

#if ATH_SUPPORT_VOWEXT
    if ( ATH_IS_VOWEXT_AGGRSIZE_ENABLED(sc) || ATH_IS_VOWEXT_RCA_ENABLED(sc) )  {
        n_head_fail = 0;
        n_tail_fail = 0;
        
        for(idx = 0; idx < (nframes >> 1); idx++) {
          n_head_fail += (((babmp >> (nframes - (idx + 1))) & 0x1) ? 0 : 1);
          n_tail_fail += (((babmp >> idx) & 0x1) ? 0 : 1);
        }
    
        retval = 0;
        retval |= ((nbad & 0xff) << 16);
        retval |= ((n_head_fail & 0xff) << 8);
        retval |= (n_tail_fail & 0xff);
    
        return retval;
    } else return nbad;
#else
    return nbad;
#endif
}

#if ATH_SUPPORT_VOWEXT || ATH_SWRETRY
/*
 * return values are 
 *      -ve  - s1 < s2
 *      0       s1 == s2
 *      +ve     s1 > s2
 *
*/
#define IEEE80211_SEQ_WRAP_START 4031
inline static int compare_seq_no (int s1, int s2)
{
    if ((s1 > IEEE80211_SEQ_WRAP_START)  && (s2 < WME_MAX_BA))
        return (s1 + WME_MAX_BA - IEEE80211_SEQ_MAX) - (s2 + WME_MAX_BA);
    if ((s1 < WME_MAX_BA) && (s2 > IEEE80211_SEQ_WRAP_START))
        return ((s1+ WME_MAX_BA) - (s2 - IEEE80211_SEQ_MAX + WME_MAX_BA));
    if ((s1 > IEEE80211_SEQ_WRAP_START) && (s2 > IEEE80211_SEQ_WRAP_START))
        return ((s1+WME_MAX_BA-IEEE80211_SEQ_MAX) - (s2+WME_MAX_BA-IEEE80211_SEQ_MAX));

    return (s1 - s2);
}
/*
 * @brief - Check if the pending buffer is in sorted order, do not try sort it
 *          will do that based on need.
 *  
*/
int
ath_check_seq_order(ath_bufhead *bf_pending)
{
    struct ath_buf *bf = TAILQ_FIRST(bf_pending);
    u_int8_t sorted = 1;
    int prev_seq_no = bf->bf_seqno;
    
    TAILQ_FOREACH(bf, bf_pending, bf_list) {
        if (compare_seq_no(prev_seq_no, bf->bf_seqno) <= 0) { 
            prev_seq_no = bf->bf_seqno;
        } else sorted = 0;
    }
    return sorted;
}

/*
 *  - first_pen_seq_no - sequence number in first pending buffer
 *  - last_pen_seq_no  - sequence number in last pending buffer
 *  - first_tid_seq_no - sequence number in first tid buffer
 *  - last_tid_seq_no  - sequence number in last tid buffer
 *  Logic:
 *      if (last_pend_seq_no < first_tid_seq_no)
 *          copy the entire pending buffer list to the head
 *      if (first_pend_seq_no > last_tid_seq_no)
 *          copy the entire pending buffer to the end of the tid tail
 *      otherwise sort the list by wlaking through the elements of both of the
 *      lists. If seq number in pending buffer is lesser, copy the buffer otherwise 
 *      move the tid buffer by 1, until either of the list ends.
 
*/
int
ath_insertq_inorder(struct ath_softc *sc, struct ath_atx_tid *tid, ath_bufhead *bf_pending)
{
    int first_pen_seq_no=0, last_pen_seq_no=0;
    int first_tid_seq_no=0, last_tid_seq_no=0;

    struct ath_buf *first_pen_buf = NULL, *last_pen_buf = NULL;
    struct ath_buf *first_tid_buf = NULL, *last_tid_buf = NULL;
    int sorted = 0;
    struct ath_buf *tid_buf=NULL, *pending_buf=NULL;

    if (!sc || !tid || !bf_pending) {
        return -1;
    } 

    first_pen_buf = TAILQ_FIRST(bf_pending);
    first_pen_seq_no = first_pen_buf->bf_seqno;
    
    last_pen_buf = TAILQ_LAST(bf_pending, ath_bufhead_s);
    last_pen_seq_no = last_pen_buf->bf_seqno;
    
    first_tid_buf = TAILQ_FIRST(&tid->buf_q);
    first_tid_seq_no = first_tid_buf->bf_seqno;
    last_tid_buf = TAILQ_LAST(&tid->buf_q, ath_bufhead_s);
    last_tid_seq_no = last_tid_buf->bf_seqno;

    /* check if entier aggregate can fit before*/
    if (compare_seq_no(last_pen_seq_no, first_tid_seq_no) < 0) {
        TAILQ_INSERTQ_HEAD(&tid->buf_q, bf_pending, bf_list );
        return 0;
    }

    /* neither fits first, nor last, */
    /* do a linear sort, we assume two lists are sorted */
    
    tid_buf = TAILQ_FIRST(&tid->buf_q);
    pending_buf = TAILQ_FIRST(bf_pending);
    
    while (!sorted && tid_buf && pending_buf ) {
        int cresult = compare_seq_no(pending_buf->bf_seqno, tid_buf->bf_seqno);
        if (cresult <= 0) {
            /* insert before the current buffer in the tid buf list*/
            TAILQ_REMOVE(bf_pending, pending_buf, bf_list);
            TAILQ_INSERT_BEFORE(tid_buf, pending_buf, bf_list);
            pending_buf = TAILQ_FIRST(bf_pending);
            if (!pending_buf) {
                sorted = 1; /* we entirely ran out of pending list, every thing is placed properly*/
            }
        } if ( cresult > 0) {
            /* do not insert, but move the tid buffer by one */
            tid_buf = TAILQ_NEXT(tid_buf, bf_list);
        }
    } /* end while*/
    
    /* if there are more buffers in tid queue and no buffers to copy in pending queue, 
       need not do any thing, as copy happens in place for tid queue. 
    */
    if (!sorted && !tid_buf && pending_buf) { //there are more buffers in pending queue
        TAILQ_CONCAT (&tid->buf_q, bf_pending, bf_list);
    } 
    return 0;
}
#ifndef ATH_SWRETRY
#endif
#endif
/*
 * Completion routine of an aggregate
 */
void
ath_tx_complete_aggr_rifs(struct ath_softc *sc, struct ath_txq *txq, struct ath_buf *bf,
                          ath_bufhead *bf_q, struct ath_tx_status *ts, int txok)
{
    struct ath_node *an = bf->bf_node;
    struct ath_atx_tid *tid = ATH_AN_2_TID(an, bf->bf_tidno);
    struct ath_buf *bf_last = bf->bf_lastbf;
#if ATH_SUPPORT_IQUE
    struct ath_node *tan;
#endif
    struct ath_buf *bf_next, *bf_lastq = NULL;
    ath_bufhead bf_head, bf_pending;
    u_int16_t seq_st = 0;
    u_int32_t ba[WME_BA_BMP_SIZE >> 5];
    int isaggr, txfail, txpending, sendbar = 0, needreset = 0;
    int isnodegone= (an->an_flags & ATH_NODE_CLEAN);
    u_int sw_retry_limit = ATH_MAX_SW_RETRIES;

#if ATH_SUPPORT_IQUE && ATH_SUPPORT_IQUE_EXT
    u_int32_t currts, lapsed_td, qin_ts, fxmit_ts;
#endif

#ifdef ATH_RIFS
    int isrifs = 0;
    struct ath_buf *bar_bf = NULL;
#endif
#if ATH_SUPPORT_VOWEXT
        int8_t is_sorted = 1;
#endif 
    /* defer completeion on  atleast one buffer   */
    struct _defer_completion {
            struct ath_buf *bf;
            ath_bufhead bf_head;
            u_int8_t txfail;
    } defer_completion;

    OS_MEMZERO(&defer_completion, sizeof(defer_completion));

    OS_MEMZERO(ba, WME_BA_BMP_SIZE >> 3);

    isaggr = bf->bf_isaggr;
#ifdef ATH_RIFS
    isrifs = (ATH_RIFS_SUBFRAME_FIRST == bf->bf_rifsburst_elem) ? 1:0;


    if (isrifs) {
        bar_bf = bf->bf_lastbf;
        ASSERT(ATH_RIFS_BAR == bar_bf->bf_rifsburst_elem);
    }

    if (isaggr || isrifs)
#else
    if (isaggr)
#endif
    {
#ifdef ATH_RIFS
        isrifs ? __11nstats(sc, tx_comprifs) : __11nstats(sc, tx_compaggr);
#else
        __11nstats(sc, tx_compaggr);
#endif
        if (txok) {
            if (ATH_DS_TX_BA(ts)) {
                /*
                 * extract starting sequence and block-ack bitmap
                 */
                seq_st = ATH_DS_BA_SEQ(ts);
                OS_MEMCPY(ba, ATH_DS_BA_BITMAP(ts), WME_BA_BMP_SIZE >> 3);
            } else {
#ifdef ATH_RIFS
                isrifs ? __11nstats(sc, txrifs_babug) :
                         __11nstats(sc, txaggr_babug);
#else
                __11nstats(sc, txaggr_babug);
#endif
                DPRINTF(sc, ATH_DEBUG_TX_PROC, "%s: BA bit not set.\n", __func__);

                /*
                 * Owl can become deaf/mute when BA bug happens.
                 * Chip needs to be reset. See bug 32789.
                 */
                needreset = 1;
            }
        } 
    }

    TAILQ_INIT(&bf_pending);

#if ATH_SUPPORT_IQUE && ATH_SUPPORT_IQUE_EXT
    if(sc->sc_retry_duration>0 || sc->total_delay_timeout>0) {
      currts = ath_hal_gettsf32(sc->sc_ah);
    }
    else {
      currts = 0;
    }
#endif

#ifdef ATH_RIFS
    while (bf && bf->bf_rifsburst_elem != ATH_RIFS_BAR)
#else
    while (bf)
#endif
    {
        txfail = txpending = 0;
        bf_next = bf->bf_next;

        if (ATH_BA_ISSET(ba, ATH_BA_INDEX(seq_st, bf->bf_seqno))) {
            /*
             * transmit completion, subframe is acked by block ack
             */
#ifdef ATH_RIFS
            isrifs ? __11nstats(sc, txrifs_compgood) :
                     __11nstats(sc, txaggr_compgood);
#else
            __11nstats(sc, txaggr_compgood);
#endif
        }
#ifdef ATH_RIFS
        else if ((!isaggr && !isrifs) && txok)
#else
        else if (!isaggr && txok)
#endif
        {
            /*
             * transmit completion
             */
#ifdef ATH_RIFS
            isrifs ? __11nstats(sc, tx_compnorifs) :
                     __11nstats(sc, tx_compunaggr);
#else
            __11nstats(sc, tx_compunaggr);
#endif
        } else {
            /*
             * retry the un-acked ones
             */
            if (ts->ts_flags & HAL_TXERR_XRETRY) {
                __11nstats(sc,tx_sf_hw_xretries);
            }

#ifdef ATH_RIFS
            isrifs ? __11nstats(sc, txrifs_compretries) :
                     __11nstats(sc, txaggr_compretries);
#else
            __11nstats(sc, txaggr_compretries);
#endif


#if ATH_SUPPORT_IQUE
            /* For the frames to be droped who block the headline of the AC_VI queue,
             * these frames should not be sw-retried. So mark them as already xretried.
             */
            tan = ATH_NODE(bf->bf_node);
            if (sc->sc_ieee_ops->get_hbr_block_state(tan->an_node) &&             
                            TID_TO_WME_AC(bf->bf_tidno) == WME_AC_VI) {
                bf->bf_retries = sw_retry_limit;
            }
#endif      
            if (!tid->cleanup_inprogress && !isnodegone &&
                !bf_last->bf_isswaborted) {

                if (ATH_ENAB_AOW(sc) && (TID_TO_WME_AC(bf->bf_tidno) == WME_AC_VO))
                {
                    sw_retry_limit = ATH_SW_RETRY_LIMIT(sc);
                }

#if UMAC_SUPPORT_SMARTANTENNA               
               /* disable software retries for smart antenna training packet*/ 
                if ((((bf->bf_retries < sw_retry_limit) || (atomic_read(&sc->sc_in_reset)))&&!(wbuf_is_sa_train_packet(bf->bf_mpdu)))) {
#else
                if ((((bf->bf_retries < sw_retry_limit) || (atomic_read(&sc->sc_in_reset))))) {
#endif                    
                    ath_tx_set_retry(sc, bf);

#if ATH_SUPPORT_IQUE && ATH_SUPPORT_IQUE_EXT
                    /* 
                    * Note: This reset-in-progress could cause the retry timeout to be exceeded
                    * since we are requeuing this packet regardless of the duration time. See above
                    * check for sc->sc_in_reset.
                    */
                    qin_ts = wbuf_get_qin_timestamp(bf->bf_mpdu);
                    fxmit_ts = wbuf_get_firstxmitts(bf->bf_mpdu);
                    lapsed_td = (currts>=qin_ts) ? (currts - qin_ts) : 
                                                   ((0xffffffff - qin_ts) + currts);

                    bf->bf_txduration = (currts>=fxmit_ts) ? (currts - fxmit_ts) : 
                                                             ((0xffffffff - fxmit_ts) + currts);

                    if (((bf->bf_txduration >= sc->sc_retry_duration) && (sc->sc_retry_duration > 0) && (!atomic_read(&sc->sc_in_reset))) ||
                        ((lapsed_td >= sc->total_delay_timeout) && (sc->total_delay_timeout > 0) && (!atomic_read(&sc->sc_in_reset))))
                    {
                        __11nstats(sc, tx_xretries);
                        bf->bf_isxretried = 1;
                        txfail = 1;
                        sendbar = tid->addba_exchangecomplete;
                    } else {
                        txpending = 1;
                    }
#else
                    txpending = 1;
#endif
                } else {
                    __11nstats(sc, tx_xretries);
                    bf->bf_isxretried = 1;
                    txfail = 1;
                    /* Note : Temporarily disabling BAR usage for SA Training packets as this is causing tx stuck in some corner cases
                     * 
                     */
                    if (!(wbuf_is_sa_train_packet(bf->bf_mpdu)))
                        sendbar = tid->addba_exchangecomplete;

                    DPRINTF(sc, ATH_DEBUG_TX_PROC, "%s drop tx frame tid %d bf_seqno %d\n", __func__, tid->tidno, bf->bf_seqno);
                }
            } else {
                /*
                 * the entire aggregate is aborted by software due to
                 * reset, channel change, node left and etc.
                 */
                if (bf_last->bf_isswaborted) {
                    __11nstats(sc, txaggr_comperror);
                }

                /*
                 * cleanup in progress, just fail
                 * the un-acked sub-frames
                 */
                txfail = 1;
            }
        }

        /*
         * Remove ath_buf's of this sub-frame from aggregate queue.
         */
        if (bf_next == NULL && !sc->sc_enhanceddmasupport) {  /* last subframe in the aggregate */
            ASSERT(bf->bf_lastfrm == bf_last);

            /*
             * The last descriptor of the last sub frame could be a holding descriptor
             * for h/w. If that's the case, bf->bf_lastfrm won't be in the bf_q.
             * Make sure we handle bf_q properly here.
             */
            bf_lastq = TAILQ_LAST(bf_q, ath_bufhead_s);
            if (bf_lastq) {
                TAILQ_REMOVE_HEAD_UNTIL(bf_q, &bf_head, bf_lastq, bf_list);
            } else {
                /*
                 * XXX: if the last subframe only has one descriptor which is also being used as
                 * a holding descriptor. Then the ath_buf is not in the bf_q at all.
                 */
                ASSERT(TAILQ_EMPTY(bf_q));
                TAILQ_INIT(&bf_head);
            }
        } else {
            ASSERT(!TAILQ_EMPTY(bf_q));
            TAILQ_REMOVE_HEAD_UNTIL(bf_q, &bf_head, bf->bf_lastfrm, bf_list);
        }

#ifndef REMOVE_PKT_LOG
        /* do pktlog */
        {
            struct log_tx log_data;
            struct ath_buf *tbf;

            TAILQ_FOREACH(tbf, &bf_head, bf_list) {
                log_data.firstds = tbf->bf_desc;
                log_data.bf = tbf;
                ath_log_txctl(sc, &log_data, 0);
            }

            if (bf->bf_next == NULL &&
                bf_last->bf_status & ATH_BUFSTATUS_STALE) {
                log_data.firstds = bf_last->bf_desc;
                log_data.bf = bf_last;
                ath_log_txctl(sc, &log_data, 0);
            }
        }
#endif

        if (!txpending) {
            /*
             * complete the acked-ones/xretried ones; update block-ack window
             */
            ATH_TXQ_LOCK(txq);
            ath_tx_update_baw(sc, tid, bf->bf_seqno);
            if ((isnodegone) && (tid->cleanup_inprogress)) {
                if (tid->baw_head == tid->baw_tail) {
                    tid->addba_exchangecomplete = 0;
                    tid->addba_exchangeattempts = 0;
                    tid->addba_exchangestatuscode = IEEE80211_STATUS_UNSPECIFIED;
                    /* resume the tid */
                    tid->paused--;
                    __11nstats(sc, tx_tidresumed);
                    tid->cleanup_inprogress = AH_FALSE;
                }
            }
            ATH_TXQ_UNLOCK(txq);

            if( defer_completion.bf ) {
#ifdef ATH_SUPPORT_TxBF
                ath_tx_complete_buf(sc, defer_completion.bf, &defer_completion.bf_head, !defer_completion.txfail, ts->ts_txbfstatus, ts->ts_tstamp);
#else
                ath_tx_complete_buf(sc, defer_completion.bf, &defer_completion.bf_head, !defer_completion.txfail);
#endif
            } 
            /* 
             * save this sub-frame to be completed at the end. this
             * will keep the node referenced till the end of the function
             * and prevent acces to the node memory after it is freed (note tid is part of node).
             */
            defer_completion.bf = bf;
            defer_completion.txfail = txfail;
            if (!TAILQ_EMPTY(&bf_head)) {
                defer_completion.bf_head = bf_head;
                TAILQ_INIT(&bf_head);
            } else {
                TAILQ_INIT(&defer_completion.bf_head);
            }
        } else {
            /*
             * retry the un-acked ones
             */
            if (!sc->sc_enhanceddmasupport) { /* holding descriptor support for legacy */
                /*
                 * XXX: if the last descriptor is holding descriptor, in order to requeue
                 * the frame to software queue, we need to allocate a new descriptor and
                 * copy the content of holding descriptor to it.
                 */
                if (bf->bf_next == NULL &&
                    bf_last->bf_status & ATH_BUFSTATUS_STALE) {
                    struct ath_buf *tbf;
                    int nmaps;

                    /* allocate new descriptor */
                    ATH_TXBUF_LOCK(sc);
                    tbf = TAILQ_FIRST(&sc->sc_txbuf);
                    if (tbf == NULL) {
                        /*
                         * We are short on memory, release the wbuf
                         * and bail out.
                         * Complete the packet with status *Not* OK.
                         */
                        ATH_TXBUF_UNLOCK(sc);

                        ATH_TXQ_LOCK(txq);
                        ath_tx_update_baw(sc, tid, bf->bf_seqno);
                        ATH_TXQ_UNLOCK(txq);

                        if( defer_completion.bf ) {
#ifdef ATH_SUPPORT_TxBF
                            ath_tx_complete_buf(sc, defer_completion.bf, &defer_completion.bf_head, !defer_completion.txfail,ts->ts_txbfstatus, ts->ts_tstamp);
#else
                            ath_tx_complete_buf(sc, defer_completion.bf, &defer_completion.bf_head, !defer_completion.txfail);
#endif
                        } 
                        /* 
                         * save this sub-frame to be completed later
                         * this is a  holding buffer, we do not want  to return this to
                         * the free list yet. clear the bf_head so that the ath_tx_complete_buf  will
                         * not return any thing to the sc_txbuf.
                         * also mark this subframe as an error.
                         * since the bf_head in cleared, ath_tx_complete_buf will
                         * just complete the wbuf for this subframe and will not return any  
                         * ath bufs to free list. 
                         */
                        defer_completion.bf = bf;
                        defer_completion.txfail = 1;
                        TAILQ_INIT(&defer_completion.bf_head);

                        // At this point, bf_next is NULL: We are done with this aggregate.
                        break;
                    }
                    TAILQ_REMOVE(&sc->sc_txbuf, tbf, bf_list);
#if ATH_TX_BUF_FLOW_CNTL
                    if (tbf) {
	                    txq->axq_num_buf_used++;
                        sc->sc_txbuf_free--;
                    }
#endif
                    ATH_TXBUF_UNLOCK(sc);

                    ATH_TXBUF_RESET(tbf, sc->sc_num_txmaps);

                    /* copy descriptor content */
                    tbf->bf_mpdu = bf_last->bf_mpdu;
                    tbf->bf_node = bf_last->bf_node;
#ifndef REMOVE_PKT_LOG
                    tbf->bf_vdata = bf_last->bf_vdata;
#endif
                    for (nmaps = 0; nmaps < sc->sc_num_txmaps; nmaps++) {
                        tbf->bf_buf_addr[nmaps] = bf_last->bf_buf_addr[nmaps];
                        tbf->bf_buf_len[nmaps] = bf_last->bf_buf_len[nmaps];
                        tbf->bf_avail_buf--;
                    }
                    memcpy(tbf->bf_desc, bf_last->bf_desc, sc->sc_txdesclen);

                    /* link it to the frame */
                    if (bf_lastq) {
           	            ath_hal_setdesclink(sc->sc_ah, bf_lastq->bf_desc, tbf->bf_daddr);
                        bf->bf_lastfrm = tbf;
                        ath_hal_cleartxdesc(sc->sc_ah, bf->bf_lastfrm->bf_desc);
                    } else {
                        tbf->bf_state = bf_last->bf_state;
                        tbf->bf_lastfrm = tbf;
                        ath_hal_cleartxdesc(sc->sc_ah, tbf->bf_lastfrm->bf_desc);

                        /* copy the DMA context */
                        OS_COPY_DMA_MEM_CONTEXT(OS_GET_DMA_MEM_CONTEXT(tbf, bf_dmacontext),
                                                OS_GET_DMA_MEM_CONTEXT(bf_last, bf_dmacontext));
                    }
                    TAILQ_INSERT_TAIL(&bf_head, tbf, bf_list);
                } else {
                    /*
                     * Clear descriptor status words for software retry
                     */
                    ath_hal_cleartxdesc(sc->sc_ah, bf->bf_lastfrm->bf_desc);
                }
            }

            /*
             * Put this buffer to the temporary pending queue to retain ordering
             */
            TAILQ_CONCAT(&bf_pending, &bf_head, bf_list);
        }

        bf = bf_next;
    }

    /*
     * node is already gone. no more assocication
     * with the node. the node might have been freed
     * any  node acces can result in panic.note tid
     * is part of the node. 
     */  
    if (isnodegone) goto done;

    /*
     * Refer to EV#82220, for some Intel series STA, 
     * STA will send DELBA to AP when STA detects there is few data to be transmitted.
     * Therefore the cleanup_inprogess flag will be true, and it will bypass sending BAR.
     * Somehow the BAR message plays an important role on the Intel series STA.
     * If AP does not send the corresponding BAR, it will make Tx hang from the Intel series STA side.
     * So, sending BAR message whether cleanup procedure is in progress or not.
     */
     
    /* 
     * During DFS testing, we observe extensive lost of Block ACK, which causes the starting
     * sequence out of sync. This is a WAR which re-sync the starting sequence once
     * radar is detected.
     */
    if (sendbar || sc->sc_dfs_radar_detected[tid->tidno]) {
        ath_bar_tx(sc, an, tid);
	if (sc->sc_dfs_radar_detected[tid->tidno]) {
	    /*
            	printk("%s: Send bar for tid %d due to radar detected\n",
		   __func__, tid->tidno);
            */
	    sc->sc_dfs_radar_detected[tid->tidno] = false;
	}
    }

    if (tid->cleanup_inprogress) {
        /* check to see if we're done with cleaning the h/w queue */
        ATH_TXQ_LOCK(txq);

        if (tid->baw_head == tid->baw_tail) {
            tid->addba_exchangecomplete = 0;
            tid->addba_exchangeattempts = 0;
            tid->addba_exchangestatuscode = IEEE80211_STATUS_UNSPECIFIED;

            ath_wmi_aggr_enable((ath_dev_t) sc, an, tid->tidno, 0);

            ATH_TXQ_UNLOCK(txq);
            
            tid->cleanup_inprogress = AH_FALSE;
 
            /* send buffered frames as singles */
            ATH_TX_RESUME_TID(sc, tid);
        } else {
            ATH_TXQ_UNLOCK(txq);
        }

        goto done;
    }

#ifdef ATH_RIFS
    if (isrifs)
        ath_rifsburst_bar_buf_free(sc, bar_bf);
#endif
    /*
     * prepend un-acked frames to the beginning of the pending frame queue
     */
    if (!TAILQ_EMPTY(&bf_pending)) {
        
#ifdef ATH_RIFS
        isrifs ? __11nstats(sc, txrifs_prepends) :
                 __11nstats(sc, txaggr_prepends);
#else
        __11nstats(sc, txaggr_prepends);
#endif
        
        ATH_TXQ_LOCK(txq);
#if ATH_SUPPORT_VOWEXT
        if ((ATH_IS_VOWEXT_BUFFREORDER_ENABLED(sc)))
        {
            if (TAILQ_EMPTY(&tid->buf_q)) {
                is_sorted = 0;
            } else if( ath_insertq_inorder(sc, tid, &bf_pending) < 0) { 
                is_sorted= 0;
            }
        } else {
            is_sorted = 0;
        }
        
        /* either not VI, or not VOW enabled */
        if (!is_sorted) { 
            TAILQ_INSERTQ_HEAD(&tid->buf_q, &bf_pending, bf_list);
        }
#else
        TAILQ_INSERTQ_HEAD(&tid->buf_q, &bf_pending, bf_list);
#endif
#ifdef VOW_TIDSCHED
        ath_tx_queue_tid(sc, txq, tid);
#else
        ath_tx_queue_tid(txq, tid);
#endif
        ATH_TXQ_UNLOCK(txq);
#ifdef ATH_SWRETRY
        if (!tid->an->an_tim_set &&  
            (tid->an->an_flags & ATH_NODE_PWRSAVE) && ((sc->sc_ieee_ops->get_pwrsaveq_len(tid->an->an_node)==0)) &&
            !tid->cleanup_inprogress && !isnodegone)
        {
            ATH_NODE_SWRETRY_TXBUF_LOCK(tid->an);
            sc->sc_ieee_ops->set_tim(tid->an->an_node,1);
            tid->an->an_tim_set = AH_TRUE;
            ATH_NODE_SWRETRY_TXBUF_UNLOCK(tid->an);
        }
#endif		

    }

    if (needreset) {
        /*
         * AP code may have sychronization issues
         * when perform internal reset in this routine.
         * Only enable reset in STA mode for now.
         */
        if (sc->sc_opmode == HAL_M_STA){
            sc->sc_reset_type = ATH_RESET_NOLOSS;
            ath_internal_reset(sc);
            sc->sc_reset_type = ATH_RESET_DEFAULT;
        }
    }


done:
    /*
     * complete the defrred buffer. 
     * at this point the associated node could be freed.
     */
    if (defer_completion.bf) {
#ifdef ATH_SUPPORT_TxBF
        ath_tx_complete_buf(sc, defer_completion.bf, &defer_completion.bf_head, !defer_completion.txfail,ts->ts_txbfstatus, ts->ts_tstamp);
#else
        ath_tx_complete_buf(sc, defer_completion.bf, &defer_completion.bf_head, !defer_completion.txfail);
#endif
    }

    return;
}

static void
ath_complete_drained_tid_buf(struct ath_softc *sc, ath_bufhead *bf_q)
{
    struct ath_buf *bf;
    ath_bufhead bf_head;

    for (;;) {
        bf = TAILQ_FIRST(bf_q);
        if (bf == NULL)
            break;

        TAILQ_REMOVE_HEAD_UNTIL(bf_q, &bf_head, bf->bf_lastfrm, bf_list);
#ifdef ATH_SUPPORT_TxBF
        ath_tx_complete_buf(sc, bf, &bf_head, 0, 0, 0);
#else
        ath_tx_complete_buf(sc, bf, &bf_head, 0);
#endif

        __11nstats(sc, tx_drain_bufs);
    }

}

/* This function clear buffers in software queue for this tid */
static void
ath_tid_swq_cleanup(struct ath_softc *sc, struct ath_txq *txq,
                    struct ath_atx_tid *tid, ath_bufhead *drained_bf_head)
{
    struct ath_buf *bf;
    ath_bufhead bf_head;

    for (;;) {
        bf = TAILQ_FIRST(&tid->buf_q);
        if (bf == NULL)
            break;

        TAILQ_REMOVE_HEAD_UNTIL(&tid->buf_q, &bf_head, bf->bf_lastfrm, bf_list);

        /* update baw for software retried frame */
        if (bf->bf_isretried)
        {
            ath_tx_update_baw(sc, tid, bf->bf_seqno);
        }

        /*
         * Calles needs to complete the sub-frame. If this is the last
         * reference count on the node, node memory will be freed.
         */
        TAILQ_CONCAT(drained_bf_head, &bf_head, bf_list);
    }
}

/* This function is called in the process of chip reset - and
 * the assumption is all the buffers in the HW queue are
 * removed already.
 * Called with txq lock held */
static void
ath_tid_drain(struct ath_softc *sc, struct ath_txq *txq, struct ath_atx_tid *tid)
{
    ath_bufhead drained_bf_head;

    __11nstats(sc, tx_drain_tid);

    TAILQ_INIT(&drained_bf_head);
    ath_tid_swq_cleanup(sc, txq, tid, &drained_bf_head);
    /*
     * TODO: For frame(s) that are in the retry state, we will reuse the 
     * sequence number(s) without setting the retry bit. The alternative is to
     * give up on these and BAR the receiver's window forward.
     */
    tid->seq_next = tid->seq_start;
    tid->baw_tail = tid->baw_head;

    /* 
     * The following Unlock/lock sequence seems to give chance for a race
     * to acces the txq and cause a crash.
     * The lock seems to be needed through out the loop in which 
     * ath_tid_drain is called. so commenting them.
     */
    /*   ATH_TXQ_UNLOCK(txq); */
    ath_complete_drained_tid_buf(sc, &drained_bf_head);
    /*    ATH_TXQ_LOCK(txq); */
}


/* This function is called in the process of node cleanup.
 * Need to check for pending buffers in the HW queue.
 */
static void
ath_tid_cleanup(struct ath_softc *sc, struct ath_txq *txq, struct ath_atx_tid *tid)
{
    ath_bufhead drained_bf_head;

    __11nstats(sc, tx_cleanup_tid);

    TAILQ_INIT(&drained_bf_head);
    ath_tid_swq_cleanup(sc, txq, tid, &drained_bf_head);

    if (tid->cleanup_inprogress) {
        goto done;
    }
    /* Check for pending packets in HW queue */
    if (tid->baw_head != tid->baw_tail) {
        /* Frames in HW queue */
        /* Pause the tid and set cleanup in progress to True */
        tid->paused++;
        __11nstats(sc, tx_tidpaused);
        tid->cleanup_inprogress = AH_TRUE;
    } else {
        tid->addba_exchangecomplete = 0;
        tid->addba_exchangeattempts = 0;
        tid->addba_exchangestatuscode = IEEE80211_STATUS_UNSPECIFIED;
    }

done:
    /* 
     * The following Unlock/lock sequence seems to give chance for a race
     * to acces the txq and cause a crash.
     * The lock seems to be needed through out the loop in which 
     * ath_tid_cleanup is called. so commenting them.
     */
    /*   ATH_TXQ_UNLOCK(txq); */
    ath_complete_drained_tid_buf(sc, &drained_bf_head);
    /*    ATH_TXQ_LOCK(txq); */
}

/*
 * Drain all pending buffers
 * NB: must be called with txq lock held
 */
void
ath_txq_drain_pending_buffers(struct ath_softc *sc, struct ath_txq *txq)
{
    struct ath_atx_ac *ac;
    struct ath_atx_tid *tid, *next_tid;
    
    __11nstats(sc,tx_resetq);
    while ((ac = TAILQ_FIRST(&txq->axq_acq)) != NULL) {
        TAILQ_REMOVE(&txq->axq_acq, ac, ac_qelem);
        ac->sched = AH_FALSE;

        TAILQ_FOREACH_SAFE(tid, &ac->tid_q, tid_qelem, next_tid) {
            TAILQ_REMOVE(&ac->tid_q, tid, tid_qelem);
            tid->sched = AH_FALSE;
#ifdef VOW_TIDSCHED
            if (tid->ac->qnum < HAL_NUM_DATA_QUEUES && is_tid_in_sctidqueue(sc, tid)) {
                TAILQ_REMOVE(&sc->tid_q, tid, wrr_tid_qelem);
            }
#endif
            ath_tid_drain(sc, txq, tid);
        }
    }
}

/*
 * Initialize per-node transmit state
 */
void
ath_tx_node_init(struct ath_softc *sc, struct ath_node *an)
{
    struct ath_atx_tid *tid;
    struct ath_atx_ac *ac;
    int tidno, acno;

    an->an_aggr.tx.maxampdu = sc->sc_config.ampdu_limit;

    /*
     * Init per tid tx state
     */
    for (tidno = 0, tid = &an->an_tx_tid[tidno]; tidno < WME_NUM_TID;
         tidno++, tid++)
    {
        tid->an        = an;
        tid->tidno     = tidno;
        tid->seq_start = tid->seq_next = 0;
        tid->baw_size  = WME_MAX_BA;
        tid->baw_head  = tid->baw_tail = 0;
#if ATH_SUPPORT_IQUE
        tid->min_depth = (TID_TO_WME_AC(tidno) >= WME_AC_VI) ? 1:ATH_AGGR_MIN_QDEPTH;
#else
        tid->min_depth = ATH_AGGR_MIN_QDEPTH;
#endif
        tid->sched     = AH_FALSE;
        tid->filtered  = AH_FALSE;
        tid->paused = AH_FALSE;
        tid->cleanup_inprogress = AH_FALSE;
        TAILQ_INIT(&tid->buf_q);

        acno = TID_TO_WME_AC(tidno);
        tid->ac = &an->an_tx_ac[acno];

        ath_initialize_timer(sc->sc_osdev, &tid->addba_requesttimer, ADDBA_TIMEOUT,
                             ath_addba_timer, tid);

        /* ADDBA state */
        tid->addba_exchangecomplete     = 0;
        tid->addba_exchangeinprogress   = 0;
        tid->addba_exchangeattempts     = 0;
        tid->addba_exchangestatuscode   = IEEE80211_STATUS_UNSPECIFIED;
            
        /* We will reset the tgt tx state in tgt node creation function, such that we do not need it here. */
        //ath_wmi_aggr_enable((ath_dev_t) sc, an, tidno, 0);
    }

    /*
     * Init per ac tx state
     */
    for (acno = 0, ac = &an->an_tx_ac[acno]; acno < WME_NUM_AC; acno++, ac++) {
        ac->sched    = AH_FALSE;
        ac->filtered = AH_FALSE;
        ac->hwqcnt   = 0;
        TAILQ_INIT(&ac->tid_q);

        switch(acno) {
        case WME_AC_BE:
            ac->qnum = ath_tx_get_qnum(sc, HAL_TX_QUEUE_DATA, HAL_WME_AC_BE);
            break;
        case WME_AC_BK:
            ac->qnum = ath_tx_get_qnum(sc, HAL_TX_QUEUE_DATA, HAL_WME_AC_BK);
            break;
        case WME_AC_VI:
            ac->qnum = ath_tx_get_qnum(sc, HAL_TX_QUEUE_DATA, HAL_WME_AC_VI);
            break;
        case WME_AC_VO:
            ac->qnum = ath_tx_get_qnum(sc, HAL_TX_QUEUE_DATA, HAL_WME_AC_VO);
            break;
        }
    }
}

/*
 * Cleanupthe pending buffers for the node. 
 */
void
ath_tx_node_cleanup(struct ath_softc *sc, struct ath_node *an)
{
    int i, tidno;
    struct ath_atx_ac *ac, *next_ac;
    struct ath_atx_tid *tid, *next_tid;
    struct ath_txq *txq;
    for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
        if (ATH_TXQ_SETUP(sc, i)) {
            txq = &sc->sc_txq[i];
            ATH_TXQ_LOCK(txq);

            TAILQ_FOREACH_SAFE(ac, &txq->axq_acq, ac_qelem, next_ac) {
                tid = TAILQ_FIRST(&ac->tid_q);
                if (tid && tid->an != an) {
                    continue;
                }
                TAILQ_REMOVE(&txq->axq_acq, ac, ac_qelem);
                ac->sched = AH_FALSE;

                TAILQ_FOREACH_SAFE(tid, &ac->tid_q, tid_qelem, next_tid) {
                    TAILQ_REMOVE(&ac->tid_q, tid, tid_qelem);
                    tid->sched = AH_FALSE;
#ifdef VOW_TIDSCHED
                    if(tid->ac->qnum < HAL_NUM_DATA_QUEUES && is_tid_in_sctidqueue(sc, tid)) {
                      TAILQ_REMOVE(&sc->tid_q, tid, wrr_tid_qelem);
                    }
#endif
                    /* stop ADDBA request timer (if ADDBA in progress) */
                    if (cmpxchg(&tid->addba_exchangeinprogress, 1, 0) == 1) {
                        ath_cancel_timer(&tid->addba_requesttimer, CANCEL_NO_SLEEP);
                        /* Tid is paused - resume the tid */
                        tid->paused--;
                        __11nstats(sc, tx_tidresumed);
                    }
                    ath_tid_cleanup(sc, txq, tid);
                }
            }
            ATH_TXQ_UNLOCK(txq);
        }
    }

    /* Free the unscheduled tid buffer queue for this node */
    for (tidno = 0, tid = &an->an_tx_tid[tidno]; tidno < WME_NUM_TID; 
            tidno++, tid++) {
        txq = &sc->sc_txq[tid->ac->qnum];
        ATH_TXQ_LOCK(txq);
        /* stop ADDBA request timer (if ADDBA in progress) */
        if (cmpxchg(&tid->addba_exchangeinprogress, 1, 0) == 1) {
            ath_cancel_timer(&tid->addba_requesttimer, CANCEL_NO_SLEEP);
            /* Tid is paused - resume the tid */
            tid->paused--;
            __11nstats(sc, tx_tidresumed);
        }
        ath_tid_cleanup(sc, txq, tid);
        ATH_TXQ_UNLOCK(txq);
    }

}


#ifdef AR_DEBUG
/*
 * print per node tid info . 
 */
void
ath_tx_node_queue_stats(struct ath_softc *sc , ath_node_t node)
{
    struct ath_node *an = ATH_NODE(node);
    int tidno;
    struct ath_atx_tid *tid;
    int count;
    struct ath_buf *bf;
    DPRINTF(sc, ATH_DEBUG_ANY,"%s[%d] Enter\n", __func__, __LINE__);
    for (tidno = 0; tidno < WME_NUM_TID; tidno++) {
        tid = &an->an_tx_tid[tidno];
        bf = TAILQ_FIRST(&tid->buf_q);
        if (!bf) {
            DPRINTF(sc, ATH_DEBUG_ANY,"%s[%d] tidno %d tid %p bf %p\n", __func__, __LINE__, tidno, tid, bf);
            continue;
        }
        count=0;
        while(bf) {
            bf = TAILQ_NEXT(bf, bf_list);
            ++count;
        }
      
        DPRINTF(sc, ATH_DEBUG_ANY, 
                "%s: tid: %d  #frames: %d seqstart %d paused %d sched %d filtered %d cleanup %d\n"
                ,__func__, tidno, count, tid->seq_start,tid->paused,tid->sched,tid->filtered,tid->cleanup_inprogress);

    }
    DPRINTF(sc, ATH_DEBUG_ANY,"%s[%d] Exit\n", __func__, __LINE__);
}
#endif /* AR_DEBUG */

/*
 * Cleanup per node transmit state
 */
void
ath_tx_node_free(struct ath_softc *sc, struct ath_node *an)
{
    struct ath_atx_tid *tid;
    int tidno, i;

    /* Init per tid rx state */
    for (tidno = 0, tid = &an->an_tx_tid[tidno]; tidno < WME_NUM_TID;
         tidno++, tid++)
    {
        /* better safe than sorry */
        ath_cancel_timer(&tid->addba_requesttimer, CANCEL_NO_SLEEP);
        ath_free_timer(&tid->addba_requesttimer);

        for (i = 0; i < ATH_TID_MAX_BUFS; i++){
            ASSERT(!TX_BUF_BITMAP_IS_SET(tid->tx_buf_bitmap, i));
        }
    }
}

void
ath_tx_node_pause(struct ath_softc *sc, struct ath_node *an)
{
    int tidno;
    struct ath_atx_tid *tid;

    for (tidno = 0, tid = &an->an_tx_tid[tidno]; tidno < WME_NUM_TID;
         tidno++, tid++)
    {
        ATH_TX_PAUSE_TID(sc,tid);
    }
}

void
ath_tx_node_resume(struct ath_softc *sc, struct ath_node *an)
{
    int tidno;
    struct ath_atx_tid *tid;

    ath_vap_pause_txq_use_inc(sc);
    for (tidno = 0, tid = &an->an_tx_tid[tidno]; tidno < WME_NUM_TID;
         tidno++, tid++)
    {
        ATH_TX_RESUME_TID(sc,tid);
    }
    ath_vap_pause_txq_use_dec(sc);
}

void
ath_set_ampduparams(ath_dev_t dev, ath_node_t node, u_int16_t maxampdu,
                   u_int32_t mpdudensity)
{
    ATH_NODE(node)->an_aggr.tx.maxampdu = maxampdu;
    ATH_NODE(node)->an_aggr.tx.mpdudensity = mpdudensity;
}

void
ath_set_weptkip_rxdelim(ath_dev_t dev, ath_node_t node, u_int8_t rxdelim)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t txdelim = 0;

    /* 
     * Delimiter count for WEP/TKIP is the maximum of 
     * the delim count required by the receiver
     * and the delim count required by the device for transmitting.
     */
    (void)ath_hal_gettxdelimweptkipaggr(sc->sc_ah, &txdelim);
    ATH_NODE(node)->an_aggr.tx.weptkipdelim = MAX(rxdelim, txdelim);
}

int
ath_get_amsdusupported(ath_dev_t dev, ath_node_t node, int tidno)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = ATH_NODE(node);
    ath_atx_tid_t *tid = ATH_AN_2_TID(an, tidno);

    if (sc->sc_txamsdu && tid->addba_exchangecomplete) {
        return (tid->addba_amsdusupported);
    }
    return (FALSE);
}

#else /* ATH_SUPPORT_HT */

int ath_aggr_check(ath_dev_t dev, ath_node_t node, u_int8_t tidno)
{
    return 0;
}

void ath_addba_requestsetup(
    ath_dev_t dev, ath_node_t node,
    u_int8_t tidno,
    struct ieee80211_ba_parameterset *baparamset,
    u_int16_t *batimeout,
    struct ieee80211_ba_seqctrl *basequencectrl,
    u_int16_t buffersize
    )
{
    KASSERT(0,("ATH_SUPPORT_HT not defined"));
}

void
ath_addba_responseprocess(
    ath_dev_t dev, ath_node_t node,
    u_int16_t statuscode,
    struct ieee80211_ba_parameterset *baparamset,
    u_int16_t batimeout
    )
{
    KASSERT(0,("ATH_SUPPORT_HT not defined"));
}

void
ath_addba_clear(ath_dev_t dev, ath_node_t node)
{
}

void
ath_addba_cancel_timers(ath_dev_t dev, ath_node_t node)
{
    KASSERT(0,("ATH_SUPPORT_HT not defined"));
}

u_int16_t
ath_addba_status(ath_dev_t dev, ath_node_t node, u_int8_t tidno)
{
    return  0xFFFF;
}

void
ath_set_weptkip_rxdelim(ath_dev_t dev, ath_node_t node, u_int8_t rxdelim)
{

}

void
ath_set_ampduparams(ath_dev_t dev, ath_node_t node, u_int16_t maxampdu,
                   u_int32_t mpdudensity)
{

}

void
ath_aggr_teardown(ath_dev_t dev, ath_node_t node, u_int8_t tidno, u_int8_t initiator)
{

}

int
ath_get_amsdusupported(ath_dev_t dev, ath_node_t node, int tidno)
{
    return (FALSE);
}

#endif
