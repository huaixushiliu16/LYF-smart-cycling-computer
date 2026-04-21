/**
 * @file hal_touch.cpp
 * @brief Touch HAL封装实现
 * @note 阶段2.5：软件分层架构搭建
 */

#include "hal.h"
#include "bsp_touch.h"
#include "esp_log.h"

static const char *TAG = "HAL_TOUCH";
static bool s_initialized = false;

namespace HAL
{

void Touch_Init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Touch HAL already initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing Touch HAL...");
    
    // 注意：BSP层Touch驱动应该在HAL层初始化之前由main.c初始化
    // 这里只标记HAL层已初始化，不重复初始化BSP层
    // 如果BSP层未初始化，后续的HAL操作可能会失败
    
    s_initialized = true;
    ESP_LOGI(TAG, "Touch HAL initialized");
}

void Touch_Update(void)
{
    // Touch更新操作（如果需要）
    // 目前Touch驱动不需要定期更新
}

bool Touch_GetInfo(Touch_Info_t *info)
{
    if (!s_initialized || info == nullptr) {
        return false;
    }
    
    bsp_touch_point_t point;
    esp_err_t ret = bsp_touch_read(&point);
    if (ret != ESP_OK) {
        return false;
    }
    
    info->x = point.x;
    info->y = point.y;
    info->pressed = point.pressed;
    
    return true;
}

} // namespace HAL
