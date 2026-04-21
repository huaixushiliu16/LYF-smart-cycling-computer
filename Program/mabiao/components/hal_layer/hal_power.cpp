/**
 * @file hal_power.cpp
 * @brief Power HAL封装实现
 * @note 阶段6：电源驱动开发
 * @note 2026-04-20 重构：GetInfo 改用缓存读取，不再每次调用都阻塞采样；
 *                        采样由 Power_Update 统一驱动（dp_power 定时器 1s 一次）
 */

#include "hal.h"
#include "bsp_power.h"
#include "esp_log.h"

static const char *TAG = "HAL_POWER";
static bool s_initialized = false;
static bool s_last_charging = false;

namespace HAL
{

void Power_Init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Power already initialized");
        return;
    }

    ESP_LOGI(TAG, "Initializing Power HAL...");

    esp_err_t ret = bsp_power_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BSP Power: %s", esp_err_to_name(ret));
        return;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Power HAL initialized");
}

void Power_Update(void)
{
    if (!s_initialized) {
        return;
    }
    // 触发一次硬件采样并更新 BSP 层缓存（~70ms 阻塞）
    bsp_power_update();
}

bool Power_GetInfo(Power_Info_t *info)
{
    if (!s_initialized || info == nullptr) {
        return false;
    }

    // 读取 BSP 层缓存（无阻塞，不触发 ADC 采样）
    bsp_power_info_t bsp_data;
    esp_err_t ret = bsp_power_get_cached(&bsp_data);
    if (ret != ESP_OK) {
        return false;
    }

    // 转换 BSP 层数据到 HAL 层数据
    info->voltage = bsp_data.voltage_mv;
    info->usage = bsp_data.battery_percent;
    info->isCharging = bsp_data.is_charging;

    // 充电状态变化检测（同步 HAL 层状态记录，DataProc 也会记）
    if (bsp_data.is_charging != s_last_charging) {
        ESP_LOGI(TAG, "Charging status changed: %s",
                 bsp_data.is_charging ? "Charging" : "Not charging");
        s_last_charging = bsp_data.is_charging;
    }

    return true;
}

} // namespace HAL

// C兼容接口实现
extern "C" {

bool Power_GetInfo_C(Power_Info_t_C *info)
{
    return HAL::Power_GetInfo(info);
}

} // extern "C"
