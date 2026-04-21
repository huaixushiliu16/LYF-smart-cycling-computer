/**
 * @file hal_gps.cpp
 * @brief GPS HAL封装实现
 * @note 阶段2.5：软件分层架构搭建 - 框架实现（待后续完善）
 */

#include "hal.h"
#include "bsp_gps.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "HAL_GPS";
static bool s_initialized = false;

// GPS 无定位（无卫星/无 fix）时的默认坐标（十进制度）
// 北纬：37°28'12'' = 37.470000
// 东经：116°19'28'' = 116.324444
static constexpr double GPS_DEFAULT_LONGITUDE = 116.324444;
static constexpr double GPS_DEFAULT_LATITUDE = 37.470000;

namespace HAL
{

void GPS_Init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "GPS already initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing GPS HAL (framework only)...");
    
    // 初始化BSP层GPS驱动
    esp_err_t ret = bsp_gps_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BSP GPS: %s", esp_err_to_name(ret));
        return;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "GPS HAL initialized");
}

void GPS_Update(void)
{
    if (!s_initialized) {
        return;
    }
    
    // 更新BSP层GPS数据
    bsp_gps_update();
}

bool GPS_GetInfo(GPS_Info_t *info)
{
    if (!s_initialized || info == nullptr) {
        return false;
    }
    
    bsp_gps_data_t bsp_data;
    esp_err_t ret = bsp_gps_read(&bsp_data);
    if (ret != ESP_OK) {
        return false;
    }
    
    // 无定位时也输出默认坐标，便于 UI/地图模块稳定工作
    if (!bsp_data.is_valid) {
        info->longitude = GPS_DEFAULT_LONGITUDE;
        info->latitude = GPS_DEFAULT_LATITUDE;
        info->altitude = 0.0f;
        info->course = 0.0f;
        info->speed = 0.0f;
        info->satellites = bsp_data.satellites;  // 可能为 0
        info->isVaild = false;
        memset(&info->clock, 0, sizeof(info->clock));
        return true;
    }
    
    // 经纬度范围检查（-180~180, -90~90）
    if (bsp_data.longitude < -180.0 || bsp_data.longitude > 180.0 ||
        bsp_data.latitude < -90.0 || bsp_data.latitude > 90.0) {
        ESP_LOGW(TAG, "Invalid GPS coordinates: lon=%.6f, lat=%.6f", 
                 bsp_data.longitude, bsp_data.latitude);
        info->longitude = GPS_DEFAULT_LONGITUDE;
        info->latitude = GPS_DEFAULT_LATITUDE;
        info->altitude = 0.0f;
        info->course = 0.0f;
        info->speed = 0.0f;
        info->satellites = bsp_data.satellites;
        info->isVaild = false;
        memset(&info->clock, 0, sizeof(info->clock));
        return true;
    }
    
    // 速度范围检查（0~300 km/h）
    if (bsp_data.speed < 0.0f || bsp_data.speed > 300.0f) {
        ESP_LOGW(TAG, "Invalid GPS speed: %.2f km/h", bsp_data.speed);
        return false;
    }
    
    // 卫星数范围检查（0~99）
    if (bsp_data.satellites < 0 || bsp_data.satellites > 99) {
        ESP_LOGW(TAG, "Invalid GPS satellites: %d", bsp_data.satellites);
        return false;
    }
    
    // 转换BSP层数据到HAL层数据
    info->longitude = bsp_data.longitude;
    info->latitude = bsp_data.latitude;
    info->altitude = bsp_data.altitude;
    info->course = bsp_data.course;
    info->speed = bsp_data.speed;
    info->satellites = bsp_data.satellites;
    info->isVaild = bsp_data.is_valid;
    
    // UTC时间转换（从BSP层获取）
    info->clock.year = bsp_data.year;
    info->clock.month = bsp_data.month;
    info->clock.day = bsp_data.day;
    info->clock.week = 0;  // GPS不提供周数，设为0
    info->clock.hour = bsp_data.hour;
    info->clock.minute = bsp_data.minute;
    info->clock.second = bsp_data.second;
    info->clock.millisecond = 0;  // NMEA不提供毫秒，设为0
    
    return true;
}

} // namespace HAL

// C兼容接口
extern "C" {
void GPS_Update_C(void)
{
    HAL::GPS_Update();
}

bool GPS_GetInfo_C(GPS_Info_t_C *info)
{
    // GPS_Info_t_C 是 HAL::GPS_Info_t 的别名，可以直接转换
    HAL::GPS_Info_t *hal_info = reinterpret_cast<HAL::GPS_Info_t*>(info);
    return HAL::GPS_GetInfo(hal_info);
}
}
