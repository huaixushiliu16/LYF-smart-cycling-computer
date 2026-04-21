#ifndef __SYSTEM_MONITOR
#define __SYSTEM_MONITOR

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建系统监控器（CPU和FPS显示）
 * @note 在lv_layer_top()上创建，全局可见
 */
void SystemMonitor_Create(void);

/**
 * @brief 更新系统监控器显示
 * @note 由定时器调用，定期更新CPU和FPS
 */
void SystemMonitor_Update(void);

/**
 * @brief 通知帧刷新（在LVGL flush回调中调用）
 * @note 用于FPS统计
 */
void SystemMonitor_NotifyFrame(void);

#ifdef __cplusplus
}
#endif

#endif
