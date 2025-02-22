/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
 * Copyright (c) 2008-2010, Atheros Communications Inc.
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

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR5212

#if !defined(AH_SUPPORT_5112) && !defined(AH_SUPPORT_5111) && !defined(AH_SUPPORT_2413) && !defined (AH_SUPPORT_5413)
#error "No 5212 RF support defined"
#endif

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"

#include "ar5212/ar5212.h"
#include "ar5212/ar5212reg.h"
#include "ar5212/ar5212phy.h"
#include "ar5212/ar5212desc.h"
#ifdef AH_SUPPORT_AR5311
#include "ar5212/ar5311reg.h"
#endif
#include "ah_desc.h"

static HAL_BOOL ar5212GetChipPowerLimits(
    struct ath_hal *ah, HAL_CHANNEL *chans, u_int32_t nchans);
static int16_t ar5212GetMinCCAPwr(struct ath_hal *ah);
static void ar5212SetNominalUserNFVal(struct ath_hal *ah, int16_t val,
    HAL_BOOL is2GHz);
static int16_t ar5212GetNominalUserNFVal(struct ath_hal *ah, HAL_BOOL is2GHz);
static void ar5212SetMinUserNFVal(struct ath_hal *ah, int16_t val,
    HAL_BOOL is2GHz);
static int16_t ar5212GetMinUserNFVal(struct ath_hal *ah, HAL_BOOL is2GHz);
static void ar5212SetMaxUserNFVal(struct ath_hal *ah, int16_t val,
    HAL_BOOL is2GHz);
static int16_t ar5212GetMaxUserNFVal(struct ath_hal *ah, HAL_BOOL is2GHz);
static void ar5212SetNfDeltaVal(struct ath_hal *ah, int16_t val);
static int16_t ar5212GetNfDeltaVal(struct ath_hal *ah);

#ifdef ATH_CCX
HAL_BOOL
ar5212RecordSerialNumber(struct ath_hal *ah, u_int8_t *sn);
#endif
static void
ar5212ConfigPciPowerSave(struct ath_hal *ah, int restore, int powerOff);

static HAL_BOOL
ar5212FindHB63(struct ath_hal *ah);

#if ATH_SUPPORT_WIRESHARK
#include "ah_radiotap.h" /* ah_rx_radiotap_header */
static void ar5212FillRadiotapHdr(struct ath_hal *ah,
                           struct ah_rx_radiotap_header *rh,                           
                           struct ah_ppi_data *ppi,
                           struct ath_desc *ds, void *buf_addr);
#endif /* ATH_SUPPORT_WIRESHARK */

int ar5212GetCalIntervals(struct ath_hal *ah, HAL_CALIBRATION_TIMER **timerp, HAL_CAL_QUERY query);

/* WIN32 does not support C99 */
static const struct ath_hal_private ar5212hal = {
    {
        AR5212_MAGIC,
        HAL_ABI_VERSION,
        0,
        0,
        AH_NULL,
        AH_NULL,
        0,
        0,
        0,
        CTRY_DEFAULT,

        0,
        0,
        0,
        0,
        0,
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // decomp array
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        },

#ifdef __CARRIER_PLATFORM__
        1, /* ah_legacy_dev */
#endif
#ifdef ATH_TX99_DIAG
        0, /* ah_pwr_offset */
#endif
        0, /* ah_rxNumCurAggrGood*/
        {0,0,0,0,0,0},
        ar5212GetRateTable,
        ar5212Detach,

        /* Reset Functions */
        ar5212Reset,
        ar5212PhyDisable,
        ar5212Disable,
        ar5212ConfigPciPowerSave,
        ar5212SetPCUConfig,
        ar5212PerCalibration,
        ar5212ResetCalValid,
        ar5212SetTxPowerLimit,

        ar5212RadarWait,
        /*New DFS functions*/
        ar5212CheckDfs,
        ar5212DfsFound,
        ar5212EnableDfs,
        ar5212GetDfsThresh,
        ar5212GetDfsRadars,
        ar5212GetExtensionChannel,
        ar5212IsFastClockEnabled,
        ar5212_adjust_difs,
        ar5212_dfs_cac_war,
        AH_NULL,
      /* Xr Functions */
        ar5212XrEnable,
        ar5212XrDisable,

        /* Transmit functions */
        ar5212UpdateTxTrigLevel,
        ar5212GetTxTrigLevel,
        ar5212SetupTxQueue,
        ar5212SetTxQueueProps,
        ar5212GetTxQueueProps,
        ar5212ReleaseTxQueue,
        ar5212ResetTxQueue,
        ar5212GetTxDP,
        ar5212SetTxDP,
        ar5212NumTxPending,
        ar5212StartTxDma,
        ar5212StopTxDma,
        ar5212AbortTxDma,
        ar5212FillTxDesc,
        ar5212SetDescLink,
        ar5212GetDescLinkPtr,
        ar5212ClearTxDescStatus,
#ifdef ATH_SWRETRY        
        ar5212ClearDestMask,
#endif        
        ar5212FillKeyTxDesc,
        ar5212ProcTxDesc,
        AH_NULL,
        AH_NULL,
        ar5212GetTxIntrQueue,
        ar5212SetGlobalTxTimeout,
        ar5212GetGlobalTxTimeout,
        ar5212IntrReqTxDesc,
        ar5212CalcTxAirtime,
        AH_NULL,

        /* RX Functions */
        ar5212GetRxDP,
        ar5212SetRxDP,
        ar5212EnableReceive,
        ar5212StopDmaReceive,
        ar5212StartPcuReceive,
        ar5212StopPcuReceive,
        ar5212AbortPcuReceive,
        ar5212SetMulticastFilter,
        ar5212SetMulticastFilterIndex,
        ar5212ClrMulticastFilterIndex,
        ar5212GetRxFilter,
        ar5212SetRxFilter,
        AH_NULL, /* PCU_MISC_SEL_EVM is not defined in ar5212 */
        ar5212SetRxAbort,
        ar5212SetupRxDesc,
        ar5212ProcRxDesc,
        ar5212GetRxKeyIdx,
        ar5212ProcRxDescFast,
        ar5212AniArPoll,
        ar5212ProcessMibIntr,

        /* Misc Functions */
        ar5212GetCapability,
        ar5212SetCapability,
        ar5212GetDiagState,
        ar5212GetMacAddress,
        ar5212SetMacAddress,
        ar5212GetBssIdMask,
        ar5212SetBssIdMask,
        ar5212SetRegulatoryDomain,
        ar5212SetLedState,
        ar5212SetPowerLedState,
        ar5212SetNetworkLedState,
        ar5212WriteAssocid,
        ar5212ForceTSFSync,
        ar5212GpioCfgInput,
        ar5212GpioCfgOutput,
        ar5212GpioGet,
        ar5212GpioSet,
        ar5212GpioSetIntr,
        ar5212GetTsf32,
        ar5212GetTsf64,
        ar5212GetTsf2_32,
        ar5212GetTsf2_64,
        ar5212ResetTsf,
        ar5212DetectCardPresent,
        ar5212UpdateMibMacStats,
        ar5212GetMibMacStats,
        ar5212GetRfgain,
        ar5212GetDefAntenna,
        ar5212SetDefAntenna,
        ar5212SetSlotTime,
        ar5212GetSlotTime,
        ar5212SetAckTimeout,
        ar5212GetAckTimeout,
        ar5212SetCTSTimeout,
        ar5212GetCTSTimeout,
        ar5212SetDecompMask,
        ar5212SetCoverageClass,
        ar5212SetQuiet,
        ar5212SetAntennaSwitch,
        ar5212GetDescInfo,
        ar5212MarkPhyInactive,
        ar5212SelectAntConfig,
        ar5212GetNumAntCfg,
        ar5212SetEifsMask,
        ar5212GetEifsMask,
        ar5212SetEifsDur,
        ar5212GetEifsDur,
	AH_NULL,
        ar5212EnableTPC,
        AH_NULL,                  /* ah_olpcTempCompensation */
        ar5212DisablePhyRestart,
        ar5212_enable_keysearch_always,
        ar5212InterferenceIsPresent,     
        AH_NULL,                  /* ah_DispTPCTables */
        /* Key Cache Functions */
        ar5212GetKeyCacheSize,
        ar5212ResetKeyCacheEntry,
        ar5212IsKeyCacheEntryValid,
        ar5212SetKeyCacheEntry,
        ar5212SetKeyCacheEntryMac,
        ar5212PrintKeyCache,

        /* Power Management Functions */
        ar5212SetPowerMode,
        ar5212GetPowerMode,
        ar5212SetSmPowerMode,
#if ATH_WOW        
        ar5212WowApplyPattern,
        ar5212WowEnable,
        ar5212WowWakeUp,
#endif

        /* Get Channel Noise */
        ath_hal_getChanNoise,
        ar5212ChainNoiseFloor,

        /* Beacon Functions */
        ar5212BeaconInit,
        ar5212SetStaBeaconTimers,
        ar5212ResetStaBeaconTimers,
        ar5212WaitForBeaconDone,

        /* Interrupt Functions */
        ar5212IsInterruptPending,
        ar5212GetPendingInterrupts,
        ar5212GetInterrupts,
        ar5212SetInterrupts,
        ar5212SetIntrMitigationTimer,
        ar5212GetIntrMitigationTimer,
	ar5212ForceVCS,
        ar5212SetDfs3StreamFix,
        ar5212Get3StreamSignature,

        /* 11n specific functions (NOT applicable to ar5212) */
        ar5212Set11nTxDesc,
        /* Start PAPRD functions - supported in ar9300 onwards */
        NULL, /* ah_setPAPRDTxDesc */
        NULL, /* ah_PAPRDInitTable */
        NULL, /* ah_PAPRDSetupGainTable */
        NULL, /* ah_PAPRDCreateCurve */
        NULL, /* ah_PAPRDisDone */
        NULL, /* ah_PAPRDEnable */
        NULL, /* ah_PAPRDPopulateTable */
        NULL, /* ah_isTxDone */
        ar5212_paprd_dec_tx_pwr,    /* ah_paprd_dec_tx_pwr */
        NULL, /* ah_PAPRDThermalSend */
        /* End PAPRD functions - supported in ar9300 onwards */
#ifdef  ATH_SUPPORT_TxBF
        /*for TxBF*/
        AH_NULL,
#ifdef TXBF_TODO
        AH_NULL,
		AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
#endif
        AH_NULL,        
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
        /*for TxBF*/
#endif
        ar5212Set11nRateScenario,
        ar5212Set11nAggrFirst,
        ar5212Set11nAggrMiddle,
        ar5212Set11nAggrLast,
        ar5212Clear11nAggr,
        ar5212Set11nRifsBurstMiddle,
        ar5212Set11nRifsBurstLast,
        ar5212Clr11nRifsBurst,
        ar5212Set11nAggrRifsBurst,
        ar5212Set11nRxRifs,
        ar5212Get11nRxRifs,
        AH_NULL,
        ar5212DetectBbHang,
        ar5212DetectMacHang,
        ar5212SetImmunity,
        ar5212GetHwHangs,
        ar5212Set11nBurstDuration,
        ar5212Set11nVirtMoreFrag,
        ar5212Get11nExtBusy,
        ar5212Set11nMac2040,
        ar5212Get11nRxClear,
        ar5212Set11nRxClear,
        ar5212Get11nHwPlatform,
        NULL,
        ar5212GetMibCycleCountsPct,
        ar5212DmaRegDump,

        /* ForcePPM specific functions (NOT applicable to ar5212) */
        ar5212PpmGetRssiDump,
        ar5212PpmArmTrigger,
        ar5212PpmGetTrigger,
        ar5212PpmForce,
        ar5212PpmUnForce,
        ar5212PpmGetForceState,

        ar5212GetSpurInfo,
        ar5212SetSpurInfo,

        ar5212GetMinCCAPwr,
        ar5212SetNominalUserNFVal,
        ar5212GetNominalUserNFVal,
        ar5212SetMinUserNFVal,
        ar5212GetMinUserNFVal,
        ar5212SetMaxUserNFVal,
        ar5212GetMaxUserNFVal,
        ar5212SetNfDeltaVal,
        ar5212GetNfDeltaVal,

        ar5212GreenApPsOnOff,
        ar5212IsSingleAntPowerSavePossible,
        ar5212GetVowStats, /* ah_get_vow_stats */
#ifdef ATH_CCX
        /* CCX Radio Measurement Specific Functions */
        ar5212GetMibCycleCounts,
        ar5212ClearMibCounters,
        ar5212GetCcaThreshold,
        ar5212GetCurRssi,
#endif
#ifdef ATH_BT_COEX
        /* Bluetooth Coexistence functions */
        ar5212SetBTCoexInfo,
        ar5212BTCoexConfig,
        ar5212BTCoexSetQcuThresh,
        ar5212BTCoexSetWeights,
        ar5212BTCoexSetupBmissThresh,
        ar5212BTCoexSetParameter,
        ar5212BTCoexDisable,
        ar5212BTCoexEnable,
#endif
        /* Generic Timer functions */
        ar5212AllocGenericTimer,
        ar5212FreeGenericTimer,
        ar5212StartGenericTimer,
        ar5212StopGenericTimer,
        ar5212GetGenTimerInterrupts,

        ar5212SetDcsMode,
        ar5212GetDcsMode,

#if ATH_ANT_DIV_COMB
        ar5212AntDivCombGetConfig,
        ar5212AntDivCombSetConfig,
#endif
        AH_NULL, /* ah_printBbPanicInfo */
        AH_NULL, /* ah_handle_radar_bb_panic */
        AH_NULL, /* ah_setHalResetReason */

#if ATH_SUPPORT_SPECTRAL        
        /* Spectral scan 
         * No Spectral Support for ar5212
         */
        AH_NULL, 
        AH_NULL, 
        AH_NULL, 
        AH_NULL, 
        AH_NULL, 
        AH_NULL, 
        AH_NULL, 
        AH_NULL, 
        AH_NULL, 
        AH_NULL, 
        AH_NULL, 
        AH_NULL, 
        AH_NULL, 
#endif  /* ATH_SUPPORT_SPECTRAL */

#if ATH_SUPPORT_RAW_ADC_CAPTURE
        /* Spectral raw ADC capture 
         * No Spectral Support for ar5212
         */
        AH_NULL, /* ar5212EnableTestAddacMode, */
        AH_NULL, /* ar5212DisableTestAddacMode,*/ 
        AH_NULL, /* ar5212BeginAdcCapture,*/ 
        AH_NULL, /* ar5212RetrieveCaptureData,*/ 
        AH_NULL, /* ah_arCalculateADCRefPowers,*/
        AH_NULL, /* ah_arGetMinAGCGain,*/
#endif

        ar5212PromiscMode,
        ar5212ReadPktlogReg,
        ar5212WritePktlogReg,
        ar5212SetProxySTA,       /*ah_setProxySTA */
        ar5212GetCalIntervals,

#if ATH_SUPPORT_WIRESHARK
        ar5212FillRadiotapHdr,
#endif
#if ATH_TRAFFIC_FAST_RECOVER
        AH_NULL,                /* ah_getPll3SqsumDvc */
#endif
#ifdef ATH_SUPPORT_HTC
        AH_NULL,
        AH_FALSE,
        AH_NULL,
#endif

#ifdef ATH_TX99_DIAG
        /* Tx99 functions */
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
#endif

        AH_NULL,
        {{AH_NULL}},
        AH_NULL,
        {AH_NULL},
        {AH_NULL},
        ar5212ChkRSSIUpdateTxPwr,
        ar5212_is_skip_paprd_by_greentx,   /* ah_is_skip_paprd_by_greentx */
        AH_NULL,                           /* ah_hwgreentx_set_pal_spare */
        AH_NULL,
#ifdef ATH_SUPPORT_WAPI
        AH_NULL,
#endif
    },

    ar5212GetChannelEdges,
    ar5212GetWirelessModes,
    ar5212EepromRead,
#ifdef AH_SUPPORT_WRITE_EEPROM
    ar5212EepromWrite,
#else
    AH_NULL,
#endif
    AH_NULL,
    ar5212GetChipPowerLimits,
    /* rest is zero'd by compiler */
};

/*
 * TODO: Need to talk to Praveen about this, these are
 * not valid 2.4 channels, either we change these
 * or I need to change the beanie coding to accept these
 */
static const u_int16_t channels11b[] = { 2412, 2447, 2484 };
static const u_int16_t channels11g[] = { 2312, 2412, 2484 };

/*
 * Disable PLL when in L0s as well as receiver clock when in L1.
 * This power saving option must be enabled through the Serdes.
 *
 * Programming the Serdes must go through the same 288 bit serial shift
 * register as the other analog registers.  Hence the 9 writes.
 *
 * XXX Clean up the magic numbers.
 */
void
ar5212ConfigPciPowerSave(struct ath_hal *ah, int restore, int powerOff)
{
        if (AH_PRIVATE(ah)->ah_isPciExpress != AH_TRUE) {
            return;
        }

        /* Do not touch SERDES registers */
        if (AH_PRIVATE(ah)->ah_config.ath_hal_pciePowerSaveEnable == 2) {
            return;
        }

    if (restore) {
    
        if (!AH_PRIVATE(ah)->ah_config.ath_hal_pcieRestore)
            return;

        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x9248fc00);
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x24924924);

        /* RX shut off when elecidle is asserted */
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x28000039);
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x53160824);

        /* If EEPROM is programmed with the tx power
         * do not override the value during SERDES writes
         */
        if (!(AH_PRIVATE(ah)->ah_eepromTxPwr)) {
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0xe5980579);
        } else {
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0xf6800579);
        }

        /* Shut off PLL and CLKREQ active in L1 */
        if (AH_PRIVATE(ah)->ah_config.ath_hal_pcieClockReq) {
                OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x001dfffe);
        } else {
               OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x001dffff);
        }

        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x1aaabe40);
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0xbe105554);
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x000e3007);

    } else {

        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x9248fc00);
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x24924924);

        /* RX shut off when elecidle is asserted */
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x28000039);
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x53160824);

        /* If EEPROM is programmed with the tx power
         * do not override the value during SERDES writes
         */
        if (!(AH_PRIVATE(ah)->ah_eepromTxPwr)) {
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0xe5980579);
        } else {
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0xf6800579);
        }

        /* Shut off PLL and CLKREQ active in L1 */
        if (AH_PRIVATE(ah)->ah_config.ath_hal_pcieClockReq) {
                OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x001deffe);
        } else {
               OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x001defff);
        }

        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x1aaabe40);
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0xbe105554);
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x000e3007);
    }

    /* Load the new settings */
    OS_REG_WRITE(ah, AR_PCIE_SERDES2, 0x00000000);

    OS_DELAY(1000);

    /* Write PCIe workaround enable register */
    if (AH_PRIVATE(ah)->ah_config.ath_hal_pcieWaen) {
        OS_REG_WRITE(ah, AR_PCIE_WAEN, AH_PRIVATE(ah)->ah_config.ath_hal_pcieWaen);
    } else {
        OS_REG_WRITE(ah, AR_PCIE_WAEN, 0x0000000f);
    }
}

u_int32_t
ar5212GetRadioRev(struct ath_hal *ah)
{
    u_int32_t val;
    int i;

    /* Read Radio Chip Rev Extract */
    OS_REG_WRITE(ah, AR_PHY(0x34), 0x00001c16);
    for (i = 0; i < 8; i++)
        OS_REG_WRITE(ah, AR_PHY(0x20), 0x00010000);
    val = (OS_REG_READ(ah, AR_PHY(256)) >> 24) & 0xff;
    val = ((val & 0xf0) >> 4) | ((val & 0x0f) << 4);
    return ath_hal_reverseBits(val, 8);
}

static void
ar5212AniSetup(struct ath_hal *ah)
{
    struct ath_hal_5212 *ahp = AH5212(ah);
    int i;

    const int totalSizeDesired[] = { -55, -55, -55, -55, -62 };
    const int coarseHigh[]       = { -14, -14, -14, -14, -12 };
    const int coarseLow[]        = { -64, -64, -64, -64, -70 };
    const int firpwr[]           = { -78, -78, -78, -78, -80 };

    for (i = 0; i < 5; i++) {
        ahp->ah_totalSizeDesired[i] = totalSizeDesired[i];
        ahp->ah_coarseHigh[i] = coarseHigh[i];
        ahp->ah_coarseLow[i] = coarseLow[i];
        ahp->ah_firpwr[i] = firpwr[i];
    }
}

/*
 * Attach for an AR5212 part.
 */
struct ath_hal_5212 *
ar5212NewState(u_int16_t devid, HAL_ADAPTER_HANDLE osdev, HAL_SOFTC sc,
    HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_BUS_TYPE bustype, 
    asf_amem_instance_handle amem_handle,
    struct hal_reg_parm *hal_conf_parm, HAL_STATUS *status)
{
#define    N(a)    (sizeof(a)/sizeof(a[0]))
    static const u_int8_t defbssidmask[IEEE80211_ADDR_LEN] =
        { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    struct ath_hal_5212 *ahp;
    struct ath_hal *ah;

    /* NB: memory is returned zero'd */
    ahp = amalloc_adv(
        amem_handle, sizeof (struct ath_hal_5212), adf_os_mem_zero_outline);
    if (ahp == AH_NULL) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "%s: cannot allocate memory for "
            "state block\n", __func__);
        *status = HAL_ENOMEM;
        return AH_NULL;
    }
    ah = &ahp->ah_priv.priv.h;
    /* set initial values */
    OS_MEMZERO(&ahp->ah_priv, sizeof(ahp->ah_priv));
    OS_MEMCPY(&ahp->ah_priv.priv, &ar5212hal, sizeof(ahp->ah_priv.priv));

    AH_PRIVATE(ah)->amem_handle = amem_handle;
    ah->ah_osdev = osdev;
    ah->ah_sc = sc;
    ah->ah_st = st;
    ah->ah_sh = sh;
    ah->ah_bustype = bustype;

    
    
    /*
    ** Initialize factory defaults in the private space
    */
    
    ath_hal_factory_defaults(AH_PRIVATE(ah), hal_conf_parm);

    if (AH_PRIVATE(ah)->ah_config.ath_hal_serializeRegMode == SER_REG_MODE_AUTO) {
        /* Non-OWL chips do not need this workaround. */
        AH_PRIVATE(ah)->ah_config.ath_hal_serializeRegMode = SER_REG_MODE_OFF;
    }
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "%s: ath_hal_serializeRegMode is %d\n",
             __func__, AH_PRIVATE(ah)->ah_config.ath_hal_serializeRegMode);

    AH_PRIVATE(ah)->ah_devid = devid;
    AH_PRIVATE(ah)->ah_subvendorid = 0;    /* XXX */

    AH_PRIVATE(ah)->ah_powerLimit = MAX_RATE_POWER;
    AH_PRIVATE(ah)->ah_tpScale = HAL_TP_SCALE_MAX;    /* no scaling */

    ahp->ah_atimWindow = 0;            /* [0..1000] */
    ahp->ah_diversityControl = AH_PRIVATE(ah)->ah_config.ath_hal_diversityControl;
    ahp->ah_bIQCalibration = AH_FALSE;
    /*
     * Enable MIC handling.
     */
    ahp->ah_staId1Defaults = AR_STA_ID1_CRPT_MIC_ENABLE;
    AH_PRIVATE(ah)->ah_config.ath_hal_desc_tpc = AH_FALSE;
    ahp->ah_macTPC = SM(MAX_RATE_POWER, AR_TPC_ACK)
               | SM(MAX_RATE_POWER, AR_TPC_CTS)
               | SM(MAX_RATE_POWER, AR_TPC_CHIRP);
    ahp->ah_enable32kHzClock = DONT_USE_32KHZ;/* XXX */
    ahp->ah_slottime = (u_int) -1;
    ahp->ah_acktimeout = (u_int) -1;
    ahp->ah_ctstimeout = (u_int) -1;
    OS_MEMCPY(&ahp->ah_bssidmask, defbssidmask, IEEE80211_ADDR_LEN);

    /*
     * 11g-specific stuff
     */
    ahp->ah_gBeaconRate = 0;        /* adhoc beacon fixed rate */

    /* Disable single write key cache */
    AH_PRIVATE(ah)->ah_singleWriteKC = 0;

    if (!hal_conf_parm->calInFlash)
        AH_PRIVATE(ah)->ah_flags |= AH_USE_EEPROM;

#ifndef WIN32
    if (ar5212EepDataInFlash(ah)) {
        ahp->ah_priv.priv.ah_eepromRead = ar5212FlashRead;
#ifdef AH_SUPPORT_WRITE_EEPROM
         ahp->ah_priv.priv.ah_eepromWrite = ar5212FlashWrite;
#endif
    }
#endif /* WIN32 */

    return ahp;
#undef N
}

typedef struct _mac_descriptor {
    u_int8_t    version;     // MAC version being described
    u_int8_t    rev_min;     // lowest MAC revision supported
    u_int8_t    rev_max;     // highest MAC revision supported
} MAC_DESCRIPTOR;

/*
 * Validate MAC version and revision. 
 * Returns AH_TRUE if successful, AH_FALSE otherwise.
 */
static HAL_BOOL
ar5212ValidateMacDescriptor (u_int8_t macVersion, u_int8_t macRev)
{
    /* valid revision numbers for each supported MAC version */
    MAC_DESCRIPTOR    ValidMacList[] = {
    /*    version                  lowest revision       highest revision     */
        { AR_SREV_VERSION_VENICE,  AR_SREV_D2PLUS,       AR_SREV_REVISION_MAX },
        { AR_SREV_VERSION_GRIFFIN, AR_SREV_D2PLUS,       AR_SREV_REVISION_MAX },
        { AR_SREV_5413,            AR_SREV_REVISION_MIN, AR_SREV_REVISION_MAX },
        { AR_SREV_5424,            AR_SREV_REVISION_MIN, AR_SREV_REVISION_MAX },
        { AR_SREV_2425,            AR_SREV_REVISION_MIN, AR_SREV_REVISION_MAX },
        { AR_SREV_2417,            AR_SREV_REVISION_MIN, AR_SREV_REVISION_MAX }
    };
    MAC_DESCRIPTOR    *pMacEntry = ValidMacList;
    int               i;
    
#define    N(a)    (sizeof(a)/sizeof(a[0]))

    /* seach list of valid MAC versions */
    for (i = 0; i < N(ValidMacList); i++) {
        /* if found the specific MAC version, validate revision and return */
        if (macVersion == pMacEntry->version) {
            if ((macRev >= pMacEntry->rev_min) && (macRev <= pMacEntry->rev_max)) {
                return AH_TRUE;
            }
            else {
                return AH_FALSE;
            }
        }

        pMacEntry++;
    }

    /* MAC version not in the list: unsupported */
    return AH_FALSE;
#undef N
}

/*
 * Attach for an AR5212 part.
 */
struct ath_hal *
ar5212Attach(u_int16_t devid,  HAL_ADAPTER_HANDLE osdev, HAL_SOFTC sc,
    HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_BUS_TYPE bustype, 
    asf_amem_instance_handle amem_handle,
    struct hal_reg_parm *hal_conf_parm, HAL_STATUS *status)
{
    struct ath_hal_5212 *ahp;
    struct ath_hal *ah;
    u_int i;
    u_int32_t sum, val, eepMax;
    u_int16_t eeval;
    HAL_STATUS ecode;
    HAL_BOOL rfStatus;

    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "%s: sc %p st %p sh %p\n",
        __func__, sc, (void *)st, (void*) sh);

    /* NB: memory is returned zero'd */
    ahp = ar5212NewState(
        devid, osdev, sc, st, sh, bustype, amem_handle, hal_conf_parm, status);
    if (ahp == AH_NULL) return AH_NULL;
    ah = &ahp->ah_priv.priv.h;
    ah->ah_st = st;
    ah->ah_sh = sh;
    ah->ah_bustype = bustype;

    if (!ar5212SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE)) {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: couldn't wakeup chip\n", __func__);
        ecode = HAL_EIO;
        goto bad;
    }

    /* Read Revisions from Chips before taking out of reset */
    val = OS_REG_READ(ah, AR_SREV) & AR_SREV_ID;
    AH_PRIVATE(ah)->ah_macVersion = val >> AR_SREV_ID_S;
    AH_PRIVATE(ah)->ah_macRev = val & AR_SREV_REVISION;

    if (! ar5212ValidateMacDescriptor(AH_PRIVATE(ah)->ah_macVersion, AH_PRIVATE(ah)->ah_macRev)) {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: Mac Chip Rev 0x%02x.%x is not supported by "
            "this driver\n", __func__,
            AH_PRIVATE(ah)->ah_macVersion,
            AH_PRIVATE(ah)->ah_macRev);
        ecode = HAL_ENOTSUPP;
        goto bad;
    }

    val = OS_REG_READ(ah, AR_PCICFG);
    val = MS(val, AR_PCICFG_EEPROM_SIZE);
    if (val == 0) {
        if ( AH_PRIVATE(ah)->ah_macVersion != AR_SREV_5424 &&
             AH_PRIVATE(ah)->ah_macVersion != AR_SREV_2425 &&
            !(AH_PRIVATE(ah)->ah_macVersion == AR_SREV_5413 &&
              AH_PRIVATE(ah)->ah_macRev <= AR_SREV_D2PLUS_MS)) {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: EEPROM size = %d. Must be %d (16k).\n", __func__,
                 val, AR_PCICFG_EEPROM_SIZE_16K);
            ecode = HAL_EESIZE;
            goto bad;
        }

        /* We have verified that this is a PCI Express chip */
        AH_PRIVATE(ah)->ah_isPciExpress = AH_TRUE;

    } else if (val != AR_PCICFG_EEPROM_SIZE_16K) {
        if (AR_PCICFG_EEPROM_SIZE_FAILED == val) {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: unsupported EEPROM size "
                     "%u (0x%x) found\n", __func__, val, val);
            ecode = HAL_EESIZE;
            goto bad;
        }

        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: EEPROM size = %d. Must be %d (16k).\n", __func__,
             val, AR_PCICFG_EEPROM_SIZE_16K);
        ecode = HAL_EESIZE;
        goto bad;
    }

    if (!ar5212ChipReset(ah, AH_NULL)) {    /* reset chip */
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: chip reset failed\n", __func__);
        ecode = HAL_EIO;
        goto bad;
    }

    AH_PRIVATE(ah)->ah_phyRev = OS_REG_READ(ah, AR_PHY_CHIP_ID);

    if (IS_PCIE(ah)) {
        /* Hack to determine whether the EEPROM is newer
          * version whose SERDES contents have to be reused or not
         */
        if (ar5212EepromRead(ah, 0x2, &eeval)) {
            AH_PRIVATE(ah)->ah_eepromTxPwr = (eeval == 0x40) ? 0 : 1;
        }

        /* XXX: build flag to disable this? */
        ar5212ConfigPciPowerSave(ah, 0, 0);
    }

    if (!ar5212ChipTest(ah)) {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: hardware self-test failed\n", __func__);
        ecode = HAL_ESELFTEST;
        goto bad;
    }

    if (ar5212FindHB63(ah) == AH_TRUE)
        AH_PRIVATE(ah)->ah_flags |= AH_IS_HB63;
    else 
         AH_PRIVATE(ah)->ah_flags &= ~AH_IS_HB63;

    /* Enable PCI core retry fix in software for Hainan and up */
    if (AH_PRIVATE(ah)->ah_macVersion >= AR_SREV_VERSION_VENICE) {
        OS_REG_RMW_FIELD(ah, AR_PCICFG, AR_PCICFG_ENABLE_RETRYFIX, 1);
    }

    /*
     * Set correct Baseband to analog shift
     * setting to access analog chips.
     */
    OS_REG_WRITE(ah, AR_PHY(0), 0x00000007);

    /* Read Radio Chip Rev Extract */
    AH_PRIVATE(ah)->ah_analog5GhzRev = ar5212GetRadioRev(ah);
    /* NB: silently accept anything in release code per Atheros */
    switch (AH_PRIVATE(ah)->ah_analog5GhzRev & AR_RADIO_SREV_MAJOR) {
    case AR_RAD5111_SREV_MAJOR:
    case AR_RAD5112_SREV_MAJOR:
    case AR_RAD2111_SREV_MAJOR:
    case AR_RAD2413_SREV_MAJOR:
    case AR_RAD5413_SREV_MAJOR:
    case AR_RAD5424_SREV_MAJOR:
        break;
    default:
        if (AH_PRIVATE(ah)->ah_analog5GhzRev == 0) {
            /*
             * WAR for bug 10062.  When RF_Silent is used, the
             * analog chip is reset.  So when the system boots
             * up with the radio switch off we cannot determine
             * the RF chip rev.  To workaround this check the
             * mac+phy revs and if Hainan, set the radio rev
             * to Derby.
             */
            if (AH_PRIVATE(ah)->ah_macVersion == AR_SREV_VERSION_VENICE &&
                AH_PRIVATE(ah)->ah_macRev == AR_SREV_HAINAN &&
                AH_PRIVATE(ah)->ah_phyRev == AR_PHYREV_HAINAN) {
                AH_PRIVATE(ah)->ah_analog5GhzRev = AR_ANALOG5REV_HAINAN;
                break;
            }
            if (IS_2413(ah)) {        /* Griffin */
                AH_PRIVATE(ah)->ah_analog5GhzRev = 0x51;
                break;
            }
            if (IS_5413(ah)) {        /* Eagle */
                AH_PRIVATE(ah)->ah_analog5GhzRev = 0x62;
                break;
            }
            if (IS_2425(ah) || IS_2417(ah)) {/* Swan or Nala */
                AH_PRIVATE(ah)->ah_analog5GhzRev = 0xA2;
                break;
            }
        }
#ifdef AH_DEBUG
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: 5G Radio Chip Rev 0x%02X is not supported by "
            "this driver\n", __func__,
            AH_PRIVATE(ah)->ah_analog5GhzRev);
        ecode = HAL_ENOTSUPP;
        goto bad;
#endif
    }
    if (!IS_5413(ah) && IS_5112(ah) && IS_RAD5112_REV1(ah)) {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: 5112 Rev 1 is not supported by this "
            "driver (analog5GhzRev 0x%x)\n", __func__,
            AH_PRIVATE(ah)->ah_analog5GhzRev);
        ecode = HAL_ENOTSUPP;
        goto bad;
    }
    if (!ar5212EepromRead(ah, AR_EEPROM_VERSION, &eeval)) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: unable to read EEPROM version\n", __func__);
        ecode = HAL_EEREAD;
        goto bad;
    }
    if (eeval < AR_EEPROM_VER3_2) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: unsupported EEPROM version %u (0x%x)\n",
            __func__, eeval, eeval);
        ecode = HAL_EEVERSION;
        goto bad;
    }
    ahp->ah_eeversion = eeval;

    if(IS_PCIE(ah)) {
        if (!ar5212EepromRead(ah, EEPROM_PROTECT_OFFSET_PCIE, &eeval)) {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: cannot read EEPROM protection pcie"
                "bits; read locked?\n", __func__);
            ecode = HAL_EEREAD;
            goto bad;
        }
    }
    else {
        if (!ar5212EepromRead(ah, AR_EEPROM_PROTECT, &eeval)) {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: cannot read EEPROM protection "
                "bits; read locked?\n", __func__);
            ecode = HAL_EEREAD;
            goto bad;
        }
    }

    HDPRINTF(ah, HAL_DBG_EEPROM, "EEPROM protect 0x%x\n", eeval);
    ahp->ah_eeprotect = eeval;
    /* XXX check proper access before continuing */

    /*
     * Read the Atheros EEPROM entries and calculate the checksum.
     */
    if (!ar5212EepromRead(ah, AR_EEPROM_SIZE_UPPER, &eeval)) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: cannot read EEPROM upper size\n" , __func__);
        ecode = HAL_EEREAD;
        goto bad;
    }
    if (eeval != 0)    {
        eepMax = (eeval & AR_EEPROM_SIZE_UPPER_MASK) <<
            AR_EEPROM_SIZE_ENDLOC_SHIFT;
        if (!ar5212EepromRead(ah, AR_EEPROM_SIZE_LOWER, &eeval)) {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: cannot read EEPROM lower size\n" ,
                __func__);
            ecode = HAL_EEREAD;
            goto bad;
        }
        eepMax = (eepMax | eeval) - AR_EEPROM_ATHEROS_BASE;
    } else
        eepMax = AR_EEPROM_ATHEROS_MAX;
    sum = 0;
    for (i = 0; i < eepMax; i++) {
        if (!ar5212EepromRead(ah, AR_EEPROM_ATHEROS(i), &eeval)) {
            ecode = HAL_EEREAD;
            goto bad;
        }
        sum ^= eeval;
    }
    if (sum != 0xffff) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: bad EEPROM checksum 0x%x\n", __func__, sum);
        ecode = HAL_EEBADSUM;
        goto bad;
    }

    ahp->ah_numChannels11a = NUM_11A_EEPROM_CHANNELS;
    ahp->ah_numChannels2_4 = NUM_2_4_EEPROM_CHANNELS;

    for (i = 0; i < NUM_11A_EEPROM_CHANNELS; i ++)
        ahp->ah_dataPerChannel11a[i].numPcdacValues = NUM_PCDAC_VALUES;

    /* the channel list for 2.4 is fixed, fill this in here */
    for (i = 0; i < NUM_2_4_EEPROM_CHANNELS; i++) {
        ahp->ah_channels11b[i] = channels11b[i];
        ahp->ah_channels11g[i] = channels11g[i];
        ahp->ah_dataPerChannel11b[i].numPcdacValues = NUM_PCDAC_VALUES;
        ahp->ah_dataPerChannel11g[i].numPcdacValues = NUM_PCDAC_VALUES;
    }

    if (!ath_hal_readEepromIntoDataset(ah, &ahp->ah_eeprom)) {
        ecode = HAL_EEREAD;        /* XXX */
        goto bad;
    }
    if ((ahp->ah_eeversion < AR_EEPROM_VER5_3) && (IS_5413(ah))) {
        ahp->ah_eeprom.ee_spurChans[0][1] = AR_SPUR_5413_1;
        ahp->ah_eeprom.ee_spurChans[1][1] = AR_SPUR_5413_2;
        ahp->ah_eeprom.ee_spurChans[2][1] = AR_NO_SPUR;
        ahp->ah_eeprom.ee_spurChans[0][0] = AR_NO_SPUR;
    }

    /*
     * If Bmode and AR5212, verify 2.4 analog exists
     */
    if (ahp->ah_Bmode &&
        (AH_PRIVATE(ah)->ah_analog5GhzRev & 0xF0) == AR_RAD5111_SREV_MAJOR) {
        /*
         * Set correct Baseband to analog shift
         * setting to access analog chips.
         */
        OS_REG_WRITE(ah, AR_PHY(0), 0x00004007);
        OS_DELAY(2000);
        AH_PRIVATE(ah)->ah_analog2GhzRev = ar5212GetRadioRev(ah);

        /* Set baseband for 5GHz chip */
        OS_REG_WRITE(ah, AR_PHY(0), 0x00000007);
        OS_DELAY(2000);
        if ((AH_PRIVATE(ah)->ah_analog2GhzRev & 0xF0) != AR_RAD2111_SREV_MAJOR) {
            HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: 2G Radio Chip Rev 0x%02X is not "
                "supported by this driver\n", __func__,
                AH_PRIVATE(ah)->ah_analog2GhzRev);
            ecode = HAL_ENOTSUPP;
            goto bad;
        }
    }

        if (!ar5212EepromRead(ah, AR_EEPROM_REG_DOMAIN, &eeval)) {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: cannot read regulator domain from EEPROM\n",
                __func__);
            ecode = HAL_EEREAD;
            goto bad;
        }
#ifdef ATH_CCX
    /* XXX record serial number */
    ar5212RecordSerialNumber(ah, (u_int8_t*)&ahp->ah_priv.priv.serNo);
#endif
    ahp->ah_regdomain = eeval;
    AH_PRIVATE(ah)->ah_currentRD = ahp->ah_regdomain;

    /*
     * Got everything we need now to setup the capabilities.
     */
    if (!ar5212FillCapabilityInfo(ah)) {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s:failed ar5212FillCapabilityInfo\n", __func__);
        ecode = HAL_EEREAD;
        goto bad;
    }

    rfStatus = AH_FALSE;
    if (IS_5413(ah)) {
#ifdef AH_SUPPORT_5413
        rfStatus = ar5413RfAttach(ah, &ecode);
#else
        ecode = HAL_ENOTSUPP;
#endif
    }
    else if (IS_2413(ah))
#ifdef AH_SUPPORT_2413
        rfStatus = ar2413RfAttach(ah, &ecode);
#else
        ecode = HAL_ENOTSUPP;
#endif
    else if (IS_5112(ah))
#ifdef AH_SUPPORT_5112
        rfStatus = ar5112RfAttach(ah, &ecode);
#else
        ecode = HAL_ENOTSUPP;
#endif
    else if (IS_2425(ah) || IS_2417(ah))
#ifdef AH_SUPPORT_2425
        rfStatus = ar2425RfAttach(ah, &ecode);
#else
        ecode = HAL_ENOTSUPP;
#endif
    else
#ifdef AH_SUPPORT_5111
        rfStatus = ar5111RfAttach(ah, &ecode);
#else
        ecode = HAL_ENOTSUPP;
#endif
    if (!rfStatus) {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: RF setup failed, status %u\n",
            __func__, ecode);
        goto bad;
    }

    /*
     * Determine default noise floor.
     * For legacy chips, don't distinguish between 2 GHz and 5 GHz
     * noise values.
     * Arbitrarily use the 2 GHz buffer.
     */
    AH_PRIVATE(ah)->nfp = &AH_PRIVATE(ah)->nf_2GHz;
    AH_PRIVATE(ah)->nf_2GHz.nominal = AR_PHY_CCA_MAX_GOOD_VALUE_LEGACY;

    /*
     * Set noise floor adjust method; we arrange a
     * direct call instead of thunking.
     */
    AH_PRIVATE(ah)->ah_getNfAdjust = ahp->ah_rfHal.getNfAdjust;

    /* Initialize gain ladder thermal calibration structure */
    ar5212InitializeGainValues(ah);

    sum = 0;
    for (i = 0; i < 3; i++) {
        if (!ar5212EepromRead(ah, AR_EEPROM_MAC(2-i), &eeval)) {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: cannot read EEPROM location %u\n",
                      __func__, i);
            ecode = HAL_EEREAD;
            goto bad;
        }
        sum += eeval;
        ahp->ah_macaddr[2*i] = eeval >> 8;
        ahp->ah_macaddr[2*i + 1] = eeval & 0xff;
    }
    if (sum == 0 || sum == 0xffff*3) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: mac address read failed: %s\n",
                 __func__, ath_hal_ether_sprintf(ahp->ah_macaddr));
        ecode = HAL_EEBADMAC;
        goto bad;
    }

    ar5212AniSetup(ah);    /* setup 5212-specific ANI tables */
    ar5212AniAttach(ah);
    /* Setup of Radar/AR structures happens in ath_hal_initchannels*/

    /* XXX EAR stuff goes here */
#ifdef __CARRIER_PLATFORM__
    HDPRINTF(ah, HAL_DBG_EEPROM, "%s: use legacy device: %s\n",
        __func__, ah->ah_legacy_dev ? "true" : "false");
#endif

    return ah;

bad:
    if (ahp)
        ar5212Detach((struct ath_hal *) ahp);
    if (status)
        *status = ecode;
    return AH_NULL;
}

void
ar5212Detach(struct ath_hal *ah)
{
    HALASSERT(ah != AH_NULL);
    HALASSERT(ah->ah_magic == AR5212_MAGIC);

    /* Make sure that chip is awake before writing to it */
    ar5212SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE);

    /* XXX EEPROM allocated state */
    ar5212AniDetach(ah);
    ar5212RfDetach(ah);

        ar5212Disable(ah);
    ar5212SetPowerMode(ah, HAL_PM_FULL_SLEEP, AH_TRUE);
    ath_hal_eepromDetach(ah, &AH5212(ah)->ah_eeprom);

    ath_hal_free(ah, ah);
}

HAL_BOOL
ar5212ChipTest(struct ath_hal *ah)
{
    u_int32_t regAddr[2] = { AR_STA_ID0, AR_PHY_BASE+(8 << 2) };
    u_int32_t regHold[2];
    u_int32_t patternData[4] =
        { 0x55555555, 0xaaaaaaaa, 0x66666666, 0x99999999 };
    int i, j;

    /* Test PHY & MAC registers */
    for (i = 0; i < 2; i++) {
        u_int32_t addr = regAddr[i];
        u_int32_t wrData, rdData;

        regHold[i] = OS_REG_READ(ah, addr);
        for (j = 0; j < 0x100; j++) {
            wrData = (j << 16) | j;
            OS_REG_WRITE(ah, addr, wrData);
            rdData = OS_REG_READ(ah, addr);
            if (rdData != wrData) {
                HDPRINTF(ah, HAL_DBG_UNMASKABLE, 
                         "%s: address test failed addr: 0x%08x - wr:0x%08x != rd:0x%08x\n",
                         __func__, addr, wrData, rdData);
                return AH_FALSE;
            }
        }
        for (j = 0; j < 4; j++) {
            wrData = patternData[j];
            OS_REG_WRITE(ah, addr, wrData);
            rdData = OS_REG_READ(ah, addr);
            if (wrData != rdData) {
                HDPRINTF(ah, HAL_DBG_UNMASKABLE,
                        "%s: address test failed addr: 0x%08x - wr:0x%08x != rd:0x%08x\n",
                        __func__, addr, wrData, rdData);
                return AH_FALSE;
            }
        }
        OS_REG_WRITE(ah, regAddr[i], regHold[i]);
    }
    OS_DELAY(100);
    return AH_TRUE;
}

/*
 * Store the channel edges for the requested operational mode
 */
HAL_BOOL
ar5212GetChannelEdges(struct ath_hal *ah,
    u_int16_t flags, u_int16_t *low, u_int16_t *high)
{
    if (flags & CHANNEL_5GHZ) {
        *low = 4915;
        *high = 6100;
        return AH_TRUE;
    }
    if ((flags & CHANNEL_2GHZ) && (AH5212(ah)->ah_Bmode || AH5212(ah)->ah_Gmode )) {
        *low = 2312;
        *high = 2732;
        return AH_TRUE;
    }
    return AH_FALSE;
}

static HAL_BOOL
ar5212GetChipPowerLimits(struct ath_hal *ah, HAL_CHANNEL *chans, u_int32_t nchans)
{
    struct ath_hal_5212 *ahp = AH5212(ah);

    return ahp->ah_rfHal.getChipPowerLim(ah, chans, nchans);
}

/*
 * Fill all software cached or static hardware state information.
 * Return failure if capabilities are to come from EEPROM and
 * cannot be read.
 */
HAL_BOOL
ar5212FillCapabilityInfo(struct ath_hal *ah)
{
#define    AR_KEYTABLE_SIZE    128
#define    IS_GRIFFIN_LITE(ah) \
    (AH_PRIVATE(ah)->ah_macVersion == AR_SREV_VERSION_GRIFFIN && \
     AH_PRIVATE(ah)->ah_macRev == AR_SREV_GRIFFIN_LITE)
#define    IS_COBRA(ah) \
    (AH_PRIVATE(ah)->ah_macVersion == AR_SREV_VERSION_COBRA)
#define IS_2112(ah) \
    ((AH_PRIVATE(ah)->ah_analog5GhzRev & 0xF0) == AR_RAD2112_SREV_MAJOR)
#define IS_LITE_CHIP(ah)  \
        ( ((AH_PRIVATE(ah)->ah_macVersion == AR_SREV_VERSION_GRIFFIN) && \
          (AH_PRIVATE(ah)->ah_macRev ==  AR_SREV_GRIFFIN_LITE)) || \
          ((AH_PRIVATE(ah)->ah_macVersion == AR_SREV_VERSION_EAGLE) && \
          (AH_PRIVATE(ah)->ah_macRev == AR_SREV_EAGLE_LITE)) )
        

    struct ath_hal_5212 *ahp = AH5212(ah);
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    HAL_CAPABILITIES *pCap = &ahpriv->ah_caps;
    u_int16_t capField;
    u_int16_t regcap;

    pCap->halintr_mitigation = AH_FALSE;
    /* Read the capability EEPROM location */
    capField = 0;
    if (ahp->ah_eeversion >= AR_EEPROM_VER5_1 &&
        !ath_hal_eepromRead(ah, AR_EEPROM_CAPABILITIES_OFFSET, &capField)) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: unable to read caps from eeprom\n", __func__);
        return AH_FALSE;
    }
    if (IS_2112(ah))
        ahp->ah_Amode = AH_FALSE;
    if (capField == 0 && IS_GRIFFIN_LITE(ah)) {
        /*
         * WAR for griffin-lite cards with unprogrammed capabilities.
         */
        capField = AR_EEPROM_EEPCAP_COMPRESS_DIS
             | AR_EEPROM_EEPCAP_FASTFRAME_DIS
             ;
        ahp->ah_turbo5Disable = AH_TRUE;
        ahp->ah_turbo2Disable = AH_TRUE;
        HDPRINTF(ah, HAL_DBG_UNMASKABLE,
                 "%s: override caps for griffin-lite, now 0x%x (+no turbo)\n",
                 __func__, capField);
    }

    if (IS_LITE_CHIP(ah)) {
        capField |= (AR_EEPROM_EEPCAP_COMPRESS_DIS | AR_EEPROM_EEPCAP_FASTFRAME_DIS);
        ahp->ah_turbo5Disable = AH_TRUE;
        ahp->ah_turbo2Disable = AH_TRUE;
    }

    /* Modify reg domain on newer cards that need to work with older sw */
    if (ahpriv->ah_opmode != HAL_M_HOSTAP &&
        ahpriv->ah_subvendorid == AR_SUBVENDOR_ID_NEW_A) {
        if (ahpriv->ah_currentRD == 0x64 ||
            ahpriv->ah_currentRD == 0x65)
            ahpriv->ah_currentRD += 5;
        else if (ahpriv->ah_currentRD == 0x41)
            ahpriv->ah_currentRD = 0x43;
        HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: regdomain mapped to 0x%x\n",
            __func__, ahpriv->ah_currentRD);
    }

    if ((AH_PRIVATE(ah)->ah_macVersion == AR_SREV_2417) || (AH_PRIVATE(ah)->ah_macVersion == AR_SREV_2425)) {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s Enable Bmode and Disable Turbo for Swan/Nala\n", __func__);
        ahp->ah_Bmode = 1;
        capField |= (AR_EEPROM_EEPCAP_COMPRESS_DIS | AR_EEPROM_EEPCAP_FASTFRAME_DIS);
        ahp->ah_turbo5Disable = AH_TRUE;
        ahp->ah_turbo2Disable = AH_TRUE;
    }

    /* Construct wireless mode from EEPROM */
    pCap->halWirelessModes = 0;
    if (ahp->ah_Amode) {
        pCap->halWirelessModes |= HAL_MODE_11A;
        if (!ahp->ah_turbo5Disable)
            pCap->halWirelessModes |= HAL_MODE_TURBO;
    }
    if (ahp->ah_Bmode)
        pCap->halWirelessModes |= HAL_MODE_11B;
    if (ahp->ah_Gmode &&
        ahpriv->ah_subvendorid != AR_SUBVENDOR_ID_NOG) {
        pCap->halWirelessModes |= HAL_MODE_11G;
        if (!ahp->ah_turbo2Disable)
            pCap->halWirelessModes |= HAL_MODE_108G;
    }

    pCap->halLow2GhzChan = 2312;
    if (IS_5112(ah) || IS_2413(ah) || IS_5413(ah) || IS_2425(ah) || IS_2417(ah))
        pCap->halHigh2GhzChan = 2500;
    else
        pCap->halHigh2GhzChan = 2732;

    pCap->halLow5GhzChan = 4915;
    pCap->halHigh5GhzChan = 6100;

    pCap->halCipherCkipSupport = AH_FALSE;
    pCap->halCipherTkipSupport = AH_TRUE;
    pCap->halCipherAesCcmSupport =
        (!(capField & AR_EEPROM_EEPCAP_AES_DIS) &&
         ((AH_PRIVATE(ah)->ah_macVersion > AR_SREV_VERSION_VENICE) ||
          ((AH_PRIVATE(ah)->ah_macVersion == AR_SREV_VERSION_VENICE) &&
           (AH_PRIVATE(ah)->ah_macRev >= AR_SREV_VERSION_OAHU))));

    pCap->halMicCkipSupport    = AH_FALSE;
    pCap->halMicTkipSupport    = AH_TRUE;
    pCap->halMicAesCcmSupport  = !(capField & AR_EEPROM_EEPCAP_AES_DIS);

    pCap->halChanSpreadSupport = AH_TRUE;
    pCap->halSleepAfterBeaconBroken = AH_TRUE;

    /*
     * Starting from griffin enable the feature where the 2 mic keys
     * (tx and rx) can be combined into one key slot.
     */
    if ((AH_PRIVATE(ah)->ah_macVersion >= AR_SREV_VERSION_GRIFFIN))
        ahp->ah_miscMode |= AR_MISC_MODE_MIC_NEW_LOC_ENABLE;

    if ((ahpriv->ah_macRev > 1) || IS_COBRA(ah)) {
        pCap->halCompressSupport   =
            !(capField & AR_EEPROM_EEPCAP_COMPRESS_DIS) &&
            (pCap->halWirelessModes & (HAL_MODE_11A|HAL_MODE_11G)) != 0;
#ifdef AH_SUPPORT_2417
        if (IS_2417(ah))
            pCap->halBurstSupport = 1;
        else
#endif
        pCap->halBurstSupport = !(capField & AR_EEPROM_EEPCAP_BURST_DIS);
        pCap->halFastFramesSupport =
            !(capField & AR_EEPROM_EEPCAP_FASTFRAME_DIS) &&
            (pCap->halWirelessModes & (HAL_MODE_11A|HAL_MODE_11G)) != 0;
        pCap->halChapTuningSupport = AH_TRUE;
        pCap->halTurboPrimeSupport = AH_TRUE;
    }
    pCap->halTurboGSupport = pCap->halWirelessModes & HAL_MODE_108G;
    /* Give XR support unless both disable bits are set */
    pCap->halXrSupport = !(ahp->ah_disableXr5 && ahp->ah_disableXr2);

    pCap->halPSPollBroken = AH_TRUE;    /* XXX fixed in later revs? */
    pCap->halVEOLSupport = AH_TRUE;
    pCap->halBssIdMaskSupport = AH_TRUE;
    pCap->halMcastKeySrchSupport = AH_TRUE;
    if ((ahpriv->ah_macVersion == AR_SREV_VERSION_VENICE &&
         ahpriv->ah_macRev == 8) ||
        ahpriv->ah_macVersion > AR_SREV_VERSION_VENICE)
        pCap->halTsfAddSupport = AH_TRUE;

    if (capField & AR_EEPROM_EEPCAP_MAXQCU)
        pCap->halTotalQueues = MS(capField, AR_EEPROM_EEPCAP_MAXQCU);
    else
        pCap->halTotalQueues = HAL_NUM_TX_QUEUES;

    if (capField & AR_EEPROM_EEPCAP_KC_ENTRIES)
        pCap->halKeyCacheSize =
            1 << MS(capField, AR_EEPROM_EEPCAP_KC_ENTRIES);
    else
        pCap->halKeyCacheSize = AR_KEYTABLE_SIZE;

    if (IS_5112(ah)) {
        pCap->halChanHalfRate = AH_TRUE;
        pCap->halChanQuarterRate = AH_TRUE;
    } else {
        /* XXX not needed */
        pCap->halChanHalfRate = AH_FALSE;
        pCap->halChanQuarterRate = AH_FALSE;
    }

    pCap->hal49Ghz = AH_TRUE;

    if (ahp->ah_rfKill &&
        ath_hal_eepromRead(ah, AR_EEPROM_RFSILENT, &ahpriv->ah_rfsilent)) {
        /* NB: enabled by default */
        ahpriv->ah_rfkillEnabled = AH_TRUE;
        pCap->halRfSilentSupport = AH_TRUE;
    }

    /* 11n capabilities */
    pCap->halHTSupport = AH_FALSE;
    pCap->halGTTSupport = AH_FALSE;
    pCap->halFastCCSupport = AH_FALSE;
    pCap->halNumMRRetries = 4;
    pCap->halTxChainMask = 0;
    pCap->halRxChainMask = 0;
    pCap->halTxTrigLevelMax = MAX_TX_FIFO_THRESHOLD;
    pCap->halNumGpioPins = AR_NUM_GPIO;
    pCap->halWowSupport = (AH_PRIVATE(ah)->ah_macVersion == AR_SREV_2425) ? AH_TRUE : AH_FALSE;
    pCap->halWowMatchPatternExact = AH_FALSE;
    pCap->halCSTSupport = AH_FALSE;
    pCap->halRifsRxSupport = AH_FALSE;
    pCap->halRifsTxSupport = AH_FALSE;
    pCap->halforcePpmSupport = AH_FALSE;
    pCap->halEnhancedPmSupport = (AH_PRIVATE(ah)->ah_macVersion >= AR_SREV_2413) ? AH_TRUE : AH_FALSE;
    pCap->halRtsAggrLimit = 0;
    pCap->halWpsPushButton = AH_FALSE;
    pCap->halBtCoexSupport = AH_FALSE;
    pCap->halGenTimerSupport = AH_FALSE;
    pCap->halEnhancedDmaSupport = AH_FALSE;
#ifdef ATH_SUPPORT_DFS
    pCap->hal_enhanced_dfs_support = AH_FALSE;
#endif
    pCap->halIsrRacSupport = AH_FALSE;
    pCap->halNumTxMaps = 1;
    pCap->halTxDescLen = sizeof(struct ath_desc);
    pCap->halTxStatusLen = 0;
    pCap->halRxStatusLen = 0;
    pCap->halWepTkipAggrSupport = AH_FALSE;
    pCap->halWepTkipAggrNumTxDelim = 0;
    pCap->halWepTkipAggrNumRxDelim = 0;
    pCap->halWepTkipMaxHtRate = 0;
    pCap->halHwUapsdTrig = AH_FALSE;

    /*
     * Legacy chips can automatically return to network sleep mode after
     * waking up to receive TIM.
     */
    pCap->halAutoSleepSupport = AH_TRUE;
    pCap->halMbssidAggrSupport = AH_FALSE;

    pCap->hal4kbSplitTransSupport = AH_TRUE;
    pCap->halProxySTASupport = AH_FALSE;

    /* Get Japan regulatory domain flags */
    if (!ath_hal_eepromRead(ah, (ahp->ah_eeversion >= AR_EEPROM_VER4_0)? 
            AR_EEPROM_REG_CAPABILITIES_OFFSET:
            AR_EEPROM_REG_CAPABILITIES_OFFSET_PRE4_0, 
            &regcap)) {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: unable to read EEPROM regcaps\n", __func__);
            return 0;
    }

    if (ahp->ah_eeversion  < AR_EEPROM_VER4_0) {
        /* 
         * EEPROM Ver < 4.0 
         * Only three combinations should exist. No bit, NEW_11A, NEW_11A + U1_ODD.
         * No bit ==> U1 odd with active scan.
         * NEW_11A or NEW_11A + U1_ODD ==> U1 odd with passive scan + U1 even.
         * NEW_11A bit implies U1 odd with passive scan.
         */
        u_int16_t   newregcap = 0;

        if (regcap & AR_EEPROM_EEREGCAP_EN_KK_U1_ODD_PRE4_0) {
            newregcap |= AR_EEPROM_EEREGCAP_EN_KK_U1_ODD;
        }
        if (regcap & AR_EEPROM_EEREGCAP_EN_KK_NEW_11A_PRE4_0) {
            newregcap |= (AR_EEPROM_EEREGCAP_EN_KK_NEW_11A | AR_EEPROM_EEREGCAP_EN_KK_U1_ODD);
        }
        regcap = newregcap;

        if (!regcap) {
            /*
             * If no bits are set, it's a card neven been updated. Just allow U1 ODD wtih active scan.
             */
            regcap |= AR_EEPROM_EEREGCAP_EN_KK_U1_ODD;
        }

        /*
         * For legacy devices the UNI-1 Even support must be set based on if the "Japan New 11a"
         * flag is set.  The HAL will only returns capabilities for "Japan New 11a" and "Odd
         * UNI-1 channel support"
         */
        regcap |= (regcap & AR_EEPROM_EEREGCAP_EN_KK_NEW_11A) ? AR_EEPROM_EEREGCAP_EN_KK_U1_EVEN : 0;
    }
    else if (ahp->ah_eeversion  < AR_EEPROM_VER5_3) {
        /* 
         * 4.0 <= EEPROM Ver < 5.3 
         * Only three combinations should exist. No bit, NEW_11A, NEW_11A + U1_ODD.
         * No bit ==> U1 odd with active scan.
         * NEW_11A or NEW_11A + U1_ODD ==> U1 odd with passive scan + U1 even.
         * NEW_11A bit implies U1 odd with passive scan.
         */
        if (!(regcap & AR_EEPROM_EEREGCAP_EN_KK_NEW_11A)) {
            /*
             * This device has legacy EEPROM and has not been updated to indicate it is certified
             * to operate under the new 11a Japan laws.  In this case we must continue to operate
             * on the funky odd channels, so clear any of the supported MKK reg capabilities
             */
            regcap &= AR_EEPROM_EEREGCAP_EN_FCC_MIDBAND;
            regcap |= AR_EEPROM_EEREGCAP_EN_KK_U1_ODD;
        }
        /*
         * For legacy devices the UNI-1 Even support must be set based on if the "Japan New 11a"
         * flag is set.  The HAL will only returns capabilities for "Japan New 11a" and "Odd
         * UNI-1 channel support"
         */
        regcap |= (regcap & AR_EEPROM_EEREGCAP_EN_KK_NEW_11A) ? 
                           (AR_EEPROM_EEREGCAP_EN_KK_U1_EVEN | AR_EEPROM_EEREGCAP_EN_KK_U1_ODD) : 0;
    }
    else if (ahp->ah_eeversion  >= AR_EEPROM_VER5_3) {
        /* EEPROM Ver >= 5.3 */
        /* For EERPOM v5.3 or greater can not be certified under new rules to operate in ODD only
         * we will prevent this configuration by disabling odd support when no even support
         * is enabled
         */
        if (!(regcap & AR_EEPROM_EEREGCAP_EN_KK_U1_EVEN)) {
            regcap &= ~AR_EEPROM_EEREGCAP_EN_KK_U1_ODD;
        }
    }
    pCap->halRegCap = regcap;
    pCap->halCfendFixSupport = AH_FALSE;
    pCap->halAggrExtraDelimWar = AH_FALSE;
    pCap->halRxTxAbortSupport = AH_FALSE;
    pCap->halAniPollInterval = AR5212_ANI_POLLINTERVAL;
    pCap->halChannelSwitchTimeUsec = AR5212_CHANNEL_SWITCH_TIME_USEC;
 	pCap->halRxDescTimestampBits = 15;
    pCap->halRxTxAbortSupport = AH_FALSE;
 
#if ATH_SUPPORT_WAPI
    /*
     * WAPI engine support 1 stream rates at most currently
     */
    pCap->hal_wapi_max_tx_chains = 1;
    pCap->hal_wapi_max_rx_chains = 1;
#endif

   return AH_TRUE;
#undef IS_COBRA
#undef IS_GRIFFIN_LITE
#undef AR_KEYTABLE_SIZE
#undef IS_LITE_CHIP
}

static HAL_BOOL
ar5212DummyStartTxDma(struct ath_hal *ah, u_int q)
{
    HAL_CHANNEL hchan;
    ar5212RadarWait(ah,&hchan);
    return AH_TRUE;
}

void
ar5212TxEnable(struct ath_hal *ah,HAL_BOOL enable)
{
    if (enable == AH_TRUE)
        ah->ah_startTxDma = ar5212StartTxDma;
    else
        ah->ah_startTxDma = ar5212DummyStartTxDma;
}

/* 11n specific declarations. Unused in ar5212 */

void
ar5212Set11nAggrFirst(struct ath_hal *ah, void *ds, u_int aggrLen)
{
}

void
ar5212Set11nAggrMiddle(struct ath_hal *ah, void *ds, u_int numDelims)
{
}

void
ar5212Set11nAggrLast(struct ath_hal *ah, void *ds)
{
}

void
ar5212Clear11nAggr(struct ath_hal *ah, void *ds)
{
}

HAL_BOOL
ar5212SetGlobalTxTimeout(struct ath_hal *ah, u_int tu)
{
    return AH_TRUE;
}

u_int
ar5212GetGlobalTxTimeout(struct ath_hal *ah)
{
    return 0;
}

void
ar5212Set11nRifsBurstMiddle(struct ath_hal *ah, void *ds)
{
}

void
ar5212Set11nRifsBurstLast(struct ath_hal *ah, void *ds)
{
}

void
ar5212Clr11nRifsBurst(struct ath_hal *ah, void *ds)
{
}

void
ar5212Set11nAggrRifsBurst(struct ath_hal *ah, void *ds)
{
}

HAL_BOOL
ar5212Set11nRxRifs(struct ath_hal *ah, HAL_BOOL enable)
{
    return AH_FALSE;
}

HAL_BOOL
ar5212Get11nRxRifs(struct ath_hal *ah)
{
    return AH_FALSE;
}

HAL_BOOL
ar5212DetectBbHang(struct ath_hal *ah)
{
    return AH_FALSE;
}

HAL_BOOL
ar5212DetectMacHang(struct ath_hal *ah)
{
    return AH_FALSE;
}

void
ar5212ResetCalValid(struct ath_hal *ah, HAL_CHANNEL *chan, HAL_BOOL *isIQdone, u_int32_t calType)
{
}

void
ar5212Set11nBurstDuration(struct ath_hal *ah, void *ds, u_int burstDuration)
{
}

void
ar5212Set11nVirtMoreFrag(struct ath_hal *ah, void *ds, u_int vmf)
{
}

int8_t
ar5212Get11nExtBusy(struct ath_hal *ah)
{
    return 0;
}

void
ar5212Set11nMac2040(struct ath_hal *ah, HAL_HT_MACMODE macmode)
{
}

HAL_HT_RXCLEAR
ar5212Get11nRxClear(struct ath_hal *ah)
{
    return HAL_RX_CLEAR_CTL_LOW;
}

void
ar5212Set11nRxClear(struct ath_hal *ah, HAL_HT_RXCLEAR rxclear)
{
}

int
ar5212Get11nHwPlatform(struct ath_hal *ah)
{
    return 0;
}

void
ar5212DmaRegDump(struct ath_hal *ah)
{
}

u_int32_t
ar5212PpmGetRssiDump(struct ath_hal *ah)
{
    return 0;
}

u_int32_t
ar5212PpmArmTrigger(struct ath_hal *ah)
{
    return 0;
}

int
ar5212PpmGetTrigger(struct ath_hal *ah)
{
    return 0;
}

u_int32_t
ar5212PpmForce(struct ath_hal *ah)
{
    return 0;
}

void
ar5212PpmUnForce(struct ath_hal *ah)
{
}

u_int32_t
ar5212PpmGetForceState(struct ath_hal *ah)
{
    return 0;
}

static HAL_BOOL
ar5212FindHB63(struct ath_hal *ah)
{
    u_int16_t eeval;

    if(IS_2425(ah)) {
        if (ar5212EepromRead(ah, AR_EEPROM_VERSION, &eeval)) {
            if (eeval >= AR_EEPROM_VER5_4) {
                if (ar5212EepromRead(ah, 0x0b, &eeval)) {
                    if (eeval == 1) {
                        return AH_TRUE;
                    }
                }
            }
        }
    }
    return AH_FALSE;
}

#ifdef ATH_CCX
HAL_BOOL
ar5212RecordSerialNumber(struct ath_hal *ah, u_int8_t *sn)
{
    u_int16_t   i, data=0;
    HAL_BOOL    status=AH_TRUE;

    for(i=0; i<AR_EEPROM_SERIAL_NUM_SIZE/2; i++) {
        status = ar5212EepromRead(ah, AR_EEPROM_SERIAL_NUM_OFFSET+i, &data);

        if(status != AH_TRUE){
            break;
        }
        sn[2 * i]     = (u_int8_t)(data & 0xFF);
        sn[2 * i + 1] = (u_int8_t)((data >> 8) & 0xFF);
    }

    if ((u_int8_t)sn[AR_EEPROM_SERIAL_NUM_SIZE - 1] == 0xFF) {
        u_int8_t tempChar;
        for (i = 0; i < (AR_EEPROM_SERIAL_NUM_SIZE - 1) / 2; i ++) {
            tempChar = sn[i];
            sn[i] = sn[AR_EEPROM_SERIAL_NUM_SIZE - 2 - i];
            sn[AR_EEPROM_SERIAL_NUM_SIZE - 2 - i] = tempChar;
        }
        sn[AR_EEPROM_SERIAL_NUM_SIZE - 1] = '\0';
    }
    return status;
}
#endif

void ar5212ChainNoiseFloor(struct ath_hal *ah, int16_t *nfBuf, HAL_CHANNEL *chan, int is_scan)
{
    int i;

    nfBuf[0] = ath_hal_getChanNoise(ah, chan);
    for (i = 1; i < NUM_NF_READINGS; i++) {
        /* Fill 0 for unsupported chains */
        nfBuf[i] = 0;
    }
}

#ifdef ATH_BT_COEX
void
ar5212SetBTCoexInfo(struct ath_hal *ah, HAL_BT_COEX_INFO *btinfo)
{
}

void ar5212BTCoexConfig(struct ath_hal *ah, HAL_BT_COEX_CONFIG *btconf)
{
}

void
ar5212BTCoexSetQcuThresh(struct ath_hal *ah, int qnum)
{
}

void ar5212BTCoexSetWeights(struct ath_hal *ah, u_int32_t stompType)
{
}

void ar5212BTCoexSetupBmissThresh(struct ath_hal *ah, u_int32_t thresh)
{
}

void ar5212BTCoexSetParameter(struct ath_hal *ah, u_int32_t type, u_int32_t value)
{
}

void ar5212BTCoexDisable(struct ath_hal *ah)
{
}

int
ar5212BTCoexEnable(struct ath_hal *ah)
{
    return 0;
}
#endif /* ATH_BT_COEX */

int
ar5212AllocGenericTimer(struct ath_hal *ah, HAL_GEN_TIMER_DOMAIN tsf)
{
    return -1;
}

void
ar5212FreeGenericTimer(struct ath_hal *ah, int index)
{
}

void
ar5212StartGenericTimer(struct ath_hal *ah, int index, u_int32_t timer_next, u_int32_t timer_period)
{
}

void
ar5212StopGenericTimer(struct ath_hal *ah, int index)
{
}

void
ar5212GetGenTimerInterrupts(struct ath_hal *ah, u_int32_t *trigger, u_int32_t *thresh)
{
}

void
ar5212SetSmPowerMode(struct ath_hal *ah, HAL_SMPS_MODE mode)
{
}

void
ar5212SetImmunity(struct ath_hal *ah, HAL_BOOL enable)
{
}

/* Get the number of antenna config. Since this feature is not
 * implemented in 5212 chips and it returns only 1 possible antenna
 * configeration, thus disabling the feature
 */
int ar5212GetNumAntCfg(struct ath_hal *ah)
{
    return 1;
}

void ar5212GreenApPsOnOff(struct ath_hal *ah, u_int16_t rxMask)
{
}

/* This is a dummy function for single antenna power save feature.
 * This feature is not implemented in 5212 chips and therefore, it always
 * returns 0
 */
u_int16_t ar5212IsSingleAntPowerSavePossible( struct ath_hal *ah)
{
    return 0;
}

static int16_t
ar5212GetMinCCAPwr(struct ath_hal *ah)
{
    return 0;
}

static void
ar5212SetNominalUserNFVal(struct ath_hal *ah, int16_t val, HAL_BOOL is2GHz)
{
}

static int16_t
ar5212GetNominalUserNFVal(struct ath_hal *ah, HAL_BOOL is2GHz)
{
    return 0;
}

static void
ar5212SetMinUserNFVal(struct ath_hal *ah, int16_t val, HAL_BOOL is2GHz)
{
}

static int16_t
ar5212GetMinUserNFVal(struct ath_hal *ah, HAL_BOOL is2GHz)
{
    return 0;
}

static void
ar5212SetMaxUserNFVal(struct ath_hal *ah, int16_t val, HAL_BOOL is2GHz)
{
}

static int16_t
ar5212GetMaxUserNFVal(struct ath_hal *ah, HAL_BOOL is2GHz)
{
    return 0;
}

static void
ar5212SetNfDeltaVal(struct ath_hal *ah, int16_t val)
{
}

static int16_t
ar5212GetNfDeltaVal(struct ath_hal *ah)
{
    return 0;
}

void ar5212SetIntrMitigationTimer(
    struct ath_hal* ah, HAL_INT_MITIGATION reg, u_int32_t value)
{
}

u_int32_t ar5212GetIntrMitigationTimer(
    struct ath_hal* ah, HAL_INT_MITIGATION reg)
{
    return 0;
}

#if ATH_ANT_DIV_COMB
void
ar5212AntDivCombGetConfig(struct ath_hal *ah, HAL_ANT_COMB_CONFIG* divCombConf)
{
}

void
ar5212AntDivCombSetConfig(struct ath_hal *ah, HAL_ANT_COMB_CONFIG* divCombConf)
{
}
#endif /* ATH_ANT_DIV_COMB */

#if ATH_SUPPORT_WIRESHARK
void ar5212FillRadiotapHdr(struct ath_hal *ah,
                           struct ah_rx_radiotap_header *rh,                           
                           struct ah_ppi_data *ppi,
                           struct ath_desc *ds, void *buf_addr)
{
    struct ar5212_desc *adsp = AR5212DESC(ds);

    if (rh != NULL) {
        OS_MEMZERO(rh, sizeof(struct ah_rx_radiotap_header));

        rh->tsf = ar5212GetTsf64(ah); /* AR5212 specific */
        rh->wr_antsignal = MS(adsp->ds_rxstatus0, AR_RcvSigStrength);


        if (adsp->ds_rxstatus1 & AR_CRCErr) {
            rh->wr_flags |= AH_RADIOTAP_F_BADFCS;
            rh->wr_rx_flags |= AH_RADIOTAP_F_RX_BADFCS;
        }

        if (adsp->ds_rxstatus1 & AR_PHYErr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_PHYERR;
        }

        if (adsp->ds_rxstatus1 & AR_DecryptCRCErr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_DECRYPTCRCERR;
        }
    }
    
    if (ppi != NULL) {
        struct ah_ppi_pfield_common             *pcommon;
        struct ah_ppi_pfield_mac_extensions     *pmac;
        struct ah_ppi_pfield_macphy_extensions  *pmacphy;
        
        // Fill out PPI Data
        pcommon = ppi->ppi_common;
        pmac = ppi->ppi_mac_ext;
        pmacphy = ppi->ppi_macphy_ext;

        // Zero out all fields
        OS_MEMZERO(pcommon, sizeof(struct ah_ppi_pfield_common));

        /* Common Fields */
        // Grab the tsf
        pcommon->common_tsft =  ar5212GetTsf64(ah);
        // Fill out common flags
        if (adsp->ds_rxstatus1 & AR_CRCErr) {
            pcommon->common_flags |= 4;
        }
        if (adsp->ds_rxstatus1 & AR_PHYErr) {
            pcommon->common_flags |= 8;
        }
        // Set the rate in the calling layer - it's already translated etc there.        
        // Channel frequency
        pcommon->common_chanFreq = (AH_PRIVATE(ah)->ah_curchan)?AH_PRIVATE(ah)->ah_curchan->channel:0;
        // Channel flags
        pcommon->common_chanFlags = AH_PRIVATE(ah)->ah_curchan->channelFlags;
        pcommon->common_dBmAntSignal = MS(adsp->ds_rxstatus0, AR_RcvSigStrength)-96;


        if (pmac != NULL) {        
            OS_MEMZERO(pmac, sizeof(struct ah_ppi_pfield_mac_extensions));
        }
        if (pmacphy != NULL) {        
            OS_MEMZERO(pmacphy, sizeof(struct ah_ppi_pfield_macphy_extensions));
        }

    }
}
#endif /* ATH_SUPPORT_WIRESHARK */

int
ar5212GetCalIntervals(struct ath_hal *ah, HAL_CALIBRATION_TIMER **timerp, HAL_CAL_QUERY query)
{
    *timerp = AH_NULL;

    return 0;
}

#endif /* AH_SUPPORT_AR5212 */
