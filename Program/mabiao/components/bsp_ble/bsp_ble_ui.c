/**
 * @file bsp_ble_ui.c
 * @brief BSP层BLE驱动LVGL调试界面
 * @note 阶段4：BLE Client驱动开发
 */

#include "bsp_ble.h"
#include "bsp_gps.h"
#include "bsp_imu.h"
#include "hal.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "BSP_BLE_UI";

// LVGL互斥锁函数指针（从bsp_ble_init配置中获取）
static bsp_ble_lvgl_lock_fn_t s_lvgl_lock = NULL;
static bsp_ble_lvgl_unlock_fn_t s_lvgl_unlock = NULL;

/**
 * @brief 设置LVGL互斥锁函数指针（内部函数，由bsp_ble_init调用）
 */
void bsp_ble_ui_set_lvgl_lock_functions(bsp_ble_lvgl_lock_fn_t lock_fn, bsp_ble_lvgl_unlock_fn_t unlock_fn)
{
    s_lvgl_lock = lock_fn;
    s_lvgl_unlock = unlock_fn;
}

/**
 * @brief 设置LVGL互斥锁函数指针（公共接口）
 */
void bsp_ble_set_lvgl_lock_functions(bsp_ble_lvgl_lock_fn_t lock_fn, bsp_ble_lvgl_unlock_fn_t unlock_fn)
{
    bsp_ble_ui_set_lvgl_lock_functions(lock_fn, unlock_fn);
}

// UI对象
static lv_obj_t *s_hr_label = NULL;
static lv_obj_t *s_speed_label = NULL;
static lv_obj_t *s_cadence_label = NULL;
// GPS数据标签
static lv_obj_t *s_gps_longitude_label = NULL;
static lv_obj_t *s_gps_latitude_label = NULL;
static lv_obj_t *s_gps_altitude_label = NULL;
static lv_obj_t *s_gps_time_label = NULL;
// IMU数据标签（陀螺仪）
static lv_obj_t *s_imu_gx_label = NULL;
static lv_obj_t *s_imu_gy_label = NULL;
static lv_obj_t *s_imu_gz_label = NULL;
// IMU数据标签（磁力计）
static lv_obj_t *s_imu_cx_label = NULL;  // 磁力计X
static lv_obj_t *s_imu_cy_label = NULL;  // 磁力计Y
static lv_obj_t *s_imu_cz_label = NULL;  // 磁力计Z
// 温度数据标签
static lv_obj_t *s_temp_label = NULL;
// 电压数据标签
static lv_obj_t *s_voltage_label = NULL;
// CPU和FPS显示标签（右上角）
static lv_obj_t *s_cpu_label = NULL;
static lv_obj_t *s_fps_label = NULL;

// CPU和FPS统计变量
static uint32_t s_frame_count = 0;
static uint32_t s_last_fps_time = 0;
static float s_current_fps = 0.0f;
static uint32_t s_last_cpu_time = 0;
static float s_current_cpu = 0.0f;

// 移除：update_connection_icon 函数（不再需要）

/**
 * @brief 更新心率显示
 */
static void update_heart_rate_ui(uint16_t heart_rate)
{
    if (s_hr_label == NULL) return;

    if (s_lvgl_lock && !s_lvgl_lock(-1)) {
        return;
    }

    char buf[32];
    if (heart_rate > 0) {
        snprintf(buf, sizeof(buf), "HR: %d bpm", heart_rate);
    } else {
        snprintf(buf, sizeof(buf), "HR: --");
    }
    lv_label_set_text(s_hr_label, buf);

    if (s_lvgl_unlock) {
        s_lvgl_unlock();
    }
}

/**
 * @brief 更新速度显示
 */
static void update_speed_ui(float speed)
{
    if (s_speed_label == NULL) return;

    if (s_lvgl_lock && !s_lvgl_lock(-1)) {
        return;
    }

    char buf[32];
    if (speed > 0.0f) {
        snprintf(buf, sizeof(buf), "Speed: %.1f km/h", speed);
    } else {
        snprintf(buf, sizeof(buf), "Speed: --");
    }
    lv_label_set_text(s_speed_label, buf);

    if (s_lvgl_unlock) {
        s_lvgl_unlock();
    }
}

/**
 * @brief 更新踏频显示
 */
static void update_cadence_ui(float cadence)
{
    if (s_cadence_label == NULL) return;

    if (s_lvgl_lock && !s_lvgl_lock(-1)) {
        return;
    }

    char buf[32];
    if (cadence > 0.0f) {
        snprintf(buf, sizeof(buf), "Cadence: %.1f RPM", cadence);
    } else {
        snprintf(buf, sizeof(buf), "Cadence: --");
    }
    lv_label_set_text(s_cadence_label, buf);

    if (s_lvgl_unlock) {
        s_lvgl_unlock();
    }
}

// 移除：update_scan_status_ui 函数（不再需要）

// 移除：设备列表、扫描按钮相关的函数（不再需要）
// - device_item_click_cb
// - update_device_list_ui
// - scan_btn_event_cb
// - stop_scan_btn_event_cb

/**
 * @brief 初始化LVGL调试界面
 */
void bsp_ble_ui_init(void)
{
    ESP_LOGI(TAG, "Initializing BLE UI...");

    if (s_lvgl_lock && !s_lvgl_lock(-1)) {
        ESP_LOGE(TAG, "Failed to lock LVGL mutex");
        return;
    }

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
    
    // 启用屏幕上下滑动功能（LVGL 8.3）
    lv_obj_set_scroll_dir(scr, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);  // 隐藏滚动条

    // 右上角CPU和FPS显示标签
    s_cpu_label = lv_label_create(scr);
    lv_label_set_text(s_cpu_label, "CPU: --%");
    lv_obj_set_style_text_color(s_cpu_label, lv_color_hex(0xFFAA00), 0);  // 橙色
    lv_obj_set_style_text_font(s_cpu_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_cpu_label, LV_ALIGN_TOP_RIGHT, -10, 5);

    s_fps_label = lv_label_create(scr);
    lv_label_set_text(s_fps_label, "FPS: --");
    lv_obj_set_style_text_color(s_fps_label, lv_color_hex(0xFFAA00), 0);  // 橙色
    lv_obj_set_style_text_font(s_fps_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_fps_label, LV_ALIGN_TOP_RIGHT, -10, 25);

    // 初始化统计变量
    s_frame_count = 0;
    s_last_fps_time = esp_timer_get_time() / 1000;  // 转换为毫秒
    s_current_fps = 0.0f;
    s_last_cpu_time = esp_timer_get_time() / 1000;
    s_current_cpu = 0.0f;

    int y_pos = 10;  // 起始Y位置

    // BLE数据显示标签
    s_hr_label = lv_label_create(scr);
    lv_label_set_text(s_hr_label, "HR: --");
    lv_obj_set_style_text_color(s_hr_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_hr_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_hr_label, LV_ALIGN_TOP_LEFT, 10, y_pos);
    y_pos += 30;

    s_speed_label = lv_label_create(scr);
    lv_label_set_text(s_speed_label, "Speed: --");
    lv_obj_set_style_text_color(s_speed_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_speed_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_speed_label, LV_ALIGN_TOP_LEFT, 10, y_pos);
    y_pos += 30;

    s_cadence_label = lv_label_create(scr);
    lv_label_set_text(s_cadence_label, "Cadence: --");
    lv_obj_set_style_text_color(s_cadence_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_cadence_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_cadence_label, LV_ALIGN_TOP_LEFT, 10, y_pos);
    y_pos += 40;

    // GPS数据显示标签
    s_gps_longitude_label = lv_label_create(scr);
    lv_label_set_text(s_gps_longitude_label, "Lon: --");
    lv_obj_set_style_text_color(s_gps_longitude_label, lv_color_hex(0x00FFFF), 0);  // 青色
    lv_obj_set_style_text_font(s_gps_longitude_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_gps_longitude_label, LV_ALIGN_TOP_LEFT, 10, y_pos);
    y_pos += 30;

    s_gps_latitude_label = lv_label_create(scr);
    lv_label_set_text(s_gps_latitude_label, "Lat: --");
    lv_obj_set_style_text_color(s_gps_latitude_label, lv_color_hex(0x00FFFF), 0);  // 青色
    lv_obj_set_style_text_font(s_gps_latitude_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_gps_latitude_label, LV_ALIGN_TOP_LEFT, 10, y_pos);
    y_pos += 30;

    s_gps_altitude_label = lv_label_create(scr);
    lv_label_set_text(s_gps_altitude_label, "Alt: -- m");
    lv_obj_set_style_text_color(s_gps_altitude_label, lv_color_hex(0x00FFFF), 0);  // 青色
    lv_obj_set_style_text_font(s_gps_altitude_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_gps_altitude_label, LV_ALIGN_TOP_LEFT, 10, y_pos);
    y_pos += 30;

    s_gps_time_label = lv_label_create(scr);
    lv_label_set_text(s_gps_time_label, "Time: --");
    lv_obj_set_style_text_color(s_gps_time_label, lv_color_hex(0x00FFFF), 0);  // 青色
    lv_obj_set_style_text_font(s_gps_time_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_gps_time_label, LV_ALIGN_TOP_LEFT, 10, y_pos);
    y_pos += 40;

    // IMU陀螺仪数据显示标签
    s_imu_gx_label = lv_label_create(scr);
    lv_label_set_text(s_imu_gx_label, "GX: --");
    lv_obj_set_style_text_color(s_imu_gx_label, lv_color_hex(0xFFFF00), 0);  // 黄色
    lv_obj_set_style_text_font(s_imu_gx_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_imu_gx_label, LV_ALIGN_TOP_LEFT, 10, y_pos);
    y_pos += 30;

    s_imu_gy_label = lv_label_create(scr);
    lv_label_set_text(s_imu_gy_label, "GY: --");
    lv_obj_set_style_text_color(s_imu_gy_label, lv_color_hex(0xFFFF00), 0);  // 黄色
    lv_obj_set_style_text_font(s_imu_gy_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_imu_gy_label, LV_ALIGN_TOP_LEFT, 10, y_pos);
    y_pos += 30;

    s_imu_gz_label = lv_label_create(scr);
    lv_label_set_text(s_imu_gz_label, "GZ: --");
    lv_obj_set_style_text_color(s_imu_gz_label, lv_color_hex(0xFFFF00), 0);  // 黄色
    lv_obj_set_style_text_font(s_imu_gz_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_imu_gz_label, LV_ALIGN_TOP_LEFT, 10, y_pos);
    y_pos += 40;

    // IMU磁力计数据显示标签
    s_imu_cx_label = lv_label_create(scr);
    lv_label_set_text(s_imu_cx_label, "CX: --");
    lv_obj_set_style_text_color(s_imu_cx_label, lv_color_hex(0x00FF00), 0);  // 绿色
    lv_obj_set_style_text_font(s_imu_cx_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_imu_cx_label, LV_ALIGN_TOP_LEFT, 10, y_pos);
    y_pos += 30;

    s_imu_cy_label = lv_label_create(scr);
    lv_label_set_text(s_imu_cy_label, "CY: --");
    lv_obj_set_style_text_color(s_imu_cy_label, lv_color_hex(0x00FF00), 0);  // 绿色
    lv_obj_set_style_text_font(s_imu_cy_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_imu_cy_label, LV_ALIGN_TOP_LEFT, 10, y_pos);
    y_pos += 30;

    s_imu_cz_label = lv_label_create(scr);
    lv_label_set_text(s_imu_cz_label, "CZ: --");
    lv_obj_set_style_text_color(s_imu_cz_label, lv_color_hex(0x00FF00), 0);  // 绿色
    lv_obj_set_style_text_font(s_imu_cz_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_imu_cz_label, LV_ALIGN_TOP_LEFT, 10, y_pos);
    y_pos += 40;

    // 温度数据显示标签
    s_temp_label = lv_label_create(scr);
    lv_label_set_text(s_temp_label, "Temp: N/A");
    lv_obj_set_style_text_color(s_temp_label, lv_color_hex(0xFF00FF), 0);  // 洋红色
    lv_obj_set_style_text_font(s_temp_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_temp_label, LV_ALIGN_TOP_LEFT, 10, y_pos);
    y_pos += 30;

    // 电压数据显示标签
    s_voltage_label = lv_label_create(scr);
    lv_label_set_text(s_voltage_label, "Voltage: --");
    lv_obj_set_style_text_color(s_voltage_label, lv_color_hex(0xFFA500), 0);  // 橙色
    lv_obj_set_style_text_font(s_voltage_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_voltage_label, LV_ALIGN_TOP_LEFT, 10, y_pos);
    y_pos += 30;

    if (s_lvgl_unlock) {
        s_lvgl_unlock();
    }

    ESP_LOGI(TAG, "BLE UI initialized");
}

/**
 * @brief 更新GPS数据显示
 */
static void update_gps_ui(void)
{
    bsp_gps_data_t gps_data;
    if (bsp_gps_read(&gps_data) != ESP_OK) {
        return;
    }

    if (s_lvgl_lock && !s_lvgl_lock(-1)) {
        return;
    }

    char buf[64];

    // 更新经度
    if (s_gps_longitude_label) {
        if (gps_data.is_valid) {
            snprintf(buf, sizeof(buf), "Lon: %.6f°", gps_data.longitude);
        } else {
            snprintf(buf, sizeof(buf), "Lon: --");
        }
        lv_label_set_text(s_gps_longitude_label, buf);
    }

    // 更新纬度
    if (s_gps_latitude_label) {
        if (gps_data.is_valid) {
            snprintf(buf, sizeof(buf), "Lat: %.6f°", gps_data.latitude);
        } else {
            snprintf(buf, sizeof(buf), "Lat: --");
        }
        lv_label_set_text(s_gps_latitude_label, buf);
    }

    // 更新海拔（从IMU读取，而不是GPS）
    if (s_gps_altitude_label) {
        bsp_imu_data_t imu_data;
        if (bsp_imu_read(&imu_data) == ESP_OK) {
            snprintf(buf, sizeof(buf), "Alt: %.2f m", imu_data.height);
        } else {
            snprintf(buf, sizeof(buf), "Alt: -- m");
        }
        lv_label_set_text(s_gps_altitude_label, buf);
    }

    // 更新时间
    if (s_gps_time_label) {
        if (gps_data.is_valid && gps_data.year > 0) {
            snprintf(buf, sizeof(buf), "Time: %04d-%02d-%02d %02d:%02d:%02d",
                     gps_data.year, gps_data.month, gps_data.day,
                     gps_data.hour, gps_data.minute, gps_data.second);
        } else {
            snprintf(buf, sizeof(buf), "Time: --");
        }
        lv_label_set_text(s_gps_time_label, buf);
    }

    if (s_lvgl_unlock) {
        s_lvgl_unlock();
    }
}

/**
 * @brief 更新IMU陀螺仪数据显示
 */
static void update_imu_ui(void)
{
    bsp_imu_data_t imu_data;
    if (bsp_imu_read(&imu_data) != ESP_OK) {
        return;
    }

    if (s_lvgl_lock && !s_lvgl_lock(-1)) {
        return;
    }

    char buf[32];

    // 更新陀螺仪数据
    if (s_imu_gx_label) {
        snprintf(buf, sizeof(buf), "GX: %d", imu_data.gx);
        lv_label_set_text(s_imu_gx_label, buf);
    }

    if (s_imu_gy_label) {
        snprintf(buf, sizeof(buf), "GY: %d", imu_data.gy);
        lv_label_set_text(s_imu_gy_label, buf);
    }

    if (s_imu_gz_label) {
        snprintf(buf, sizeof(buf), "GZ: %d", imu_data.gz);
        lv_label_set_text(s_imu_gz_label, buf);
    }

    // 更新磁力计数据
    if (s_imu_cx_label) {
        snprintf(buf, sizeof(buf), "CX: %.2f μT", imu_data.mx);
        lv_label_set_text(s_imu_cx_label, buf);
    }

    if (s_imu_cy_label) {
        snprintf(buf, sizeof(buf), "CY: %.2f μT", imu_data.my);
        lv_label_set_text(s_imu_cy_label, buf);
    }

    if (s_imu_cz_label) {
        snprintf(buf, sizeof(buf), "CZ: %.2f μT", imu_data.mz);
        lv_label_set_text(s_imu_cz_label, buf);
    }

    if (s_lvgl_unlock) {
        s_lvgl_unlock();
    }
}

/**
 * @brief 更新温度数据显示
 */
static void update_temp_ui(void)
{
    bsp_imu_data_t imu_data;
    if (bsp_imu_read(&imu_data) != ESP_OK) {
        return;
    }

    if (s_lvgl_lock && !s_lvgl_lock(-1)) {
        return;
    }

    char buf[32];
    if (s_temp_label) {
        snprintf(buf, sizeof(buf), "Temp: %.2f°C", imu_data.temperature);
        lv_label_set_text(s_temp_label, buf);
    }

    if (s_lvgl_unlock) {
        s_lvgl_unlock();
    }
}

/**
 * @brief 更新电压数据显示
 */
static void update_voltage_ui(void)
{
    Power_Info_t power_info;
    if (!Power_GetInfo_C(&power_info)) {
        return;
    }

    if (s_lvgl_lock && !s_lvgl_lock(-1)) {
        return;
    }

    char buf[32];
    if (s_voltage_label) {
        snprintf(buf, sizeof(buf), "Voltage: %d mV (%d%%)", 
                 power_info.voltage, power_info.usage);
        lv_label_set_text(s_voltage_label, buf);
    }

    if (s_lvgl_unlock) {
        s_lvgl_unlock();
    }
}

/**
 * @brief 更新CPU和FPS显示
 */
static void update_cpu_fps_ui(void)
{
    if (s_lvgl_lock && !s_lvgl_lock(-1)) {
        return;
    }

    char buf[32];
    uint32_t current_time = esp_timer_get_time() / 1000;  // 转换为毫秒
    
    // 更新FPS（每秒计算一次）
    if (current_time - s_last_fps_time >= 1000) {
        if (s_last_fps_time > 0) {
            s_current_fps = (float)s_frame_count * 1000.0f / (current_time - s_last_fps_time);
        }
        s_frame_count = 0;
        s_last_fps_time = current_time;
    }
    
    // 更新CPU占用率（每秒计算一次，使用简化的方法）
    if (current_time - s_last_cpu_time >= 1000) {
        // 使用FreeRTOS任务统计功能计算CPU占用率
        TaskStatus_t *pxTaskStatusArray;
        UBaseType_t uxArraySize;
        uint32_t ulTotalRunTime;
        
        uxArraySize = uxTaskGetNumberOfTasks();
        pxTaskStatusArray = (TaskStatus_t *)pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
        
        if (pxTaskStatusArray != NULL) {
            uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);
            
            // 查找空闲任务
            for (UBaseType_t x = 0; x < uxArraySize; x++) {
                if (pxTaskStatusArray[x].xHandle == xTaskGetIdleTaskHandle()) {
                    // CPU占用率 = 100% - (空闲时间 / 总时间) * 100%
                    if (ulTotalRunTime > 0) {
                        s_current_cpu = 100.0f - ((float)pxTaskStatusArray[x].ulRunTimeCounter / (float)ulTotalRunTime * 100.0f);
                        if (s_current_cpu < 0.0f) s_current_cpu = 0.0f;
                        if (s_current_cpu > 100.0f) s_current_cpu = 100.0f;
                    }
                    break;
                }
            }
            
            vPortFree(pxTaskStatusArray);
        }
        
        s_last_cpu_time = current_time;
    }
    
    // 更新CPU标签
    if (s_cpu_label) {
        snprintf(buf, sizeof(buf), "CPU: %.1f%%", s_current_cpu);
        lv_label_set_text(s_cpu_label, buf);
    }
    
    // 更新FPS标签
    if (s_fps_label) {
        snprintf(buf, sizeof(buf), "FPS: %.1f", s_current_fps);
        lv_label_set_text(s_fps_label, buf);
    }
    
    if (s_lvgl_unlock) {
        s_lvgl_unlock();
    }
}

/**
 * @brief 通知帧刷新（在LVGL flush回调中调用）
 */
void bsp_ble_ui_notify_frame(void)
{
    s_frame_count++;
}

/**
 * @brief 更新BLE UI（需要在主循环中定期调用）
 */
void bsp_ble_ui_update(void)
{
    // 更新BLE数据显示
    update_heart_rate_ui(bsp_ble_get_heart_rate());
    update_speed_ui(bsp_ble_get_speed());
    update_cadence_ui(bsp_ble_get_cadence());

    // 更新GPS数据显示
    update_gps_ui();

    // 更新IMU陀螺仪数据显示
    update_imu_ui();

    // 更新温度数据显示
    update_temp_ui();
    
    // 更新电压数据显示
    update_voltage_ui();
    
    // 更新CPU和FPS显示
    update_cpu_fps_ui();
}
