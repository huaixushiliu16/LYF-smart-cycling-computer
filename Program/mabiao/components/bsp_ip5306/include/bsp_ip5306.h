/**
 * @file bsp_ip5306.h
 * @brief IP5306电源管理驱动头文件（BSP层）
 * @note 阶段6：电源驱动开发
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化IP5306周期激活功能
 * 
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t bsp_ip5306_init(void);

/**
 * @brief 启动IP5306周期激活
 * 启动后，每10秒自动触发一次激活脉冲
 */
void bsp_ip5306_start(void);

/**
 * @brief 停止IP5306周期激活
 */
void bsp_ip5306_stop(void);

/**
 * @brief 手动触发一次IP5306激活脉冲（用于测试）
 */
void bsp_ip5306_trigger(void);

#ifdef __cplusplus
}
#endif
