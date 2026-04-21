/**
 * @file dataproc.h
 * @brief DataProc层主接口定义
 * @note 阶段2.5：软件分层架构搭建
 */

#ifndef __DATAPROC_H
#define __DATAPROC_H

#include "dataproc_def.h"

#ifdef __cplusplus
#include "hal_def.h"
// Forward declarations for C++ types
class DataCenter;
class Account;

extern "C" {
#endif

// 初始化所有DataProc模块
void DataProc_Init(void);

// GPS数据处理
void DP_GPS_Init(void);
void DP_GPS_Update(void);
#ifdef __cplusplus
bool DP_GPS_GetInfo(HAL::GPS_Info_t *info);
#else
// C语言环境下需要使用void*，实际使用时需要包含hal_def.h
bool DP_GPS_GetInfo(void *info);
#endif

// IMU数据处理
void DP_IMU_Init(void);
void DP_IMU_Update(void);
#ifdef __cplusplus
bool DP_IMU_GetInfo(HAL::IMU_Info_t *info);
#else
bool DP_IMU_GetInfo(void *info);
#endif

// 运动数据统计
void DP_Sport_Init(void);
void DP_Sport_Update(void);
bool DP_Sport_GetInfo(SportStatus_Info_t *info);
void DP_Sport_SetWeight(float weight_kg);

// 电源数据处理
void DP_Power_Init(void);
void DP_Power_Update(void);
#ifdef __cplusplus
bool DP_Power_GetInfo(HAL::Power_Info_t *info);
#else
bool DP_Power_GetInfo(void *info);
#endif

// BLE数据处理
void DP_BLE_Init(void);
void DP_BLE_Update(void);
#ifdef __cplusplus
bool DP_BLE_GetInfo(HAL::BLE_Info_t *info);
#else
bool DP_BLE_GetInfo(void *info);
#endif

// 存储数据处理
void DP_Storage_Init(void);

// StatusBar数据处理
void DP_StatusBar_Init(void);

// SysConfig数据处理
void DP_SysConfig_Init(void);
void DP_SysConfig_RequestLoad(void);

// TrackFilter数据处理
void DP_TrackFilter_Init(void);

// RGB数据处理
void DP_RGB_Init(void);
void DP_RGB_Update(void);
bool DP_RGB_GetInfo(RGB_Info_t *info);
bool DP_RGB_SetMode(RGB_Mode_t mode);
bool DP_RGB_SetBrightness(uint8_t brightness);
bool DP_RGB_SetSpeed(uint8_t speed);
bool DP_RGB_SetEnabled(bool enabled);
bool DP_RGB_SetSolidColor(const RGB_Color_t *color);

#ifdef __cplusplus
}
#endif

// 获取DataCenter实例（供App层使用）
// 这些函数在extern "C"块外声明，使用C++链接
#ifdef __cplusplus
DataCenter* DataProc_GetDataCenter(void);
Account* DataProc_SearchAccount(const char* name);
#endif

#endif /* __DATAPROC_H */
