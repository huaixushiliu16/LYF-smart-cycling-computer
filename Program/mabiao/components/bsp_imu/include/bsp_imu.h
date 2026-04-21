/**
 * @file bsp_imu.h
 * @brief IMU驱动头文件（BSP层）
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
 * @brief IMU初始化配置结构体
 */
typedef struct {
    uint32_t baud_rate;  // 波特率（默认115200）
    uint8_t uart_num;    // UART编号（默认UART_NUM_1）
} bsp_imu_config_t;

/**
 * @brief IMU数据信息结构体（十轴数据）
 */
typedef struct {
    int16_t ax;      // X轴加速度
    int16_t ay;      // Y轴加速度
    int16_t az;      // Z轴加速度
    int16_t gx;      // X轴角速度
    int16_t gy;      // Y轴角速度
    int16_t gz;      // Z轴角速度
    float roll;      // 横滚角
    float pitch;     // 俯仰角
    float yaw;       // 偏航角
    // 磁力计数据（μT）
    float mx;
    float my;
    float mz;
    // 四元数
    float q0;
    float q1;
    float q2;
    float q3;
    // 环境数据
    float temperature;  // 温度（°C）
    float pressure;     // 气压（Pa）
    float height;        // 高度（m）
    int16_t steps;   // 步数
} bsp_imu_data_t;

/**
 * @brief 初始化IMU驱动
 * 
 * @param config IMU配置参数（可为NULL使用默认配置）
 * @return esp_err_t 
 */
esp_err_t bsp_imu_init(const bsp_imu_config_t *config);

/**
 * @brief 读取IMU数据
 * 
 * @param data 输出参数，返回IMU数据
 * @return esp_err_t 
 */
esp_err_t bsp_imu_read(bsp_imu_data_t *data);

/**
 * @brief 更新IMU数据（轮询方式）
 * 
 * @return esp_err_t 
 */
esp_err_t bsp_imu_update(void);

/**
 * @brief 反初始化IMU驱动
 * 
 * @return esp_err_t 
 */
esp_err_t bsp_imu_deinit(void);

/**
 * @brief 获取坡度（基于Pitch角）
 * 
 * @return float 坡度（百分比，-100% ~ +100%）
 */
float bsp_imu_get_slope(void);

#ifdef __cplusplus
}
#endif
