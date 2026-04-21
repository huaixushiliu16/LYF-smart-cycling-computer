/**
 * @file dp_imu.cpp
 * @brief IMU数据处理实现
 * @note 阶段7：HAL层封装和DataProc层实现
 * @note 阶段8.5：集成Account模式
 * @note 传感器数据已由传感器自身滤波和校准，DataProc层只负责数据解析和传递
 */

#include "dataproc.h"
#include "dataproc_def.h"
#include "hal.h"
#include "esp_log.h"
#include "utils/datacenter/Account.h"
#include "utils/datacenter/DataCenter.h"

static const char *TAG = "DP_IMU";
static bool s_initialized = false;
static Account* s_imu_account = nullptr;

// 使用公共类型 MAG_Info_t
static Account* s_mag_account = nullptr;
static MAG_Info_t s_mag_info = {};

// Account事件回调函数（C++函数，不在extern "C"中）
static int DP_IMU_OnEvent(Account* account, Account::EventParam_t* param)
{
    if (param->event == Account::EVENT_TIMER)
    {
        DP_IMU_Update();
        return Account::RES_OK;
    }
    
    if (param->event == Account::EVENT_SUB_PULL)
    {
        HAL::IMU_Info_t* info = (HAL::IMU_Info_t*)param->data_p;
        if (param->size != sizeof(HAL::IMU_Info_t))
        {
            return Account::RES_SIZE_MISMATCH;
        }
        HAL::IMU_GetInfo(info);
        return Account::RES_OK;
    }
    
    return Account::RES_UNSUPPORTED_REQUEST;
}

// MAG Account事件回调函数
static int DP_MAG_OnEvent(Account* account, Account::EventParam_t* param)
{
    if (param->event == Account::EVENT_TIMER)
    {
        // 从IMU数据提取磁力计数据
        HAL::IMU_Info_t imuInfo;
        if (HAL::IMU_GetInfo(&imuInfo))
        {
            s_mag_info.magX = imuInfo.mx;
            s_mag_info.magY = imuInfo.my;
            s_mag_info.magZ = imuInfo.mz;
            s_mag_info.isValid = true;
        }
        else
        {
            s_mag_info.isValid = false;
        }
        
        // 发布数据
        if (s_mag_account)
        {
            s_mag_account->Commit(&s_mag_info, sizeof(s_mag_info));
            s_mag_account->Publish();
        }
        return Account::RES_OK;
    }
    
    if (param->event == Account::EVENT_SUB_PULL)
    {
        MAG_Info_t* info = (MAG_Info_t*)param->data_p;
        if (param->size != sizeof(MAG_Info_t))
        {
            return Account::RES_SIZE_MISMATCH;
        }
        *info = s_mag_info;
        return Account::RES_OK;
    }
    
    return Account::RES_UNSUPPORTED_REQUEST;
}

extern "C" {

void DP_IMU_Init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "DP_IMU already initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing IMU DataProc...");
    
    // 获取DataCenter
    DataCenter* center = DataProc_GetDataCenter();
    if (!center) {
        ESP_LOGE(TAG, "DataCenter not initialized");
        return;
    }
    
    // 创建IMU Account
    s_imu_account = new Account("IMU", center, sizeof(HAL::IMU_Info_t));
    s_imu_account->SetEventCallback(DP_IMU_OnEvent);
    s_imu_account->SetTimerPeriod(100);  // 10Hz更新
    s_imu_account->SetTimerEnable(true);
    
    // 创建MAG Account（从IMU数据提取）
    s_mag_account = new Account("MAG", center, sizeof(MAG_Info_t));
    s_mag_account->SetEventCallback(DP_MAG_OnEvent);
    s_mag_account->SetTimerPeriod(100);  // 10Hz更新
    s_mag_account->SetTimerEnable(true);
    
    // 初始化MAG数据
    s_mag_info.magX = 0.0f;
    s_mag_info.magY = 0.0f;
    s_mag_info.magZ = 0.0f;
    s_mag_info.isValid = false;
    
    s_initialized = true;
    ESP_LOGI(TAG, "IMU DataProc initialized");
}

void DP_IMU_Update(void)
{
    if (!s_initialized) {
        return;
    }
    
    // 更新HAL层IMU数据
    HAL::IMU_Update();
    
    // 获取IMU信息
    HAL::IMU_Info_t imuInfo;
    if (!HAL::IMU_GetInfo(&imuInfo)) {
        return;
    }
    
    // 发布数据到Account
    if (s_imu_account)
    {
        s_imu_account->Commit(&imuInfo, sizeof(imuInfo));
        s_imu_account->Publish();
    }
}

bool DP_IMU_GetInfo(HAL::IMU_Info_t *info)
{
    if (!s_initialized || info == nullptr) {
        return false;
    }
    
    // 从HAL层获取IMU信息（传感器已滤波和校准，直接返回）
    return HAL::IMU_GetInfo(info);
}

} // extern "C"
