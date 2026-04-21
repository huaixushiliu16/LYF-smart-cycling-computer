#include "RGBControlModel.h"
#include "dataproc.h"
#include "dataproc_def.h"
#include "Account.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "RGBControlModel";

namespace Page
{

void RGBControlModel::Init()
{
    ESP_LOGI(TAG, "Initializing RGBControlModel");
    
    // Get DataCenter and create Account for RGB subscription
    DataCenter* center = DataProc_GetDataCenter();
    if (center == nullptr) {
        ESP_LOGE(TAG, "DataCenter is nullptr, cannot create Account");
        return;
    }
    
    // Create Account for subscribing to RGB
    rgb_account = new Account("RGBControlModel", center, 0, this);
    if (rgb_account == nullptr) {
        ESP_LOGE(TAG, "Failed to create Account");
        return;
    }
    
    // Subscribe to RGB Account
    Account* rgb_pub = rgb_account->Subscribe("RGB");
    if (rgb_pub) {
        ESP_LOGI(TAG, "Subscribed to RGB Account");
    } else {
        ESP_LOGE(TAG, "Failed to subscribe to RGB Account");
    }
    
    // Subscribe to SportStatus Account (for heart rate data)
    sport_account = rgb_account->Subscribe("SportStatus");
    if (sport_account) {
        ESP_LOGI(TAG, "Subscribed to SportStatus Account");
    } else {
        ESP_LOGW(TAG, "Failed to subscribe to SportStatus Account");
    }
}

void RGBControlModel::Deinit()
{
    if (rgb_account) {
        delete rgb_account;
    }
    rgb_account = nullptr;
    sport_account = nullptr;
}

void RGBControlModel::GetRGBInfo(RGB_Info_t *info)
{
    if (rgb_account == nullptr || info == nullptr) {
        return;
    }
    
    int ret = rgb_account->Pull("RGB", info, sizeof(RGB_Info_t));
    if (ret != Account::RES_OK) {
        ESP_LOGW(TAG, "Failed to pull RGB info: %d", ret);
        // Set default values
        info->mode = RGB_MODE_OFF;
        info->brightness = 60;
        info->speed = 50;
        info->solid_color.r = 255;
        info->solid_color.g = 255;
        info->solid_color.b = 255;
        info->enabled = false;
    }
}

void RGBControlModel::SetMode(RGB_Mode_t mode)
{
    if (rgb_account == nullptr) {
        return;
    }
    
    RGB_Info_t info;
    GetRGBInfo(&info);
    info.mode = mode;
    
    int ret = rgb_account->Notify("RGB", &info, sizeof(RGB_Info_t));
    if (ret != Account::RES_OK) {
        ESP_LOGE(TAG, "Failed to set RGB mode: %d", ret);
    }
}

void RGBControlModel::SetBrightness(uint8_t brightness)
{
    if (rgb_account == nullptr) {
        return;
    }
    
    RGB_Info_t info;
    GetRGBInfo(&info);
    info.brightness = brightness;
    
    int ret = rgb_account->Notify("RGB", &info, sizeof(RGB_Info_t));
    if (ret != Account::RES_OK) {
        ESP_LOGE(TAG, "Failed to set RGB brightness: %d", ret);
    }
}

void RGBControlModel::SetSpeed(uint8_t speed)
{
    if (rgb_account == nullptr) {
        return;
    }
    
    RGB_Info_t info;
    GetRGBInfo(&info);
    info.speed = speed;
    
    int ret = rgb_account->Notify("RGB", &info, sizeof(RGB_Info_t));
    if (ret != Account::RES_OK) {
        ESP_LOGE(TAG, "Failed to set RGB speed: %d", ret);
    }
}

void RGBControlModel::SetEnabled(bool enabled)
{
    if (rgb_account == nullptr) {
        return;
    }
    
    RGB_Info_t info;
    GetRGBInfo(&info);
    info.enabled = enabled;
    
    int ret = rgb_account->Notify("RGB", &info, sizeof(RGB_Info_t));
    if (ret != Account::RES_OK) {
        ESP_LOGE(TAG, "Failed to set RGB enabled: %d", ret);
    }
}

void RGBControlModel::SetSolidColor(const RGB_Color_t *color)
{
    if (rgb_account == nullptr || color == nullptr) {
        return;
    }
    
    RGB_Info_t info;
    GetRGBInfo(&info);
    info.solid_color = *color;
    
    int ret = rgb_account->Notify("RGB", &info, sizeof(RGB_Info_t));
    if (ret != Account::RES_OK) {
        ESP_LOGE(TAG, "Failed to set RGB solid color: %d", ret);
    }
}

void RGBControlModel::GetHeartRate(uint16_t *heart_rate_bpm)
{
    if (rgb_account == nullptr || heart_rate_bpm == nullptr) {
        *heart_rate_bpm = 0;
        return;
    }
    
    SportStatus_Info_t sport_info = {};
    int ret = rgb_account->Pull("SportStatus", &sport_info, sizeof(SportStatus_Info_t));
    if (ret == Account::RES_OK) {
        *heart_rate_bpm = sport_info.heart_rate_bpm;
    } else {
        *heart_rate_bpm = 0;
    }
}

void RGBControlModel::SetStatusBarStyle(StatusBar_Style_t style)
{
    DataCenter* center = DataProc_GetDataCenter();
    if (!center) {
        return;
    }
    
    Account* account = DataProc_SearchAccount("StatusBar");
    if (account) {
        StatusBar_Info_t info;
        memset(&info, 0, sizeof(info));
        info.cmd = STATUS_BAR_CMD_SET_STYLE;
        info.param.style = style;
        account->Notify("StatusBar", &info, sizeof(info));
    }
}

void RGBControlModel::SetStatusBarAppear(bool en)
{
    Account* account = DataProc_SearchAccount("StatusBar");
    if (account) {
        StatusBar_Info_t info;
        memset(&info, 0, sizeof(info));
        info.cmd = STATUS_BAR_CMD_APPEAR;
        info.param.appear = en;
        account->Notify("StatusBar", &info, sizeof(info));
    }
}

}
