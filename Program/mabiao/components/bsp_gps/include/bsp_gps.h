/**
 * @file bsp_gps.h
 * @brief GPS驱动头文件（BSP层）
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
 * @brief GPS初始化配置结构体
 */
typedef struct {
    uint32_t baud_rate;  // 波特率（默认9600）
    uint8_t uart_num;    // UART编号（默认UART_NUM_2）
} bsp_gps_config_t;

/**
 * @brief GPS数据信息结构体
 */
typedef struct {
    double longitude;   // 经度
    double latitude;    // 纬度
    float altitude;     // 海拔
    float course;       // 航向
    float speed;        // 速度（km/h）
    int16_t satellites; // 卫星数量
    bool is_valid;      // 数据是否有效
    // UTC时间
    uint16_t year;      // 年份
    uint8_t month;      // 月份
    uint8_t day;        // 日期
    uint8_t hour;       // 小时
    uint8_t minute;     // 分钟
    uint8_t second;     // 秒
} bsp_gps_data_t;

/**
 * @brief 初始化GPS驱动
 * 
 * @param config GPS配置参数（可为NULL使用默认配置）
 * @return esp_err_t 
 */
esp_err_t bsp_gps_init(const bsp_gps_config_t *config);

/**
 * @brief 读取GPS数据
 * 
 * @param data 输出参数，返回GPS数据
 * @return esp_err_t 
 */
esp_err_t bsp_gps_read(bsp_gps_data_t *data);

/**
 * @brief 更新GPS数据（轮询方式）
 * 
 * @return esp_err_t 
 */
esp_err_t bsp_gps_update(void);

/**
 * @brief 反初始化GPS驱动
 * 
 * @return esp_err_t 
 */
esp_err_t bsp_gps_deinit(void);

#ifdef __cplusplus
}
#endif
