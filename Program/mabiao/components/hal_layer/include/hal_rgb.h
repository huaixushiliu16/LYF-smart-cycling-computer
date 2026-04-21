/**
 * @file hal_rgb.h
 * @brief HAL layer RGB LED interface definition
 * @note Phase 8: RGB LED control interface
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "bsp_rgb_led.h"

#ifdef __cplusplus
namespace HAL
{
    /**
     * @brief RGB模式枚举（与BSP层保持一致）
     */
    typedef bsp_rgb_mode_t RGB_Mode_t;
    
    /**
     * @brief RGB颜色结构体（与BSP层保持一致）
     */
    typedef bsp_rgb_color_t RGB_Color_t;
    
    /**
     * @brief RGB信息结构体
     */
    typedef struct {
        RGB_Mode_t mode;            // 当前模式
        uint8_t brightness;         // 亮度 (0-100)
        uint8_t speed;              // 速度 (0-100)
        RGB_Color_t solid_color;    // 纯色模式的颜色
        bool enabled;                // 是否启用
        // 心率同步模式专用参数
        uint8_t hr_sync_brightness_amplitude; // 心率同步亮度幅度 (0-100)
        bool hr_sync_smooth_transition;       // 心率同步平滑过渡
    } RGB_Info_t;
    
    /**
     * @brief 初始化RGB LED
     */
    void RGB_Init(void);
    
    /**
     * @brief 更新RGB LED状态
     */
    void RGB_Update(void);
    
    /**
     * @brief 获取RGB LED信息
     * 
     * @param info RGB信息结构体指针
     * @return true 成功
     * @return false 失败
     */
    bool RGB_GetInfo(RGB_Info_t *info);
    
    /**
     * @brief 设置RGB模式
     * 
     * @param mode RGB模式
     * @return true 成功
     * @return false 失败
     */
    bool RGB_SetMode(RGB_Mode_t mode);
    
    /**
     * @brief 设置RGB亮度
     * 
     * @param brightness 亮度 (0-100)
     * @return true 成功
     * @return false 失败
     */
    bool RGB_SetBrightness(uint8_t brightness);
    
    /**
     * @brief 设置RGB速度
     * 
     * @param speed 速度 (0-100)
     * @return true 成功
     * @return false 失败
     */
    bool RGB_SetSpeed(uint8_t speed);
    
    /**
     * @brief 设置心率同步模式的心率值
     * 
     * @param heart_rate_bpm 心率值 (bpm)
     * @return true 成功
     * @return false 失败
     */
    bool RGB_SetHeartRate(uint16_t heart_rate_bpm);
    
    /**
     * @brief 设置纯色模式的颜色
     * 
     * @param color RGB颜色
     * @return true 成功
     * @return false 失败
     */
    bool RGB_SetSolidColor(const RGB_Color_t *color);
    
    /**
     * @brief 启用/禁用RGB LED
     * 
     * @param enabled true启用，false禁用
     * @return true 成功
     * @return false 失败
     */
    bool RGB_SetEnabled(bool enabled);
    
} // namespace HAL

// C兼容接口
typedef HAL::RGB_Info_t RGB_Info_t_C;

extern "C" {
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
// C语言环境下的接口声明
#include <stdbool.h>
#include <stdint.h>

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
