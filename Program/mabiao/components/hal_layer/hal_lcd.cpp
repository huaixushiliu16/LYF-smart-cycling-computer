/**
 * @file hal_lcd.cpp
 * @brief LCD HAL封装实现
 * @note 阶段2.5：软件分层架构搭建
 */

#include "hal.h"
#include "bsp_lcd.h"
#include "esp_log.h"

static const char *TAG = "HAL_LCD";
static bool s_initialized = false;

namespace HAL
{

void LCD_Init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "LCD HAL already initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing LCD HAL...");
    
    // 注意：BSP层LCD驱动应该在HAL层初始化之前由main.c初始化
    // 这里只标记HAL层已初始化，不重复初始化BSP层
    // 如果BSP层未初始化，后续的HAL操作可能会失败
    
    s_initialized = true;
    ESP_LOGI(TAG, "LCD HAL initialized");
}

void LCD_Update(void)
{
    // LCD更新操作（如果需要）
    // 目前LCD驱动不需要定期更新
}

} // namespace HAL
