/**
 * @file hal_imu.cpp
 * @brief IMU HAL封装实现
 * @note 阶段2.5：软件分层架构搭建 - 框架实现（待后续完善）
 */

#include "hal.h"
#include "bsp_imu.h"
#include "esp_log.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *TAG = "HAL_IMU";
static bool s_initialized = false;

namespace HAL
{

void IMU_Init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "IMU already initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing IMU HAL (framework only)...");
    
    // 初始化BSP层IMU驱动
    esp_err_t ret = bsp_imu_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BSP IMU: %s", esp_err_to_name(ret));
        return;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "IMU HAL initialized");
}

void IMU_Update(void)
{
    if (!s_initialized) {
        return;
    }
    
    // 更新BSP层IMU数据
    bsp_imu_update();
}

bool IMU_GetInfo(IMU_Info_t *info)
{
    if (!s_initialized || info == nullptr) {
        return false;
    }
    
    bsp_imu_data_t bsp_data;
    esp_err_t ret = bsp_imu_read(&bsp_data);
    if (ret != ESP_OK) {
        return false;
    }
    
    // 转换BSP层数据到HAL层数据
    // 注意：BSP层的加速度和角速度已经是int16_t类型（单位转换后），直接复制
    info->ax = bsp_data.ax;
    info->ay = bsp_data.ay;
    info->az = bsp_data.az;
    info->gx = bsp_data.gx;
    info->gy = bsp_data.gy;
    info->gz = bsp_data.gz;
    // 姿态角是float类型，直接复制
    info->roll = bsp_data.roll;
    info->pitch = bsp_data.pitch;
    info->yaw = bsp_data.yaw;
    // 磁力计数据
    info->mx = bsp_data.mx;
    info->my = bsp_data.my;
    info->mz = bsp_data.mz;
    // 四元数
    info->q0 = bsp_data.q0;
    info->q1 = bsp_data.q1;
    info->q2 = bsp_data.q2;
    info->q3 = bsp_data.q3;
    // 环境数据
    info->temperature = bsp_data.temperature;
    info->pressure = bsp_data.pressure;
    info->height = bsp_data.height;
    info->steps = bsp_data.steps;
    
    // 坡度计算：tan(pitch) × 100%
    float pitch_rad = bsp_data.pitch * M_PI / 180.0f;  // 度转弧度
    info->slope = tanf(pitch_rad) * 100.0f;  // 转换为百分比
    
    // 数据有效性检查：姿态角范围（-180~180度）
    if (bsp_data.roll < -180.0f || bsp_data.roll > 180.0f ||
        bsp_data.pitch < -180.0f || bsp_data.pitch > 180.0f ||
        bsp_data.yaw < -180.0f || bsp_data.yaw > 180.0f) {
        ESP_LOGW(TAG, "Invalid IMU attitude angles: roll=%.2f, pitch=%.2f, yaw=%.2f",
                 bsp_data.roll, bsp_data.pitch, bsp_data.yaw);
        // 不返回false，因为可能是传感器初始化阶段的数据
    }
    
    return true;
}

} // namespace HAL

// C兼容接口
extern "C" {
void IMU_Update_C(void)
{
    HAL::IMU_Update();
}

bool IMU_GetInfo_C(IMU_Info_t_C *info)
{
    // IMU_Info_t_C 是 HAL::IMU_Info_t 的别名，可以直接转换
    HAL::IMU_Info_t *hal_info = reinterpret_cast<HAL::IMU_Info_t*>(info);
    return HAL::IMU_GetInfo(hal_info);
}
}
