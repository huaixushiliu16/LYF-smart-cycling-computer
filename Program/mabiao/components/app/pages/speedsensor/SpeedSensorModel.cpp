#include "SpeedSensorModel.h"
#include "Account.h"
#include "dataproc.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SpeedSensorModel";

using namespace Page;

void SpeedSensorModel::Init()
{
    account = nullptr;
    scanning = false;
    wheel_mm = 0;

    DataCenter* center = DataProc_GetDataCenter();
    if (center == nullptr) {
        ESP_LOGE(TAG, "DataCenter is nullptr, cannot create Account");
        return;
    }

    account = new Account("SpeedSensor", center);
    if (account == nullptr) {
        ESP_LOGE(TAG, "Failed to create Account");
        return;
    }
    account->Subscribe("StatusBar");
    account->Subscribe("SysConfig");

    wheel_mm = (uint16_t)(bsp_ble_get_wheel_circumference() * 1000.0f + 0.5f);
}

void SpeedSensorModel::Deinit()
{
    if (account) {
        delete account;
        account = nullptr;
    }
    if (scanning) {
        StopScan();
    }
}

esp_err_t SpeedSensorModel::StartScan(uint32_t duration_sec)
{
    esp_err_t ret = bsp_ble_start_scan(duration_sec);
    if (ret == ESP_OK) {
        scanning = true;
    }
    return ret;
}

esp_err_t SpeedSensorModel::StopScan()
{
    esp_err_t ret = bsp_ble_stop_scan();
    if (ret == ESP_OK) {
        scanning = false;
    }
    return ret;
}

bool SpeedSensorModel::IsScanning() const
{
    return scanning;
}

esp_err_t SpeedSensorModel::GetDevicesCSCS(bsp_ble_device_info_t *devices, int *count, int max_count)
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
        if (all[i].type == BSP_BLE_DEVICE_TYPE_CSCS) {
            devices[out++] = all[i];
        }
    }
    *count = out;
    return ESP_OK;
}

esp_err_t SpeedSensorModel::ConnectDevice(const uint8_t *addr, uint8_t addr_type)
{
    return bsp_ble_connect(addr, addr_type);
}

esp_err_t SpeedSensorModel::DisconnectCSCS()
{
    uint16_t handle = bsp_ble_get_conn_handle(BSP_BLE_DEVICE_TYPE_CSCS);
    if (handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return bsp_ble_disconnect(handle);
}

bool SpeedSensorModel::IsConnected() const
{
    return bsp_ble_is_device_connected(BSP_BLE_DEVICE_TYPE_CSCS);
}

bsp_ble_cscs_mode_t SpeedSensorModel::GetMode() const
{
    return bsp_ble_get_cscs_mode();
}

esp_err_t SpeedSensorModel::SetMode(bsp_ble_cscs_mode_t mode)
{
    return bsp_ble_set_cscs_mode(mode);
}

uint16_t SpeedSensorModel::GetWheelMM() const
{
    return wheel_mm;
}

void SpeedSensorModel::SetWheelMM(uint16_t mm)
{
    wheel_mm = mm;
}

esp_err_t SpeedSensorModel::SaveWheelToSysConfig()
{
    if (account == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    SysConfig_Info_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.cmd = SYSCONFIG_CMD_SAVE;
    cfg.weight_kg = 0.0f;  // Keep current weight in dp_sysconfig (it merges from UI; 0 means leave unchanged?).
    cfg.wheel_circumference_m = (float)wheel_mm / 1000.0f;

    // Pull current SysConfig first so we don't overwrite weight with 0.
    SysConfig_Info_t cur;
    if (account->Pull("SysConfig", &cur, sizeof(cur)) == Account::RES_OK) {
        cfg.weight_kg = cur.weight_kg;
    } else {
        cfg.weight_kg = 70.0f;
    }

    account->Notify("SysConfig", &cfg, sizeof(cfg));
    return ESP_OK;
}

void SpeedSensorModel::SetStatusBarStyle(int style)
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

void SpeedSensorModel::SetStatusBarAppear(bool en)
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

