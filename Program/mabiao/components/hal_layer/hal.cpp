/**
 * @file hal.cpp
 * @brief HAL层主实现
 * @note 阶段2.5：软件分层架构搭建
 */

#include "hal.h"
#include "esp_log.h"

static const char *TAG = "HAL";

namespace HAL
{

void HAL_Init(void)
{
    ESP_LOGI(TAG, "Initializing HAL layer...");
    
    // 初始化各HAL模块
    LCD_Init();
    Touch_Init();
    GPS_Init();
    IMU_Init();
    Power_Init();
    RGB_Init();  // RGB LED初始化
    // BLE_Init();  // BLE默认不初始化，只有在BLE界面启用时才会初始化
    
    ESP_LOGI(TAG, "HAL layer initialized");
}

void HAL_Update(void)
{
    // 更新各HAL模块
    LCD_Update();
    Touch_Update();
    GPS_Update();
    IMU_Update();
    Power_Update();
    RGB_Update();
    BLE_Update();
}

} // namespace HAL

// C兼容接口实现
extern "C" {

void HAL_Init_C(void)
{
    HAL::HAL_Init();
}

void HAL_Update_C(void)
{
    HAL::HAL_Update();
}

} // extern "C"