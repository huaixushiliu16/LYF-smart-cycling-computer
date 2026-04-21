#include "HeartRateModel.h"
#include "Account.h"
#include "dataproc.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "HeartRateModel";

using namespace Page;

void HeartRateModel::Init()
{
    account = nullptr;
    scanning = false;
    weight_kg = 70;

    DataCenter* center = DataProc_GetDataCenter();
    if (center == nullptr) {
        ESP_LOGE(TAG, "DataCenter is nullptr, cannot create Account");
        return;
    }

    account = new Account("HeartRate", center);
    if (account == nullptr) {
        ESP_LOGE(TAG, "Failed to create Account");
        return;
    }
    account->Subscribe("StatusBar");
    account->Subscribe("SysConfig");

    SysConfig_Info_t cur;
    if (account->Pull("SysConfig", &cur, sizeof(cur)) == Account::RES_OK) {
        weight_kg = (int)(cur.weight_kg + 0.5f);
    }
}

void HeartRateModel::Deinit()
{
    if (account) {
        delete account;
        account = nullptr;
    }
    if (scanning) {
        StopScan();
    }
}

esp_err_t HeartRateModel::StartScan(uint32_t duration_sec)
{
    esp_err_t ret = bsp_ble_start_scan(duration_sec);
    if (ret == ESP_OK) {
        scanning = true;
    }
    return ret;
}

esp_err_t HeartRateModel::StopScan()
{
    esp_err_t ret = bsp_ble_stop_scan();
    if (ret == ESP_OK) {
        scanning = false;
    }
    return ret;
}

bool HeartRateModel::IsScanning() const
{
    return scanning;
}

esp_err_t HeartRateModel::GetDevicesHR(bsp_ble_device_info_t *devices, int *count, int max_count)
{
    if (devices == nullptr || count == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    bsp_ble_device_info_t all[MAX_DEVICES];
    uint8_t all_count = 0;
    esp_err_t ret = bsp_ble_get_scanned_devices(all, &all_count, MAX_DEVICES);
    if (ret != ESP_OK) {
        *count = 0;
        return ret;
    }

    int out = 0;
    for (int i = 0; i < (int)all_count && out < max_count; i++) {
        if (all[i].type == BSP_BLE_DEVICE_TYPE_HR) {
            devices[out++] = all[i];
        }
    }
    *count = out;
    return ESP_OK;
}

esp_err_t HeartRateModel::ConnectDevice(const uint8_t *addr, uint8_t addr_type)
{
    return bsp_ble_connect(addr, addr_type);
}

esp_err_t HeartRateModel::DisconnectHR()
{
    uint16_t handle = bsp_ble_get_conn_handle(BSP_BLE_DEVICE_TYPE_HR);
    if (handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return bsp_ble_disconnect(handle);
}

bool HeartRateModel::IsConnected() const
{
    return bsp_ble_is_device_connected(BSP_BLE_DEVICE_TYPE_HR);
}

int HeartRateModel::GetWeightKG() const
{
    return weight_kg;
}

void HeartRateModel::SetWeightKG(int kg)
{
    if (kg < 30) kg = 30;
    if (kg > 200) kg = 200;
    weight_kg = kg;
}

esp_err_t HeartRateModel::SaveWeightToSysConfig()
{
    if (account == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    // Pull current SysConfig so we don't overwrite wheel with defaults.
    SysConfig_Info_t cur;
    memset(&cur, 0, sizeof(cur));
    if (account->Pull("SysConfig", &cur, sizeof(cur)) != Account::RES_OK) {
        cur.wheel_circumference_m = bsp_ble_get_wheel_circumference();
    }

    SysConfig_Info_t cfg = cur;
    cfg.cmd = SYSCONFIG_CMD_SAVE;
    cfg.weight_kg = (float)weight_kg;
    account->Notify("SysConfig", &cfg, sizeof(cfg));
    return ESP_OK;
}

void HeartRateModel::SetStatusBarStyle(int style)
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

void HeartRateModel::SetStatusBarAppear(bool en)
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

