/**
 * @file hal_def.h
 * @brief HAL层数据结构定义
 * @note 阶段2.5：软件分层架构搭建
 */

#ifndef __HAL_DEF_H
#define __HAL_DEF_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
namespace HAL
{
    /* Clock */
    typedef struct
    {
        uint16_t year;
        uint8_t month;
        uint8_t day;
        uint8_t week;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
        uint16_t millisecond;
    } Clock_Info_t;

    /* GPS */
    typedef struct
    {
        double longitude;
        double latitude;
        float altitude;
        float course;
        float speed;
        int16_t satellites;
        bool isVaild;
        Clock_Info_t clock;
    } GPS_Info_t;

    /* IMU */
    typedef struct
    {
        int16_t ax;
        int16_t ay;
        int16_t az;
        int16_t gx;
        int16_t gy;
        int16_t gz;
        float roll;
        float pitch;
        float yaw;
        // 磁力计（μT）
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
        float pressure;      // 气压（Pa）
        float height;        // 高度（m）
        int16_t steps;
        float slope;        // 坡度（%），基于Pitch角计算
    } IMU_Info_t;

    /* Power */
    typedef struct
    {
        uint16_t voltage;      // 电压（mV）
        uint8_t usage;        // 电量百分比（0-100）
        bool isCharging;      // 是否充电
    } Power_Info_t;

    /* Touch */
    typedef struct
    {
        uint16_t x;         // X坐标
        uint16_t y;         // Y坐标
        bool pressed;       // 是否按下
    } Touch_Info_t;

    /* BLE */
    typedef struct
    {
        char *name;         // 设备名称
        bool isConnected;  // 是否已连接
        bool isEnabled;    // 是否启用
        uint16_t heartRate; // 心率（bpm），新增字段
        float speed;        // 速度（km/h）
        float cadence;      // 踏频（RPM）
    } BLE_Info_t;
} // namespace HAL

#else
// C语言环境下的类型定义（与C++命名空间中的定义相同）
typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t week;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t millisecond;
} Clock_Info_t;

typedef struct {
    double longitude;
    double latitude;
    float altitude;
    float course;
    float speed;
    int16_t satellites;
    bool isVaild;
    Clock_Info_t clock;
} GPS_Info_t;

typedef struct {
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;
    float roll;
    float pitch;
    float yaw;
    // 磁力计（μT）
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
    float pressure;      // 气压（Pa）
    float height;        // 高度（m）
    int16_t steps;
    float slope;        // 坡度（%），基于Pitch角计算
} IMU_Info_t;

typedef struct {
    uint16_t voltage;
    uint8_t usage;
    bool isCharging;
} Power_Info_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    bool pressed;
} Touch_Info_t;

typedef struct {
    char *name;
    bool isConnected;
    bool isEnabled;
    uint16_t heartRate;
    float speed;
    float cadence;
} BLE_Info_t;
#endif

#endif /* __HAL_DEF_H */
