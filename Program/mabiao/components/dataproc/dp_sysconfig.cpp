/**
 * @file dp_sysconfig.cpp
 * @brief 系统配置数据处理实现
 * @note LiveMap移植：系统配置管理
 */

#include "dataproc.h"
#include "dataproc_def.h"
#include "hal_def.h"
#include "esp_log.h"
#include "utils/datacenter/Account.h"
#include "utils/datacenter/DataCenter.h"
#include "config.h"  // 使用app组件的config.h
#include "bsp_ble.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "DP_SYSCONFIG";
static bool s_initialized = false;
static Account* s_sysconfig_account = nullptr;
static SysConfig_Info_t s_sysconfig = {};

static const char* NVS_NAMESPACE = "sss_cfg";
static const char* NVS_KEY_WEIGHT_KG = "weight_kg";
static const char* NVS_KEY_WHEEL_M = "wheel_m";

#define SYSCGF_STRCPY(dest, src) \
do{ \
   strncpy(dest, src, sizeof(dest)); \
   dest[sizeof(dest) - 1] = '\0'; \
}while(0)

static void DP_SysConfig_ApplyRuntimeSideEffects(void)
{
    // Apply user settings to modules that need runtime parameters.
    if (s_sysconfig.wheel_circumference_m > 0.0f) {
        bsp_ble_set_wheel_circumference(s_sysconfig.wheel_circumference_m);
    }
    if (s_sysconfig.weight_kg > 0.0f) {
        DP_Sport_SetWeight(s_sysconfig.weight_kg);
    }
}

static void DP_SysConfig_LoadFromNVS(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open readonly failed: %s", esp_err_to_name(err));
        return;
    }

    float weight = 0.0f;
    size_t len = sizeof(weight);
    err = nvs_get_blob(h, NVS_KEY_WEIGHT_KG, &weight, &len);
    if (err == ESP_OK && len == sizeof(weight)) {
        s_sysconfig.weight_kg = weight;
    }

    float wheel_m = 0.0f;
    len = sizeof(wheel_m);
    err = nvs_get_blob(h, NVS_KEY_WHEEL_M, &wheel_m, &len);
    if (err == ESP_OK && len == sizeof(wheel_m)) {
        s_sysconfig.wheel_circumference_m = wheel_m;
    }

    nvs_close(h);
}

static void DP_SysConfig_SaveToNVS(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open readwrite failed: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_blob(h, NVS_KEY_WEIGHT_KG, &s_sysconfig.weight_kg, sizeof(s_sysconfig.weight_kg));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS set weight failed: %s", esp_err_to_name(err));
    }

    err = nvs_set_blob(h, NVS_KEY_WHEEL_M, &s_sysconfig.wheel_circumference_m, sizeof(s_sysconfig.wheel_circumference_m));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS set wheel failed: %s", esp_err_to_name(err));
    }

    err = nvs_commit(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS commit failed: %s", esp_err_to_name(err));
    }
    nvs_close(h);
}

// Account事件回调
static int DP_SysConfig_OnEvent(Account* account, Account::EventParam_t* param)
{
    if (param->size != sizeof(SysConfig_Info_t))
    {
        return Account::RES_SIZE_MISMATCH;
    }

    SysConfig_Info_t* info = (SysConfig_Info_t*)param->data_p;

    switch (param->event)
    {
    case Account::EVENT_NOTIFY:
    {
        if (info->cmd == SYSCONFIG_CMD_LOAD)
        {
            DP_SysConfig_LoadFromNVS();
            DP_SysConfig_ApplyRuntimeSideEffects();
            ESP_LOGI(TAG, "SysConfig loaded: weight=%.1f kg wheel=%.3f m",
                     (double)s_sysconfig.weight_kg, (double)s_sysconfig.wheel_circumference_m);
        }
        else if (info->cmd == SYSCONFIG_CMD_SAVE)
        {
            // Merge values from notifier (UI) into the stored config.
            s_sysconfig.weight_kg = info->weight_kg;
            s_sysconfig.wheel_circumference_m = info->wheel_circumference_m;

            // 可以从GPS获取当前位置
            HAL::GPS_Info_t gpsInfo;
            if(account->Pull("GPS", &gpsInfo, sizeof(gpsInfo)) == Account::RES_OK)
            {
                if(gpsInfo.isVaild)
                {
                    s_sysconfig.longitude = (float)gpsInfo.longitude;
                    s_sysconfig.latitude = (float)gpsInfo.latitude;
                }
            }

            DP_SysConfig_SaveToNVS();
            DP_SysConfig_ApplyRuntimeSideEffects();
            ESP_LOGI(TAG, "SysConfig saved: weight=%.1f kg wheel=%.3f m",
                     (double)s_sysconfig.weight_kg, (double)s_sysconfig.wheel_circumference_m);
        }
    }
    break;
    case Account::EVENT_SUB_PULL:
    {
        memcpy(info, &s_sysconfig, sizeof(s_sysconfig));
    }
    break;
    default:
        return Account::RES_UNSUPPORTED_REQUEST;
    }

    return Account::RES_OK;
}

extern "C" {

void DP_SysConfig_Init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "SysConfig already initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing SysConfig DataProc...");
    
    // 获取DataCenter
    DataCenter* center = DataProc_GetDataCenter();
    if (!center) {
        ESP_LOGE(TAG, "DataCenter not initialized");
        return;
    }
    
    // 创建SysConfig Account
    s_sysconfig_account = new Account("SysConfig", center, 0);
    s_sysconfig_account->Subscribe("Storage");
    s_sysconfig_account->Subscribe("GPS");
    s_sysconfig_account->SetEventCallback(DP_SysConfig_OnEvent);
    
    // 初始化默认配置值
    memset(&s_sysconfig, 0, sizeof(s_sysconfig));
    
    s_sysconfig.cmd = SYSCONFIG_CMD_LOAD;
    s_sysconfig.longitude = CONFIG_GPS_LONGITUDE_DEFAULT;
    s_sysconfig.latitude = CONFIG_GPS_LATITUDE_DEFAULT;
    s_sysconfig.timeZone = CONFIG_SYSTEM_TIME_ZONE_DEFAULT;
    s_sysconfig.soundEnable = CONFIG_SYSTEM_SOUND_ENABLE_DEFAULT;
    SYSCGF_STRCPY(s_sysconfig.language, CONFIG_SYSTEM_LANGUAGE_DEFAULT);
    SYSCGF_STRCPY(s_sysconfig.arrowTheme, CONFIG_ARROW_THEME_DEFAULT);
    SYSCGF_STRCPY(s_sysconfig.mapDirPath, CONFIG_MAP_DIR_PATH_DEFAULT);
    SYSCGF_STRCPY(s_sysconfig.mapExtName, CONFIG_MAP_EXT_NAME_DEFAULT);
    s_sysconfig.mapWGS84 = CONFIG_MAP_USE_WGS84_DEFAULT;
    s_sysconfig.weight_kg = 70.0f;
    s_sysconfig.wheel_circumference_m = bsp_ble_get_wheel_circumference();
    
    s_initialized = true;
    ESP_LOGI(TAG, "SysConfig DataProc initialized");
}

void DP_SysConfig_RequestLoad(void)
{
    if (!s_initialized) {
        return;
    }
    // Load synchronously. Previously we used Notify("SysConfig") to trigger
    // the LOAD branch of OnEvent, but an Account cannot notify a publisher
    // it did not subscribe to, so DataCenter silently rejected the call
    // (observed as: "sub[SysConfig] was not subscribe pub[SysConfig]") and
    // the NVS-stored values were never applied on boot.
    DP_SysConfig_LoadFromNVS();
    DP_SysConfig_ApplyRuntimeSideEffects();
    ESP_LOGI(TAG, "SysConfig loaded: weight=%.1f kg wheel=%.3f m",
             (double)s_sysconfig.weight_kg,
             (double)s_sysconfig.wheel_circumference_m);
}

} // extern "C"
