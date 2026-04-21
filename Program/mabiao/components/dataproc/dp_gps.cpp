/**
 * @file dp_gps.cpp
 * @brief GPS数据处理实现
 * @note 阶段7：HAL层封装和DataProc层实现
 * @note 阶段8.5：集成Account模式
 */

#include "dataproc.h"
#include "hal.h"
#include "esp_log.h"
#include "utils/datacenter/Account.h"
#include "utils/datacenter/DataCenter.h"

static const char *TAG = "DP_GPS";
static bool s_initialized = false;
static Account* s_gps_account = nullptr;

// GPS状态枚举
typedef enum {
    GPS_STATUS_DISCONNECT,  // 卫星数=0
    GPS_STATUS_UNSTABLE,    // 卫星数3~6
    GPS_STATUS_CONNECT,     // 卫星数≥7
} GPS_Status_t;

static GPS_Status_t s_last_status = GPS_STATUS_DISCONNECT;

// Account事件回调函数（C++函数，不在extern "C"中）
static int DP_GPS_OnEvent(Account* account, Account::EventParam_t* param)
{
    if (param->event == Account::EVENT_TIMER)
    {
        DP_GPS_Update();
        return Account::RES_OK;
    }
    
    if (param->event == Account::EVENT_SUB_PULL)
    {
        HAL::GPS_Info_t* info = (HAL::GPS_Info_t*)param->data_p;
        if (param->size != sizeof(HAL::GPS_Info_t))
        {
            return Account::RES_SIZE_MISMATCH;
        }
        HAL::GPS_GetInfo(info);
        return Account::RES_OK;
    }
    
    return Account::RES_UNSUPPORTED_REQUEST;
}

extern "C" {

void DP_GPS_Init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "DP_GPS already initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing GPS DataProc...");
    
    // 获取DataCenter
    DataCenter* center = DataProc_GetDataCenter();
    if (!center) {
        ESP_LOGE(TAG, "DataCenter not initialized");
        return;
    }
    
    // 创建GPS Account
    s_gps_account = new Account("GPS", center, sizeof(HAL::GPS_Info_t));
    s_gps_account->SetEventCallback(DP_GPS_OnEvent);
    s_gps_account->SetTimerPeriod(1000);  // 1秒更新一次
    s_gps_account->SetTimerEnable(true);
    
    s_initialized = true;
    s_last_status = GPS_STATUS_DISCONNECT;
    ESP_LOGI(TAG, "GPS DataProc initialized");
}

void DP_GPS_Update(void)
{
    if (!s_initialized) {
        return;
    }
    
    // 更新HAL层GPS数据
    HAL::GPS_Update();
    
    // 获取GPS信息并判断状态
    HAL::GPS_Info_t gpsInfo;
    if (!HAL::GPS_GetInfo(&gpsInfo)) {
        return;
    }
    
    int satellites = gpsInfo.satellites;
    GPS_Status_t nowStatus = GPS_STATUS_DISCONNECT;
    
    // 状态判断
    if (satellites >= 7) {
        nowStatus = GPS_STATUS_CONNECT;
    } else if (satellites >= 3 && satellites < 7) {
        nowStatus = GPS_STATUS_UNSTABLE;
    } else {
        nowStatus = GPS_STATUS_DISCONNECT;
    }
    
    // 状态变化通知
    if (nowStatus != s_last_status) {
        const char* status_str[] = {
            "DISCONNECT",
            "UNSTABLE",
            "CONNECT"
        };
        ESP_LOGI(TAG, "GPS status changed: %s -> %s (satellites: %d)", 
                 status_str[s_last_status], status_str[nowStatus], satellites);
        s_last_status = nowStatus;
    }
    
    // 发布数据到Account
    if (s_gps_account)
    {
        s_gps_account->Commit(&gpsInfo, sizeof(gpsInfo));
        s_gps_account->Publish();
    }
}

bool DP_GPS_GetInfo(HAL::GPS_Info_t *info)
{
    if (!s_initialized || info == nullptr) {
        return false;
    }
    
    // 从HAL层获取GPS信息
    return HAL::GPS_GetInfo(info);
}

} // extern "C"
