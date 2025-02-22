/*
 *  Copyright (c) 2008 Atheros Communications Inc.
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

#include <osdep.h>

#include <ieee80211_var.h>

static int
_ieee80211_crypto_delkey(struct ieee80211vap *,
                         struct ieee80211_key *,
                         struct ieee80211_node *);

/*
 * Default "null" key management routines.
 */
static int
null_key_alloc(struct ieee80211vap *vap, struct ieee80211_key *k)
{
    return IEEE80211_KEYIX_NONE;
}
static int
null_key_delete(struct ieee80211vap *vap, const struct ieee80211_key *k,
                struct ieee80211_node *ni)
{
    return 1;
}
static     int
null_key_set(struct ieee80211vap *vap, const struct ieee80211_key *k,
         const u_int8_t mac[IEEE80211_ADDR_LEN])
{
    return 1;
}
static void null_key_update(struct ieee80211vap *vap) {}


/*
 * Write-arounds for common operations.
 */
static INLINE void
cipher_detach(struct ieee80211_key *key)
{
    key->wk_cipher->ic_detach(key);
}

static INLINE void *
cipher_attach(struct ieee80211vap *vap, struct ieee80211_key *key)
{
    return key->wk_cipher->ic_attach(vap, key);
}

/*
 * Wrappers for driver key management methods.
 */
static INLINE int
dev_key_alloc(struct ieee80211vap *vap,
                struct ieee80211_key *key)
{
    return vap->iv_key_alloc(vap, key);
}

/* Cleanses the key material from the keycache AND liberates the index */
static INLINE int
dev_key_delete(struct ieee80211vap *vap,
               const struct ieee80211_key *key,
               struct ieee80211_node *ni)
{
    return vap->iv_key_delete(vap, key, ni);
}

static INLINE int
dev_key_set(struct ieee80211vap *vap, const struct ieee80211_key *key,
            const u_int8_t bssid[IEEE80211_ADDR_LEN], struct ieee80211_node *ni)
{
    return vap->iv_key_set(vap, key, bssid);
}


/*
 * Setup crypto support for a device/shared instance.
 */
void
ieee80211_crypto_attach(struct ieee80211com *ic)
{
    /* NB: we assume everything is pre-zero'd */
    ieee80211_crypto_register_none(ic);
    ieee80211_crypto_register_wep(ic);
    ieee80211_crypto_register_tkip(ic);
    ieee80211_crypto_register_ccmp(ic);
#if ATH_SUPPORT_WAPI
    ieee80211_crypto_register_sms4(ic);
#endif
}

/*
 * Teardown crypto support.
 */
void
ieee80211_crypto_detach(struct ieee80211com *ic)
{
    ieee80211_crypto_unregister_none(ic);
    ieee80211_crypto_unregister_wep(ic);
    ieee80211_crypto_unregister_tkip(ic);
    ieee80211_crypto_unregister_ccmp(ic);
#if ATH_SUPPORT_WAPI
    ieee80211_crypto_unregister_sms4(ic);
#endif
}

/*
 * Setup crypto support for a vap.
 */
void
ieee80211_crypto_vattach(struct ieee80211vap *vap)
{
    int i;

    /* NB: we assume everything is pre-zero'd */
    vap->iv_def_txkey = IEEE80211_DEFAULT_KEYIX;
    for (i = 0; i < IEEE80211_WEP_NKID; i++) {
        ieee80211_crypto_resetkey(vap, &vap->iv_nw_keys[i],
                                  IEEE80211_KEYIX_NONE);
    }

    /*
     * Initialize the driver key support routines to noop entries.
     * This is useful especially for the cipher test modules.
     */
    vap->iv_key_alloc = null_key_alloc;
    vap->iv_key_set = null_key_set;
    vap->iv_key_delete = null_key_delete;
    vap->iv_key_update_begin = null_key_update;
    vap->iv_key_update_end = null_key_update;
    ieee80211_rsn_vattach(vap);
}

/*
 * Teardown crypto support for a vap.
 */
void
ieee80211_crypto_vdetach(struct ieee80211vap *vap)
{
    ieee80211_crypto_delglobalkeys(vap);
}

/*
 * Register a crypto cipher module.
 */
void
ieee80211_crypto_register(struct ieee80211com *ic, const struct ieee80211_cipher *cip)
{
    ASSERT(cip != NULL);

    if (cip->ic_cipher >= IEEE80211_CIPHER_MAX) {
        printk("%s: cipher %s has an invalid cipher index %u\n",
               __func__, cip->ic_name, cip->ic_cipher);
        return;
    }
    if (ic->ciphers[cip->ic_cipher] != NULL && ic->ciphers[cip->ic_cipher] != cip) {
        printk("%s: cipher %s registered with a different template\n",
               __func__, cip->ic_name);
        return;
    }
    ic->ciphers[cip->ic_cipher] = cip;
}

/*
 * Unregister a crypto cipher module.
 */
void
ieee80211_crypto_unregister(struct ieee80211com *ic, const struct ieee80211_cipher *cip)
{
    ASSERT(cip != NULL);
    if (cip->ic_cipher >= IEEE80211_CIPHER_MAX) {
        printk("%s: cipher %s has an invalid cipher index %u\n",
               __func__, cip->ic_name, cip->ic_cipher);
        return;
    }
    if (ic->ciphers[cip->ic_cipher] != NULL && ic->ciphers[cip->ic_cipher] != cip) {
        printk("%s: cipher %s registered with a different template\n",
               __func__, cip->ic_name);
        return;
    }
    /* NB: don't complain about not being registered */
    /* XXX disallow if references */
    ic->ciphers[cip->ic_cipher] = NULL;
}

int
ieee80211_crypto_available(struct ieee80211com *ic, u_int cipher)
{
    return cipher < IEEE80211_CIPHER_MAX && ic->ciphers[cipher] != NULL;
}

#ifdef NOT_USED
/* XXX well-known names! */
static const char *cipher_modnames[] = {
    "wlan_wep",    /* IEEE80211_CIPHER_WEP */
    "wlan_tkip",    /* IEEE80211_CIPHER_TKIP */
    "wlan_aes_ocb",    /* IEEE80211_CIPHER_AES_OCB */
    "wlan_ccmp",    /* IEEE80211_CIPHER_AES_CCM */
    "wlan_ckip",    /* IEEE80211_CIPHER_CKIP */
};
#endif

/*
 * Establish a relationship between the specified key and cipher
 * and, if necessary, allocate a hardware index from the driver.
 * Note that when a fixed key index is required it must be specified
 * and we blindly assign it w/o consulting the driver (XXX).
 *
 * This must be the first call applied to a key; all the other key
 * routines assume wk_cipher is setup.
 *
 * Locking must be handled by the caller using:
 *    ieee80211_key_update_begin(vap);
 *    ieee80211_key_update_end(vap);
 */
int
ieee80211_crypto_newkey(struct ieee80211vap *vap,
                        int cipher, int flags, struct ieee80211_key *key)
{
#define    N(a)    (sizeof(a) / sizeof(a[0]))
    const struct ieee80211_cipher *cip;
    void *keyctx;
    int oflags;
    struct ieee80211com *ic = vap->iv_ic;

    /*
     * Validate cipher and set reference to cipher routines.
     */
    if (cipher >= IEEE80211_CIPHER_MAX) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                          "%s: invalid cipher %u\n", __func__, cipher);
        vap->iv_stats.is_crypto_badcipher++;
        return 0;
    }
    cip = ic->ciphers[cipher];
    KASSERT(cip != NULL, ("No cipher list\n"));

    oflags = key->wk_flags;
    flags &= IEEE80211_KEY_COMMON;
    /*
     * If the hardware does not support the cipher (or how we're using it in
     * the case of perSta key lookups) then fallback to a host-based implementation.
     */
#if ATH_SUPPORT_IBSS_WPA2 && ATH_SUPPORT_IBSS_PERSTA_HWKEY
    /* In this case, per sta mcast key is put into h/w */
    if (((vap->iv_caps & cipher2cap(cipher)) == 0)) {
#else
    if (((vap->iv_caps & cipher2cap(cipher)) == 0) || (flags & IEEE80211_KEY_PERSTA)) {
#endif
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                          "%s: no h/w support for cipher %s, falling back to s/w\n",
                          __func__, cip->ic_name);
        flags |= IEEE80211_KEY_SWCRYPT;
    }
    /*
     * Hardware TKIP with software MIC is an important
     * combination; we handle it by flagging each key,
     * the cipher modules honor it.
     */
    if (cipher == IEEE80211_CIPHER_TKIP) {
        if ((vap->iv_caps & IEEE80211_C_TKIPMIC) == 0) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s: using sw mic\n", __func__);
            flags |= IEEE80211_KEY_SWMIC;
        } else if (((vap->iv_caps & IEEE80211_C_WME_TKIPMIC) == 0) &&
                   ieee80211_vap_wme_is_set(vap))
        {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s: using sw mic\n", __func__);
            flags |= IEEE80211_KEY_SWMIC;
        }
    }

    /*
     * Bind cipher to key instance.  Note we do this
     * after checking the device capabilities so the
     * cipher module can optimize space usage based on
     * whether or not it needs to do the cipher work.
     */
    if (key->wk_cipher != cip || key->wk_flags != flags) {
again:
        /*
         * Fillin the flags so cipher modules can see s/w
         * crypto requirements and potentially allocate
         * different state and/or attach different method
         * pointers.
         *
         * XXX this is not right when s/w crypto fallback
         *     fails and we try to restore previous state.
         */
        key->wk_flags = flags;
        keyctx = cip->ic_attach(vap, key);
        if (keyctx == NULL) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                              "%s: unable to attach cipher %s\n",
                              __func__, cip->ic_name);
            key->wk_flags = oflags;    /* restore old flags */
            vap->iv_stats.is_crypto_attachfail++;
            return 0;
        }
        cipher_detach(key);
        key->wk_cipher = cip;        /* XXX refcnt? */
        key->wk_private = keyctx;
    }
    /*
     * Commit to requested usage so driver can see the flags.
     */
    key->wk_flags = flags;

    /*
     * Ask the driver for a key index if we don't have one.
     * Note that entries in the global key table always have
     * an index; this means it's safe to call this routine
     * for these entries just to setup the reference to the
     * cipher template.  Note also that when using software
     * crypto we also call the driver to give us a key index.
     */
    if (key->wk_keyix == IEEE80211_KEYIX_NONE) {
        key->wk_keyix = dev_key_alloc(vap, key);
        if (key->wk_keyix == IEEE80211_KEYIX_NONE) {
            /*
             * Driver has no room; fallback to doing crypto
             * in the host. We change the flags and start the
             * procedure over. If we get back here then there's
             * no hope and we bail. Note that this can leave
             * the key in a inconsistent state if the caller
             * continues to use it.
             */
            if ((key->wk_flags & IEEE80211_KEY_SWCRYPT) == 0) {
                vap->iv_stats.is_crypto_swfallback++;
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                                  "%s: no h/w resources for cipher %s, "
                                  "falling back to s/w\n", __func__,
                                  cip->ic_name);
                oflags = key->wk_flags;
                flags |= IEEE80211_KEY_SWCRYPT;
                if (cipher == IEEE80211_CIPHER_TKIP)
                    flags |= IEEE80211_KEY_SWMIC;
                goto again;
            }
            vap->iv_stats.is_crypto_keyfail++;
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                              "%s: unable to setup cipher %s\n",
                              __func__, cip->ic_name);
            return 0;
        }
    }
    return 1;
#undef N
}

/*
 * Remove the key (no locking, for internal use).
 */
static int
_ieee80211_crypto_delkey(struct ieee80211vap *vap, struct ieee80211_key *key,
                         struct ieee80211_node *ni)
{
    u_int16_t keyix;

    KASSERT(key->wk_cipher != NULL, ("No cipher!"));

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                      "%s: %s keyix %u flags 0x%x tsc %llu len %u\n",
                      __func__, key->wk_cipher->ic_name,
                      key->wk_keyix, key->wk_flags,
                      key->wk_keytsc, key->wk_keylen);

    keyix = key->wk_keyix;
    if ((key->wk_valid || key->wk_keyix!=IEEE80211_KEYIX_NONE) && !IEEE80211_IS_KEY_PERSTA_SW(key)) {
        /*
         * Remove hardware entry.
         * invalidate the key first to avoid the race.
         */
        key->wk_valid=0;
        /* XXX key cache */
        if (!dev_key_delete(vap, key, ni)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                              "%s: driver did not delete key index %u\n",
                              __func__, keyix);
            vap->iv_stats.is_crypto_delkey++;
            /* XXX recovery? */
        }
        key->wk_keyix = IEEE80211_KEYIX_NONE;
    }
    return 1;
}

void
ieee80211_crypto_freekey(struct ieee80211vap *vap, struct ieee80211_key *key)
{
    cipher_detach(key);
    OS_MEMZERO(key, sizeof(*key));
    ieee80211_crypto_resetkey(vap, key, IEEE80211_KEYIX_NONE);
}


/*
 * Remove the specified key.
 */
int
ieee80211_crypto_delkey(struct ieee80211vap *vap, struct ieee80211_key *key, 
                        struct ieee80211_node *ni)
{
    int status;

    ieee80211_key_update_begin(vap);
    status = _ieee80211_crypto_delkey(vap, key, ni);
    ieee80211_key_update_end(vap);

    return status;
}

/*
 * Clear the global key table.
 */
void
ieee80211_crypto_delglobalkeys(struct ieee80211vap *vap)
{
    int i;

    ieee80211_key_update_begin(vap);
    for (i = 0; i < IEEE80211_WEP_NKID; i++)
        (void) _ieee80211_crypto_delkey(vap, &vap->iv_nw_keys[i], NULL);
    ieee80211_key_update_end(vap);
}

/*
 * Set the contents of the specified key.
 *
 * Locking must be handled by the caller using:
 *    ieee80211_key_update_begin(vap);
 *    ieee80211_key_update_end(vap);
 */
int
ieee80211_crypto_setkey(struct ieee80211vap *vap, struct ieee80211_key *key,
                        const u_int8_t bssid[IEEE80211_ADDR_LEN],
                        struct ieee80211_node *ni)
{
    const struct ieee80211_cipher *cip = key->wk_cipher;
    int ret;

    KASSERT(cip != NULL, ("No cipher!"));

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                      "%s: %s keyix %u flags 0x%x mac %s  tsc %llu len %u\n",
                      __func__, cip->ic_name, key->wk_keyix,
                      key->wk_flags, ether_sprintf(bssid),
                      key->wk_keytsc, key->wk_keylen);

    /*
     * Give cipher a chance to validate key contents.
     * XXX should happen before modifying state.
     */
    if (!cip->ic_setkey(key)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                          "%s: cipher %s rejected key index %u len %u flags 0x%x\n",
                          __func__, cip->ic_name, key->wk_keyix,
                          key->wk_keylen, key->wk_flags);
        vap->iv_stats.is_crypto_setkey_cipher++;
        return 0;
    }

    if (key->wk_keyix == IEEE80211_KEYIX_NONE) {
        /* nothing allocated, should not happen */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                          "%s: no key index; should not happen!\n", __func__);
        vap->iv_stats.is_crypto_setkey_nokey++;
        return 0;
    }

    if (IEEE80211_IS_KEY_PERSTA_SW(key)) {
        ret = 1;
    } else {
        ret = dev_key_set(vap, key, bssid, ni);

    }

    if (ret) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,"%s", "key is valid\n");
        key->wk_valid = 1;
    }

    return ret;
}

/*
 * Add privacy headers appropriate for a key which we lookup in here.
 * We know the Encryption (fka WEP) bit is set on the frame.
 */
struct ieee80211_key *
ieee80211_crypto_encap(struct ieee80211_node *ni, wbuf_t wbuf)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_key *k;
    struct ieee80211_frame *wh;
    const struct ieee80211_cipher *cip;
    u_int8_t keyid;

    if (!IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)) {
        /*
         * Do not send encrypted frames when privacy is off.
         */
        return NULL;
    }

    /*
     * Multicast traffic always uses the multicast key.
     * Otherwise if a unicast key is set we use that and
     * it is always key index 0.  When no unicast key is
     * set we fall back to the default transmit key.
     */
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);

    /*
     * In the regular encap code, we have reserved space for our IV and a sw MIC for TKIP.
     * We have not allocated space for any ICV (neither AES or TKIP).
     */
    if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
        ni->ni_ucastkey.wk_cipher == &ieee80211_cipher_none) {
        if (vap->iv_def_txkey == IEEE80211_KEYIX_NONE) {
            IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO,
                               wh->i_addr1,
                               "no default transmit key (%s) deftxkey %u",
                               __func__, vap->iv_def_txkey);
            vap->iv_stats.is_tx_nodefkey++;
            return NULL;
        }
        keyid = vap->iv_def_txkey;
        k = &vap->iv_nw_keys[vap->iv_def_txkey];
    } else {
#if ATH_SUPPORT_WAPI            
        if (ieee80211_vap_wapi_is_set(vap))
            keyid = ni->ni_wkused;    
        else
#endif
        keyid = 0;
        k = &ni->ni_ucastkey;
    }
    if (!k->wk_valid)
        return NULL;
    cip = k->wk_cipher;

    if (wbuf_hdrspace(wbuf) < cip->ic_header) {
        /*
         * Should not happen; each OS should
         * have allocated enough space for all headers.
         */
        ASSERT(0);
        IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO, wh->i_addr1,
                           "%s: malformed packet for cipher %s; headroom %u \n",
                           __func__, cip->ic_name, wbuf_hdrspace(wbuf));
        vap->iv_stats.is_tx_noheadroom++;
        return NULL;
    }

    return (cip->ic_encap(k, wbuf, keyid<<6) ? k : NULL);
}

/*
 * Validate and strip privacy headers (and trailer) for a
 * received frame that has the WEP/Privacy bit set.
 */
struct ieee80211_key *
ieee80211_crypto_decap(struct ieee80211_node *ni, wbuf_t wbuf, int hdrlen, struct ieee80211_rx_status *rs)
{
#define    IEEE80211_WEP_HDRLEN    (IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN)
#define    IEEE80211_WEP_MINLEN                            \
    (sizeof(struct ieee80211_frame) +                   \
     IEEE80211_WEP_HDRLEN + IEEE80211_WEP_CRCLEN)
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_key *k;
    struct ieee80211_frame *wh;
    const struct ieee80211_cipher *cip;
    const u_int8_t *ivp, *origHdr;
    u_int8_t keyid;
    int ismcast;

    if (!IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)) {
        /*
         * Discard encrypted frames when privacy is off.
         */
        vap->iv_stats.is_rx_noprivacy++;
        IEEE80211_NODE_STAT(ni, rx_noprivacy);
        return NULL;
    }
    
    /*
     * Locate the key. If unicast and there is no unicast
     * key then we fall back to the key id in the header.
     * This assumes unicast keys are only configured when
     * the key id in the header is meaningless (typically 0).
     */
    origHdr = wbuf_header(wbuf);
    wh      = (struct ieee80211_frame *)origHdr;
    ivp     = origHdr + hdrlen;
#if ATH_SUPPORT_WAPI
    if (ieee80211_vap_wapi_is_set(vap))
        keyid = ivp[WPI_KID_OFFSET] & 0x1;
    else
#endif
    keyid   = ivp[IEEE80211_WEP_IVLEN];
    ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
    
    /* NB: this minimum size data frame could be bigger */
    if (wbuf_get_pktlen(wbuf) < IEEE80211_WEP_MINLEN) {
        IEEE80211_NOTE(vap, IEEE80211_MSG_ANY, ni,
                       "%s: WEP data frame too short, len %u",
                       __func__, wbuf_get_pktlen(wbuf));
        if (ismcast)
            vap->iv_multicast_stats.ims_rx_discard++;
        else
            vap->iv_unicast_stats.ims_rx_discard++;
        return NULL;
    }

    /*
     * If mcast adhoc with RSNA, then use the perSta swkey
     */
    if (vap->iv_opmode == IEEE80211_M_IBSS &&
        RSN_AUTH_IS_RSNA(&vap->iv_rsn) &&
        ismcast) {

        k = IEEE80211_CRYPTO_KEY(vap, ni, keyid); 
    } else if (ismcast || !ni->ni_ucastkey.wk_valid) {
#if ATH_SUPPORT_WAPI
    if (ieee80211_vap_wapi_is_set(vap))
        k = &vap->iv_nw_keys[keyid];
    else
#endif
        k = &vap->iv_nw_keys[keyid >> 6];
    } else {
        k = &ni->ni_ucastkey;
    }

    cip = k->wk_cipher;

    if (!k->wk_valid) {
        if (ismcast)
            vap->iv_multicast_stats.ims_rx_badkeyid++;
        else
            vap->iv_unicast_stats.ims_rx_badkeyid++;
        IEEE80211_NOTE(vap, IEEE80211_MSG_CRYPTO, ni,
                       "%s: WEP data frame no valid key, len %u mcast %d ",
                       __func__, wbuf_get_pktlen(wbuf), ismcast);
        return NULL;
    }

    return (cip->ic_decap(k, wbuf, hdrlen, rs) ? k : NULL);
#undef IEEE80211_WEP_MINLEN
#undef IEEE80211_WEP_HDRLEN
}

void
ieee80211_notify_michael_failure(struct ieee80211vap *vap,
                                 const struct ieee80211_frame *wh,
                                 u_int keyix)
{
    IEEE80211_DELIVER_EVENT_MIC_FAILURE(vap,(const u_int8_t *)wh, keyix); 
}

void
ieee80211_notify_replay_failure(struct ieee80211vap *vap,
                                const struct ieee80211_frame *wh,
                                const struct ieee80211_key *key, u_int64_t rsc)
{
    IEEE80211_DELIVER_EVENT_REPLAY_FAILURE(vap,(const u_int8_t *)wh, key->wk_keyix); 
}

#if ATH_WOW
#define WEP_IV_FIELD_SIZE       4       /* wep IV field size */
#define WEP_ICV_FIELD_SIZE      4       /* wep ICV field size */
#define AES_ICV_FIELD_SIZE      8       /* AES ICV field size */
#define EXT_IV_FIELD_SIZE       4       /* ext IV field size */

static INLINE u_int32_t
ieee80211_get_iv_length(struct ieee80211_key *k)
{
    u_int32_t ivlen = 0;
    
    ivlen = WEP_IV_FIELD_SIZE;

    if ((k->wk_cipher->ic_cipher == IEEE80211_CIPHER_TKIP) ||
        (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_AES_OCB) ||
        (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_AES_CCM)
        ) {
        ivlen += EXT_IV_FIELD_SIZE;
    }

    return ivlen;
}

void
ieee80211_find_iv_lengths(struct ieee80211vap *vap, u_int32_t *pBrKeyIVLength, u_int32_t *pUniKeyIVLength)
{
    struct ieee80211_key *key;
    u_int32_t defkey_ivlen = 0;
    u_int32_t ivlen = 0;
    int i;

    if (IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)) {
        /* broadcast key IV length */
        for (i = 0; i < IEEE80211_WEP_NKID; i++) {
            key = &vap->iv_nw_keys[i];
            if (key->wk_valid) {
                defkey_ivlen = ieee80211_get_iv_length(key);
                break;
            }
        }

        /* unicast Key IV Length */
        key = &vap->iv_bss->ni_ucastkey;
        if (key->wk_valid)
            ivlen = ieee80211_get_iv_length(key);
    }

    *pBrKeyIVLength = defkey_ivlen;
    *pUniKeyIVLength = ivlen;

    return;
}

#endif

