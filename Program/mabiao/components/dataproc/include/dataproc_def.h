/**
 * @file dataproc_def.h
 * @brief DataProc层数据结构定义
 * @note 阶段2.5：软件分层架构搭建
 */

#ifndef __DATAPROC_DEF_H
#define __DATAPROC_DEF_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
#include "hal_def.h"
#endif

/**
 * @brief SysConfig命令枚举
 */
typedef enum {
    SYSCONFIG_CMD_LOAD = 0,  // 加载配置
    SYSCONFIG_CMD_SAVE = 1,  // 保存配置
} SysConfig_Cmd_t;

/**
 * @brief SysConfig信息结构体
 */
typedef struct {
    SysConfig_Cmd_t cmd;
    float longitude;
    float latitude;
    int16_t timeZone;
    bool soundEnable;
    char language[8];
    char arrowTheme[16];
    char mapDirPath[16];
    char mapExtName[8];
    bool mapWGS84;
    float weight_kg;               // User weight (kg), for calorie calculation
    float wheel_circumference_m;   // Wheel circumference (m), for CSCS speed
} SysConfig_Info_t;

/**
 * @brief TrackFilter命令枚举
 */
typedef enum {
    TRACK_FILTER_CMD_START = 0,     // 开始记录
    TRACK_FILTER_CMD_PAUSE = 1,     // 暂停
    TRACK_FILTER_CMD_CONTINUE = 2,  // 继续
    TRACK_FILTER_CMD_STOP = 3,      // 停止
} TrackFilter_Cmd_t;

/**
 * @brief TrackFilter信息结构体
 */
typedef struct {
    TrackFilter_Cmd_t cmd;
    void *pointCont;
    uint8_t level;
    bool isActive;
} TrackFilter_Info_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 运动状态信息结构体
 */
typedef struct {
    uint32_t last_tick;
    float weight;
    float speed_kph;        // 当前速度（km/h）
    float speed_max_kph;    // 最大速度（km/h）
    float speed_avg_kph;    // 平均速度（km/h）
    uint64_t total_time;    // 总时间（秒）
    float total_distance;   // 总距离（km）
    uint64_t single_time;   // 单次时间（秒）
    float single_distance;  // 单次距离（km）
    float single_calorie;   // 单次卡路里（kcal）
    
    // 阶段8新增字段
    float cadence_rpm;        // 踏频（RPM）
    uint16_t heart_rate_bpm;  // 心率（bpm）
    float slope_percent;       // 坡度（%）
    float altitude_m;         // 海拔（m）
    float direction_deg;      // 方向（度，0-360）
} SportStatus_Info_t;

/**
 * @brief 磁力计信息结构体
 */
typedef struct {
    float magX;      // 磁力计X轴（μT）
    float magY;      // 磁力计Y轴（μT）
    float magZ;      // 磁力计Z轴（μT）
    bool isValid;    // 数据是否有效
} MAG_Info_t;

/**
 * @brief RGB模式枚举（与BSP层保持一致）
 */
typedef enum {
    RGB_MODE_OFF = 0,           // 关闭
    RGB_MODE_RAINBOW,           // 彩虹渐变
    RGB_MODE_LIGHTNING,         // 闪电闪烁
    RGB_MODE_FIRE,              // 火焰效果
    RGB_MODE_STARRY,            // 星空闪烁
    RGB_MODE_WAVE,              // 波浪效果
    RGB_MODE_FIREWORKS,         // 烟花爆炸
    RGB_MODE_HEARTBEAT,         // 心跳效果（固定频率）
    RGB_MODE_HEART_RATE_SYNC,   // 心率同步（实时心率数据）
    RGB_MODE_SPIRAL,            // 螺旋旋转
    RGB_MODE_COLOR_BLOCK,       // 色块跳跃
    RGB_MODE_AURORA,            // 极光效果
    RGB_MODE_SOLID_COLOR,       // 纯色模式
    RGB_MODE_MAX
} RGB_Mode_t;

/**
 * @brief RGB颜色结构体
 */
typedef struct {
    uint8_t r;  // 红色分量 (0-255)
    uint8_t g;  // 绿色分量 (0-255)
    uint8_t b;  // 蓝色分量 (0-255)
} RGB_Color_t;

/**
 * @brief RGB信息结构体
 */
typedef struct {
    RGB_Mode_t mode;            // 当前模式
    uint8_t brightness;         // 亮度 (0-100)
    uint8_t speed;              // 速度 (0-100)
    RGB_Color_t solid_color;    // 纯色模式的颜色
    bool enabled;               // 是否启用
} RGB_Info_t;

/**
 * @brief 存储基础信息结构体（用于Pull操作）
 */
typedef struct {
    bool isDetect;        // 是否检测到存储设备
    float totalSizeMB;     // 总容量（MB）
    float freeSizeMB;      // 可用容量（MB）
    const char* type;      // 存储类型（如 "SD Card" 或 "None"）
} Storage_Basic_Info_t;

/**
 * @brief StatusBar样式枚举
 */
typedef enum {
    STATUS_BAR_STYLE_TRANSP = 0,  // 透明样式
    STATUS_BAR_STYLE_BLACK = 1,   // 黑色样式
} StatusBar_Style_t;

/**
 * @brief StatusBar命令枚举
 */
typedef enum {
    STATUS_BAR_CMD_APPEAR = 0,      // 显示/隐藏
    STATUS_BAR_CMD_SET_STYLE = 1,    // 设置样式
    STATUS_BAR_CMD_SET_LABEL_REC = 2, // 设置录制标签（暂不实现）
} StatusBar_Cmd_t;

/**
 * @brief StatusBar信息结构体（用于Notify命令）
 */
typedef struct {
    StatusBar_Cmd_t cmd;  // 命令类型
    union {
        bool appear;                    // APPEAR命令：是否显示
        StatusBar_Style_t style;       // SET_STYLE命令：样式
        struct {
            bool show;
            const char* str;
        } labelRec;                     // SET_LABEL_REC命令（暂不实现）
    } param;
} StatusBar_Info_t;

#ifdef __cplusplus
}
#endif

#endif /* __DATAPROC_DEF_H */
