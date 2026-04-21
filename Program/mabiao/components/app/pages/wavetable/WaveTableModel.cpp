#include "WaveTableModel.h"
#include "dataproc.h"
#include "dataproc_def.h"
#include "Account.h"
#include "esp_log.h"
#include <string.h>

using namespace Page;

void WaveTableModel::Init()
{
    ESP_LOGI("WaveTableModel", "Initializing WaveTableModel...");
    
    DataCenter* center = DataProc_GetDataCenter();
    if (center == nullptr) {
        ESP_LOGE("WaveTableModel", "DataCenter is nullptr, cannot create Account");
        return;
    }
    
    account = new Account("WaveTable", center);
    if (account == nullptr) {
        ESP_LOGE("WaveTableModel", "Failed to create Account");
        return;
    }
    
    // 订阅 StatusBar 以接收状态栏更新
    account->Subscribe("StatusBar");
    
    ESP_LOGI("WaveTableModel", "WaveTableModel initialized successfully");
}

void WaveTableModel::Deinit()
{
    if (account)
    {
        delete account;
        account = nullptr;
    }
}

void WaveTableModel::SetStatusBarStyle(int style)
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

void WaveTableModel::SetStatusBarAppear(bool en)
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
