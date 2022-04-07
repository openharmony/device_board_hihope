/*
 * hdf_mac80211_ap.c
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
#include <typedefs.h>
#include <ethernet.h>
#include <bcmutils.h>
#include <securec.h>

#include "wifi_mac80211_ops.h"
#include "hdf_wlan_utils.h"
#include "wifi_module.h"

#include "osal_mem.h"
#include "hdf_wifi_event.h"
#include "hdf_log.h"

#include "net_device_adapter.h"
#include "hdf_public_ap6275s.h"

#define HDF_LOG_TAG bcmdhd

struct net_device *g_save_kernel_net = NULL;
/*--------------------------------------------------------------------------------------------------*/
/* public */
/*--------------------------------------------------------------------------------------------------*/
#define MAX_NUM_OF_ASSOCIATED_DEV		64
#define WLC_GET_ASSOCLIST		159
#define ETH_MAC_LEN 6


typedef struct maclist {
    uint32 count;			/**< number of MAC addresses */
    struct ether_addr ea[1];	/**< variable length array of MAC addresses */
} maclist_t;


int ChangDelSta(struct net_device *dev, const uint8_t *macAddr, uint8_t addrLen)
{
    int ret;
    struct NetDevice *netDev = GetHdfNetDeviceByLinuxInf(dev);
    ret = HdfWifiEventDelSta(netDev, macAddr, ETH_MAC_LEN);
    return ret;
}

int ChangNewSta(struct net_device *dev, const uint8_t *macAddr, uint8_t addrLen,
    const struct station_info *info)
{
    int ret;
    struct NetDevice *netDev = NULL;
    struct StationInfo Info = {0};
 
    Info.assocReqIes = info->assoc_req_ies;
    Info.assocReqIesLen = info->assoc_req_ies_len;
    netDev = GetHdfNetDeviceByLinuxInf(dev);
    ret = HdfWifiEventNewSta(netDev, macAddr, addrLen, &Info);
    return ret;
}

int32_t wl_get_all_sta(struct net_device *ndev, uint32_t *num)
{
    int ret = 0;
    char mac_buf[MAX_NUM_OF_ASSOCIATED_DEV *
    sizeof(struct ether_addr) + sizeof(uint)] = {0};
    struct maclist *assoc_maclist = (struct maclist *)mac_buf;
    assoc_maclist->count = MAX_NUM_OF_ASSOCIATED_DEV;
    ret = wldev_ioctl_get(ndev, WLC_GET_ASSOCLIST,
        assoc_maclist, sizeof(mac_buf));
    *num = assoc_maclist->count;
    return 0;
}
#define MAX_NUM_OF_ASSOCLIST    64
#define CMD_ASSOC_CLIENTS "ASSOCLIST"
int wl_get_all_sta_info(struct net_device *ndev, char* mac, uint32_t num)
{
    int bytes_written = 0;
    int  error = 0;
    int len = 0;
    int i;
    char mac_buf[MAX_NUM_OF_ASSOCLIST *sizeof(struct ether_addr) + sizeof(uint)] = {0};
    struct maclist *assoc_maclist = (struct maclist *)mac_buf;
    assoc_maclist->count = (MAX_NUM_OF_ASSOCLIST);
    error = wldev_ioctl_get(ndev, WLC_GET_ASSOCLIST, assoc_maclist, sizeof(mac_buf));
    if (error) {
        return -1;
    }
    assoc_maclist->count = assoc_maclist->count;
        bytes_written = snprintf_s(mac, num, num, "%s listcount: %d Stations:",
        CMD_ASSOC_CLIENTS, assoc_maclist->count);
    for (i = 0; i < assoc_maclist->count; i++) {
    len = snprintf(mac + bytes_written, num - bytes_written, " " MACDBG,
                    MAC2STRDBG(assoc_maclist->ea[i].octet));

        if ((len > 0) && (len < (num - bytes_written))) {
            bytes_written += len;
        } else {
            bytes_written = -1;
            break;
        }
    }
    return bytes_written;
}

#define RATE_OFFSET  2
#define CAP_OFFSET  3

static void bdh6_nl80211_check_ap_rate_selectors(struct cfg80211_ap_settings *params,
    const u8 *rates)
{
    int i;
    if (!rates)
        return;

    for (i = 0; i < rates[1]; i++) {
        if (rates[RATE_OFFSET + i] == BSS_MEMBERSHIP_SELECTOR_HT_PHY)
            params->ht_required = true;
        if (rates[RATE_OFFSET + i] == BSS_MEMBERSHIP_SELECTOR_VHT_PHY)
            params->vht_required = true;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
        if (rates[RATE_OFFSET + i] == BSS_MEMBERSHIP_SELECTOR_HE_PHY)
            params->he_required = true;
#endif
	}
}

static void bdh6_nl80211_calculate_ap_params(struct cfg80211_ap_settings *params)
{
    const struct cfg80211_beacon_data *bcn = &params->beacon;
    size_t ies_len = bcn->tail_len;
    const u8 *ies = bcn->tail;
    const u8 *rates;
    const u8 *cap;

    rates = cfg80211_find_ie(WLAN_EID_SUPP_RATES, ies, ies_len);
    HDF_LOGE("lijg: find beacon tail WLAN_EID_SUPP_RATES=%p", rates);
    bdh6_nl80211_check_ap_rate_selectors(params, rates);

    rates = cfg80211_find_ie(WLAN_EID_EXT_SUPP_RATES, ies, ies_len);
    HDF_LOGE("lijg: find beacon tail WLAN_EID_EXT_SUPP_RATES=%p", rates);
    bdh6_nl80211_check_ap_rate_selectors(params, rates);

    cap = cfg80211_find_ie(WLAN_EID_HT_CAPABILITY, ies, ies_len);
    HDF_LOGE("lijg: find beacon tail WLAN_EID_HT_CAPABILITY=%p", cap);
    if (cap && cap[1] >= sizeof(*params->ht_cap))
        params->ht_cap = (void *)(cap + RATE_OFFSET);
    cap = cfg80211_find_ie(WLAN_EID_VHT_CAPABILITY, ies, ies_len);
    HDF_LOGE("lijg: find beacon tail WLAN_EID_VHT_CAPABILITY=%p", cap);
    if (cap && cap[1] >= sizeof(*params->vht_cap))
        params->vht_cap = (void *)(cap + RATE_OFFSET);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
    cap = cfg80211_find_ext_ie(WLAN_EID_EXT_HE_CAPABILITY, ies, ies_len);
    HDF_LOGE("lijg: find beacon tail WLAN_EID_EXT_HE_CAPABILITY=%p", cap);
    if (cap && cap[1] >= sizeof(*params->he_cap) + 1)
        params->he_cap = (void *)(cap + CAP_OFFSET);
    cap = cfg80211_find_ext_ie(WLAN_EID_EXT_HE_OPERATION, ies, ies_len);
    HDF_LOGE("lijg: find beacon tail WLAN_EID_EXT_HE_OPERATION=%p", cap);
    if (cap && cap[1] >= sizeof(*params->he_oper) + 1)
        params->he_oper = (void *)(cap + CAP_OFFSET);
#endif
}


static struct ieee80211_channel g_ap_ieee80211_channel;
static struct cfg80211_ap_settings g_ap_setting_info;
static u8 g_ap_ssid[IEEE80211_MAX_SSID_LEN];
static int start_ap_flag = 0;

static int32_t SetupWireLessDev(struct net_device *netDev, struct WlanAPConf *apSettings)
{
    struct wireless_dev *wdev = NULL;
    struct ieee80211_channel *chan = NULL;
    
    wdev = netDev->ieee80211_ptr;
    if (wdev == NULL) {
        HDF_LOGE("%s: wdev is null", __func__);
        return -1;
    }
    HDF_LOGE("%s:%p, chan=%p, channel=%u, centfreq1=%u, centfreq2=%u, band=%u, width=%u", __func__,
        wdev, wdev->preset_chandef.chan,
        apSettings->channel, apSettings->centerFreq1, apSettings->centerFreq2, apSettings->band, apSettings->width);

    wdev->iftype = NL80211_IFTYPE_AP;
    wdev->preset_chandef.width = (enum nl80211_channel_type)apSettings->width;
    wdev->preset_chandef.center_freq1 = apSettings->centerFreq1;
    wdev->preset_chandef.center_freq2 = apSettings->centerFreq2;
    
    chan = ieee80211_get_channel(wdev->wiphy, apSettings->centerFreq1);
    if (chan) {
        wdev->preset_chandef.chan = chan;
        HDF_LOGE("%s: use valid channel %u", __func__, chan->center_freq);
    } else if (wdev->preset_chandef.chan == NULL) {
        wdev->preset_chandef.chan = &g_ap_ieee80211_channel;
        wdev->preset_chandef.chan->hw_value = apSettings->channel;
        wdev->preset_chandef.chan->band = apSettings->band; // IEEE80211_BAND_2GHZ;
        wdev->preset_chandef.chan->center_freq = apSettings->centerFreq1;
        HDF_LOGE("%s: fill new channel %u", __func__, wdev->preset_chandef.chan->center_freq);
    }

    g_ap_setting_info.chandef = wdev->preset_chandef;
    
    return HDF_SUCCESS;
}

/*--------------------------------------------------------------------------------------------------*/
/* HdfMac80211APOps */
/*--------------------------------------------------------------------------------------------------*/
int32_t WalConfigAp(NetDevice *hnetDev, struct WlanAPConf *apConf)
{
    int32_t ret = 0;
    struct net_device *netdev = NULL;
    struct NetDevice *netDev = NULL;
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

    memset_s(&g_ap_setting_info, sizeof(g_ap_setting_info), 0, sizeof(g_ap_setting_info));
    ret = SetupWireLessDev(netdev, apConf);
    if (ret != 0) {
        HDF_LOGE("%s: set up wireless device failed!", __func__);
        return HDF_FAILURE;
    }

    HDF_LOGE("%s: before iftype = %d!", __func__, netdev->ieee80211_ptr->iftype);
    netdev->ieee80211_ptr->iftype = NL80211_IFTYPE_AP;
    HDF_LOGE("%s: after  iftype = %d!", __func__, netdev->ieee80211_ptr->iftype);

    g_ap_setting_info.ssid_len = apConf->ssidConf.ssidLen;
    memcpy_s(&g_ap_ssid[0], IEEE80211_MAX_SSID_LEN, &apConf->ssidConf.ssid[0], apConf->ssidConf.ssidLen);
    g_ap_setting_info.ssid = &g_ap_ssid[0];
    ret = (int32_t)wl_cfg80211_ops.change_virtual_intf(wiphy, netdev,
        (enum nl80211_iftype)netdev->ieee80211_ptr->iftype, NULL);
    if (ret < 0) {
        HDF_LOGE("%s: set mode failed!", __func__);
    }

    return HDF_SUCCESS;
}

int32_t WalSetCountryCode(NetDevice *hnetDev, const char *code, uint32_t len)
{
    int32_t ret = 0;
    struct net_device *netdev = NULL;
    struct NetDevice *netDev = NULL;
    netDev = get_real_netdev(hnetDev);
    netdev = GetLinuxInfByNetDevice(netDev);
    if (!netdev) {
        HDF_LOGE("%s: net_device is NULL", __func__);
        return -1;
    }

    ret = (int32_t)wl_cfg80211_set_country_code(netdev, (char*)code, false, true, len);
    if (ret < 0) {
        HDF_LOGE("%s: set_country_code failed!", __func__);
    }
    return ret;
}

int32_t WalStopAp(NetDevice *hnetDev)
{
    int32_t ret = 0;
    struct net_device *netdev = NULL;
    struct NetDevice *netDev = NULL;
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

    HDF_LOGE("%s: start...", __func__);
    ret = (int32_t)wl_cfg80211_ops.stop_ap(wiphy, netdev);
    return ret;
}

int32_t WalStartAp(NetDevice *hnetDev)
{
    int32_t ret = 0;
    struct net_device *netdev = NULL;
    struct wiphy *wiphy = NULL;
    struct wireless_dev *wdev = NULL;
    struct NetDevice *netDev = NULL;
    netDev = get_real_netdev(hnetDev);

    if (start_ap_flag) {
        WalStopAp(netDev);
        HDF_LOGE("do not start up, because start_ap_flag=%d", start_ap_flag);
    }
    
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
    wdev = netdev->ieee80211_ptr;
    HDF_LOGE("%s: start...", __func__);
    ret = (int32_t)wl_cfg80211_ops.start_ap(wiphy, netdev, &g_ap_setting_info);
    if (ret < 0) {
        HDF_LOGE("%s: start_ap failed!", __func__);
    } else {
        wdev->preset_chandef = g_ap_setting_info.chandef;
        wdev->beacon_interval = g_ap_setting_info.beacon_interval;
        wdev->chandef = g_ap_setting_info.chandef;
        wdev->ssid_len = g_ap_setting_info.ssid_len;
        memcpy_s(wdev->ssid, wdev->ssid_len, g_ap_setting_info.ssid, wdev->ssid_len);
        start_ap_flag = 1;
    }
    return ret;
}

int32_t WalChangeBeacon(NetDevice *hnetDev, struct WlanBeaconConf *param)
{
    int32_t ret = 0;
    struct cfg80211_beacon_data info;
    struct net_device *netdev = NULL;
    struct wiphy *wiphy = NULL;
    struct NetDevice *netDev = NULL;
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

    HDF_LOGE("%s: start...", __func__);

    memset(&info, 0x00, sizeof(info));

#if 1
    if (g_ap_setting_info.beacon.head != NULL) {
        OsalMemFree((uint8_t *)g_ap_setting_info.beacon.head);
        g_ap_setting_info.beacon.head = NULL;
    }
    if (g_ap_setting_info.beacon.tail != NULL) {
        OsalMemFree((uint8_t *)g_ap_setting_info.beacon.tail);
        g_ap_setting_info.beacon.tail = NULL;
    }

    if (param->headIEs && param->headIEsLength > 0) {
        g_ap_setting_info.beacon.head = OsalMemCalloc(param->headIEsLength);
        memcpy((uint8_t *)g_ap_setting_info.beacon.head, param->headIEs, param->headIEsLength);
        g_ap_setting_info.beacon.head_len = param->headIEsLength;

        //info.head = g_ap_setting_info.beacon.head;
        //info.head_len = g_ap_setting_info.beacon.head_len;
    }

    if (param->tailIEs && param->tailIEsLength > 0) {
        g_ap_setting_info.beacon.tail = OsalMemCalloc(param->tailIEsLength);
        memcpy((uint8_t *)g_ap_setting_info.beacon.tail, param->tailIEs, param->tailIEsLength);
        g_ap_setting_info.beacon.tail_len = param->tailIEsLength;

        //info.tail = g_ap_setting_info.beacon.tail;
        //info.tail_len = g_ap_setting_info.beacon.tail_len;
    }
#endif    
    
    info.head = param->headIEs;
    info.head_len = (size_t)param->headIEsLength;
    info.tail = param->tailIEs;
    info.tail_len = (size_t)param->tailIEsLength;

    info.beacon_ies = NULL;
    info.proberesp_ies = NULL;
    info.assocresp_ies = NULL;
    info.probe_resp = NULL;
    //info.lci = NULL;
    //info.civicloc = NULL;
    //info.ftm_responder = 0X00;
    info.beacon_ies_len = 0X00;
    info.proberesp_ies_len = 0X00;
    info.assocresp_ies_len = 0X00;
    info.probe_resp_len = 0X00;
    //info.lci_len = 0X00;
    //info.civicloc_len = 0X00;

    /* add beacon data for start ap*/
    g_ap_setting_info.dtim_period = param->DTIMPeriod;
    g_ap_setting_info.hidden_ssid = param->hiddenSSID;
    g_ap_setting_info.beacon_interval = param->interval;
    HDF_LOGE("%s: dtim_period:%d---hidden_ssid:%d---beacon_interval:%d!",
        __func__, g_ap_setting_info.dtim_period, g_ap_setting_info.hidden_ssid, g_ap_setting_info.beacon_interval);
	
    //g_ap_setting_info.beacon.head = param->headIEs;
    //g_ap_setting_info.beacon.head_len = param->headIEsLength;
    //g_ap_setting_info.beacon.tail = param->tailIEs;
    //g_ap_setting_info.beacon.tail_len = param->tailIEsLength;

    g_ap_setting_info.beacon.beacon_ies = NULL;
    g_ap_setting_info.beacon.proberesp_ies = NULL;
    g_ap_setting_info.beacon.assocresp_ies = NULL;
    g_ap_setting_info.beacon.probe_resp = NULL;
    //g_ap_setting_info.beacon.lci = NULL;
    // g_ap_setting_info.beacon.civicloc = NULL;
    // g_ap_setting_info.beacon.ftm_responder = 0X00;
    g_ap_setting_info.beacon.beacon_ies_len = 0X00;
    g_ap_setting_info.beacon.proberesp_ies_len = 0X00;
    g_ap_setting_info.beacon.assocresp_ies_len = 0X00;
    g_ap_setting_info.beacon.probe_resp_len = 0X00;
    // g_ap_setting_info.beacon.lci_len = 0X00;
    // g_ap_setting_info.beacon.civicloc_len = 0X00;

    // call nl80211_calculate_ap_params(&params);
    bdh6_nl80211_calculate_ap_params(&g_ap_setting_info);

    HDF_LOGE("%s: headIEsLen:%d---tailIEsLen:%d!", __func__, param->headIEsLength, param->tailIEsLength);
    ret = WalStartAp(netDev);
    HDF_LOGE("call start_ap ret=%d", ret);

    ret = (int32_t)wl_cfg80211_ops.change_beacon(wiphy, netdev, &info);
	
    if (ret < 0) {
        HDF_LOGE("%s: change_beacon failed!", __func__);
    }
	
    return HDF_SUCCESS;
}

int32_t WalDelStation(NetDevice *hnetDev, const uint8_t *macAddr)
{
    int32_t ret = 0;
    struct net_device *netdev = NULL;
    struct wiphy *wiphy = NULL;
    struct NetDevice *netDev = NULL;
    struct station_del_parameters del_param = {macAddr, 10, 0};
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

    (void)macAddr;
    HDF_LOGE("%s: start...", __func__);

    ret = (int32_t)wl_cfg80211_ops.del_station(wiphy, netdev, &del_param);
    if (ret < 0) {
        HDF_LOGE("%s: del_station failed!", __func__);
    }
    return ret;
}

int32_t WalGetAssociatedStasCount(NetDevice *hnetDev, uint32_t *num)
{
    int32_t ret = 0;
    struct net_device *netdev = NULL;
    struct NetDevice *netDev = NULL;
    netDev = get_real_netdev(hnetDev);
    netdev = GetLinuxInfByNetDevice(netDev);
    if (!netdev) {
        HDF_LOGE("%s: net_device is NULL", __func__);
        return -1;
    }

    ret = (int32_t)wl_get_all_sta(netdev, num);
    if (ret < 0) {
        HDF_LOGE("%s: wl_get_all_sta failed!", __func__);
    }
    return ret;
}

int32_t WalGetAssociatedStasInfo(NetDevice *hnetDev, WifiStaInfo *staInfo, uint32_t num)
{
    int32_t ret = 0;
    struct net_device *netdev = NULL;
    struct NetDevice *netDev = NULL;
    netDev = get_real_netdev(hnetDev);
    netdev = GetLinuxInfByNetDevice(netDev);
    if (!netdev) {
        HDF_LOGE("%s: net_device is NULL", __func__);
        return -1;
    }
    HDF_LOGE("%s: start...", __func__);
    ret = (int32_t)wl_get_all_sta_info(netdev, staInfo->mac, num);
    if (ret < 0) {
        HDF_LOGE("%s: wl_get_all_sta_info failed!", __func__);
    }
    return ret;
}

struct HdfMac80211APOps g_bdh6_apOps = {
    .ConfigAp = WalConfigAp,
    .StartAp = WalStartAp,
    .StopAp = WalStopAp,
    .ConfigBeacon = WalChangeBeacon,
    .DelStation = WalDelStation,
    .SetCountryCode = WalSetCountryCode,
    .GetAssociatedStasCount = WalGetAssociatedStasCount,
    .GetAssociatedStasInfo = WalGetAssociatedStasInfo
};
