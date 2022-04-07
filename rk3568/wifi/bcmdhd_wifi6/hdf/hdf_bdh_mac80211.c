/*
 * hdf_bdh_mac80211.c
 *
 * hdf driver
 *
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#include <net/cfg80211.h>
#include <net/regulatory.h>
#include <securec.h>
#include <linux/version.h>

#include "wifi_module.h"
#include "wifi_mac80211_ops.h"
#include "hdf_wlan_utils.h"
#include "net_bdh_adpater.h"
#include "hdf_wl_interface.h"
#include "hdf_public_ap6275s.h"
#include "hdf_mac80211_sta.h"

#define HDF_LOG_TAG BDH6Driver
extern struct cfg80211_ops wl_cfg80211_ops;
extern struct net_device_ops dhd_ops_pri;
extern struct hdf_inf_map g_hdf_infmap[HDF_INF_MAX];
extern u32 p2p_remain_freq;
extern int g_mgmt_tx_event_ifidx;
typedef enum {
    WLAN_BAND_2G,
    WLAN_BAND_5G,
    WLAN_BAND_BUTT
} wlan_channel_band_enum;

#define WIFI_24G_CHANNEL_NUMS   (14)
#define WAL_MIN_CHANNEL_2G      (1)
#define WAL_MAX_CHANNEL_2G      (14)
#define WAL_MIN_FREQ_2G         (2412 + 5*(WAL_MIN_CHANNEL_2G - 1))
#define WAL_MAX_FREQ_2G         (2484)
#define WAL_FREQ_2G_INTERVAL    (5)

#define WLAN_WPS_IE_MAX_SIZE    (352) // (WLAN_MEM_EVENT_SIZE2 - 32)   /* 32表示事件自身占用的空间 */
#define MAC_80211_FRAME_LEN                 24      /* 非四地址情况下，MAC帧头的长度 */

struct NetDevice *get_real_netdev(NetDevice *netDev);
int32_t WalStopAp(NetDevice *netDev);
struct wiphy *get_linux_wiphy_ndev(struct net_device *ndev)
{
    if (ndev == NULL || ndev->ieee80211_ptr == NULL) {
        return NULL;
    }

    return ndev->ieee80211_ptr->wiphy;
}

struct wiphy *get_linux_wiphy_hdfdev(NetDevice *netDev)
{
    struct net_device *ndev = GetLinuxInfByNetDevice(netDev);
    return get_linux_wiphy_ndev(ndev);
}

int32_t BDH6WalSetMode(NetDevice *hnetDev, enum WlanWorkMode iftype)
{
    int32_t retVal = 0;
    struct net_device *netdev = NULL;
    NetDevice *netDev = NULL;
    struct wiphy *wiphy = NULL;
    netDev = get_real_netdev(hnetDev);
	enum nl80211_iftype old_iftype = 0;
    
    netdev = GetLinuxInfByNetDevice(netDev);
    if (!netdev) {
        HDF_LOGE("%s: net_device is NULL", __func__);
        return -1;
    }
    
    wiphy = get_linux_wiphy_ndev(netdev);
    if (!wiphy) {
        HDF_LOGE("%s: wiphy is NULL", __func__);
        return -1;
    }
	old_iftype = netdev->ieee80211_ptr->iftype;

    HDF_LOGE("%s: start... iftype=%d ", __func__, iftype);
	if (old_iftype == NL80211_IFTYPE_AP && iftype != old_iftype)
		WalStopAp(netDev);
    retVal = (int32_t)wl_cfg80211_ops.change_virtual_intf(wiphy, netdev,
        (enum nl80211_iftype)iftype, NULL);
    if (retVal < 0) {
        HDF_LOGE("%s: set mode failed!", __func__);
    }

    return retVal;
}

int32_t BDH6WalAddKey(struct NetDevice *hnetDev, uint8_t keyIndex, bool pairwise, const uint8_t *macAddr,
    struct KeyParams *params)
{
    int32_t retVal = 0;
    struct NetDevice *netDev = NULL;
    struct net_device *netdev = NULL;
    struct wiphy *wiphy = NULL;
    netDev = get_real_netdev(hnetDev);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
    struct key_params keypm;
#endif

    netdev = GetLinuxInfByNetDevice(netDev);
    if (!netdev) {
        HDF_LOGE("%s: net_device is NULL", __func__);
        return -1;
    }
    
    wiphy = get_linux_wiphy_ndev(netdev);
    if (!wiphy) {
        HDF_LOGE("%s: wiphy is NULL", __func__);
        return -1;
    }
    
    HDF_LOGE("%s: start..., mac = %p, keyIndex = %u,pairwise = %d, cipher = 0x%x, seqlen = %d, keylen = %d",
        __func__, macAddr, keyIndex, pairwise, params->cipher, params->seqLen, params->keyLen);
    (void)netDev;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
    memset_s(&keypm, sizeof(struct key_params), 0, sizeof(struct key_params));
    keypm.key = params->key;
    keypm.seq = params->seq;
    keypm.key_len = params->keyLen;
    keypm.seq_len = params->seqLen;
    keypm.cipher = params->cipher;
    keypm.vlan_id = 0;
    retVal = (int32_t)wl_cfg80211_ops.add_key(wiphy, netdev, keyIndex, pairwise, macAddr, &keypm);
#else
    retVal = (int32_t)wl_cfg80211_ops.add_key(wiphy, netdev, keyIndex, pairwise, macAddr, (struct key_params *)params);
#endif
    if (retVal < 0) {
        HDF_LOGE("%s: add key failed!", __func__);
    }

    return retVal;
}

int32_t BDH6WalDelKey(struct NetDevice *hnetDev, uint8_t keyIndex, bool pairwise, const uint8_t *macAddr)
{
    int32_t retVal = 0;
    struct NetDevice *netDev = NULL;
    struct net_device *netdev = NULL;
    struct wiphy *wiphy = NULL;
    netDev = get_real_netdev(hnetDev);

    netdev = GetLinuxInfByNetDevice(netDev);
    if (!netdev) {
        HDF_LOGE("%s: net_device is NULL", __func__);
        return -1;
    }
    
    wiphy = get_linux_wiphy_ndev(netdev);
    if (!wiphy) {
        HDF_LOGE("%s: wiphy is NULL", __func__);
        return -1;
    }

    HDF_LOGE("%s: start..., mac=%p, keyIndex=%u,pairwise=%d", __func__, macAddr, keyIndex, pairwise);

    (void)netDev;
    retVal = (int32_t)wl_cfg80211_ops.del_key(wiphy, netdev, keyIndex, pairwise, macAddr);
    if (retVal < 0) {
        HDF_LOGE("%s: delete key failed!", __func__);
    }

    return retVal;
}

int32_t BDH6WalSetDefaultKey(struct NetDevice *hnetDev, uint8_t keyIndex, bool unicast, bool multicas)
{
    int32_t retVal = 0;
    struct NetDevice *netDev = NULL;
    struct net_device *netdev = NULL;
    struct wiphy *wiphy = NULL;
    netDev = get_real_netdev(hnetDev);

    netdev = GetLinuxInfByNetDevice(netDev);
    if (!netdev) {
        HDF_LOGE("%s: net_device is NULL", __func__);
        return -1;
    }
    
    wiphy = get_linux_wiphy_ndev(netdev);
    if (!wiphy) {
        HDF_LOGE("%s: wiphy is NULL", __func__);
        return -1;
    }
    HDF_LOGE("%s: start..., keyIndex=%u,unicast=%d, multicas=%d", __func__, keyIndex, unicast, multicas);
    retVal = (int32_t)wl_cfg80211_ops.set_default_key(wiphy, netdev, keyIndex, unicast, multicas);
    if (retVal < 0) {
        HDF_LOGE("%s: set default key failed!", __func__);
    }

    return retVal;
}

int32_t BDH6WalGetDeviceMacAddr(NetDevice *hnetDev, int32_t type, uint8_t *mac, uint8_t len)
{
    struct NetDevice *netDev = NULL;
    netDev = get_real_netdev(hnetDev);
    struct net_device *netdev = GetLinuxInfByNetDevice(netDev);
    if (!netdev) {
        HDF_LOGE("%s: net_device is NULL", __func__);
        return -1;
    }

    (void)len;
    (void)type;
    (void)netDev;
    HDF_LOGE("%s: start...", __func__);
    
    memcpy_s(mac, len, netdev->dev_addr, netdev->addr_len);

    return HDF_SUCCESS;
}

int32_t BDH6WalSetMacAddr(NetDevice *hnetDev, uint8_t *mac, uint8_t len)
{
    int32_t retVal = 0;
    struct NetDevice *netDev = NULL;
    netDev = get_real_netdev(hnetDev);
    struct net_device *netdev = GetLinuxInfByNetDevice(netDev);
    if (!netdev) {
        HDF_LOGE("%s: net_device is NULL", __func__);
        return -1;
    }

    (void)len;
    (void)netDev;
    HDF_LOGE("%s: start...", __func__);

    retVal = (int32_t)dhd_ops_pri.ndo_set_mac_address(netdev, mac);
    if (retVal < 0) {
        HDF_LOGE("%s: set mac address failed!", __func__);
    }

    return retVal;
}

int32_t BDH6WalSetTxPower(NetDevice *hnetDev, int32_t power)
{
    int retVal = 0;
    struct wiphy *wiphy = NULL;
    struct NetDevice *netDev = NULL;
    netDev = get_real_netdev(hnetDev);

    // sync from net_device->ieee80211_ptr
    struct wireless_dev *wdev = GET_NET_DEV_CFG80211_WIRELESS(netDev);

    wiphy = get_linux_wiphy_hdfdev(netDev);
    if (!wiphy) {
        HDF_LOGE("%s: wiphy is NULL", __func__);
        return -1;
    }

    HDF_LOGE("%s: start...", __func__);
    retVal = (int32_t)wl_cfg80211_ops.set_tx_power(wiphy, wdev, NL80211_TX_POWER_FIXED, power);
    if (retVal < 0) {
        HDF_LOGE("%s: set_tx_power failed!", __func__);
    }
        
    return HDF_SUCCESS;
}

const struct ieee80211_regdomain *bdh6_get_regdomain(void);


int32_t Bdh6Fband(NetDevice *hnetDev, int32_t band, int32_t *freqs, uint32_t *num)
{
    uint32_t freqIndex = 0;
    uint32_t channelNumber;
    uint32_t freqTmp;
    uint32_t minFreq;
    uint32_t maxFreq;
    
    struct wiphy* wiphy = NULL;
    struct NetDevice *netDev = NULL;
    struct ieee80211_supported_band *band5g = NULL;
    int32_t max5GChNum = 0;
    const struct ieee80211_regdomain *regdom = bdh6_get_regdomain();
    if (regdom == NULL) {
        HDF_LOGE("%s: wal_get_cfg_regdb failed!", __func__);
        return HDF_FAILURE;
    }

    netDev = get_real_netdev(hnetDev);
    wiphy = get_linux_wiphy_hdfdev(netDev);
    if (!wiphy) {
        HDF_LOGE("%s: wiphy is NULL", __func__);
        return -1;
    }
    
    (void)netDev;
    HDF_LOGE("%s: start..., band=%d", __func__, band);

    minFreq = regdom->reg_rules[0].freq_range.start_freq_khz / MHZ_TO_KHZ(1);
    maxFreq = regdom->reg_rules[0].freq_range.end_freq_khz / MHZ_TO_KHZ(1);
    switch (band) {
        case WLAN_BAND_2G:
            for (channelNumber = 1; channelNumber <= WIFI_24G_CHANNEL_NUMS; channelNumber++) {
                if (channelNumber < WAL_MAX_CHANNEL_2G) {
                    freqTmp = WAL_MIN_FREQ_2G + (channelNumber - 1) * WAL_FREQ_2G_INTERVAL;
                } else if (channelNumber == WAL_MAX_CHANNEL_2G) {
                    freqTmp = WAL_MAX_FREQ_2G;
                }
                if (freqTmp < minFreq || freqTmp > maxFreq) {
                    continue;
                }

                HDF_LOGE("bdh6 2G %u: freq=%u\n", freqIndex, freqTmp);
                freqs[freqIndex] = freqTmp;
                freqIndex++;
            }
            *num = freqIndex;
            break;

        case WLAN_BAND_5G:
            band5g = wiphy->bands[IEEE80211_BAND_5GHZ];
            if (NULL == band5g) {
                return HDF_ERR_NOT_SUPPORT;
            }
            
            max5GChNum = min(band5g->n_channels, WIFI_24G_CHANNEL_NUMS);
            for (freqIndex = 0; freqIndex < max5GChNum; freqIndex++) {
                freqs[freqIndex] = band5g->channels[freqIndex].center_freq;
                HDF_LOGE("bdh6 5G %u: freq=%u\n", freqIndex, freqs[freqIndex]);
            }
            *num = freqIndex;
            break;
        default:
            HDF_LOGE("%s: no support band!", __func__);
            return HDF_ERR_NOT_SUPPORT;
    }
    return HDF_SUCCESS;
}

void BDH6WalReleaseHwCapability(struct WlanHwCapability *self)
{
    uint8_t i;
    if (self == NULL) {
        return;
    }
    for (i = 0; i < IEEE80211_NUM_BANDS; i++) {
        if (self->bands[i] != NULL) {
            OsalMemFree(self->bands[i]);
            self->bands[i] = NULL;
        }
    }
    if (self->supportedRates != NULL) {
        OsalMemFree(self->supportedRates);
        self->supportedRates = NULL;
    }
    OsalMemFree(self);
}

int32_t Bdh6Ghcap(struct NetDevice *hnetDev, struct WlanHwCapability **capability)
{
    uint8_t loop = 0;
    struct wiphy* wiphy = NULL;
    struct NetDevice *netDev = NULL;
    struct ieee80211_supported_band *band = NULL;
    struct ieee80211_supported_band *band5g = NULL;
    struct WlanHwCapability *hwCapability = NULL;
    uint16_t supportedRateCount = 0;
    netDev = get_real_netdev(hnetDev);

    wiphy = get_linux_wiphy_hdfdev(netDev);
    if (!wiphy) {
        HDF_LOGE("%s: wiphy is NULL", __func__);
        return -1;
    }

    HDF_LOGE("%s: start...", __func__);
    band = wiphy->bands[IEEE80211_BAND_2GHZ];
    hwCapability = (struct WlanHwCapability *)OsalMemCalloc(sizeof(struct WlanHwCapability));
    if (hwCapability == NULL) {
        HDF_LOGE("%s: oom!\n", __func__);
        return HDF_FAILURE;
    }
    hwCapability->Release = BDH6WalReleaseHwCapability;
    
    if (hwCapability->bands[IEEE80211_BAND_2GHZ] == NULL) {
        hwCapability->bands[IEEE80211_BAND_2GHZ] =
            OsalMemCalloc(sizeof(struct WlanBand) + (sizeof(struct WlanChannel) * band->n_channels));
        if (hwCapability->bands[IEEE80211_BAND_2GHZ] == NULL) {
            BDH6WalReleaseHwCapability(hwCapability);
            return HDF_FAILURE;
        }
    }
    
    hwCapability->htCapability = band->ht_cap.cap;
    supportedRateCount = band->n_bitrates;
    
    hwCapability->bands[IEEE80211_BAND_2GHZ]->channelCount = band->n_channels;
    for (loop = 0; loop < band->n_channels; loop++) {
        hwCapability->bands[IEEE80211_BAND_2GHZ]->channels[loop].centerFreq = band->channels[loop].center_freq;
        hwCapability->bands[IEEE80211_BAND_2GHZ]->channels[loop].flags = band->channels[loop].flags;
        hwCapability->bands[IEEE80211_BAND_2GHZ]->channels[loop].channelId = band->channels[loop].hw_value;
        HDF_LOGE("bdh6 2G band %u: centerFreq=%u, channelId=%u, flags=0x%08x\n", loop,
            hwCapability->bands[IEEE80211_BAND_2GHZ]->channels[loop].centerFreq,
            hwCapability->bands[IEEE80211_BAND_2GHZ]->channels[loop].channelId,
            hwCapability->bands[IEEE80211_BAND_2GHZ]->channels[loop].flags);
    }

    if (wiphy->bands[IEEE80211_BAND_5GHZ]) { // Fill 5Ghz band
        band5g = wiphy->bands[IEEE80211_BAND_5GHZ];
        hwCapability->bands[IEEE80211_BAND_5GHZ] = OsalMemCalloc(sizeof(struct WlanBand) + (sizeof(struct WlanChannel) * band5g->n_channels));
        if (hwCapability->bands[IEEE80211_BAND_5GHZ] == NULL) {
            HDF_LOGE("%s: oom!\n", __func__);
            BDH6WalReleaseHwCapability(hwCapability);
            return HDF_FAILURE;
        }

        hwCapability->bands[IEEE80211_BAND_5GHZ]->channelCount = band5g->n_channels;
        for (loop = 0; loop < band5g->n_channels; loop++) {
            hwCapability->bands[IEEE80211_BAND_5GHZ]->channels[loop].centerFreq = band5g->channels[loop].center_freq;
            hwCapability->bands[IEEE80211_BAND_5GHZ]->channels[loop].flags = band5g->channels[loop].flags;
            hwCapability->bands[IEEE80211_BAND_5GHZ]->channels[loop].channelId = band5g->channels[loop].hw_value;
        }

        supportedRateCount += band5g->n_bitrates;
    }
    HDF_LOGE("bdh6 htCapability= %u,%u; supportedRateCount= %u,%u,%u\n", hwCapability->htCapability,
        band5g->ht_cap.cap, supportedRateCount, band->n_bitrates, band5g->n_bitrates);
    
    hwCapability->supportedRateCount = supportedRateCount;
    hwCapability->supportedRates = OsalMemCalloc(sizeof(uint16_t) * supportedRateCount);
    if (hwCapability->supportedRates == NULL) {
        HDF_LOGE("%s: oom!\n", __func__);
        BDH6WalReleaseHwCapability(hwCapability);
        return HDF_FAILURE;
    }
    
    for (loop = 0; loop < band->n_bitrates; loop++) {
        hwCapability->supportedRates[loop] = band->bitrates[loop].bitrate;
        HDF_LOGE("bdh6 2G supportedRates %u: %u\n", loop, hwCapability->supportedRates[loop]);
    }

    if (band5g) {
        for (loop = band->n_bitrates; loop < supportedRateCount; loop++) {
            hwCapability->supportedRates[loop] = band5g->bitrates[loop].bitrate;
            HDF_LOGE("bdh6 5G supportedRates %u: %u\n", loop, hwCapability->supportedRates[loop]);
        }
    }

    if (hwCapability->supportedRateCount > MAX_SUPPORTED_RATE)
        hwCapability->supportedRateCount = MAX_SUPPORTED_RATE;
    
    *capability = hwCapability;
    return HDF_SUCCESS;
}

int32_t Bdh6SAction(struct NetDevice *hhnetDev, WifiActionData *actionData)
{
    int retVal = 0;
    struct NetDevice *hnetdev = NULL;
    struct net_device *netdev = NULL;
    struct NetDevice *netDev = NULL;
    struct wiphy *wiphy = NULL;
    struct wireless_dev *wdev = NULL;
    static u64 action_cookie = 0;
    struct cfg80211_mgmt_tx_params params;
    u32 center_freq = 0;
    u8 *action_buf = NULL;
    struct ieee80211_mgmt *mgmt = NULL;
    u8 *srcMac = NULL;
    hnetdev = hhnetDev; // backup it

    g_mgmt_tx_event_ifidx = get_scan_ifidx(hnetdev->name);
    HDF_LOGE("%s: start %s... ifidx=%d", __func__, hnetdev->name, g_mgmt_tx_event_ifidx);
    
    netDev = get_real_netdev(hhnetDev);
    netdev = GetLinuxInfByNetDevice(netDev);
    if (!netdev) {
        HDF_LOGE("%s: net_device is NULL", __func__);
        return -1;
    }
    wiphy = get_linux_wiphy_ndev(netdev);
    if (!wiphy) {
        HDF_LOGE("%s: wiphy is NULL", __func__);
        return -1;
    }

    if (strcmp(hnetdev->name, "p2p0") == 0) {
        wdev = g_hdf_infmap[HDF_INF_P2P0].wdev;
        if (g_hdf_infmap[HDF_INF_P2P1].netdev)
            srcMac = wdev->address;
        else
            srcMac = actionData->src;
    } else {
        wdev = netdev->ieee80211_ptr;
        srcMac = actionData->src;
    }
    memset_s(&params, sizeof(params), 0, sizeof(params));
    params.wait = 0;
    center_freq = p2p_remain_freq;
    params.chan = ieee80211_get_channel_khz(wiphy, MHZ_TO_KHZ(center_freq));
    if (params.chan == NULL) {
        HDF_LOGE("%s: get center_freq %u faild", __func__, center_freq);
        return -1;
    }

    // build 802.11 action header
    action_buf = (u8 *)OsalMemCalloc(MAC_80211_FRAME_LEN+actionData->dataLen);
    mgmt = (struct ieee80211_mgmt *)action_buf;
    mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_ACTION);
    memcpy_s(mgmt->da, ETH_ALEN, actionData->dst, ETH_ALEN);
    memcpy_s(mgmt->sa, ETH_ALEN, srcMac, ETH_ALEN);
    memcpy_s(mgmt->bssid, ETH_ALEN, actionData->bssid, ETH_ALEN);

    /* 填充payload信息 */
    if (actionData->dataLen > 0) {
        memcpy_s(action_buf+MAC_80211_FRAME_LEN, actionData->dataLen, actionData->data, actionData->dataLen);
    }
    params.buf = action_buf;
    params.len = (MAC_80211_FRAME_LEN + actionData->dataLen);
    retVal = (int32_t)wl_cfg80211_ops.mgmt_tx(wiphy, wdev, &params, &action_cookie);
    OsalMemFree(action_buf);
    return retVal;
}

int32_t BDH6WalGetIftype(struct NetDevice *hnetDev, uint8_t *iftype)
{
    struct NetDevice *netDev = NULL;
    netDev = get_real_netdev(hnetDev);
    iftype = (uint8_t *)(&(GET_NET_DEV_CFG80211_WIRELESS(netDev)->iftype));
    HDF_LOGE("%s: start...", __func__);
    return HDF_SUCCESS;
}

static struct HdfMac80211BaseOps g_bdh6_baseOps = {
    .SetMode = BDH6WalSetMode,
    .AddKey = BDH6WalAddKey,
    .DelKey = BDH6WalDelKey,
    .SetDefaultKey = BDH6WalSetDefaultKey,
    
    .GetDeviceMacAddr = BDH6WalGetDeviceMacAddr,
    .SetMacAddr = BDH6WalSetMacAddr,
    .SetTxPower = BDH6WalSetTxPower,
    .GetValidFreqsWithBand = Bdh6Fband,
    
    .GetHwCapability = Bdh6Ghcap,
    .SendAction = Bdh6SAction,
    .GetIftype = BDH6WalGetIftype,
    
};

void BDH6Mac80211Init(struct HdfChipDriver *chipDriver)
{
    HDF_LOGE("%s: start...", __func__);

    if (chipDriver == NULL) {
        HDF_LOGE("%s: input is NULL", __func__);
        return;
    }
    
    chipDriver->ops = &g_bdh6_baseOps;
    chipDriver->staOps = &g_bdh6_staOps;
    chipDriver->apOps = &g_bdh6_apOps;
    chipDriver->p2pOps = &g_bdh6_p2pOps;
}

