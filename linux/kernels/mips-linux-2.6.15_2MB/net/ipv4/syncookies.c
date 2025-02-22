/*
 *  Syncookies implementation for the Linux kernel
 *
 *  Copyright (C) 1997 Andi Kleen
 *  Based on ideas by D.J.Bernstein and Eric Schenk. 
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 * 
 *  $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15_2MB/net/ipv4/syncookies.c#1 $
 *
 *  Missing: IPv6 support. 
 */

#include <linux/tcp.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/cryptohash.h>
#include <linux/kernel.h>
#include <net/tcp.h>

extern int sysctl_tcp_syncookies;

static __u32 syncookie_secret[2][16-3+SHA_DIGEST_WORDS];

static __init int init_syncookies(void)
{
	get_random_bytes(syncookie_secret, sizeof(syncookie_secret));
	return 0;
}
module_init(init_syncookies);

#define COOKIEBITS 24	/* Upper bits store count */
#define COOKIEMASK (((__u32)1 << COOKIEBITS) - 1)

static u32 cookie_hash(u32 saddr, u32 daddr, u32 sport, u32 dport,
		       u32 count, int c)
{
	__u32 tmp[16 + 5 + SHA_WORKSPACE_WORDS];

	memcpy(tmp + 3, syncookie_secret[c], sizeof(syncookie_secret[c]));
	tmp[0] = saddr;
	tmp[1] = daddr;
	tmp[2] = (sport << 16) + dport;
	tmp[3] = count;
	sha_transform(tmp + 16, (__u8 *)tmp, tmp + 16 + 5);

	return tmp[17];
}

static __u32 secure_tcp_syn_cookie(__u32 saddr, __u32 daddr, __u16 sport,
				   __u16 dport, __u32 sseq, __u32 count,
				   __u32 data)
{
	/*
	 * Compute the secure sequence number.
	 * The output should be:
   	 *   HASH(sec1,saddr,sport,daddr,dport,sec1) + sseq + (count * 2^24)
	 *      + (HASH(sec2,saddr,sport,daddr,dport,count,sec2) % 2^24).
	 * Where sseq is their sequence number and count increases every
	 * minute by 1.
	 * As an extra hack, we add a small "data" value that encodes the
	 * MSS into the second hash value.
	 */

	return (cookie_hash(saddr, daddr, sport, dport, 0, 0) +
		sseq + (count << COOKIEBITS) +
		((cookie_hash(saddr, daddr, sport, dport, count, 1) + data)
		 & COOKIEMASK));
}

/*
 * This retrieves the small "data" value from the syncookie.
 * If the syncookie is bad, the data returned will be out of
 * range.  This must be checked by the caller.
 *
 * The count value used to generate the cookie must be within
 * "maxdiff" if the current (passed-in) "count".  The return value
 * is (__u32)-1 if this test fails.
 */
static __u32 check_tcp_syn_cookie(__u32 cookie, __u32 saddr, __u32 daddr,
				  __u16 sport, __u16 dport, __u32 sseq,
				  __u32 count, __u32 maxdiff)
{
	__u32 diff;

	/* Strip away the layers from the cookie */
	cookie -= cookie_hash(saddr, daddr, sport, dport, 0, 0) + sseq;

	/* Cookie is now reduced to (count * 2^24) ^ (hash % 2^24) */
	diff = (count - (cookie >> COOKIEBITS)) & ((__u32) - 1 >> COOKIEBITS);
	if (diff >= maxdiff)
		return (__u32)-1;

	return (cookie -
		cookie_hash(saddr, daddr, sport, dport, count - diff, 1))
		& COOKIEMASK;	/* Leaving the data behind */
}

/* 
 * This table has to be sorted and terminated with (__u16)-1.
 * XXX generate a better table.
 * Unresolved Issues: HIPPI with a 64k MSS is not well supported.
 */
static __u16 const msstab[] = {
	64 - 1,
	256 - 1,	
	512 - 1,
	536 - 1,
	1024 - 1,	
	1440 - 1,
	1460 - 1,
	4312 - 1,
	(__u16)-1
};
/* The number doesn't include the -1 terminator */
#define NUM_MSS (ARRAY_SIZE(msstab) - 1)

/*
 * Generate a syncookie.  mssp points to the mss, which is returned
 * rounded down to the value encoded in the cookie.
 */
__u32 cookie_v4_init_sequence(struct sock *sk, struct sk_buff *skb, __u16 *mssp)
{
	struct tcp_sock *tp = tcp_sk(sk);
	int mssind;
	const __u16 mss = *mssp;

	
	tp->last_synq_overflow = jiffies;

	/* XXX sort msstab[] by probability?  Binary search? */
	for (mssind = 0; mss > msstab[mssind + 1]; mssind++)
		;
	*mssp = msstab[mssind] + 1;

	NET_INC_STATS_BH(LINUX_MIB_SYNCOOKIESSENT);

	return secure_tcp_syn_cookie(skb->nh.iph->saddr, skb->nh.iph->daddr,
				     skb->h.th->source, skb->h.th->dest,
				     ntohl(skb->h.th->seq),
				     jiffies / (HZ * 60), mssind);
}

/* 
 * This (misnamed) value is the age of syncookie which is permitted.
 * Its ideal value should be dependent on TCP_TIMEOUT_INIT and
 * sysctl_tcp_retries1. It's a rather complicated formula (exponential
 * backoff) to compute at runtime so it's currently hardcoded here.
 */
#define COUNTER_TRIES 4
/*  
 * Check if a ack sequence number is a valid syncookie. 
 * Return the decoded mss if it is, or 0 if not.
 */
static inline int cookie_check(struct sk_buff *skb, __u32 cookie)
{
	__u32 seq; 
	__u32 mssind;

	seq = ntohl(skb->h.th->seq)-1; 
	mssind = check_tcp_syn_cookie(cookie,
				      skb->nh.iph->saddr, skb->nh.iph->daddr,
				      skb->h.th->source, skb->h.th->dest,
				      seq, jiffies / (HZ * 60), COUNTER_TRIES);

	return mssind < NUM_MSS ? msstab[mssind] + 1 : 0;
}

static inline struct sock *get_cookie_sock(struct sock *sk, struct sk_buff *skb,
					   struct request_sock *req,
					   struct dst_entry *dst)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct sock *child;

	child = tp->af_specific->syn_recv_sock(sk, skb, req, dst);
	if (child)
		inet_csk_reqsk_queue_add(sk, req, child);
	else
		reqsk_free(req);

	return child;
}

struct sock *cookie_v4_check(struct sock *sk, struct sk_buff *skb,
			     struct ip_options *opt)
{
	struct inet_request_sock *ireq;
	struct tcp_request_sock *treq;
	struct tcp_sock *tp = tcp_sk(sk);
	__u32 cookie = ntohl(skb->h.th->ack_seq) - 1; 
	struct sock *ret = sk;
	struct request_sock *req; 
	int mss; 
	struct rtable *rt; 
	__u8 rcv_wscale;

	if (!sysctl_tcp_syncookies || !skb->h.th->ack)
		goto out;

  	if (time_after(jiffies, tp->last_synq_overflow + TCP_TIMEOUT_INIT) ||
	    (mss = cookie_check(skb, cookie)) == 0) {
	 	NET_INC_STATS_BH(LINUX_MIB_SYNCOOKIESFAILED);
		goto out;
	}

	NET_INC_STATS_BH(LINUX_MIB_SYNCOOKIESRECV);

	ret = NULL;
	req = reqsk_alloc(&tcp_request_sock_ops); /* for safety */
	if (!req)
		goto out;

	ireq = inet_rsk(req);
	treq = tcp_rsk(req);
	treq->rcv_isn		= htonl(skb->h.th->seq) - 1;
	treq->snt_isn		= cookie; 
	req->mss		= mss;
 	ireq->rmt_port		= skb->h.th->source;
	ireq->loc_addr		= skb->nh.iph->daddr;
	ireq->rmt_addr		= skb->nh.iph->saddr;
	ireq->opt		= NULL;

	/* We throwed the options of the initial SYN away, so we hope
	 * the ACK carries the same options again (see RFC1122 4.2.3.8)
	 */
	if (opt && opt->optlen) {
		int opt_size = sizeof(struct ip_options) + opt->optlen;

		ireq->opt = kmalloc(opt_size, GFP_ATOMIC);
		if (ireq->opt != NULL && ip_options_echo(ireq->opt, skb)) {
			kfree(ireq->opt);
			ireq->opt = NULL;
		}
	}

	ireq->snd_wscale = ireq->rcv_wscale = ireq->tstamp_ok = 0;
	ireq->wscale_ok	 = ireq->sack_ok = 0; 
	req->expires	= 0UL; 
	req->retrans	= 0; 
	
	/*
	 * We need to lookup the route here to get at the correct
	 * window size. We should better make sure that the window size
	 * hasn't changed since we received the original syn, but I see
	 * no easy way to do this. 
	 */
	{
		struct flowi fl = { .nl_u = { .ip4_u =
					      { .daddr = ((opt && opt->srr) ?
							  opt->faddr :
							  ireq->rmt_addr),
						.saddr = ireq->loc_addr,
						.tos = RT_CONN_FLAGS(sk) } },
				    .proto = IPPROTO_TCP,
				    .uli_u = { .ports =
					       { .sport = skb->h.th->dest,
						 .dport = skb->h.th->source } } };
		if (ip_route_output_key(&rt, &fl)) {
			reqsk_free(req);
			goto out; 
		}
	}

	/* Try to redo what tcp_v4_send_synack did. */
	req->window_clamp = dst_metric(&rt->u.dst, RTAX_WINDOW);
	tcp_select_initial_window(tcp_full_space(sk), req->mss,
				  &req->rcv_wnd, &req->window_clamp, 
				  0, &rcv_wscale);
	/* BTW win scale with syncookies is 0 by definition */
	ireq->rcv_wscale  = rcv_wscale; 

	ret = get_cookie_sock(sk, skb, req, &rt->u.dst);
out:	return ret;
}
