/*
 * hdf_mac80211_sta_event.c
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
#include <net/netlink.h>
#include <net/cfg80211.h>
#include <securec.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/in6.h>
#include <linux/wireless.h>

#include "osal_mem.h"
#include "net_device.h"
#include "net_device_impl.h"
#include "net_device_adapter.h"
#include "wifi_mac80211_ops.h"
#include "hdf_wifi_cmd.h"
#include "hdf_wifi_event.h"
#include "hdf_wl_interface.h"
#include "hdf_public_ap6275s.h"
#include "hdf_mac80211_sta_event.h"

#define HDF_LOG_TAG BDH6Driver
#define WIFI_SCAN_EXTRA_IE_LEN_MAX      (512)

int32_t HdfScanEventCallback(struct net_device *ndev, HdfWifiScanStatus _status)
{
    int32_t ret = 0;

    NetDevice *netDev = GetHdfNetDeviceByLinuxInf(ndev);
    WifiScanStatus status = _status;
    netDev = get_hdf_netdev(g_scan_event_ifidx);
    HDF_LOGE("%s: %d, scandone!", __func__, _status);
    ret = HdfWifiEventScanDone(netDev, status);

    return ret;
}

#define HDF_ETHER_ADDR_LEN (6)
int32_t HdfConnectResultEventCallback(struct net_device *ndev, uint8_t *bssid, uint8_t *reqIe,
    uint8_t *rspIe, uint32_t reqIeLen, uint32_t rspIeLen, uint16_t connectStatus, uint16_t freq)
{
    int32_t retVal = 0;
    NetDevice *netDev = GetHdfNetDeviceByLinuxInf(ndev);
    struct ConnetResult connResult;
    // for check p2p0 report
    netDev = get_hdf_netdev(g_conn_event_ifidx);

    HDF_LOGE("%s: enter", __func__);

    if (netDev == NULL || bssid == NULL || rspIe == NULL || reqIe == NULL) {
        HDF_LOGE("%s: netDev / bssid / rspIe / reqIe  null!", __func__);
        return -1;
    }

    memcpy_s(&connResult.bssid[0], HDF_ETHER_ADDR_LEN, bssid, HDF_ETHER_ADDR_LEN);

    connResult.rspIe = rspIe;
    connResult.rspIeLen = rspIeLen;
    connResult.reqIe = reqIe;
    connResult.reqIeLen = reqIeLen;
    connResult.connectStatus = connectStatus;
    connResult.freq = freq;
    connResult.statusCode = connectStatus;

    retVal = HdfWifiEventConnectResult(netDev, &connResult);
    if (retVal < 0) {
        HDF_LOGE("%s: hdf wifi event inform connect result failed!", __func__);
    }
    return retVal;
}

int32_t HdfDisconnectedEventCallback(struct net_device *ndev, uint16_t reason, uint8_t *ie, uint32_t len)
{
    int32_t ret = 0;

    NetDevice *netDev = GetHdfNetDeviceByLinuxInf(ndev);
    netDev = get_hdf_netdev(g_conn_event_ifidx);
    HDF_LOGE("%s: leave", __func__);

    ret = HdfWifiEventDisconnected(netDev, reason, ie, len);
    return ret;
}

void HdfInformBssFrameEventCallback(struct net_device *ndev, struct ieee80211_channel *channel, int32_t signal,
    int16_t freq, struct ieee80211_mgmt *mgmt, uint32_t mgmtLen)
{
    int32_t retVal = 0;
    NetDevice *netDev = GetHdfNetDeviceByLinuxInf(ndev);
    struct ScannedBssInfo bssInfo;
    struct WlanChannel hdfchannel;

    if (channel == NULL || netDev == NULL || mgmt == NULL) {
        HDF_LOGE("%s: inform_bss_frame channel = null or netDev = null!", __func__);
        return;
    }
    netDev = get_hdf_netdev(g_scan_event_ifidx);
    bssInfo.signal = signal;
    bssInfo.freq = freq;
    bssInfo.mgmtLen = mgmtLen;
    bssInfo.mgmt = (struct Ieee80211Mgmt *)mgmt;

    hdfchannel.flags = channel->flags;
    hdfchannel.channelId = channel->hw_value;
    hdfchannel.centerFreq = channel->center_freq;
    retVal = HdfWifiEventInformBssFrame(netDev, &hdfchannel, &bssInfo);
    if (retVal < 0) {
        HDF_LOGE("%s: hdf wifi event inform bss frame failed!", __func__);
    } else {
        HDF_LOGE("%s: hdf wifi event inform bss frame ret = %d", __func__, retVal);
    }
}

