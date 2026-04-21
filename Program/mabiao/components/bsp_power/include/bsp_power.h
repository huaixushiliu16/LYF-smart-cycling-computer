/**
 * @file bsp_power.h
 * @brief 电源管理驱动头文件（BSP层）
 * @note 阶段2.5：软件分层架构搭建
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 电源初始化配置结构体
 * @note 当前实现使用编译期宏指定引脚与通道，该结构体字段保留用于未来扩展，可传 NULL。
 */
typedef struct {
    uint8_t adc_channel;  // 保留：当前使用 ADC2_CH3 (GPIO14)
    uint8_t ctrl_pin;     // 保留：当前使用 GPIO13
} bsp_power_config_t;

/**
 * @brief 电源信息结构体
 */
typedef struct {
    uint16_t voltage_mv;       // 电池电压（mV，已滤波）
    uint8_t battery_percent;   // 电量百分比（0-100，已防抖）
    bool is_charging;          // 是否充电中（当前硬件未接 STAT，恒为 false）
} bsp_power_info_t;

/**
 * @brief 初始化电源管理驱动
 *
 * @param config 电源配置参数（当前实现忽略，可传 NULL）
 * @return esp_err_t
 */
esp_err_t bsp_power_init(const bsp_power_config_t *config);

/**
 * @brief 采样一次并返回电源信息（阻塞 ~70ms）
 *        注意：此调用会实际驱动 GPIO13 使能分压 + 做 32 次 ADC 采样，
 *        适合在后台低频任务调用。UI/订阅者请改用 bsp_power_get_cached()。
 *
 * @param info 输出参数
 * @return esp_err_t
 */
esp_err_t bsp_power_read(bsp_power_info_t *info);

/**
 * @brief 触发一次采样并更新内部缓存（等价于 bsp_power_read 但丢弃返回值）
 *
 * @return esp_err_t
 */
esp_err_t bsp_power_update(void);

/**
 * @brief 读取最近一次缓存的电源信息（无阻塞，不触发硬件采样）
 *
 * @param info 输出参数
 * @return esp_err_t
 */
esp_err_t bsp_power_get_cached(bsp_power_info_t *info);

/**
 * @brief 反初始化电源管理驱动
 *
 * @return esp_err_t
 */
esp_err_t bsp_power_deinit(void);

#ifdef __cplusplus
}
#endif
