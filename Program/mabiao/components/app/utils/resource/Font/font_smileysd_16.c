/*
 *---------------------------------------------------------------
 *                        Lvgl Font Tool
 *
 * 注:使用unicode编码
 * 注:本字体文件由Lvgl Font Tool V0.4 生成
 * 作者:阿里(qq:617622104)
 *---------------------------------------------------------------
 */

#include "lvgl.h"

typedef struct
{
    uint16_t min;
    uint16_t max;
    uint8_t bpp;
    uint8_t reserved[3];
} x_header_t;
typedef struct
{
    uint32_t pos;
} x_table_t;
typedef struct
{
    uint8_t adv_w;
    uint8_t box_w;
    uint8_t box_h;
    int8_t ofs_x;
    int8_t ofs_y;
    uint8_t r;
} glyph_dsc_t;

static x_header_t __g_xbf_hd = {
    .min = 0x0009,
    .max = 0xffe5,
    .bpp = 4,
};

#ifndef FONT_SMILEYSD_16_USE_SPIRAM
#define FONT_SMILEYSD_16_USE_SPIRAM 1
#endif

#if FONT_SMILEYSD_16_USE_SPIRAM
uint8_t *__g_font_buf; //  USE_SPIRAM
#else
static uint8_t __g_font_buf[152]; // 如bin文件存在SPI-FLASH或sd卡中可使用此buff
#endif

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>

static const char *TAG = "FontSmileysd16";

static lv_fs_file_t font_f;
static lv_fs_res_t font_res;
static uint32_t font_resCount;
static bool font_isOpen = false;
/**
 * @brief copyFontFromSD2SPIRAM 从sd卡中读取字库到SPIRAM
 */
void copyFontFromSD2SPIRAM(void)
{
    size_t font_size = 1008 * 1024; // 1008kB
    __g_font_buf = (uint8_t *)heap_caps_malloc_prefer(
        font_size,
        2,  // 2个优先级
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,  // 优先：PSRAM
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT  // 备选：SRAM
    );
    if (__g_font_buf == NULL)
    {
        ESP_LOGE(TAG, "malloc buffer from PSRAM error");
        return;  // 内存分配失败，直接返回
    }
    
    ESP_LOGI(TAG, "malloc buffer from PSRAM successful (%zu bytes)", font_size);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 尝试从SD卡读取字体文件
    lv_fs_res_t res = lv_fs_open(&font_f, "/FONT/font_smileysd_16.bin", LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK) {
        ESP_LOGW(TAG, "Failed to open font file /FONT/font_smileysd_16.bin (res=%d), font may not work", res);
        heap_caps_free(__g_font_buf);
        __g_font_buf = NULL;
        return;
    }
    
    lv_fs_seek(&font_f, 0, LV_FS_SEEK_SET);
    res = lv_fs_read(&font_f, __g_font_buf, font_size, &font_resCount);
    lv_fs_close(&font_f);
    
    if (res != LV_FS_RES_OK || font_resCount != font_size) {
        ESP_LOGW(TAG, "Failed to read font file (res=%d, read=%" PRIu32 " bytes), font may not work", res, font_resCount);
        heap_caps_free(__g_font_buf);
        __g_font_buf = NULL;
        return;
    }
    
    ESP_LOGI(TAG, "Font file loaded successfully (%" PRIu32 " bytes)", font_resCount);
}

static uint8_t *__user_font_getdata(int offset, int size)
{
    // 如字模保存在SPI FLASH: SPIFLASH_Read(__g_font_buf, offset, size);
    // 如字模已加载到SDRAM，直接返回偏移地址即可如: return (uint8_t *)(sdram_fontddr + offset);

    /* 从SPIRAM中读取字库 */
    if (__g_font_buf == NULL) {
        return NULL;  // 字体未加载，返回NULL
    }
    return (uint8_t *)(__g_font_buf + offset);

    /* 从sd卡中读取字库 */
    // if (font_isOpen == false)
    // {
    //     font_res = lv_fs_open(&font_f, "/FONT/font_smileysd_16.bin", LV_FS_MODE_RD);
    //     font_isOpen = true;
    // }
    // lv_fs_seek(&font_f, offset, LV_FS_SEEK_SET);
    // font_res = lv_fs_read(&font_f, __g_font_buf, size, &font_resCount);

    // // lv_fs_close(&font_f);
    // return __g_font_buf;

}

static const uint8_t *__user_font_get_bitmap(const lv_font_t *font, uint32_t unicode_letter)
{
    if (unicode_letter > __g_xbf_hd.max || unicode_letter < __g_xbf_hd.min)
    {
        return NULL;
    }
    uint32_t unicode_offset = sizeof(x_header_t) + (unicode_letter - __g_xbf_hd.min) * 4;
    uint32_t *p_pos = (uint32_t *)__user_font_getdata(unicode_offset, 4);
    if (p_pos[0] != 0)
    {
        uint32_t pos = p_pos[0];
        glyph_dsc_t *gdsc = (glyph_dsc_t *)__user_font_getdata(pos, sizeof(glyph_dsc_t));
        return __user_font_getdata(pos + sizeof(glyph_dsc_t), gdsc->box_w * gdsc->box_h * __g_xbf_hd.bpp / 8);
    }
    return NULL;
}

static bool __user_font_get_glyph_dsc(const lv_font_t *font, lv_font_glyph_dsc_t *dsc_out, uint32_t unicode_letter, uint32_t unicode_letter_next)
{
    if (unicode_letter > __g_xbf_hd.max || unicode_letter < __g_xbf_hd.min)
    {
        return NULL;
    }
    uint32_t unicode_offset = sizeof(x_header_t) + (unicode_letter - __g_xbf_hd.min) * 4;
    uint32_t *p_pos = (uint32_t *)__user_font_getdata(unicode_offset, 4);
    if (p_pos[0] != 0)
    {
        glyph_dsc_t *gdsc = (glyph_dsc_t *)__user_font_getdata(p_pos[0], sizeof(glyph_dsc_t));
        dsc_out->adv_w = gdsc->adv_w;
        dsc_out->box_h = gdsc->box_h;
        dsc_out->box_w = gdsc->box_w;
        dsc_out->ofs_x = gdsc->ofs_x;
        dsc_out->ofs_y = gdsc->ofs_y;
        dsc_out->bpp = __g_xbf_hd.bpp;
        return true;
    }
    return false;
}

// Smiley Sans Oblique,,-1
// 字模高度：19
// XBF字体,外部bin文件
lv_font_t font_smileysd_16 = {
    .get_glyph_bitmap = __user_font_get_bitmap,
    .get_glyph_dsc = __user_font_get_glyph_dsc,
    .line_height = 19,
    .base_line = 0,
};
