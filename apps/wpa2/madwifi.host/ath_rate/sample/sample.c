/*-
 * Copyright (c) 2005 John Bicket
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 *
 * $Id: //depot/sw/releases/Aquila_9.2.0_U11/apps/wpa2/madwifi.host/ath_rate/sample/sample.c#1 $
 */


/*
 * John Bicket's SampleRate control algorithm.
 */

#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/cache.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#include <linux/vmalloc.h>

#include <asm/uaccess.h>

#include <net80211/if_media.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_rate.h>

#include "if_athvar.h"
#include "ah_desc.h"

#include "sample.h"

#define	SAMPLE_DEBUG
#ifdef SAMPLE_DEBUG
enum {
	ATH_DEBUG_RATE		= 0x00000010	/* rate control */
};
#define	DPRINTF(sc, _fmt, ...) do {				\
	if (sc->sc_debug & ATH_DEBUG_RATE)			\
		printf(_fmt, __VA_ARGS__);			\
} while (0)
#else
#define	DPRINTF(sc, _fmt, ...)
#endif

/*
 * This file is an implementation of the SampleRate algorithm
 * in "Bit-rate Selection in Wireless Networks"
 * (http://www.pdos.lcs.mit.edu/papers/jbicket-ms.ps)
 *
 * SampleRate chooses the bit-rate it predicts will provide the most
 * throughput based on estimates of the expected per-packet
 * transmission time for each bit-rate.  SampleRate periodically sends
 * packets at bit-rates other than the current one to estimate when
 * another bit-rate will provide better performance. SampleRate
 * switches to another bit-rate when its estimated per-packet
 * transmission time becomes smaller than the current bit-rate's.
 * SampleRate reduces the number of bit-rates it must sample by
 * eliminating those that could not perform better than the one
 * currently being used.  SampleRate also stops probing at a bit-rate
 * if it experiences several successive losses.
 *
 * The difference between the algorithm in the thesis and the one in this
 * file is that the one in this file uses a ewma instead of a window.
 *
 * Also, this implementation tracks the average transmission time for
 * a few different packet sizes independently for each link.
 *
 */

#include "release.h"
static char *version = "1.2 (" RELEASE_VERSION ")";
static char *dev_info = "ath_rate_sample";


#define STALE_FAILURE_TIMEOUT_MS 10000
#define MIN_SWITCH_MS 1000

#define ENABLE_MRR 1

static int ath_smoothing_rate = 95;	/* ewma percentage (out of 100) */
static int ath_sample_rate = 10;		/* use x% of transmission time 
					 * sending at a different bit-rate */

static void ath_rate_ctl_reset(struct ath_softc *, struct ieee80211_node *);


static __inline int 
size_to_bin(int size) 
{
	unsigned int x;
	for (x = 0; x < NUM_PACKET_SIZE_BINS; x++)
		if (size <= packet_size_bins[x])
			return x;
		
	return NUM_PACKET_SIZE_BINS - 1;
}

static __inline int 
bin_to_size(int index) {
	return packet_size_bins[index];
}

static __inline int 
rate_to_ndx(struct sample_node *sn, int rate)
{
	unsigned int x;
	for (x = 0; x < sn->num_rates; x++)
		if (sn->rates[x].rate == rate)
			return x;
	return -1;
}

/*
 * Calculate the transmit duration of a frame.
 */
static unsigned
calc_usecs_unicast_packet(struct ath_softc *sc, int length, 
	int rix, int short_retries, int long_retries)
{
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	int rts, cts;
	
	unsigned t_slot;
	unsigned t_difs;
	unsigned t_sifs;
	struct ieee80211com *ic = &sc->sc_ic;
	unsigned int tt = 0;
	unsigned int x;
	unsigned int cw = WIFI_CW_MIN;
	unsigned int cix = rt->info[rix].controlRate;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));

	if (!rt->info[rix].rateKbps) {
		printk(KERN_WARNING "rix %u (%u) bad ratekbps %u mode %u\n",
		       rix, rt->info[rix].dot11Rate,
		       rt->info[rix].rateKbps,
		       sc->sc_curmode);

		return 0;
	}
	/* 
	 * XXX getting mac/phy level timings should be fixed for turbo
	 * rates, and there is probably a way to get this from the
	 * hal...
	 */
	switch (rt->info[rix].phy) {
	case IEEE80211_T_OFDM:
		t_slot = 9;
		t_sifs = 16;
		t_difs = 28;
		/* fall through */
	case IEEE80211_T_TURBO:
		t_slot = 9;
		t_sifs = 8;
		t_difs = 28;
		break;
	case IEEE80211_T_DS:
		/* fall through to default */
	default:
		/* pg 205 ieee.802.11.pdf */
		t_slot = 20;
		t_difs = 50;
		t_sifs = 10;
	}

	rts = cts = 0;

	if ((ic->ic_flags & IEEE80211_F_USEPROT) &&
	    rt->info[rix].phy == IEEE80211_T_OFDM) {
		if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
			rts = 1;
		else if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
			cts = 1;

		cix = rt->info[sc->sc_protrix].controlRate;
	}

	if (0 /*length > ic->ic_rtsthreshold */)
		rts = 1;

	if (rts || cts) {
		int ctsrate = rt->info[cix].rateCode;
		int ctsduration = 0;

		if (!rt->info[cix].rateKbps) {
			printk(KERN_WARNING "cix %u (%u) bad ratekbps %u mode %u\n",
			       cix, rt->info[cix].dot11Rate,
			       rt->info[cix].rateKbps,
			       sc->sc_curmode);
			return 0;
		}
		

		ctsrate |= rt->info[cix].shortPreamble;
		if (rts)		/* SIFS + CTS */
			ctsduration += rt->info[cix].spAckDuration;

		ctsduration += ath_hal_computetxtime(sc->sc_ah,
						     rt, length, rix, AH_TRUE);

		if (cts)	/* SIFS + ACK */
			ctsduration += rt->info[cix].spAckDuration;

		tt += (short_retries + 1) * ctsduration;
	}
	tt += t_difs;
	tt += (long_retries+1)*(t_sifs + rt->info[rix].spAckDuration);
	tt += (long_retries+1)*ath_hal_computetxtime(sc->sc_ah, rt, length, 
						rix, AH_TRUE);
	for (x = 0; x <= short_retries + long_retries; x++) {
		cw = MIN(WIFI_CW_MAX, (cw + 1) * 2);
		tt += (t_slot * cw / 2);
	}
	return tt;
}

static void
ath_rate_node_init(struct ath_softc *sc, struct ath_node *an)
{
	/* NB: assumed to be zero'd by caller */
	ath_rate_ctl_reset(sc, &an->an_node);
}

static void
ath_rate_node_cleanup(struct ath_softc *sc, struct ath_node *an)
{
}

#if 0
static void
ath_rate_node_copy(struct ath_softc *sc,
	struct ath_node *dst, const struct ath_node *src)
{
	struct sample_node *odst = ATH_NODE_SAMPLE(dst);
	const struct sample_node *osrc = (const struct sample_node *)&src[1];
	memcpy(odst, osrc, sizeof(struct sample_node));
}
#endif

/*
 * returns the ndx with the lowest average_tx_time,
 * or -1 if all the average_tx_times are 0.
 */
static __inline int best_rate_ndx(struct sample_node *sn, int size_bin, 
	int require_acked_before)
{
	unsigned int x;
        unsigned int best_rate_tt = 0;
        unsigned int best_rate_ndx = -1;
        
	for (x = 0; x < sn->num_rates; x++) {
		unsigned int tt = sn->stats[size_bin][x].average_tx_time;
		if (tt <= 0 || (require_acked_before && 
		    !sn->stats[size_bin][x].packets_acked))
			continue;

		/* 9 megabits never works better than 12 */
		if (sn->rates[x].rate == 18) 
			continue;

		/* don't use a bit-rate that has been failing */
		if (sn->stats[size_bin][x].successive_failures > 3)
			continue;

		if (!best_rate_tt || best_rate_tt > tt) {
			best_rate_tt = tt;
			best_rate_ndx = x;
		}
        }
        return best_rate_ndx;
}

/*
 * pick a good "random" bit-rate to sample other than the current one
 */
static __inline int 
pick_sample_ndx(struct sample_node *sn, int size_bin) 
{
	unsigned int x;
	unsigned current_tt;
	int current_ndx;
	
	current_ndx = sn->current_rate[size_bin];
	if (current_ndx < 0) {
		/* no successes yet, send at the lowest bit-rate */
		return 0;
	}
	
	current_tt = sn->stats[size_bin][current_ndx].average_tx_time;
	
	for (x = 0; x < sn->num_rates; x++) {
		unsigned int ndx = (sn->last_sample_ndx[size_bin] + 1 + x) % sn->num_rates;

	        /* don't sample the current bit-rate */
		if (ndx == current_ndx) 
			continue;

		/* this bit-rate is always worse than the current one */
		if (sn->stats[size_bin][ndx].perfect_tx_time > current_tt) 
			continue;

		/* rarely sample bit-rates that fail a lot */
		if (jiffies - sn->stats[size_bin][ndx].last_tx < ((HZ * STALE_FAILURE_TIMEOUT_MS) / 1000) &&
		    sn->stats[size_bin][ndx].successive_failures > 3)
			continue;

		/* don't sample more than 2 indexes higher 
		 * for rates higher than 11 megabits
		 */
		if (sn->rates[ndx].rate > 22 && ndx > current_ndx + 2)
			continue;

		/* 9 megabits never works better than 12 */
		if (sn->rates[ndx].rate == 18) 
			continue;

		/* if we're using 11 megabits, only sample up to 12 megabits
		 */
		if (sn->rates[current_ndx].rate == 22 && ndx > current_ndx + 1) 
			continue;

		sn->last_sample_ndx[size_bin] = ndx;
		return ndx;
	}
	return current_ndx;
}

static void
ath_rate_findrate(struct ath_softc *sc, struct ath_node *an,
	int shortPreamble, size_t frameLen,
	u_int8_t *rix, int *try0, u_int8_t *txrate)
{
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	struct sample_softc *ssc = ATH_SOFTC_SAMPLE(sc);
	struct ieee80211com *ic = &sc->sc_ic;
	unsigned int size_bin, mrr, change_rates;
	int ndx, best_ndx;
	unsigned average_tx_time;

	if (sn->num_rates <= 0) {
		printk(KERN_WARNING "%s: no rates for %s?\n",
		       dev_info, 
		       ether_sprintf(an->an_node.ni_macaddr));
		return;
	}

	mrr = sc->sc_mrretry && !(ic->ic_flags & IEEE80211_F_USEPROT) && ENABLE_MRR;
	size_bin = size_to_bin(frameLen);
	best_ndx = best_rate_ndx(sn, size_bin, !mrr);

	if (best_ndx >= 0)
		average_tx_time = sn->stats[size_bin][best_ndx].average_tx_time;
	else
		average_tx_time = 0;
	
	if (sn->static_rate_ndx != -1) {
		ndx = sn->static_rate_ndx;
		*try0 = ATH_TXMAXTRY;
	} else {
		*try0 = mrr ? 2 : ATH_TXMAXTRY;
		
		if (sn->sample_tt[size_bin] < average_tx_time * (sn->packets_since_sample[size_bin] * ssc->ath_sample_rate / 100)) {
			/*
			 * we want to limit the time measuring the performance
			 * of other bit-rates to ath_sample_rate% of the
			 * total transmission time.
			 */
			ndx = pick_sample_ndx(sn, size_bin);
			if (ndx != sn->current_rate[size_bin])
				sn->current_sample_ndx[size_bin] = ndx;
			else
				sn->current_sample_ndx[size_bin] = -1;
			sn->packets_since_sample[size_bin] = 0;

		} else {
			change_rates = 0;
			if (!sn->packets_sent[size_bin] || best_ndx == -1) {
				/* no packet has been sent successfully yet, so
				 * pick an rssi-appropriate bit-rate. We know if
				 * the rssi is very low that the really high
				 * bit rates will not work.
				 */
				int initial_rate = 72;
				if (an->an_avgrssi > 50) {
					initial_rate = 108; /* 54 mbps */
				} else if (an->an_avgrssi > 30) {
					initial_rate = 72; /* 36 mbps */
				} else {
					initial_rate = 22; /* 11 mbps */
				}

				for (ndx = sn->num_rates-1; ndx > 0; ndx--) {
					/* 
					 * pick the highest rate <= initial_rate/2 Mbps
					 * that hasn't failed.
					 */
					if (sn->rates[ndx].rate <= initial_rate && 
					    sn->stats[size_bin][ndx].successive_failures == 0)
						break;
				}
				change_rates = 1;
				best_ndx = ndx;
			} else if (sn->packets_sent[size_bin] < 20) {
				/* let the bit-rate switch quickly during the first few packets */
				change_rates = 1;
			} else if (jiffies - ((HZ * MIN_SWITCH_MS) / 1000) > sn->jiffies_since_switch[size_bin]) {
				/* 2 seconds have gone by */
				change_rates = 1;
			} else if (average_tx_time * 2 < sn->stats[size_bin][sn->current_rate[size_bin]].average_tx_time) {
				/* the current bit-rate is twice as slow as the best one */
				change_rates = 1;
			}

			sn->packets_since_sample[size_bin]++;
			
			if (change_rates) {
				if (best_ndx != sn->current_rate[size_bin]) {
					DPRINTF(sc, "%s: %s size %u switch rate %u (%u/%u) -> %u (%u/%u) after %u packets mrr %u\n",
						dev_info,
						ether_sprintf(an->an_node.ni_macaddr),
						packet_size_bins[size_bin],
						sn->rates[sn->current_rate[size_bin]].rate,
						sn->stats[size_bin][sn->current_rate[size_bin]].average_tx_time,
						sn->stats[size_bin][sn->current_rate[size_bin]].perfect_tx_time,
						sn->rates[best_ndx].rate,
						sn->stats[size_bin][best_ndx].average_tx_time,
						sn->stats[size_bin][best_ndx].perfect_tx_time,
						sn->packets_since_switch[size_bin],
						mrr);
				}
				sn->packets_since_switch[size_bin] = 0;
				sn->current_rate[size_bin] = best_ndx;
				sn->jiffies_since_switch[size_bin] = jiffies;
			}
			ndx = sn->current_rate[size_bin];
			sn->packets_since_switch[size_bin]++;
			if (size_bin == 0) {
	    			/* 
	    			 * set the visible txrate for this node
			         * to the rate of small packets
			         */
				an->an_node.ni_txrate = ndx;
			}
		}
	}

	KASSERT(ndx >= 0 && ndx < sn->num_rates, 
		("%s: bad ndx (%u/%u) for %s?\n",
		 dev_info, ndx, sn->num_rates, 
		 ether_sprintf(an->an_node.ni_macaddr)));
		

	*rix = sn->rates[ndx].rix;
	if (shortPreamble)
		*txrate = sn->rates[ndx].shortPreambleRateCode;
	else
		*txrate = sn->rates[ndx].rateCode;
	sn->packets_sent[size_bin]++;
}

static void
ath_rate_setupxtxdesc(struct ath_softc *sc, struct ath_node *an,
	struct ath_desc *ds, int shortPreamble, size_t frame_size, u_int8_t rix)
{
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	unsigned int size_bin;
	int ndx;
	int rateCode;

	size_bin = size_to_bin(frame_size);
	ndx = sn->current_rate[size_bin]; /* retry at the current bit-rate */
	
	if (!sn->stats[size_bin][ndx].packets_acked)
		ndx = 0;  /* use the lowest bit-rate */

	if (shortPreamble)
		rateCode = sn->rates[ndx].shortPreambleRateCode;
	else
		rateCode = sn->rates[ndx].rateCode;
	ath_hal_setupxtxdesc(sc->sc_ah, ds, 
			     rateCode, 3, 	        /* series 1 */
			     sn->rates[0].rateCode, 3, 	/* series 2 */
			     0, 0	                /* series 3 */
			    );
}

static void 
update_stats(struct ath_softc *sc, struct ath_node *an, 
	int frame_size,
	int ndx0, int tries0,
	int ndx1, int tries1,
	int ndx2, int tries2,
	int ndx3, int tries3,
	int short_tries, int tries, int status)
{
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	struct sample_softc *ssc = ATH_SOFTC_SAMPLE(sc);
	unsigned int tt = 0;
	unsigned int tries_so_far = 0;
	unsigned int size_bin;
	unsigned int size;
	unsigned int rate;

	size_bin = size_to_bin(frame_size);
	size = bin_to_size(size_bin);
	rate = sn->rates[ndx0].rate;

	if (!rt->info[ndx0].rateKbps) {
		/* 
		 * sometimes we get feedback back for packets we didn't send. 
		 * just ignore these packets.
		 */
		return;
	}
	tt += calc_usecs_unicast_packet(sc, size, sn->rates[ndx0].rix, 
		short_tries-1, 
		MIN(tries0, tries) - 1);
	tries_so_far += tries0;
	if (tries1 && tries0 < tries) {
		tt += calc_usecs_unicast_packet(sc, size, sn->rates[ndx1].rix, 
			short_tries - 1, 
			MIN(tries1 + tries_so_far, tries) - tries_so_far - 1);
	}
	tries_so_far += tries1;

	if (tries2 && tries0 + tries1 < tries) {
		tt += calc_usecs_unicast_packet(sc, size, sn->rates[ndx2].rix, 
			short_tries - 1,
			MIN(tries2 + tries_so_far, tries) - tries_so_far - 1);
	}

	tries_so_far += tries2;

	if (tries3 && tries0 + tries1 + tries2 < tries) {
		tt += calc_usecs_unicast_packet(sc, size, sn->rates[ndx3].rix, 
			short_tries - 1, 
			MIN(tries3 + tries_so_far, tries) - tries_so_far - 1);
	}
	
	if (sn->stats[size_bin][ndx0].total_packets < (100 / (100 - ssc->ath_smoothing_rate))) {
		/* just average the first few packets */
		unsigned int avg_tx = sn->stats[size_bin][ndx0].average_tx_time;
		unsigned int packets = sn->stats[size_bin][ndx0].total_packets;
		sn->stats[size_bin][ndx0].average_tx_time =
			(tt + (avg_tx * packets)) / (packets + 1);
	} else {
		/* use a ewma */
		sn->stats[size_bin][ndx0].average_tx_time = 
			((sn->stats[size_bin][ndx0].average_tx_time * ssc->ath_smoothing_rate) + 
			 (tt * (100 - ssc->ath_smoothing_rate))) / 100;
	}
	
	if (status) {
		unsigned int y;
		sn->stats[size_bin][ndx0].successive_failures++;
		for (y = size_bin + 1; y < NUM_PACKET_SIZE_BINS; y++) {
			/* also say larger packets failed since we
			 * assume if a small packet fails at a lower
			 * bit-rate then a larger one will also.
			 */
			sn->stats[y][ndx0].successive_failures++;
			sn->stats[y][ndx0].last_tx = jiffies;
			sn->stats[y][ndx0].tries += tries;
			sn->stats[y][ndx0].total_packets++;
		}
	} else {
		sn->stats[size_bin][ndx0].packets_acked++;
		sn->stats[size_bin][ndx0].successive_failures = 0;
	}
	sn->stats[size_bin][ndx0].tries += tries;
	sn->stats[size_bin][ndx0].last_tx = jiffies;
	sn->stats[size_bin][ndx0].total_packets++;


	if (ndx0 == sn->current_sample_ndx[size_bin]) {
		DPRINTF(sc, "%s: %s size %u sample rate %u tries (%u/%u) tt %u avg_tt (%u/%u) status %u\n", 
			dev_info, ether_sprintf(an->an_node.ni_macaddr), 
			size, rate, short_tries, tries, tt, 
			sn->stats[size_bin][ndx0].average_tx_time,
			sn->stats[size_bin][ndx0].perfect_tx_time,
			status);
		sn->sample_tt[size_bin] = tt;
		sn->current_sample_ndx[size_bin] = -1;
	}
}

static void
ath_rate_tx_complete(struct ath_softc *sc,
	struct ath_node *an, const struct ath_desc *ds)
{
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	struct ieee80211com *ic = &sc->sc_ic;
	const struct ar5212_desc *ads = (const struct ar5212_desc *)&ds->ds_ctl0;
	unsigned int final_rate;
	unsigned int short_tries;
	unsigned int long_tries;
	unsigned int frame_size;
	unsigned int mrr;

	final_rate = sc->sc_hwmap[ds->ds_txstat.ts_rate &~ HAL_TXSTAT_ALTRATE].ieeerate;
	short_tries = ds->ds_txstat.ts_shortretry + 1;
	long_tries = ds->ds_txstat.ts_longretry + 1;
	frame_size = ds->ds_ctl0 & 0x0fff; /* low-order 12 bits of ds_ctl0 */

	if (frame_size == 0)
		frame_size = 1500;

	if (sn->num_rates <= 0) {
		DPRINTF(sc, "%s: %s %s no rates yet\n", dev_info, 
			ether_sprintf(an->an_node.ni_macaddr), __func__);
		return;
	}

	mrr = sc->sc_mrretry && !(ic->ic_flags & IEEE80211_F_USEPROT) && ENABLE_MRR;


	if (sc->sc_mrretry && ds->ds_txstat.ts_status) {
		/* this packet failed */
		DPRINTF(sc, "%s: %s size %u rate/try %u/%u %u/%u %u/%u %u/%u status %s retries (%u/%u)\n", 
			dev_info,
			ether_sprintf(an->an_node.ni_macaddr),
			bin_to_size(size_to_bin(frame_size)),
			sc->sc_hwmap[ads->xmit_rate0].ieeerate, ads->xmit_tries0,
			sc->sc_hwmap[ads->xmit_rate1].ieeerate, ads->xmit_tries1,
			sc->sc_hwmap[ads->xmit_rate2].ieeerate, ads->xmit_tries2,
			sc->sc_hwmap[ads->xmit_rate3].ieeerate, ads->xmit_tries3,
			ds->ds_txstat.ts_status ? "FAIL" : "OK",
			short_tries, 
			long_tries);
	}


	if (!mrr || !(ds->ds_txstat.ts_rate & HAL_TXSTAT_ALTRATE)) {
		/* only one rate was used */
		int ndx = rate_to_ndx(sn, final_rate);
		if (ndx >= 0 && ndx < sn->num_rates) {
			update_stats(sc, an, frame_size, 
				ndx, long_tries,
				0, 0,
				0, 0,
				0, 0,
				short_tries, long_tries, ds->ds_txstat.ts_status);
		}
	} else {
		unsigned int rate[4], tries[4];
		int ndx[4];
		int finalTSIdx = ads->final_ts_index;

		/*
		 * Process intermediate rates that failed.
		 */

		rate[0] = sc->sc_hwmap[ads->xmit_rate0].ieeerate;
		tries[0] = ads->xmit_tries0;
		ndx[0] = rate_to_ndx(sn, rate[0]);
		
		rate[1] = sc->sc_hwmap[ads->xmit_rate1].ieeerate;
		tries[1] = ads->xmit_tries1;
		ndx[1] = rate_to_ndx(sn, rate[1]);
		
		rate[2] = sc->sc_hwmap[ads->xmit_rate2].ieeerate;
		tries[2] = ads->xmit_tries2;
		ndx[2] = rate_to_ndx(sn, rate[2]);
		
		rate[3] = sc->sc_hwmap[ads->xmit_rate3].ieeerate;
		tries[3] = ads->xmit_tries3;
		ndx[3] = rate_to_ndx(sn, rate[3]);
		
#if 0
		DPRINTF(sc, "%s: %s size %u finaltsidx %u tries %u status %u rate/try %u/%u %u/%u %u/%u %u/%u\n", 
			dev_info, ether_sprintf(an->an_node.ni_macaddr),
			bin_to_size(size_to_bin(frame_size)),
			finalTSIdx,
			long_tries, 
			ds->ds_txstat.ts_status,
			rate[0], tries[0],
			rate[1], tries[1],
			rate[2], tries[2],
			rate[3], tries[3]);
#endif

		if (tries[0])
			update_stats(sc, an, frame_size, 
				ndx[0], tries[0], 
				ndx[1], tries[1], 
				ndx[2], tries[2], 
				ndx[3], tries[3], 
				short_tries, ds->ds_txstat.ts_longretry + 1, 
				long_tries > tries[0]);
		
		if (tries[1] && finalTSIdx > 0)
			update_stats(sc, an, frame_size, 
				ndx[1], tries[1], 
				ndx[2], tries[2], 
				ndx[3], tries[3], 
				0, 0, 
				short_tries, ds->ds_txstat.ts_longretry + 1 - tries[0], 
				ds->ds_txstat.ts_status);

		if (tries[2] && finalTSIdx > 1)
			update_stats(sc, an, frame_size, 
				ndx[2], tries[2], 
				ndx[3], tries[3], 
				0, 0,
				0, 0,
				short_tries, ds->ds_txstat.ts_longretry + 1 - tries[0] - tries[1], 
				ds->ds_txstat.ts_status);

		if (tries[3] && finalTSIdx > 2)
			update_stats(sc, an, frame_size, 
				ndx[3], tries[3], 
				0, 0,
				0, 0,
				0, 0,
				short_tries, ds->ds_txstat.ts_longretry + 1 - tries[0] - tries[1] - tries[2], 
				ds->ds_txstat.ts_status);
	}
}

static void
ath_rate_newassoc(struct ath_softc *sc, struct ath_node *an, int isnew)
{
	DPRINTF(sc, "%s: %s %s\n", dev_info,
		ether_sprintf(an->an_node.ni_macaddr), __func__);
	if (isnew)
		ath_rate_ctl_reset(sc, &an->an_node);
}

/*
 * Initialize the tables for a node.
 */
static void
ath_rate_ctl_reset(struct ath_softc *sc, struct ieee80211_node *ni)
{
	struct ath_node *an = ATH_NODE(ni);
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	struct ieee80211vap *vap = ni->ni_vap;
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	unsigned int x, y;
	unsigned int srate;
	sn->num_rates = 0;

	if (rt == NULL) {
		printk(KERN_WARNING "no rates yet! mode %u\n", sc->sc_curmode);
		return;
	}
        sn->static_rate_ndx = -1;

	sn->num_rates = ni->ni_rates.rs_nrates;
        for (x = 0; x < ni->ni_rates.rs_nrates; x++) {
		sn->rates[x].rate = ni->ni_rates.rs_rates[x] & IEEE80211_RATE_VAL;
		sn->rates[x].rix = sc->sc_rixmap[sn->rates[x].rate];
		if (sn->rates[x].rix == 0xff) {
			DPRINTF(sc, "%s: %s ignore bogus rix at %u\n",
				dev_info, __func__, x);
			continue;
		}
		sn->rates[x].rateCode = rt->info[sn->rates[x].rix].rateCode;
		sn->rates[x].shortPreambleRateCode = 
			rt->info[sn->rates[x].rix].rateCode | 
			rt->info[sn->rates[x].rix].shortPreamble;
	}
	
	ni->ni_txrate = 0;
	sn->num_rates = ni->ni_rates.rs_nrates;

	if (sn->num_rates <= 0) {
		DPRINTF(sc, "%s: %s %s no rates (fixed %u) \n",
			dev_info, __func__, ether_sprintf(ni->ni_macaddr),
			vap->iv_fixed_rate);
		/* there are no rates yet we're done */
		return;
	}

	if (vap->iv_fixed_rate != -1) {
		srate = sn->num_rates - 1;

		/*
		 * A fixed rate is to be used; ic_fixed_rate is an
		 * index into the supported rate set.  Convert this
		 * to the index into the negotiated rate set for
		 * the node.  We know the rate is there because the
		 * rate set is checked when the station associates.
		 */
		/* NB: the rate set is assumed sorted */
		for (; srate >= 0 && (ni->ni_rates.rs_rates[srate] & IEEE80211_RATE_VAL) != vap->iv_fixed_rate; srate--);

		KASSERT(srate >= 0,
			("fixed rate %u not in rate set", vap->iv_fixed_rate));

		sn->static_rate_ndx = srate;
		ni->ni_txrate = srate;
		DPRINTF(sc, "%s: %s %s fixed rate %u%sMbps\n",
			dev_info, __func__, ether_sprintf(ni->ni_macaddr), 
			sn->rates[srate].rate / 2,
			(sn->rates[srate].rate % 0x1) ? ".5" : " ");
		return;
	}
	

	for (y = 0; y < NUM_PACKET_SIZE_BINS; y++) {
		unsigned int size = bin_to_size(y);
		int ndx = 0;
		sn->packets_sent[y] = 0;
		sn->current_sample_ndx[y] = -1;
		sn->last_sample_ndx[y] = 0;
		
		for (x = 0; x < ni->ni_rates.rs_nrates; x++) {
			sn->stats[y][x].successive_failures = 0;
			sn->stats[y][x].tries = 0;
			sn->stats[y][x].total_packets = 0;
			sn->stats[y][x].packets_acked = 0;
			sn->stats[y][x].last_tx = 0;

			sn->stats[y][x].perfect_tx_time = 
				calc_usecs_unicast_packet(sc, size, 
					sn->rates[x].rix,
					0, 0);
			sn->stats[y][x].average_tx_time =
				sn->stats[y][x].perfect_tx_time;

		}

		/* set the initial rate */
		for (ndx = sn->num_rates - 1; ndx > 0; ndx--)
			if (sn->rates[ndx].rate <= 72)
				break;
		sn->current_rate[y] = ndx;
	}

	DPRINTF(sc, "%s: %s %s %u rates %u%sMbps (%uus)- %u%sMbps (%uus)\n",
		dev_info, __func__, ether_sprintf(ni->ni_macaddr), 
		sn->num_rates,
		sn->rates[0].rate / 2, sn->rates[0].rate % 0x1 ? ".5" : "",
		sn->stats[1][0].perfect_tx_time,
		sn->rates[sn->num_rates-1].rate / 2,
		sn->rates[sn->num_rates-1].rate % 0x1 ? ".5" : "",
		sn->stats[1][sn->num_rates-1].perfect_tx_time);

	ni->ni_txrate = sn->current_rate[0];
}

static void
ath_rate_cb(void *arg, struct ieee80211_node *ni)
{
        ath_rate_ctl_reset(ni->ni_ic->ic_dev->priv, ni);
}

/*
 * Reset the rate control state for each 802.11 state transition.
 */
static void
ath_rate_newstate(struct ieee80211vap *vap, enum ieee80211_state newstate)
{
	struct ieee80211com *ic = vap->iv_ic;
	
	if (newstate == IEEE80211_S_RUN) {
		if (ic->ic_opmode != IEEE80211_M_STA) {
			/*
			 * Sync rates for associated stations and neighbors.
			 */
			ieee80211_iterate_nodes(&ic->ic_sta, ath_rate_cb, NULL);
		}
		ath_rate_newassoc(ic->ic_dev->priv, ATH_NODE(vap->iv_bss), 1);
	}
}

static struct ath_ratectrl *
ath_rate_attach(struct ath_softc *sc)
{
	struct sample_softc *osc;
	DPRINTF(sc, "%s: %s\n", dev_info, __func__);
	
	_MOD_INC_USE(THIS_MODULE, return NULL);
	osc = kmalloc(sizeof(struct sample_softc), GFP_ATOMIC);
	if (osc == NULL) {
		_MOD_DEC_USE(THIS_MODULE);
		return NULL;
	}
	osc->arc.arc_space = sizeof(struct sample_node);
	osc->arc.arc_vap_space = 0;

	osc->ath_smoothing_rate = ath_smoothing_rate;
	osc->ath_sample_rate = ath_sample_rate;

	return &osc->arc;
}

static void
ath_rate_detach(struct ath_ratectrl *arc)
{
	struct sample_softc *osc = (struct sample_softc *) arc;
	kfree(osc);
	_MOD_DEC_USE(THIS_MODULE);
}

static int
proc_read_nodes(struct ieee80211vap *vap, const int size, char *buf, int space)
{
	char *p = buf;
	struct ieee80211_node *ni;
	struct ath_node *an;
	struct sample_node *sn;
	struct ieee80211_node_table *nt = 
		(struct ieee80211_node_table *) &vap->iv_ic->ic_sta;
	unsigned int x;
	unsigned int size_bin;

	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		/* Assume each node needs 500 bytes */
		if (buf + space < p + 500)
			break;
		an = ATH_NODE(ni);
		sn = ATH_NODE_SAMPLE(an);
		/* Skip ourself */
		if (memcmp(vap->iv_myaddr, ni->ni_macaddr, 
					IEEE80211_ADDR_LEN)==0) {
			continue;
		}

		size_bin = size_to_bin(size);
		p += sprintf(p, "%s\n", ether_sprintf(ni->ni_macaddr));
		p += sprintf(p, 
				"rate\ttt\tperfect\tfailed\tpkts\tavg_tries\tlast_tx\n");
		for (x = 0; x < sn->num_rates; x++) {
			unsigned int a = 1;
			unsigned int t = 1;

			p += sprintf(p, "%s", 
					(x == sn->current_rate[size_bin]) ? "*" : " ");

			p += sprintf(p, "%3u%s",
					sn->rates[x].rate/2,
					(sn->rates[x].rate & 0x1) != 0 ? ".5" : "  ");

			p += sprintf(p, "\t%4u\t%4u\t%2u\t%3u",
					sn->stats[size_bin][x].average_tx_time,
					sn->stats[size_bin][x].perfect_tx_time,
					sn->stats[size_bin][x].successive_failures,
					sn->stats[size_bin][x].total_packets);

			if (sn->stats[size_bin][x].total_packets) {
				a = sn->stats[size_bin][x].total_packets;
				t = sn->stats[size_bin][x].tries;
			}
			p += sprintf(p, "\t%u.%02u\t", t/a, (t*100/a) % 100);
			
			if (sn->stats[size_bin][x].last_tx) {
				unsigned d = jiffies - 
					sn->stats[size_bin][x].last_tx;
				p += sprintf(p, "%u.%02u", d / HZ, d % HZ);
			} else {
				p += sprintf(p, "-");
			}
			p += sprintf(p, "\n");
		}
		printk("\n");
	}

	return (p - buf);
}

static int
proc_ratesample_open(struct inode *inode, struct file *file)
{
	struct proc_ieee80211_priv *pv;
	struct proc_dir_entry *dp = PDE(inode);
	struct ieee80211vap *vap = dp->data;
	unsigned long size;

	if (!(file->private_data = kmalloc(sizeof(struct proc_ieee80211_priv),
					GFP_KERNEL)))
		return -ENOMEM;

	/* initially allocate both read and write buffers */
	pv = (struct proc_ieee80211_priv *) file->private_data;
	memset(pv, 0, sizeof(struct proc_ieee80211_priv));
	pv->rbuf = vmalloc(MAX_PROC_IEEE80211_SIZE);
	if (!pv->rbuf) {
		kfree(pv);
		return -ENOMEM;
	}
	pv->wbuf = vmalloc(MAX_PROC_IEEE80211_SIZE);
	if (!pv->wbuf) {
		vfree(pv->rbuf);
		kfree(pv);
		return -ENOMEM;
	}
	memset(pv->wbuf, 0, MAX_PROC_IEEE80211_SIZE);
	memset(pv->rbuf, 0, MAX_PROC_IEEE80211_SIZE);
	pv->max_wlen = MAX_PROC_IEEE80211_SIZE;
	pv->max_rlen = MAX_PROC_IEEE80211_SIZE;

	/* Determine what size packets to get stats for based on proc filename */
	size = simple_strtoul(dp->name + 10, NULL, 0);

	/* now read the data into the buffer */
	pv->rlen = proc_read_nodes(vap, size, pv->rbuf, MAX_PROC_IEEE80211_SIZE);
	return 0;
}

static struct file_operations proc_ratesample_ops = {
	.read = NULL,
	.write = NULL,
	.open = proc_ratesample_open,
	.release = NULL,
};

static void
ath_rate_dynamic_proc_register(struct ieee80211vap *vap)
{		
	/* Create proc entries for the rate control algorithm */
	ieee80211_proc_vcreate(vap, &proc_ratesample_ops, "ratestats_250");
	ieee80211_proc_vcreate(vap, &proc_ratesample_ops, "ratestats_1600");
	ieee80211_proc_vcreate(vap, &proc_ratesample_ops, "ratestats_3000");	
}

static struct ieee80211_rate_ops ath_rate_ops = {
	.ratectl_id = IEEE80211_RATE_SAMPLE,
	.node_init = ath_rate_node_init,
	.node_cleanup = ath_rate_node_cleanup,
	.findrate = ath_rate_findrate,
	.setupxtxdesc = ath_rate_setupxtxdesc,
	.tx_complete = ath_rate_tx_complete,
	.newassoc = ath_rate_newassoc,
	.newstate = ath_rate_newstate,
	.attach = ath_rate_attach,
	.detach = ath_rate_detach,
	.dynamic_proc_register = ath_rate_dynamic_proc_register,
};

MODULE_AUTHOR("John Bicket");
MODULE_DESCRIPTION("SampleRate bit-rate selection algorithm for Atheros devices");
#ifdef MODULE_VERSION
MODULE_VERSION(RELEASE_VERSION);
#endif
#ifdef MODULE_LICENSE
MODULE_LICENSE("BSD");
#endif

static int __init
init_ath_rate_sample(void)
{
	printk(KERN_INFO "%s: %s\n", dev_info, version);
	return ieee80211_rate_register(&ath_rate_ops);
}
module_init(init_ath_rate_sample);

static void __exit
exit_ath_rate_sample(void)
{
	ieee80211_rate_unregister(&ath_rate_ops);
	printk(KERN_INFO "%s: unloaded\n", dev_info);
}
module_exit(exit_ath_rate_sample);
