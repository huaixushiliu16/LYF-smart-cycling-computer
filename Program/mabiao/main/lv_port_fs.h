/**
 * @file lv_port_fs.h
 * @brief LVGL文件系统驱动接口（用于访问SD卡文件）
 */

#ifndef LV_PORT_FS_H
#define LV_PORT_FS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**********************
 * GLOBAL PROTOTYPES
 **********************/
/**
 * @brief 初始化LVGL文件系统驱动（VFS）
 * @note 注册文件系统驱动，使LVGL能够访问SD卡上的文件
 *       路径转换：/MAP/... -> /sdcard/MAP/...
 */
void lv_fs_vfs_init(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_PORT_FS_H*/
