/**
 * @file lvgl_port.h
 * @brief LVGL显示和输入设备适配器头文件（基于LovyanGFX）
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化LVGL端口适配器（基于LovyanGFX）
 * @note 调用前必须先调用bsp_lcd_init()初始化LCD
 * @return esp_err_t 
 */
esp_err_t lvgl_port_init(void);

/**
 * @brief 反初始化LVGL端口适配器
 * 
 * @return esp_err_t 
 */
esp_err_t lvgl_port_deinit(void);

/**
 * @brief 获取LVGL互斥锁（线程安全）
 * 
 * @param timeout_ms 超时时间（毫秒），-1表示永久等待
 * @return true 成功获取锁
 * @return false 获取锁失败
 */
bool lvgl_port_lock(int timeout_ms);

/**
 * @brief 释放LVGL互斥锁
 */
void lvgl_port_unlock(void);

#ifdef __cplusplus
}
#endif
