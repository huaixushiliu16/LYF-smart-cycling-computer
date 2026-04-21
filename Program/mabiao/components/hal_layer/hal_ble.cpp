/**
 * @file hal_ble.cpp
 * @brief HAL layer BLE interface implementation
 * @note Phase 4: BLE Client driver development
 */

#include "hal.h"
#include "bsp_ble.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "HAL_BLE";

// Static device name buffer
static char s_ble_device_name[32] = "BLE Device";

// Provide global scope BLE_Info_t type alias for C-compatible interface
using BLE_Info_t = HAL::BLE_Info_t;

namespace HAL
{

void BLE_Init(void)
{
    ESP_LOGI(TAG, "Initializing BLE...");
    
    bsp_ble_config_t config = {
        .enable_scan = true,
        .scan_interval_ms = 1000,
        .lvgl_lock = NULL,      // LVGL lock function set in main.c via bsp_ble_set_lvgl_lock_functions
        .lvgl_unlock = NULL,    // LVGL unlock function set in main.c via bsp_ble_set_lvgl_lock_functions
    };
    
    esp_err_t ret = bsp_ble_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BLE init failed: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "BLE initialized");
}

void BLE_Update(void)
{
    // BLE data received via callback function, can add other update logic here
    bsp_ble_update();
}

bool BLE_GetInfo(BLE_Info_t *info)
{
    if (info == nullptr) {
        return false;
    }
    
    // Read data from BSP layer
    info->heartRate = bsp_ble_get_heart_rate();
    info->speed = bsp_ble_get_speed();
    info->cadence = bsp_ble_get_cadence();
    
    // Data validity check
    // Heart rate range: 0~220 bpm
    if (info->heartRate > 220) {
        ESP_LOGW(TAG, "Invalid heart rate: %d bpm", info->heartRate);
        info->heartRate = 0;  // Set invalid data to 0
    }
    
    // Speed range: 0~100 km/h
    if (info->speed < 0.0f || info->speed > 100.0f) {
        ESP_LOGW(TAG, "Invalid BLE speed: %.2f km/h", info->speed);
        info->speed = 0.0f;  // Set invalid data to 0
    }
    
    // Cadence range: 0~200 RPM
    if (info->cadence < 0.0f || info->cadence > 200.0f) {
        ESP_LOGW(TAG, "Invalid BLE cadence: %.2f RPM", info->cadence);
        info->cadence = 0.0f;  // Set invalid data to 0
    }
    
    // Check if connected (at least one device connected)
    bool hr_connected = bsp_ble_is_device_connected(BSP_BLE_DEVICE_TYPE_HR);
    bool cscs_connected = bsp_ble_is_device_connected(BSP_BLE_DEVICE_TYPE_CSCS);
    info->isConnected = (hr_connected || cscs_connected);
    
    info->isEnabled = true;
    info->name = s_ble_device_name;
    
    return true;
}

} // namespace HAL

// C-compatible interface implementation
extern "C" {

void BLE_Init_C(void)
{
    HAL::BLE_Init();
}

void BLE_Update_C(void)
{
    HAL::BLE_Update();
}

bool BLE_GetInfo_C(BLE_Info_t_C *info)
{
    return HAL::BLE_GetInfo(info);
}

} // extern "C"
