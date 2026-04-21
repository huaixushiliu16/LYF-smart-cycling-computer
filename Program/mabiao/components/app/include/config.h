/*
 * MIT License
 * Copyright (c) 2021 _VIFEXTech
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef __CONFIG_H
#define __CONFIG_H

#include "lvgl.h"

/*=========================
   Application configuration
 *=========================*/

#define CONFIG_GPS_REFR_PERIOD              2000 // ms
// GPS 无定位（无卫星/无 fix）时的默认坐标（十进制度）
// 北纬：37°28'12'' = 37.470000
// 东经：116°19'28'' = 116.324444
#define CONFIG_GPS_LONGITUDE_DEFAULT        116.324444f
#define CONFIG_GPS_LATITUDE_DEFAULT         37.470000f

// 系统配置宏定义
#define CONFIG_SYSTEM_TIME_ZONE_DEFAULT     8    // GMT+8
#define CONFIG_SYSTEM_SOUND_ENABLE_DEFAULT  true
#define CONFIG_SYSTEM_LANGUAGE_DEFAULT      "en-GB"

#define CONFIG_TRACK_FILTER_OFFSET_THRESHOLD  2 // pixel

#ifndef CONFIG_COMPASS_HEADING_OFFSET_DEG
// 指南针航向软件补偿（度）。正值表示顺时针旋转补偿。
// 例如：326° + 116° = 82°（归一化到 0~359°）
#define CONFIG_COMPASS_HEADING_OFFSET_DEG     116
#endif

#define CONFIG_MAP_USE_WGS84_DEFAULT        false
#define CONFIG_MAP_DIR_PATH_DEFAULT         "/MAP"

#ifndef CONFIG_MAP_EXT_NAME_DEFAULT
#define CONFIG_MAP_EXT_NAME_DEFAULT         "bin"
#endif

#ifndef CONFIG_MAP_IMG_PNG_ENABLE
#  define CONFIG_MAP_IMG_PNG_ENABLE         0
#endif

#define CONFIG_ARROW_THEME_DEFAULT          "default"

#define CONFIG_LIVE_MAP_LEVEL_DEFAULT       13

#define CONFIG_LIVE_MAP_DEBUG_ENABLE        0
#if CONFIG_LIVE_MAP_DEBUG_ENABLE
#  define CONFIG_LIVE_MAP_VIEW_WIDTH        280
#  define CONFIG_LIVE_MAP_VIEW_HEIGHT       240
#else
#  define CONFIG_LIVE_MAP_VIEW_WIDTH        LV_HOR_RES
#  define CONFIG_LIVE_MAP_VIEW_HEIGHT       LV_VER_RES
#endif

#endif
