/**
 * @file app.h
 * @brief App层主接口定义
 * @note 阶段7：HAL层封装和DataProc层实现（UI界面待重新设计）
 */

#ifndef __APP_H
#define __APP_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化App层
 * 
 * @return esp_err_t 
 */
esp_err_t App_Init(void);

/**
 * @brief 反初始化App层
 * 
 * @return esp_err_t 
 */
esp_err_t App_Uninit(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_H */
