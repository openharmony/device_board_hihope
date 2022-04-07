/*
 * hdfinit_bdh.c
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
#include <uapi/linux/nl80211.h>
#include <securec.h>
#include <asm/byteorder.h>
#include <linux/kernel.h>

#include "hdf_wifi_product.h"
#include "wifi_mac80211_ops.h"
#include "hdf_wlan_utils.h"
#include "hdf_wl_interface.h"
#include "net_bdh_adpater.h"
#include "hdf_public_ap6275s.h"

#define HDF_LOG_TAG BDH6Driver
int hdf_cfgp2p_register_ndev(struct net_device *p2p_netdev, struct net_device *primary_netdev, struct wiphy *wiphy);
int dhd_module_init(void);
struct NetDeviceInterFace *wal_get_net_p2p_ops(void);
s32 wl_get_vif_macaddr(struct bcm_cfg80211 *cfg, u16 wl_iftype, u8 *mac_addr);
struct hdf_inf_map g_hdf_infmap[HDF_INF_MAX];

int g_hdf_ifidx = HDF_INF_WLAN0;
int g_event_ifidx = HDF_INF_WLAN0;
int g_scan_event_ifidx = HDF_INF_WLAN0;
int g_conn_event_ifidx = HDF_INF_WLAN0;
int g_mgmt_tx_event_ifidx = HDF_INF_P2P0;


// BDH Wifi6 chip driver init
int32_t InitBDH6Chip(struct HdfWlanDevice *device)
{
    (void)device;
    HDF_LOGW("bdh6: call InitBDH6Chip");
    return HDF_SUCCESS;
}

int32_t DeinitBDH6Chip(struct HdfWlanDevice *device)
{
    int32_t ret = HDF_SUCCESS;
    (void)device;
    if (ret != 0) {
        HDF_LOGE("%s:Deinit failed!ret=%d", __func__, ret);
    }
    return ret;
}

int32_t BDH6Init(struct HdfChipDriver *chipDriver, struct NetDevice *netDevice)
{
    int32_t ret = 0;
    struct HdfWifiNetDeviceData *data = NULL;
    struct net_device *netdev = NULL;
    int private_data_size = 0;
    struct wiphy *wiphy = NULL;
    struct wireless_dev *wdev = NULL;
    struct net_device *p2p_netdev = NULL;
    struct NetDevice *p2p_hnetdev = NULL;
    struct bcm_cfg80211 *cfg = NULL;
    
    (void)chipDriver;
    HDF_LOGW("bdh6: call BDH6Init");

    memset_s(g_hdf_infmap, sizeof(g_hdf_infmap), 0, sizeof(g_hdf_infmap));
    g_hdf_ifidx = HDF_INF_WLAN0; // master interface

    if (netDevice == NULL) {
        HDF_LOGE("%s netdevice is null!", __func__);
        return HDF_FAILURE;
    }

    netdev = GetLinuxInfByNetDevice(netDevice);
    if (netdev == NULL) {
        HDF_LOGE("%s net_device is null!", __func__);
        return HDF_FAILURE;
    }

    data = GetPlatformData(netDevice);
    if (data == NULL) {
        HDF_LOGE("%s:netdevice data null!", __func__);
        return HDF_FAILURE;
    }

    hdf_bdh6_netdev_init(netDevice);
    netDevice->classDriverPriv = data;
    private_data_size = get_dhd_priv_data_size(); // create bdh6 private object
    netDevice->mlPriv = kzalloc(private_data_size, GFP_KERNEL);
    if (netDevice->mlPriv == NULL) {
        HDF_LOGE("%s:kzalloc mlPriv failed", __func__);
        return HDF_FAILURE;
    }

    set_krn_netdev(netDevice, netdev, g_hdf_ifidx);
    dhd_module_init();
    ret = hdf_bdh6_netdev_open(netDevice);
    if (ret != 0) {
        HDF_LOGE("%s:open netdev %s failed", __func__, netDevice->name);
        return HDF_FAILURE;
    }
    ret = BDH6InitNetdev(netDevice, sizeof(void *), NL80211_IFTYPE_P2P_DEVICE, HDF_INF_P2P0);
    if (ret != 0) {
        HDF_LOGE("%s:BDH6InitNetdev p2p0 failed", __func__);
        return HDF_FAILURE;
    }
    wdev = kzalloc(sizeof(*wdev), GFP_KERNEL);
    if (wdev == NULL) {
        HDF_LOGE("%s:get wlan0 wdev failed", __func__);
        return HDF_FAILURE;
    }
    wiphy = get_linux_wiphy_ndev(netdev);
    if (wiphy == NULL) {
        HDF_LOGE("%s:get wlan0 wiphy failed", __func__);
        return HDF_FAILURE;
    }
    p2p_hnetdev = get_hdf_netdev(g_hdf_ifidx);
    p2p_netdev = get_krn_netdev(g_hdf_ifidx);
    wdev->wiphy = wiphy;
    wdev->iftype = NL80211_IFTYPE_P2P_DEVICE;
    p2p_netdev->ieee80211_ptr = wdev;
    wdev->netdev = p2p_netdev;
    p2p_hnetdev->ieee80211Ptr = p2p_netdev->ieee80211_ptr;
    cfg = wiphy_priv(wiphy);  // update mac from wdev address
    wl_get_vif_macaddr(cfg, 7, p2p_hnetdev->macAddr);  // WL_IF_TYPE_P2P_DISC = 7
    memcpy_s(p2p_netdev->dev_addr, p2p_netdev->addr_len, p2p_hnetdev->macAddr, MAC_ADDR_SIZE);
    p2p_hnetdev->netDeviceIf = wal_get_net_p2p_ops();  // reset netdev_ops
    hdf_cfgp2p_register_ndev(p2p_netdev, netdev, wiphy);
    ret = NetDeviceAdd(p2p_hnetdev);  // Call linux register_netdev()
    HDF_LOGE("NetDeviceAdd %s ret = %d", p2p_hnetdev->name, ret);
    return HDF_SUCCESS;
}

int32_t BDH6Deinit(struct HdfChipDriver *chipDriver, struct NetDevice *netDevice)
{
    (void)chipDriver;
    (void)netDevice;
    hdf_bdh6_netdev_stop(netDevice);
    return HDF_SUCCESS;
}

struct NetDevice *get_real_netdev(NetDevice *netDev)
{
    if (strcmp(netDev->name, "p2p0") == 0) {
        return get_hdf_netdev(HDF_INF_WLAN0);
    } else {
        return netDev;
    }
}
