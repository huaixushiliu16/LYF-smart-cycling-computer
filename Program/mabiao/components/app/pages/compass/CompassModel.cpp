#include "CompassModel.h"
#include "dataproc.h"
#include "dataproc_def.h"
#include "Account.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>
#include "config.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace Page;

void CompassModel::Init()
{
    ESP_LOGI("CompassModel", "Initializing CompassModel...");
    
    DataCenter* center = DataProc_GetDataCenter();
    if (center == nullptr) {
        ESP_LOGE("CompassModel", "DataCenter is nullptr, cannot create Account");
        return;
    }
    
    account = new Account("CompassModel", center, 0, this);
    if (account == nullptr) {
        ESP_LOGE("CompassModel", "Failed to create Account");
        return;
    }
    
    ESP_LOGI("CompassModel", "Account created successfully");

    account->Subscribe("StatusBar");
    account->Subscribe("MAG");
    
    account->SetEventCallback(onEvent);
    
    ESP_LOGI("CompassModel", "CompassModel initialized successfully");
}

void CompassModel::Deinit()
{
    if (account)
    {
        delete account;
        account = nullptr;
    }
}

void CompassModel::GetMAGInfo(
    int *dir,
    int *x,
    int *y,
    int *z,
    bool *isCalibrated)
{
    if (account == nullptr || dir == nullptr || x == nullptr || 
        y == nullptr || z == nullptr || isCalibrated == nullptr) {
        *dir = 0;
        *x = 0;
        *y = 0;
        *z = 0;
        *isCalibrated = false;
        return;
    }
    
    MAG_Info_t mag = {};
    
    int ret = account->Pull("MAG", &mag, sizeof(mag));
    if (ret != Account::RES_OK || !mag.isValid) {
        *dir = 0;
        *x = 0;
        *y = 0;
        *z = 0;
        *isCalibrated = false;
        return;
    }
    
    // 计算方向角度（度），基于magX和magY
    float angle = atan2f(mag.magY, mag.magX) * 180.0f / M_PI;
    if (angle < 0) {
        angle += 360.0f;
    }

    // 软件补偿：航向偏置（度）
    // 注意：这里输出的是“度”，UI 层再做 *10 转成 LVGL 的 0.1 度单位
    int dir_raw = (int)angle;
    int dir_cal = (dir_raw + (int)CONFIG_COMPASS_HEADING_OFFSET_DEG) % 360;
    if (dir_cal < 0) {
        dir_cal += 360;
    }
    *dir = dir_cal;
    *x = (int)(mag.magX * 10);  // 转换为整数（保留1位小数）
    *y = (int)(mag.magY * 10);
    *z = (int)(mag.magZ * 10);
    *isCalibrated = mag.isValid;  // 使用isValid作为校准状态
}

void CompassModel::SetMAGCalibration(void)
{
    // 暂时留空实现，当前项目暂无校准功能
    ESP_LOGW("CompassModel", "SetMAGCalibration called but not implemented");
}

int CompassModel::onEvent(Account *account, Account::EventParam_t *param)
{
    if (param->event != Account::EVENT_PUB_PUBLISH)
    {
        return Account::RES_UNSUPPORTED_REQUEST;
    }

    // StatusBar事件处理（如果需要）
    if (strcmp(param->tran->ID, "StatusBar") == 0)
    {
        // 可以在这里处理StatusBar更新事件
        return Account::RES_OK;
    }

    return Account::RES_UNSUPPORTED_REQUEST;
}

void CompassModel::SetStatusBarStyle(StatusBar_Style_t style)
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

void CompassModel::SetStatusBarAppear(bool en)
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
