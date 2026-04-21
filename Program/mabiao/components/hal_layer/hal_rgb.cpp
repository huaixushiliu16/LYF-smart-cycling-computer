/**
 * @file hal_rgb.cpp
 * @brief HAL layer RGB LED interface implementation
 * @note Phase 8: RGB LED control interface
 */

#include "hal.h"
#include "hal_rgb.h"
#include "bsp_rgb_led.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "HAL_RGB";

namespace HAL
{

static bool s_initialized = false;
static RGB_Info_t s_rgb_info;

void RGB_Init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "RGB already initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing RGB LED...");
    
    // 初始化默认值
    s_rgb_info.mode = BSP_RGB_MODE_OFF;
    s_rgb_info.brightness = 60;
    s_rgb_info.speed = 50;
    s_rgb_info.solid_color.r = 255;
    s_rgb_info.solid_color.g = 255;
    s_rgb_info.solid_color.b = 255;
    s_rgb_info.enabled = false;
    s_rgb_info.hr_sync_brightness_amplitude = 50;
    s_rgb_info.hr_sync_smooth_transition = true;
    
    // 初始化BSP层
    bsp_rgb_led_config_t config;
    config.gpio_num = 20;
    config.max_leds = 1;
    
    esp_err_t ret = bsp_rgb_led_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RGB BSP init failed: %s", esp_err_to_name(ret));
        return;
    }
    
    // 设置默认模式
    bsp_rgb_led_set_mode(BSP_RGB_MODE_OFF);
    bsp_rgb_led_set_brightness(s_rgb_info.brightness);
    bsp_rgb_led_set_speed(s_rgb_info.speed);
    
    s_initialized = true;
    ESP_LOGI(TAG, "RGB initialized");
}

void RGB_Update(void)
{
    if (!s_initialized) {
        return;
    }
    
    // BSP层在任务中自动更新，这里可以添加其他更新逻辑
    // 如果需要同步状态，可以在这里更新
}

bool RGB_GetInfo(RGB_Info_t *info)
{
    if (info == nullptr || !s_initialized) {
        return false;
    }
    
    *info = s_rgb_info;
    return true;
}

bool RGB_SetMode(RGB_Mode_t mode)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "RGB not initialized");
        return false;
    }
    
    if (mode >= BSP_RGB_MODE_MAX) {
        ESP_LOGE(TAG, "Invalid RGB mode: %d", mode);
        return false;
    }
    
    esp_err_t ret = bsp_rgb_led_set_mode((bsp_rgb_mode_t)mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set RGB mode: %s", esp_err_to_name(ret));
        return false;
    }
    
    s_rgb_info.mode = mode;
    
    // 如果模式不是OFF且已启用，启动RGB
    if (mode != BSP_RGB_MODE_OFF && s_rgb_info.enabled) {
        esp_err_t ret = bsp_rgb_led_start();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to start RGB LED: %s", esp_err_to_name(ret));
        }
    }
    
    return true;
}

bool RGB_SetBrightness(uint8_t brightness)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "RGB not initialized");
        return false;
    }
    
    if (brightness > 100) {
        brightness = 100;
    }
    
    esp_err_t ret = bsp_rgb_led_set_brightness(brightness);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set brightness: %s", esp_err_to_name(ret));
        return false;
    }
    
    s_rgb_info.brightness = brightness;
    return true;
}

bool RGB_SetSpeed(uint8_t speed)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "RGB not initialized");
        return false;
    }
    
    if (speed > 100) {
        speed = 100;
    }
    
    esp_err_t ret = bsp_rgb_led_set_speed(speed);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set speed: %s", esp_err_to_name(ret));
        return false;
    }
    
    s_rgb_info.speed = speed;
    return true;
}

bool RGB_SetHeartRate(uint16_t heart_rate_bpm)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "RGB not initialized");
        return false;
    }
    
    esp_err_t ret = bsp_rgb_led_set_heart_rate(heart_rate_bpm);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set heart rate: %s", esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

bool RGB_SetSolidColor(const RGB_Color_t *color)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "RGB not initialized");
        return false;
    }
    
    if (color == nullptr) {
        ESP_LOGE(TAG, "Invalid color pointer");
        return false;
    }
    
    bsp_rgb_color_t bsp_color = {color->r, color->g, color->b};
    esp_err_t ret = bsp_rgb_led_set_solid_color(&bsp_color);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set solid color: %s", esp_err_to_name(ret));
        return false;
    }
    
    s_rgb_info.solid_color = *color;
    return true;
}

bool RGB_SetEnabled(bool enabled)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "RGB not initialized");
        return false;
    }
    
    s_rgb_info.enabled = enabled;
    
    if (enabled) {
        // 如果模式不是OFF，启动RGB
        if (s_rgb_info.mode != BSP_RGB_MODE_OFF) {
            esp_err_t ret = bsp_rgb_led_start();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start RGB: %s", esp_err_to_name(ret));
                return false;
            }
        }
    } else {
        // 停止RGB
        bsp_rgb_led_stop();
    }
    
    return true;
}

} // namespace HAL

// C兼容接口实现
extern "C" {

void RGB_Init_C(void)
{
    HAL::RGB_Init();
}

void RGB_Update_C(void)
{
    HAL::RGB_Update();
}

bool RGB_GetInfo_C(RGB_Info_t_C *info)
{
    if (info == nullptr) {
        return false;
    }
    
    HAL::RGB_Info_t hal_info;
    if (!HAL::RGB_GetInfo(&hal_info)) {
        return false;
    }
    
    info->mode = hal_info.mode;
    info->brightness = hal_info.brightness;
    info->speed = hal_info.speed;
    info->solid_color.r = hal_info.solid_color.r;
    info->solid_color.g = hal_info.solid_color.g;
    info->solid_color.b = hal_info.solid_color.b;
    info->enabled = hal_info.enabled;
    info->hr_sync_brightness_amplitude = hal_info.hr_sync_brightness_amplitude;
    info->hr_sync_smooth_transition = hal_info.hr_sync_smooth_transition;
    
    return true;
}

bool RGB_SetMode_C(uint8_t mode)
{
    return HAL::RGB_SetMode((HAL::RGB_Mode_t)mode);
}

bool RGB_SetBrightness_C(uint8_t brightness)
{
    return HAL::RGB_SetBrightness(brightness);
}

bool RGB_SetSpeed_C(uint8_t speed)
{
    return HAL::RGB_SetSpeed(speed);
}

bool RGB_SetHeartRate_C(uint16_t heart_rate_bpm)
{
    return HAL::RGB_SetHeartRate(heart_rate_bpm);
}

bool RGB_SetSolidColor_C(uint8_t r, uint8_t g, uint8_t b)
{
    HAL::RGB_Color_t color = {r, g, b};
    return HAL::RGB_SetSolidColor(&color);
}

bool RGB_SetEnabled_C(bool enabled)
{
    return HAL::RGB_SetEnabled(enabled);
}

} // extern "C"
