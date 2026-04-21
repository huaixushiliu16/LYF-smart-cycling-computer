/**
 * @file dp_sport.cpp
 * @brief 运动数据统计实现
 * @note 阶段8：集成Account模式
 */

#include "dataproc.h"
#include "hal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "utils/datacenter/Account.h"
#include "utils/datacenter/DataCenter.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *TAG = "DP_SPORT";
static bool s_initialized = false;
static SportStatus_Info_t s_sport_info = {};
static Account* s_sport_account = nullptr;

// 卡路里系数
#define CALORIC_COEFFICIENT 0.5f

// 地球半径（km）
#define EARTH_RADIUS_KM 6371.0

// 使用Haversine公式计算两点间距离（km）
static double DP_Sport_GetDistanceOffset(double longitude1, double latitude1,
                                          double longitude2, double latitude2)
{
    // 转换为弧度
    double dLat = (latitude2 - latitude1) * M_PI / 180.0;
    double dLon = (longitude2 - longitude1) * M_PI / 180.0;
    
    double lat1_rad = latitude1 * M_PI / 180.0;
    double lat2_rad = latitude2 * M_PI / 180.0;
    
    // Haversine公式
    double a = sin(dLat/2) * sin(dLat/2) +
               cos(lat1_rad) * cos(lat2_rad) *
               sin(dLon/2) * sin(dLon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    double distance_km = EARTH_RADIUS_KM * c;
    
    return distance_km;
}

// Account事件回调函数（C++函数，不在extern "C"中）
int DP_Sport_OnEvent(Account* account, Account::EventParam_t* param)
{
    if (param->event == Account::EVENT_TIMER)
    {
        DP_Sport_Update();
        return Account::RES_OK;
    }
    
    if (param->event == Account::EVENT_SUB_PULL)
    {
        SportStatus_Info_t* info = (SportStatus_Info_t*)param->data_p;
        if (param->size != sizeof(SportStatus_Info_t))
        {
            return Account::RES_SIZE_MISMATCH;
        }
        // 直接返回当前数据（因为这是发布者，订阅者请求拉取数据）
        *info = s_sport_info;
        return Account::RES_OK;
    }
    
    return Account::RES_UNSUPPORTED_REQUEST;
}

extern "C" {

void DP_Sport_Init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "DP_Sport already initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing Sport DataProc...");
    
    // 初始化运动数据
    s_sport_info.last_tick = xTaskGetTickCount();
    s_sport_info.weight = 70.0f;  // 默认体重70kg
    s_sport_info.speed_kph = 0.0f;
    s_sport_info.speed_max_kph = 0.0f;
    s_sport_info.speed_avg_kph = 0.0f;
    s_sport_info.total_time = 0;
    s_sport_info.total_distance = 0.0f;
    s_sport_info.single_time = 0;
    s_sport_info.single_distance = 0.0f;
    s_sport_info.single_calorie = 0.0f;
    
    // 阶段8新增字段初始化
    s_sport_info.cadence_rpm = 0.0f;
    s_sport_info.heart_rate_bpm = 0;
    s_sport_info.slope_percent = 0.0f;
    s_sport_info.altitude_m = 0.0f;
    s_sport_info.direction_deg = 0.0f;
    
    // 获取SportStatus Account（已在DataProc_Init中创建）
    DataCenter* center = DataProc_GetDataCenter();
    if (center)
    {
        s_sport_account = center->SearchAccount("SportStatus");
        if (s_sport_account)
        {
            s_sport_account->SetTimerPeriod(100);  // 100ms更新周期
            s_sport_account->SetTimerEnable(true);
            ESP_LOGI(TAG, "SportStatus Account timer enabled");
        }
        else
        {
            ESP_LOGW(TAG, "SportStatus Account not found");
        }
    }
    else
    {
        ESP_LOGW(TAG, "DataCenter not initialized");
    }
    
    s_initialized = true;
}

void DP_Sport_SetWeight(float weight_kg)
{
    if (weight_kg < 30.0f) {
        weight_kg = 30.0f;
    } else if (weight_kg > 200.0f) {
        weight_kg = 200.0f;
    }
    s_sport_info.weight = weight_kg;
}

void DP_Sport_Update(void)
{
    if (!s_initialized) {
        return;
    }
    
    // 获取GPS数据
    HAL::GPS_Info_t gpsInfo;
    bool gps_valid = HAL::GPS_GetInfo(&gpsInfo);
    
    // 获取BLE数据
    HAL::BLE_Info_t bleInfo;
    bool ble_valid = HAL::BLE_GetInfo(&bleInfo);
    
    // 获取IMU数据
    HAL::IMU_Info_t imuInfo;
    bool imu_valid = HAL::IMU_GetInfo(&imuInfo);
    
    // 计算时间差（tick -> 毫秒）
    // 注意：timeElaps 是"tick 数"，不等于毫秒。转换到毫秒必须乘以 portTICK_PERIOD_MS。
    uint32_t current_tick = xTaskGetTickCount();
    uint32_t timeElaps = current_tick - s_sport_info.last_tick;
    s_sport_info.last_tick = current_tick;
    uint32_t elapsed_ms = timeElaps * portTICK_PERIOD_MS;
    
    // 速度融合（唯一需要融合的数据）：优先BLE，备用GPS
    float speedKph = 0.0f;
    if (ble_valid && bleInfo.isConnected && bleInfo.speed > 0.0f) {
        speedKph = bleInfo.speed;  // BLE CSCS速度
    } else if (gps_valid && gpsInfo.isVaild && gpsInfo.satellites >= 3) {
        speedKph = gpsInfo.speed > 1.0f ? gpsInfo.speed : 0.0f;  // GPS速度（过滤<1km/h的噪声）
    }
    
    // 其他数据（直接使用，失败返回默认值）
    // 踏频
    if (ble_valid && bleInfo.isConnected) {
        s_sport_info.cadence_rpm = bleInfo.cadence;
    } else {
        s_sport_info.cadence_rpm = 0.0f;  // 默认值
    }
    
    // 心率
    if (ble_valid && bleInfo.isConnected && bleInfo.heartRate > 0) {
        s_sport_info.heart_rate_bpm = bleInfo.heartRate;
    } else {
        s_sport_info.heart_rate_bpm = 0;  // 默认值
    }
    
    // 坡度
    if (imu_valid) {
        s_sport_info.slope_percent = imuInfo.slope;
    } else {
        s_sport_info.slope_percent = 0.0f;  // 默认值
    }
    
    // 海拔：一律来自 IMU（内置气压计，精度和刷新率比 GPS 更稳定）
    if (imu_valid) {
        s_sport_info.altitude_m = imuInfo.height;
    } else {
        s_sport_info.altitude_m = 0.0f;
    }
    
    // 方向
    if (gps_valid && gpsInfo.isVaild) {
        s_sport_info.direction_deg = gpsInfo.course;
    } else {
        s_sport_info.direction_deg = 0.0f;  // 默认值
    }
    
    // 距离计算（基于GPS）
    static bool isFirstGPS = true;
    static double preLongitude = 0.0;
    static double preLatitude = 0.0;
    
    if (gps_valid && gpsInfo.isVaild && gpsInfo.satellites >= 3) {
        if (!isFirstGPS) {
            float dist = (float)DP_Sport_GetDistanceOffset(
                preLongitude, preLatitude,
                gpsInfo.longitude, gpsInfo.latitude);
            s_sport_info.single_distance += dist;
            s_sport_info.total_distance += dist;
        } else {
            isFirstGPS = false;
        }
        preLongitude = gpsInfo.longitude;
        preLatitude = gpsInfo.latitude;
    }
    
    // 时间统计（速度>0或GPS信号中断时累计）
    // single_time / total_time 的单位是"秒"（结构体注释约定；UI MakeTimeString 也按秒解析）。
    // 由于更新周期是 100ms，不能每次都整数除 1000 丢失精度，所以用静态毫秒累加器攒够整秒再 +=。
    bool isSignalInterruption = (gps_valid && gpsInfo.isVaild && gpsInfo.satellites == 0);
    if (speedKph > 0.0f || isSignalInterruption) {
        static uint32_t s_single_ms_accum = 0;
        static uint32_t s_total_ms_accum  = 0;
        s_single_ms_accum += elapsed_ms;
        s_total_ms_accum  += elapsed_ms;
        if (s_single_ms_accum >= 1000) {
            uint32_t sec = s_single_ms_accum / 1000;
            s_sport_info.single_time += sec;
            s_single_ms_accum -= sec * 1000;
        }
        if (s_total_ms_accum >= 1000) {
            uint32_t sec = s_total_ms_accum / 1000;
            s_sport_info.total_time += sec;
            s_total_ms_accum -= sec * 1000;
        }

        // 平均速度计算（单次距离/单次时间）
        if (s_sport_info.single_time > 0) {
            // single_time 已经是秒，直接参与：km/s * 3600 = km/h
            float time_sec = (float)s_sport_info.single_time;
            if (time_sec > 0.0f) {
                s_sport_info.speed_avg_kph = s_sport_info.single_distance * 3600.0f / time_sec;
            }
        }

        // 最大速度记录
        if (speedKph > s_sport_info.speed_max_kph) {
            s_sport_info.speed_max_kph = speedKph;
        }

        // 卡路里计算：速度(km/h) × 体重(kg) × 0.5 × 时间(小时)
        if (speedKph > 0.0f) {
            float time_hour = (float)elapsed_ms / 1000.0f / 3600.0f;
            float calorie = speedKph * s_sport_info.weight * CALORIC_COEFFICIENT * time_hour;
            s_sport_info.single_calorie += calorie;
        }
    }
    
    s_sport_info.speed_kph = speedKph;
    
    // 异常值过滤（简单过滤明显异常值）
    #define SPEED_MAX_KPH     100.0f
    #define CADENCE_MAX_RPM   200.0f
    #define HEART_RATE_MAX    220
    
    if (s_sport_info.speed_kph > SPEED_MAX_KPH) {
        s_sport_info.speed_kph = 0.0f;  // 异常值设为0
    }
    if (s_sport_info.cadence_rpm > CADENCE_MAX_RPM) {
        s_sport_info.cadence_rpm = 0.0f;
    }
    if (s_sport_info.heart_rate_bpm > HEART_RATE_MAX) {
        s_sport_info.heart_rate_bpm = 0;
    }
    
    // 发布数据到Account
    if (s_sport_account)
    {
        s_sport_account->Commit(&s_sport_info, sizeof(s_sport_info));
        s_sport_account->Publish();
    }
}

bool DP_Sport_GetInfo(SportStatus_Info_t *info)
{
    if (!s_initialized || info == nullptr) {
        return false;
    }
    
    *info = s_sport_info;
    return true;
}

} // extern "C"
