/**
 * @file bsp_lcd.cpp
 * @brief ILI9341 LCD驱动实现（基于LovyanGFX）
 * @note 参考version2项目，使用LovyanGFX替代esp_lcd_ili9341
 *       解决偏色、模糊、界面异常等显示问题
 */

#include "bsp_lcd.h"
#include "lgfx_sss.hpp"
#include "esp_log.h"

static const char *TAG = "BSP_LCD";

// LovyanGFX全局实例
static LGFX_SSS *s_lcd = nullptr;
static bool s_initialized = false;

esp_err_t bsp_lcd_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "LCD already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing LCD driver (LovyanGFX)...");
    ESP_LOGI(TAG, "SPI Host: SPI3_HOST");
    ESP_LOGI(TAG, "Pins: SCLK=%d, MOSI=%d, MISO=%d, CS=%d, DC=%d, RST=%d, BL=%d",
             BSP_LCD_PIN_SCLK, BSP_LCD_PIN_MOSI, BSP_LCD_PIN_MISO,
             BSP_LCD_PIN_CS, BSP_LCD_PIN_DC, BSP_LCD_PIN_RST, BSP_LCD_PIN_BL);

    // 创建LovyanGFX实例
    s_lcd = new LGFX_SSS();
    if (s_lcd == nullptr) {
        ESP_LOGE(TAG, "Failed to create LGFX instance");
        return ESP_ERR_NO_MEM;
    }

    // 初始化LCD
    if (!s_lcd->init()) {
        ESP_LOGE(TAG, "LGFX init failed");
        delete s_lcd;
        s_lcd = nullptr;
        return ESP_FAIL;
    }

    // 开启背光（LovyanGFX PWM）
    s_lcd->setBrightness(255);

    // 清屏为黑色，避免花屏
    s_lcd->fillScreen(0x0000);

    s_initialized = true;
    ESP_LOGI(TAG, "LCD driver (LovyanGFX) initialized successfully");
    return ESP_OK;
}

esp_err_t bsp_lcd_set_backlight(bool level)
{
    if (s_lcd == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    s_lcd->setBrightness(level ? 255 : 0);
    return ESP_OK;
}

esp_err_t bsp_lcd_deinit(void)
{
    if (s_lcd != nullptr) {
        delete s_lcd;
        s_lcd = nullptr;
    }
    s_initialized = false;
    ESP_LOGI(TAG, "LCD driver deinitialized");
    return ESP_OK;
}

void *bsp_lcd_get_lgfx(void)
{
    return (void *)s_lcd;
}
