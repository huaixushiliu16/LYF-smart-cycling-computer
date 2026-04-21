/**
 * @file bsp_touch.h
 * @brief FT6336G触摸屏驱动头文件（BSP层）
 * @note 阶段2.5：软件分层架构搭建
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 触摸屏引脚配置（与硬件原理图 CTP_*一致）
#define BSP_TOUCH_PIN_SCL   9   // CTP_SCL
#define BSP_TOUCH_PIN_SDA   10  // CTP_SDA
// 本项目不使用 CTP_INT（轮询方式读取触摸），保留为 -1 表示未连接/不占用任何 GPIO
#define BSP_TOUCH_PIN_INT   (-1)
#define BSP_TOUCH_PIN_RST   11  // CTP_RST

// FT6336G I2C地址
#define BSP_TOUCH_I2C_ADDR  0x38

// FT6336G寄存器地址
#define BSP_TOUCH_REG_POINT_NUM   0x02  // 触摸点数量
#define BSP_TOUCH_REG_XH          0x03  // X坐标高位
#define BSP_TOUCH_REG_XL          0x04  // X坐标低位
#define BSP_TOUCH_REG_YH          0x05  // Y坐标高位
#define BSP_TOUCH_REG_YL          0x06  // Y坐标低位

// 触摸屏分辨率（与LCD匹配）
#define BSP_TOUCH_WIDTH    240
#define BSP_TOUCH_HEIGHT   320

// 触摸坐标变换配置（对齐 version2 经验）
// 根据 LCD 显示方向配置触摸坐标变换
// 默认配置：竖屏模式（240x320），Y轴反转以匹配LCD显示方向
// 注意：X轴反转已启用以修复左右镜像问题
#define BSP_TOUCH_SWAP_XY      0  // 是否交换XY轴（0=不交换，1=交换）
#define BSP_TOUCH_REVERSE_X    1  // 是否反转X轴（0=不反转，1=反转）- 已启用修复左右镜像
#define BSP_TOUCH_REVERSE_Y    1  // 是否反转Y轴（0=不反转，1=反转，默认对齐version2）

/**
 * @brief 触摸点数据结构
 */
typedef struct {
    uint16_t x;         // X坐标 (0-239)
    uint16_t y;         // Y坐标 (0-319)
    bool pressed;       // 是否按下
} bsp_touch_point_t;

/**
 * @brief 初始化触摸屏驱动
 * 
 * @return esp_err_t 
 */
esp_err_t bsp_touch_init(void);

/**
 * @brief 读取触摸点数据
 * 
 * @param point 输出参数，返回触摸点数据
 * @return esp_err_t 
 */
esp_err_t bsp_touch_read(bsp_touch_point_t *point);

/**
 * @brief 检查是否有触摸
 * 
 * @return true 有触摸
 * @return false 无触摸
 */
bool bsp_touch_is_touched(void);

/**
 * @brief 反初始化触摸屏驱动
 * 
 * @return esp_err_t 
 */
esp_err_t bsp_touch_deinit(void);

#ifdef __cplusplus
}
#endif
