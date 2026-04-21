/**
 * @file dp_statusbar.cpp
 * @brief StatusBar数据处理实现
 * @note 阶段8.5：集成Account模式
 */

#include "dataproc.h"
#include "dataproc_def.h"
#include "esp_log.h"
#include "utils/datacenter/Account.h"
#include "utils/datacenter/DataCenter.h"

static const char *TAG = "DP_STATUSBAR";
static bool s_initialized = false;
static Account* s_statusbar_account = nullptr;

// StatusBar UI接口函数（在StatusBar.cpp中实现，C++命名空间）
namespace Page {
    extern void StatusBar_SetAppear(bool en);
    extern void StatusBar_SetStyle(int style);
    extern void StatusBar_SetLabelRec(bool show, const char *str);
}

// Account事件回调
static int DP_StatusBar_OnEvent(Account* account, Account::EventParam_t* param)
{
    if (param->event == Account::EVENT_NOTIFY)
    {
        if (param->size != sizeof(StatusBar_Info_t))
        {
            return Account::RES_SIZE_MISMATCH;
        }
        
        StatusBar_Info_t* info = (StatusBar_Info_t*)param->data_p;
        
        switch (info->cmd)
        {
        case STATUS_BAR_CMD_APPEAR:
            Page::StatusBar_SetAppear(info->param.appear);
            return Account::RES_OK;
            
        case STATUS_BAR_CMD_SET_STYLE:
            Page::StatusBar_SetStyle((int)info->param.style);
            return Account::RES_OK;
            
        case STATUS_BAR_CMD_SET_LABEL_REC:
            Page::StatusBar_SetLabelRec(info->param.labelRec.show, info->param.labelRec.str);
            return Account::RES_OK;
            
        default:
            return Account::RES_PARAM_ERROR;
        }
    }
    
    return Account::RES_UNSUPPORTED_REQUEST;
}

extern "C" {

void DP_StatusBar_Init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "StatusBar already initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing StatusBar DataProc...");
    
    // 获取DataCenter
    DataCenter* center = DataProc_GetDataCenter();
    if (!center) {
        ESP_LOGE(TAG, "DataCenter not initialized");
        return;
    }
    
    // 创建StatusBar Account
    s_statusbar_account = new Account("StatusBar", center, sizeof(StatusBar_Info_t));
    s_statusbar_account->SetEventCallback(DP_StatusBar_OnEvent);
    
    s_initialized = true;
    ESP_LOGI(TAG, "StatusBar DataProc initialized");
}

} // extern "C"
