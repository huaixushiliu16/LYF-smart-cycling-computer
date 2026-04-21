/**
 * @file hal.h
 * @brief HAL layer main interface definition
 * @note Phase 2.5: Software layered architecture setup
 */

#ifndef __HAL_H
#define __HAL_H

#ifdef __cplusplus
#include "hal_def.h"
#include "hal_rgb.h"  // 必须在hal_def.h之后，因为需要bsp_rgb_led.h

namespace HAL
{
    // 初始化所有HAL模块
    void HAL_Init(void);
    
    // 更新所有HAL模块（可选）
    void HAL_Update(void);
    
    // LCD接口
    void LCD_Init(void);
    void LCD_Update(void);
    
    // Touch接口
    void Touch_Init(void);
    void Touch_Update(void);
    bool Touch_GetInfo(Touch_Info_t *info);
    
    // GPS接口
    void GPS_Init(void);
    void GPS_Update(void);
    bool GPS_GetInfo(GPS_Info_t *info);
    
    // IMU接口
    void IMU_Init(void);
    void IMU_Update(void);
    bool IMU_GetInfo(IMU_Info_t *info);
    
    // Power接口
    void Power_Init(void);
    void Power_Update(void);
    bool Power_GetInfo(Power_Info_t *info);
    
    // BLE接口
    void BLE_Init(void);
    void BLE_Update(void);
    bool BLE_GetInfo(BLE_Info_t *info);
    
    // RGB接口
    void RGB_Init(void);
    void RGB_Update(void);
    bool RGB_GetInfo(RGB_Info_t *info);
    bool RGB_SetMode(RGB_Mode_t mode);
    bool RGB_SetBrightness(uint8_t brightness);
    bool RGB_SetSpeed(uint8_t speed);
    bool RGB_SetHeartRate(uint16_t heart_rate_bpm);
    bool RGB_SetSolidColor(const RGB_Color_t *color);
    bool RGB_SetEnabled(bool enabled);
    
}

// C兼容接口（供C代码调用）
// 在extern "C"块之前定义类型别名，以便在函数声明中使用
typedef HAL::GPS_Info_t GPS_Info_t_C;
typedef HAL::IMU_Info_t IMU_Info_t_C;
typedef HAL::Power_Info_t Power_Info_t_C;
typedef HAL::BLE_Info_t BLE_Info_t_C;
// RGB_Info_t_C 在 hal_rgb.h 中定义

extern "C" {
    // 初始化所有HAL模块（C接口）
    void HAL_Init_C(void);
    
    // 更新所有HAL模块（C接口）
    void HAL_Update_C(void);
    
    // GPS C接口
    void GPS_Update_C(void);
    bool GPS_GetInfo_C(GPS_Info_t_C *info);
    
    // IMU C接口
    void IMU_Update_C(void);
    bool IMU_GetInfo_C(IMU_Info_t_C *info);
    
    // Power C接口
    bool Power_GetInfo_C(Power_Info_t_C *info);
    
    // BLE C接口
    void BLE_Init_C(void);
    void BLE_Update_C(void);
    bool BLE_GetInfo_C(BLE_Info_t_C *info);
    
    // RGB C接口
    void RGB_Init_C(void);
    void RGB_Update_C(void);
    bool RGB_GetInfo_C(RGB_Info_t_C *info);
    bool RGB_SetMode_C(uint8_t mode);
    bool RGB_SetBrightness_C(uint8_t brightness);
    bool RGB_SetSpeed_C(uint8_t speed);
    bool RGB_SetHeartRate_C(uint16_t heart_rate_bpm);
    bool RGB_SetSolidColor_C(uint8_t r, uint8_t g, uint8_t b);
    bool RGB_SetEnabled_C(bool enabled);
}

#else
// C语言环境下，只提供C接口声明
#include <stdbool.h>
#include <stdint.h>
#include "hal_def.h"

// RGB类型定义（与hal_rgb.h中的C语言部分保持一致）
typedef struct {
    uint8_t r, g, b;
} RGB_Color_t_C;

typedef struct {
    uint8_t mode;
    uint8_t brightness;
    uint8_t speed;
    RGB_Color_t_C solid_color;
    bool enabled;
    uint8_t hr_sync_brightness_amplitude;
    bool hr_sync_smooth_transition;
} RGB_Info_t_C;

// C兼容接口
void HAL_Init_C(void);
void HAL_Update_C(void);

// GPS C接口
void GPS_Update_C(void);
bool GPS_GetInfo_C(GPS_Info_t *info);

// IMU C接口
void IMU_Update_C(void);
bool IMU_GetInfo_C(IMU_Info_t *info);

// Power C接口
bool Power_GetInfo_C(Power_Info_t *info);

// BLE C接口
void BLE_Init_C(void);
void BLE_Update_C(void);
bool BLE_GetInfo_C(BLE_Info_t *info);

// RGB C接口
void RGB_Init_C(void);
void RGB_Update_C(void);
bool RGB_GetInfo_C(RGB_Info_t_C *info);
bool RGB_SetMode_C(uint8_t mode);
bool RGB_SetBrightness_C(uint8_t brightness);
bool RGB_SetSpeed_C(uint8_t speed);
bool RGB_SetHeartRate_C(uint16_t heart_rate_bpm);
bool RGB_SetSolidColor_C(uint8_t r, uint8_t g, uint8_t b);
bool RGB_SetEnabled_C(bool enabled);
#endif

#endif /* __HAL_H */
