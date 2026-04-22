/**
 * @file dp_power.cpp
 * @brief ?????????????
 * @note ???7??HAL??????DataProc?????
 * @note ???8.5??????Account??
 * @note 2026-04-20 ????????? 1s ??????????SUB_PULL ?????????????????????????
 */

#include "dataproc.h"
#include "hal.h"
#include "bsp_buzzer.h"
#include "esp_log.h"
#include "utils/datacenter/Account.h"
#include "utils/datacenter/DataCenter.h"

static const char *TAG = "DP_POWER";
static bool s_initialized = false;
static Account* s_power_account = nullptr;
static uint8_t s_last_usage = 100;
static bool s_last_charging = false;
static bool s_low_battery_warned = false;

// Account????????????C++??????????extern "C"?§µ?
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
        // ?????? HAL ???—¤???? DP_Power_Update ????????????????
        // ?????? PULL ¡¤???????????????????? UI ????????? ~70ms ????
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
    s_power_account->SetTimerPeriod(1000);  // 1 ????????
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

    // 1) ??????? HAL/BSP ???????~70ms ??????1s ??????????????
    HAL::Power_Update();

    // 2) ?????????????????????????¦Â?????
    HAL::Power_Info_t powerInfo;
    if (!HAL::Power_GetInfo(&powerInfo)) {
        return;
    }

    // 3) ???????????????<20% ??????¦Î??—±??? >=20% ??????
    if (powerInfo.usage < 20 && powerInfo.usage > 0) {
        if (!s_low_battery_warned || s_last_usage >= 20) {
            ESP_LOGW(TAG, "Low battery warning: %d%%", powerInfo.usage);
            bsp_buzzer_request(BSP_BUZZ_PATTERN_BATTERY_LOW);
            s_low_battery_warned = true;
        }
    } else if (powerInfo.usage >= 20) {
        s_low_battery_warned = false;
    }

    // 4) ??????£???
    if (powerInfo.isCharging != s_last_charging) {
        ESP_LOGI(TAG, "Charging status changed: %s",
                 powerInfo.isCharging ? "Charging" : "Not charging");
        s_last_charging = powerInfo.isCharging;
    }

    s_last_usage = powerInfo.usage;

    // 5) ?????? Account?????§Ø??????StatusBar / SystemInfos ?????? Pull ?????
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
    // ?? HAL ????????????????
    return HAL::Power_GetInfo(info);
}

} // extern "C"
