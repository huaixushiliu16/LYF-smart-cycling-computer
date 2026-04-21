/**
 * @file dp_rgb.cpp
 * @brief DataProc layer RGB LED data processing implementation
 * @note Phase 8: RGB LED control with Account mode
 */

#include "dataproc.h"
#include "hal.h"
#include "esp_log.h"
#include "utils/datacenter/Account.h"
#include "utils/datacenter/DataCenter.h"
#include <string.h>

static const char *TAG = "DP_RGB";
static bool s_initialized = false;
static Account* s_rgb_account = nullptr;
static RGB_Info_t s_rgb_info = {
    RGB_MODE_OFF,  // mode
    60,            // brightness
    50,            // speed
    {255, 255, 255}, // solid_color
    false          // enabled
};

// RGB Account事件回调函数
static int DP_RGB_OnEvent(Account* account, Account::EventParam_t* param)
{
    if (param->event == Account::EVENT_TIMER)
    {
        DP_RGB_Update();
        return Account::RES_OK;
    }
    
    if (param->event == Account::EVENT_SUB_PULL)
    {
        RGB_Info_t* info = (RGB_Info_t*)param->data_p;
        if (param->size != sizeof(RGB_Info_t))
        {
            return Account::RES_SIZE_MISMATCH;
        }
        *info = s_rgb_info;
        return Account::RES_OK;
    }
    
    if (param->event == Account::EVENT_PUB_PUBLISH)
    {
        // 检查是否来自SportStatus Account
        if (param->tran && strcmp(param->tran->ID, "SportStatus") == 0)
        {
            // 收到SportStatus数据更新
            SportStatus_Info_t* sport_info = (SportStatus_Info_t*)param->data_p;
            if (param->size == sizeof(SportStatus_Info_t))
            {
                // 如果当前是心率同步模式，更新心率值
                if (s_rgb_info.mode == RGB_MODE_HEART_RATE_SYNC && s_rgb_info.enabled)
                {
                    uint16_t heart_rate = sport_info->heart_rate_bpm;
                    
                    // 数据有效性检查
                    if (heart_rate >= 30 && heart_rate <= 220)
                    {
                        HAL::RGB_SetHeartRate(heart_rate);
                        ESP_LOGD(TAG, "Heart rate updated: %d bpm", heart_rate);
                    }
                    else
                    {
                        // 无效心率，设置为0
                        HAL::RGB_SetHeartRate(0);
                        ESP_LOGW(TAG, "Invalid heart rate: %d bpm", heart_rate);
                    }
                }
            }
            return Account::RES_OK;
        }
    }
    
    if (param->event == Account::EVENT_NOTIFY)
    {
        // 接收控制命令
        if (param->size == sizeof(RGB_Info_t))
        {
            RGB_Info_t* cmd = (RGB_Info_t*)param->data_p;
            
            // 应用设置
            if (cmd->mode < RGB_MODE_MAX) {
                s_rgb_info.mode = cmd->mode;
                HAL::RGB_SetMode((HAL::RGB_Mode_t)cmd->mode);
            }
            
            s_rgb_info.brightness = cmd->brightness;
            HAL::RGB_SetBrightness(cmd->brightness);
            
            s_rgb_info.speed = cmd->speed;
            HAL::RGB_SetSpeed(cmd->speed);
            
            s_rgb_info.solid_color = cmd->solid_color;
            HAL::RGB_SetSolidColor((HAL::RGB_Color_t*)&cmd->solid_color);
            
            s_rgb_info.enabled = cmd->enabled;
            HAL::RGB_SetEnabled(cmd->enabled);
            
            // 发布更新后的状态
            s_rgb_account->Commit(&s_rgb_info, sizeof(s_rgb_info));
            s_rgb_account->Publish();
            
            ESP_LOGI(TAG, "RGB settings updated: mode=%d, brightness=%d, speed=%d, enabled=%d",
                     s_rgb_info.mode, s_rgb_info.brightness, s_rgb_info.speed, s_rgb_info.enabled);
            
            return Account::RES_OK;
        }
    }
    
    return Account::RES_UNSUPPORTED_REQUEST;
}

extern "C" {

void DP_RGB_Init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "DP_RGB already initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing RGB DataProc...");
    
    // Get DataCenter
    DataCenter* center = DataProc_GetDataCenter();
    if (!center) {
        ESP_LOGE(TAG, "DataCenter not initialized");
        return;
    }
    
    // Create RGB Account
    s_rgb_account = new Account("RGB", center, sizeof(RGB_Info_t));
    s_rgb_account->SetEventCallback(DP_RGB_OnEvent);
    s_rgb_account->SetTimerPeriod(50);  // 50ms更新周期（用于RGB效果更新）
    s_rgb_account->SetTimerEnable(true);
    
    // 订阅SportStatus Account以获取心率数据
    Account* sport_account = s_rgb_account->Subscribe("SportStatus");
    if (sport_account) {
        ESP_LOGI(TAG, "Subscribed to SportStatus Account for heart rate data");
    } else {
        ESP_LOGW(TAG, "Failed to subscribe to SportStatus Account");
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "RGB DataProc initialized");
}

void DP_RGB_Update(void)
{
    if (!s_initialized) {
        return;
    }
    
    // 更新HAL层
    HAL::RGB_Update();
    
    // 获取当前RGB状态
    HAL::RGB_Info_t hal_info;
    if (HAL::RGB_GetInfo(&hal_info)) {
        // 同步状态到DataProc层
        s_rgb_info.mode = (RGB_Mode_t)hal_info.mode;
        s_rgb_info.brightness = hal_info.brightness;
        s_rgb_info.speed = hal_info.speed;
        s_rgb_info.solid_color.r = hal_info.solid_color.r;
        s_rgb_info.solid_color.g = hal_info.solid_color.g;
        s_rgb_info.solid_color.b = hal_info.solid_color.b;
        s_rgb_info.enabled = hal_info.enabled;
        
        // 发布数据到Account
        if (s_rgb_account) {
            s_rgb_account->Commit(&s_rgb_info, sizeof(s_rgb_info));
            s_rgb_account->Publish();
        }
    }
}

bool DP_RGB_GetInfo(RGB_Info_t *info)
{
    if (!s_initialized || info == nullptr) {
        return false;
    }
    
    *info = s_rgb_info;
    return true;
}

bool DP_RGB_SetMode(RGB_Mode_t mode)
{
    if (!s_initialized) {
        return false;
    }
    
    if (mode >= RGB_MODE_MAX) {
        ESP_LOGE(TAG, "Invalid RGB mode: %d", mode);
        return false;
    }
    
    s_rgb_info.mode = mode;
    bool ret = HAL::RGB_SetMode((HAL::RGB_Mode_t)mode);
    
    if (ret) {
        // 发布更新
        if (s_rgb_account) {
            s_rgb_account->Commit(&s_rgb_info, sizeof(s_rgb_info));
            s_rgb_account->Publish();
        }
    }
    
    return ret;
}

bool DP_RGB_SetBrightness(uint8_t brightness)
{
    if (!s_initialized) {
        return false;
    }
    
    s_rgb_info.brightness = brightness;
    bool ret = HAL::RGB_SetBrightness(brightness);
    
    if (ret && s_rgb_account) {
        s_rgb_account->Commit(&s_rgb_info, sizeof(s_rgb_info));
        s_rgb_account->Publish();
    }
    
    return ret;
}

bool DP_RGB_SetSpeed(uint8_t speed)
{
    if (!s_initialized) {
        return false;
    }
    
    s_rgb_info.speed = speed;
    bool ret = HAL::RGB_SetSpeed(speed);
    
    if (ret && s_rgb_account) {
        s_rgb_account->Commit(&s_rgb_info, sizeof(s_rgb_info));
        s_rgb_account->Publish();
    }
    
    return ret;
}

bool DP_RGB_SetEnabled(bool enabled)
{
    if (!s_initialized) {
        return false;
    }
    
    s_rgb_info.enabled = enabled;
    bool ret = HAL::RGB_SetEnabled(enabled);
    
    if (ret && s_rgb_account) {
        s_rgb_account->Commit(&s_rgb_info, sizeof(s_rgb_info));
        s_rgb_account->Publish();
    }
    
    return ret;
}

bool DP_RGB_SetSolidColor(const RGB_Color_t *color)
{
    if (!s_initialized || color == nullptr) {
        return false;
    }
    
    s_rgb_info.solid_color = *color;
    HAL::RGB_Color_t hal_color = {color->r, color->g, color->b};
    bool ret = HAL::RGB_SetSolidColor(&hal_color);
    
    if (ret && s_rgb_account) {
        s_rgb_account->Commit(&s_rgb_info, sizeof(s_rgb_info));
        s_rgb_account->Publish();
    }
    
    return ret;
}

} // extern "C"
