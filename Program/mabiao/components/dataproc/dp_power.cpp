/**
 * @file dp_power.cpp
 * @brief 电源数据处理实现
 * @note 阶段7：HAL层封装和DataProc层实现
 * @note 阶段8.5：集成Account模式
 * @note 2026-04-20 重构：消除 1s 内双重采样；SUB_PULL 使用缓存读取，避免订阅者触发阻塞
 */

#include "dataproc.h"
#include "hal.h"
#include "esp_log.h"
#include "utils/datacenter/Account.h"
#include "utils/datacenter/DataCenter.h"

static const char *TAG = "DP_POWER";
static bool s_initialized = false;
static Account* s_power_account = nullptr;
static uint8_t s_last_usage = 100;
static bool s_last_charging = false;
static bool s_low_battery_warned = false;

// Account事件回调函数（C++函数，不在extern "C"中）
static int DP_Power_OnEvent(Account* account, Account::EventParam_t* param)
{
    if (param->event == Account::EVENT_TIMER)
    {
        DP_Power_Update();
        return Account::RES_OK;
    }

    if (param->event == Account::EVENT_SUB_PULL)
    {
        HAL::Power_Info_t* info = (HAL::Power_Info_t*)param->data_p;
        if (param->size != sizeof(HAL::Power_Info_t))
        {
            return Account::RES_SIZE_MISMATCH;
        }
        // 仅返回 HAL 层缓存（已由 DP_Power_Update 定时更新，无阻塞）
        // 不再在 PULL 路径上触发硬件采样，避免 UI 订阅者导致 ~70ms 阻塞
        HAL::Power_GetInfo(info);
        return Account::RES_OK;
    }

    return Account::RES_UNSUPPORTED_REQUEST;
}

extern "C" {

void DP_Power_Init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "DP_Power already initialized");
        return;
    }

    ESP_LOGI(TAG, "Initializing Power DataProc...");

    DataCenter* center = DataProc_GetDataCenter();
    if (!center) {
        ESP_LOGE(TAG, "DataCenter not initialized");
        return;
    }

    s_power_account = new Account("Power", center, sizeof(HAL::Power_Info_t));
    s_power_account->SetEventCallback(DP_Power_OnEvent);
    s_power_account->SetTimerPeriod(1000);  // 1 秒更新一次
    s_power_account->SetTimerEnable(true);

    s_initialized = true;
    s_last_usage = 100;
    s_last_charging = false;
    s_low_battery_warned = false;
    ESP_LOGI(TAG, "Power DataProc initialized");
}

void DP_Power_Update(void)
{
    if (!s_initialized) {
        return;
    }

    // 1) 触发一次 HAL/BSP 层采样（~70ms 阻塞，1s 一次是合理开销）
    HAL::Power_Update();

    // 2) 从缓存读取最新结果（不再触发二次采样）
    HAL::Power_Info_t powerInfo;
    if (!HAL::Power_GetInfo(&powerInfo)) {
        return;
    }

    // 3) 低电量报警（电量<20% 时记录一次警告；回到 >=20% 时重置）
    if (powerInfo.usage < 20 && powerInfo.usage > 0) {
        if (!s_low_battery_warned || s_last_usage >= 20) {
            ESP_LOGW(TAG, "Low battery warning: %d%%", powerInfo.usage);
            s_low_battery_warned = true;
        }
    } else if (powerInfo.usage >= 20) {
        s_low_battery_warned = false;
    }

    // 4) 充电状态变化检测
    if (powerInfo.isCharging != s_last_charging) {
        ESP_LOGI(TAG, "Charging status changed: %s",
                 powerInfo.isCharging ? "Charging" : "Not charging");
        s_last_charging = powerInfo.isCharging;
    }

    s_last_usage = powerInfo.usage;

    // 5) 发布到 Account，所有订阅者（StatusBar / SystemInfos …）经 Pull 拿缓存
    if (s_power_account)
    {
        s_power_account->Commit(&powerInfo, sizeof(powerInfo));
        s_power_account->Publish();
    }
}

bool DP_Power_GetInfo(HAL::Power_Info_t *info)
{
    if (!s_initialized || info == nullptr) {
        return false;
    }
    // 从 HAL 层缓存读取（无阻塞）
    return HAL::Power_GetInfo(info);
}

} // extern "C"
