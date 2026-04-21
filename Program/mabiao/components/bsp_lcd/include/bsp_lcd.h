/**
 * @file bsp_lcd.h
 * @brief ILI9341 LCD驱动头文件（基于LovyanGFX）
 * @note 参考version2项目，使用LovyanGFX驱动ILI9341
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// LCD引脚配置（与version2一致）
#define BSP_LCD_PIN_SCLK    18  // SPI时钟
#define BSP_LCD_PIN_MOSI    17  // SPI数据输出
#define BSP_LCD_PIN_MISO    3   // SPI数据输入
#define BSP_LCD_PIN_CS      7   // 片选
#define BSP_LCD_PIN_DC      16  // 数据/命令
#define BSP_LCD_PIN_RST     15  // 复位
#define BSP_LCD_PIN_BL      8   // 背光

// LCD参数
#define BSP_LCD_WIDTH       240
#define BSP_LCD_HEIGHT      320

/**
 * @brief 初始化LCD驱动（LovyanGFX）
 * 
 * @return esp_err_t 
 */
esp_err_t bsp_lcd_init(void);

/**
 * @brief 设置LCD背光
 * 
 * @param level 背光电平（false=关闭，true=开启）
 * @return esp_err_t 
 */
esp_err_t bsp_lcd_set_backlight(bool level);

/**
 * @brief 反初始化LCD驱动（释放资源）
 * 
 * @return esp_err_t 
 */
esp_err_t bsp_lcd_deinit(void);

/**
 * @brief 获取LovyanGFX实例指针（供LVGL flush回调使用）
 * 
 * @return void* LGFX_SSS实例指针，需要在C++中转换为LGFX_SSS*
 */
void *bsp_lcd_get_lgfx(void);

#ifdef __cplusplus
}
#endif
