#include "BleDevicesModel.h"
#include "dataproc.h"
#include "dataproc_def.h"
#include "Account.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "BleDevicesModel";

using namespace Page;

void BleDevicesModel::Init()
{
    ESP_LOGI(TAG, "Initializing BleDevicesModel...");
    
    // 初始化成员变量
    account = nullptr;
    scanning = false;
    
    DataCenter* center = DataProc_GetDataCenter();
    if (center == nullptr) {
        ESP_LOGE(TAG, "DataCenter is nullptr, cannot create Account");
        return;
    }
    
    account = new Account("BleDevices", center);
    if (account == nullptr) {
        ESP_LOGE(TAG, "Failed to create Account");
        return;
    }
    
    // 订阅 StatusBar 以接收状态栏更新
    account->Subscribe("StatusBar");
    
    ESP_LOGI(TAG, "BleDevicesModel initialized successfully");
}

void BleDevicesModel::Deinit()
{
    if (account)
    {
        delete account;
        account = nullptr;
    }
    
    // 停止扫描
    if (scanning) {
        StopScan();
    }
}

esp_err_t BleDevicesModel::StartScan(uint32_t duration_sec)
{
    ESP_LOGI(TAG, "Starting BLE scan (duration: %u sec)", (unsigned int)duration_sec);
    esp_err_t ret = bsp_ble_start_scan(duration_sec);
    if (ret == ESP_OK) {
        scanning = true;
    }
    return ret;
}

esp_err_t BleDevicesModel::StopScan()
{
    ESP_LOGI(TAG, "Stopping BLE scan");
    esp_err_t ret = bsp_ble_stop_scan();
    if (ret == ESP_OK) {
        scanning = false;
    }
    return ret;
}

bool BleDevicesModel::IsScanning()
{
    return scanning;
}

int BleDevicesModel::GetDeviceCount()
{
    uint8_t count = 0;
    bsp_ble_device_info_t devices[MAX_DEVICES];
    esp_err_t ret = bsp_ble_get_scanned_devices(devices, &count, MAX_DEVICES);
    if (ret == ESP_OK) {
        return (int)count;
    }
    return 0;
}

esp_err_t BleDevicesModel::GetDevices(bsp_ble_device_info_t *devices, int *count, int max_count)
{
    if (devices == nullptr || count == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t device_count = 0;
    esp_err_t ret = bsp_ble_get_scanned_devices(devices, &device_count, max_count);
    if (ret == ESP_OK) {
        *count = (int)device_count;
    } else {
        *count = 0;
    }
    
    return ret;
}

esp_err_t BleDevicesModel::ConnectDevice(const uint8_t *addr, uint8_t addr_type)
{
    ESP_LOGI(TAG, "Connecting to device: %02X:%02X:%02X:%02X:%02X:%02X",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    return bsp_ble_connect(addr, addr_type);
}

esp_err_t BleDevicesModel::DisconnectDevice(uint16_t conn_handle)
{
    ESP_LOGI(TAG, "Disconnecting device (handle: %d)", conn_handle);
    return bsp_ble_disconnect(conn_handle);
}

bool BleDevicesModel::IsDeviceConnected(bsp_ble_device_type_t type)
{
    return bsp_ble_is_device_connected(type);
}

void BleDevicesModel::SetStatusBarStyle(int style)
{
    if (account == nullptr) {
        return;
    }
    
    StatusBar_Info_t info;
    memset(&info, 0, sizeof(info));
    info.cmd = STATUS_BAR_CMD_SET_STYLE;
    info.param.style = (StatusBar_Style_t)style;
    
    account->Notify("StatusBar", &info, sizeof(info));
}

void BleDevicesModel::SetStatusBarAppear(bool en)
{
    if (account == nullptr) {
        return;
    }
    
    StatusBar_Info_t info;
    memset(&info, 0, sizeof(info));
    info.cmd = STATUS_BAR_CMD_APPEAR;
    info.param.appear = en;
    
    account->Notify("StatusBar", &info, sizeof(info));
}

int BleDevicesModel::GetConnectedDeviceCount()
{
    int count = 0;
    if (bsp_ble_is_device_connected(BSP_BLE_DEVICE_TYPE_HR)) {
        count++;
    }
    if (bsp_ble_is_device_connected(BSP_BLE_DEVICE_TYPE_CSCS)) {
        count++;
    }
    return count;
}
