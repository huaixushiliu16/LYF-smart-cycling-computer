/**
 * @file lvgl_port.cpp
 * @brief LVGL显示和输入设备适配器（基于LovyanGFX）
 * @note 参考version2项目的lv_port_disp.cpp，使用LovyanGFX的DMA刷新
 */

#include "lvgl_port.h"
#include "lv_port_fs.h"  // LVGL文件系统驱动
#include "bsp_lcd.h"
#include "bsp_touch.h"
#include "SystemMonitor.h"
#include "lgfx_mabiao.hpp"  // LGFX_MABIAO
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "lvgl.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "LVGL_PORT";

// LVGL配置
#define LVGL_TICK_PERIOD_MS    2
#define LVGL_TASK_STACK_SIZE   (8 * 1024)
#define LVGL_TASK_PRIORITY     2
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 1

static SemaphoreHandle_t s_lvgl_mutex = NULL;
static esp_timer_handle_t s_lvgl_tick_timer = NULL;
static LGFX_MABIAO *s_lcd = NULL;

/**
 * @brief LVGL flush回调函数（使用LovyanGFX逐行DMA刷新）
 * @note 参考version2的lv_port_disp.cpp实现
 */
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    if (s_lcd == NULL) {
        lv_disp_flush_ready(drv);
        return;
    }

    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;

    // 计算原始区域的宽度（LVGL缓冲区数据的行宽）
    int32_t area_w = (x2 - x1 + 1);

    // 边界检查
    if (x2 < 0 || y2 < 0 || x1 >= BSP_LCD_WIDTH || y1 >= BSP_LCD_HEIGHT) {
        lv_disp_flush_ready(drv);
        return;
    }

    // 裁剪
    int32_t pixel_skip = 0;
    if (x1 < 0) { pixel_skip = -x1; x1 = 0; }
    if (y1 < 0) { pixel_skip += (-y1) * area_w; y1 = 0; }
    if (x2 >= BSP_LCD_WIDTH)  x2 = BSP_LCD_WIDTH - 1;
    if (y2 >= BSP_LCD_HEIGHT) y2 = BSP_LCD_HEIGHT - 1;

    uint32_t w = (x2 - x1 + 1);
    uint32_t h = (y2 - y1 + 1);

    if (w == 0 || h == 0) {
        lv_disp_flush_ready(drv);
        return;
    }

    // 参考version2：逐行DMA刷新，确保像素数据指针正确对齐
    lv_color_t *src_ptr = color_p + pixel_skip;

    for (uint32_t row = 0; row < h; row++) {
        int32_t row_y = y1 + row;
        if (row_y < 0 || row_y >= BSP_LCD_HEIGHT) {
            src_ptr += area_w;
            continue;
        }

        s_lcd->setAddrWindow(x1, row_y, w, 1);
        s_lcd->startWrite();
        s_lcd->writePixelsDMA((uint16_t *)src_ptr, w, true);
        s_lcd->endWrite();

        // 使用原始区域宽度计算下一行的偏移
        src_ptr += area_w;
    }

    // 通知SystemMonitor帧刷新（用于FPS统计）
    SystemMonitor_NotifyFrame();

    // 通知LVGL传输完成
    lv_disp_flush_ready(drv);
}

/**
 * @brief LVGL触摸输入设备读取回调
 */
static void lvgl_touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    bsp_touch_point_t point;
    esp_err_t ret = bsp_touch_read(&point);

    if (ret == ESP_OK && point.pressed) {
        data->point.x = point.x;
        data->point.y = point.y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/**
 * @brief LVGL tick定时器回调
 */
static void lvgl_tick_timer_cb(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

/**
 * @brief LVGL主任务
 */
static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL task started");

    // 将当前任务添加到看门狗监控列表
    esp_task_wdt_add(NULL);

    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    uint32_t wdt_counter = 0;

    while (1) {
        // 锁定互斥锁（LVGL不是线程安全的）
        if (xSemaphoreTakeRecursive(s_lvgl_mutex, portMAX_DELAY) == pdTRUE) {
            task_delay_ms = lv_timer_handler();
            xSemaphoreGiveRecursive(s_lvgl_mutex);
        }

        // 限制延迟时间范围
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }

        // 定期喂看门狗
        wdt_counter++;
        if (wdt_counter >= 10) {
            esp_task_wdt_reset();
            wdt_counter = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

/**
 * @brief 获取LVGL互斥锁
 */
bool lvgl_port_lock(int timeout_ms)
{
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_lvgl_mutex, timeout_ticks) == pdTRUE;
}

/**
 * @brief 释放LVGL互斥锁
 */
void lvgl_port_unlock(void)
{
    xSemaphoreGiveRecursive(s_lvgl_mutex);
}

esp_err_t lvgl_port_init(void)
{
    esp_err_t ret = ESP_OK;

    // 获取LovyanGFX实例
    s_lcd = (LGFX_MABIAO *)bsp_lcd_get_lgfx();
    if (s_lcd == NULL) {
        ESP_LOGE(TAG, "LGFX instance is NULL, LCD not initialized?");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Initializing LVGL port (LovyanGFX backend)...");

    // 创建互斥锁
    s_lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    if (s_lvgl_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL mutex");
        return ESP_ERR_NO_MEM;
    }

    // 初始化LVGL库
    lv_init();
    ESP_LOGI(TAG, "LVGL library initialized");

    // 初始化LVGL文件系统驱动（必须在lv_init之后，SD卡挂载之后）
    lv_fs_vfs_init();

    // 使用内部SRAM分配显示缓冲区（避免PSRAM与LovyanGFX SPI/DMA冲突）
    size_t buf_size = BSP_LCD_WIDTH * 20 * sizeof(lv_color_t);  // 20行缓冲区，每个9600字节

    ESP_LOGI(TAG, "Allocating display buffers from SRAM: %d bytes each...", (int)buf_size);

    static lv_color_t buf1[BSP_LCD_WIDTH * 20];
    static lv_color_t buf2[BSP_LCD_WIDTH * 20];

    memset(buf1, 0, buf_size);
    memset(buf2, 0, buf_size);

    ESP_LOGI(TAG, "Display buffers allocated from SRAM OK");

    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, BSP_LCD_WIDTH * 20);

    ESP_LOGI(TAG, "Display draw buffer initialized");

    // 注册显示驱动
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = BSP_LCD_WIDTH;
    disp_drv.ver_res = BSP_LCD_HEIGHT;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.full_refresh = 0;  // 部分刷新

    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to register LVGL display driver");
        heap_caps_free(buf1);
        heap_caps_free(buf2);
        vSemaphoreDelete(s_lvgl_mutex);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LVGL display driver registered");

    // 注册输入设备驱动（触摸）
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read_cb;

    lv_indev_t *indev = lv_indev_drv_register(&indev_drv);
    if (indev == NULL) {
        ESP_LOGE(TAG, "Failed to register LVGL input device driver");
        heap_caps_free(buf1);
        heap_caps_free(buf2);
        vSemaphoreDelete(s_lvgl_mutex);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LVGL input device driver registered");

    // 创建LVGL tick定时器
    const esp_timer_create_args_t tick_timer_args = {
        .callback = &lvgl_tick_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lvgl_tick",
        .skip_unhandled_events = false,
    };
    ret = esp_timer_create(&tick_timer_args, &s_lvgl_tick_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LVGL tick timer: %s", esp_err_to_name(ret));
        heap_caps_free(buf1);
        heap_caps_free(buf2);
        vSemaphoreDelete(s_lvgl_mutex);
        return ret;
    }

    ret = esp_timer_start_periodic(s_lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start LVGL tick timer: %s", esp_err_to_name(ret));
        esp_timer_delete(s_lvgl_tick_timer);
        heap_caps_free(buf1);
        heap_caps_free(buf2);
        vSemaphoreDelete(s_lvgl_mutex);
        return ret;
    }

    ESP_LOGI(TAG, "LVGL tick timer started (%d ms)", LVGL_TICK_PERIOD_MS);

    // 创建LVGL任务
    BaseType_t task_ret = xTaskCreate(
        lvgl_task,
        "LVGL",
        LVGL_TASK_STACK_SIZE,
        NULL,
        LVGL_TASK_PRIORITY,
        NULL
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LVGL task");
        esp_timer_stop(s_lvgl_tick_timer);
        esp_timer_delete(s_lvgl_tick_timer);
        heap_caps_free(buf1);
        heap_caps_free(buf2);
        vSemaphoreDelete(s_lvgl_mutex);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LVGL port initialized successfully (LovyanGFX backend)");
    return ESP_OK;
}

esp_err_t lvgl_port_deinit(void)
{
    if (s_lvgl_tick_timer != NULL) {
        esp_timer_stop(s_lvgl_tick_timer);
        esp_timer_delete(s_lvgl_tick_timer);
        s_lvgl_tick_timer = NULL;
    }

    if (s_lvgl_mutex != NULL) {
        vSemaphoreDelete(s_lvgl_mutex);
        s_lvgl_mutex = NULL;
    }

    ESP_LOGI(TAG, "LVGL port deinitialized");
    return ESP_OK;
}
