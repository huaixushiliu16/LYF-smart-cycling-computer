/**
 * @file bsp_buzzer.h
 * @brief 无源蜂鸣器（GPIO19，原理图经 NPN 低端开关，高电平导通）
 * @note  使用 LEDC 输出约 4kHz 方波；提示音在独立任务中播放，避免阻塞 BLE 回调。
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

typedef enum {
    BSP_BUZZ_PATTERN_CONNECT = 0,   /**< 连接成功：一声短促 */
    BSP_BUZZ_PATTERN_DISCONNECT, /**< 断开：两声短促 */
} bsp_buzz_pattern_t;

/**
 * @brief 初始化蜂鸣器 PWM（可重复调用，已初始化则直接返回 ESP_OK）
 */
esp_err_t bsp_buzzer_init(void);

/**
 * @brief 请求播放提示（非阻塞；若队列满则丢弃）
 */
void bsp_buzzer_request(bsp_buzz_pattern_t pattern);
