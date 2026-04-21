#include "DialplateModel.h"
#include "dataproc.h"
#include "dataproc_def.h"
#include "Account.h"
#include "hal_def.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

using namespace Page;

void DialplateModel::Init()
{
    ESP_LOGI("DialplateModel", "Initializing DialplateModel...");
    
    DataCenter* center = DataProc_GetDataCenter();
    if (center == nullptr) {
        ESP_LOGE("DialplateModel", "DataCenter is nullptr, cannot create Account");
        return;
    }
    
    account = new Account("DialplateModel", center, 0, this);
    if (account == nullptr) {
        ESP_LOGE("DialplateModel", "Failed to create Account");
        return;
    }
    
    ESP_LOGI("DialplateModel", "Account created successfully");

    account->Subscribe("SportStatus");
    // 移除：account->Subscribe("Recorder");
    account->Subscribe("StatusBar");
    account->Subscribe("GPS");
    account->Subscribe("TrackFilter");  // 需要订阅才能向TrackFilter发布数据
    account->Subscribe("IMU");  // 订阅IMU数据
    
    account->SetEventCallback(onEvent);
    
    ESP_LOGI("DialplateModel", "DialplateModel initialized successfully");
}

void DialplateModel::Deinit()
{
    if (account)
    {
        delete account;
        account = nullptr;
    }
}

bool DialplateModel::GetGPSReady()
{
    if (account == nullptr) {
        return false;
    }
    
    HAL::GPS_Info_t gps;
    memset(&gps, 0, sizeof(gps));
    if (account->Pull("GPS", &gps, sizeof(gps)) != Account::RES_OK)
    {
        return false;
    }
    return (gps.satellites > 0);
}

int DialplateModel::onEvent(Account *account, Account::EventParam_t *param)
{
    if (param->event != Account::EVENT_PUB_PUBLISH)
    {
        return Account::RES_UNSUPPORTED_REQUEST;
    }

    // 适配字段名：HAL::SportStatus_Info_t -> SportStatus_Info_t
    if (strcmp(param->tran->ID, "SportStatus") != 0 || param->size != sizeof(SportStatus_Info_t))
    {
        return Account::RES_PARAM_ERROR;
    }

    DialplateModel *instance = (DialplateModel *)account->UserData;
    memcpy(&(instance->sportStatusInfo), param->data_p, param->size);

    return Account::RES_OK;
}

void DialplateModel::TrackFilterCommand(RecCmd_t cmd)
{
    if (account == nullptr) {
        return;
    }

    // 如果不是REC_READY_STOP，发送命令到TrackFilter
    if (cmd != REC_READY_STOP)
    {
        TrackFilter_Info_t trackInfo;
        memset(&trackInfo, 0, sizeof(trackInfo));
        trackInfo.cmd = (TrackFilter_Cmd_t)cmd;
        
        account->Notify("TrackFilter", &trackInfo, sizeof(trackInfo));
        ESP_LOGI("DialplateModel", "TrackFilter command sent: %d", cmd);
    }

    // 更新状态栏标签
    StatusBar_Info_t statInfo;
    memset(&statInfo, 0, sizeof(statInfo));
    statInfo.cmd = STATUS_BAR_CMD_SET_LABEL_REC;

    switch (cmd)
    {
    case REC_START:
    case REC_CONTINUE:
        statInfo.param.labelRec.show = true;
        statInfo.param.labelRec.str = "REC";
        break;
    case REC_PAUSE:
        statInfo.param.labelRec.show = true;
        statInfo.param.labelRec.str = "PAUSE";
        break;
    case REC_READY_STOP:
        statInfo.param.labelRec.show = true;
        statInfo.param.labelRec.str = "STOP";
        break;
    case REC_STOP:
        statInfo.param.labelRec.show = false;
        break;
    default:
        break;
    }

    account->Notify("StatusBar", &statInfo, sizeof(statInfo));
}

void DialplateModel::SetStatusBarStyle(StatusBar_Style_t style)
{
    if (account == nullptr) {
        return;
    }
    
    StatusBar_Info_t info;
    memset(&info, 0, sizeof(info));
    info.cmd = STATUS_BAR_CMD_SET_STYLE;
    info.param.style = style;
    
    account->Notify("StatusBar", &info, sizeof(info));
}

void DialplateModel::SetStatusBarAppear(bool en)
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

bool DialplateModel::GetIMUInfo(HAL::IMU_Info_t* info)
{
    if (account == nullptr || info == nullptr) {
        ESP_LOGW("DialplateModel", "GetIMUInfo: account or info is nullptr");
        return false;
    }
    
    int res = account->Pull("IMU", info, sizeof(HAL::IMU_Info_t));
    if (res != Account::RES_OK) {
        ESP_LOGW("DialplateModel", "GetIMUInfo: Pull failed, res=%d", res);
        return false;
    }
    
    return true;
}

const char* DialplateModel::MakeTimeString(uint64_t seconds, char* buf, uint16_t len)
{
    if (buf == nullptr || len == 0) {
        return buf;
    }
    
    // 当前项目的 single_time 单位是秒，version2 是毫秒
    // 这里直接处理秒数
    uint64_t ss = seconds;
    uint64_t mm = ss / 60;
    uint32_t hh = (uint32_t)(mm / 60);

    snprintf(
        buf, len,
        "%" PRIu32 ":%02" PRIu32 ":%02" PRIu32,
        hh,
        (uint32_t)(mm % 60),
        (uint32_t)(ss % 60)
    );

    return buf;
}
