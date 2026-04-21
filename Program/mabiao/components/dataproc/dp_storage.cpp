/**
 * @file dp_storage.cpp
 * @brief 存储数据处理实现
 * @note 阶段8.5：集成Account模式
 */

#include "dataproc.h"
#include "dataproc_def.h"
#include "bsp_sd.h"
#include "esp_log.h"
#include "utils/datacenter/Account.h"
#include "utils/datacenter/DataCenter.h"

static const char *TAG = "DP_STORAGE";
static bool s_initialized = false;
static Account* s_storage_account = nullptr;

// 使用公共类型 Storage_Basic_Info_t
static Storage_Basic_Info_t s_storage_info = {};
static const char* s_storage_type_sd = "SD Card";
static const char* s_storage_type_none = "None";

// Account事件回调
static int DP_Storage_OnEvent(Account* account, Account::EventParam_t* param)
{
    if (param->event == Account::EVENT_TIMER)
    {
        // 更新存储信息
        bsp_sd_info_t sd_info;
        if (bsp_sd_get_info(&sd_info) == ESP_OK && sd_info.is_mounted)
        {
            s_storage_info.isDetect = true;
            s_storage_info.totalSizeMB = (float)sd_info.total_size_mb;
            s_storage_info.freeSizeMB = (float)sd_info.free_size_mb;
            s_storage_info.type = s_storage_type_sd;
        }
        else
        {
            s_storage_info.isDetect = false;
            s_storage_info.totalSizeMB = 0;
            s_storage_info.freeSizeMB = 0;
            s_storage_info.type = s_storage_type_none;
        }
        
        // 发布数据
        if (s_storage_account)
        {
            s_storage_account->Commit(&s_storage_info, sizeof(s_storage_info));
            s_storage_account->Publish();
        }
        return Account::RES_OK;
    }
    
    if (param->event == Account::EVENT_SUB_PULL)
    {
        Storage_Basic_Info_t* info = (Storage_Basic_Info_t*)param->data_p;
        if (param->size != sizeof(Storage_Basic_Info_t))
        {
            return Account::RES_SIZE_MISMATCH;
        }
        *info = s_storage_info;
        return Account::RES_OK;
    }
    
    return Account::RES_UNSUPPORTED_REQUEST;
}

extern "C" {

void DP_Storage_Init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Storage already initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing Storage DataProc...");
    
    // 获取DataCenter
    DataCenter* center = DataProc_GetDataCenter();
    if (!center) {
        ESP_LOGE(TAG, "DataCenter not initialized");
        return;
    }
    
    // 创建Storage Account
    s_storage_account = new Account("Storage", center, sizeof(Storage_Basic_Info_t));
    s_storage_account->SetEventCallback(DP_Storage_OnEvent);
    s_storage_account->SetTimerPeriod(1000);  // 1秒更新一次
    s_storage_account->SetTimerEnable(true);
    
    // 初始化数据
    s_storage_info.isDetect = false;
    s_storage_info.totalSizeMB = 0;
    s_storage_info.freeSizeMB = 0;
    s_storage_info.type = s_storage_type_none;
    
    s_initialized = true;
    ESP_LOGI(TAG, "Storage DataProc initialized");
}

} // extern "C"
