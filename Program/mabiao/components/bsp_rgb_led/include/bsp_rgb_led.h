/**
 * @file bsp_rgb_led.h
 * @brief RGB LED驱动头文件（BSP层）
 * @note 默认 GPIO20（RGB_DIN，与硬件原理图一致）
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RGB LED初始化配置结构体
 */
typedef struct {
    uint8_t gpio_num;      // GPIO引脚号（默认20）
    uint8_t max_leds;       // LED数量（默认1）
} bsp_rgb_led_config_t;

/**
 * @brief RGB颜色结构体
 */
typedef struct {
    uint8_t r;  // 红色分量 (0-255)
    uint8_t g;  // 绿色分量 (0-255)
    uint8_t b;  // 蓝色分量 (0-255)
} bsp_rgb_color_t;

/**
 * @brief RGB模式枚举
 */
typedef enum {
    BSP_RGB_MODE_OFF = 0,           // 关闭
    BSP_RGB_MODE_RAINBOW,           // 彩虹渐变
    BSP_RGB_MODE_LIGHTNING,         // 闪电闪烁
    BSP_RGB_MODE_FIRE,              // 火焰效果
    BSP_RGB_MODE_STARRY,            // 星空闪烁
    BSP_RGB_MODE_WAVE,              // 波浪效果
    BSP_RGB_MODE_FIREWORKS,         // 烟花爆炸
    BSP_RGB_MODE_HEARTBEAT,         // 心跳效果（固定频率）
    BSP_RGB_MODE_HEART_RATE_SYNC,   // 心率同步（实时心率数据）
    BSP_RGB_MODE_SPIRAL,            // 螺旋旋转
    BSP_RGB_MODE_COLOR_BLOCK,       // 色块跳跃
    BSP_RGB_MODE_AURORA,            // 极光效果
    BSP_RGB_MODE_SOLID_COLOR,       // 纯色模式
    BSP_RGB_MODE_MAX
} bsp_rgb_mode_t;

/**
 * @brief 初始化RGB LED驱动
 * 
 * @param config 配置参数（可为NULL使用默认配置：GPIO 20, 1个LED）
 * @return esp_err_t 
 */
esp_err_t bsp_rgb_led_init(const bsp_rgb_led_config_t *config);

/**
 * @brief 反初始化RGB LED驱动
 * 
 * @return esp_err_t 
 */
esp_err_t bsp_rgb_led_deinit(void);

/**
 * @brief 启动RGB LED渐变色效果（自动运行）
 * 
 * @return esp_err_t 
 */
esp_err_t bsp_rgb_led_start(void);

/**
 * @brief 停止RGB LED效果
 * 
 * @return esp_err_t 
 */
esp_err_t bsp_rgb_led_stop(void);

/**
 * @brief 设置RGB LED颜色（手动控制）
 * 
 * @param color RGB颜色结构体
 * @return esp_err_t 
 */
esp_err_t bsp_rgb_led_set_color(const bsp_rgb_color_t *color);

/**
 * @brief 清除RGB LED（关闭）
 * 
 * @return esp_err_t 
 */
esp_err_t bsp_rgb_led_clear(void);

/**
 * @brief 更新RGB LED效果（需要在任务中周期性调用）
 * 
 * @return esp_err_t 
 */
esp_err_t bsp_rgb_led_update(void);

/**
 * @brief 设置RGB LED模式
 * 
 * @param mode RGB模式
 * @return esp_err_t 
 */
esp_err_t bsp_rgb_led_set_mode(bsp_rgb_mode_t mode);

/**
 * @brief 设置RGB LED亮度
 * 
 * @param brightness 亮度 (0-100)
 * @return esp_err_t 
 */
esp_err_t bsp_rgb_led_set_brightness(uint8_t brightness);

/**
 * @brief 设置RGB LED速度
 * 
 * @param speed 速度 (0-100)
 * @return esp_err_t 
 */
esp_err_t bsp_rgb_led_set_speed(uint8_t speed);

/**
 * @brief 设置心率同步模式的心率值
 * 
 * @param heart_rate_bpm 心率值 (bpm)
 * @return esp_err_t 
 */
esp_err_t bsp_rgb_led_set_heart_rate(uint16_t heart_rate_bpm);

/**
 * @brief 设置纯色模式的颜色
 * 
 * @param color RGB颜色
 * @return esp_err_t 
 */
esp_err_t bsp_rgb_led_set_solid_color(const bsp_rgb_color_t *color);

#ifdef __cplusplus
}
#endif
