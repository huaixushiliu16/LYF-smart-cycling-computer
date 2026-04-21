#include "SystemMonitor.h"
#include "ResourcePool.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "SystemMonitor";

// CPU和FPS显示标签
static lv_obj_t *s_cpu_label = NULL;
static lv_obj_t *s_fps_label = NULL;

// CPU和FPS统计变量
static uint32_t s_frame_count = 0;
static uint32_t s_last_fps_time = 0;
static float s_current_fps = 0.0f;
static uint32_t s_last_cpu_time = 0;
static float s_current_cpu = 0.0f;

// 定时器
static lv_timer_t *s_update_timer = NULL;

/**
 * @brief 更新CPU和FPS显示
 */
static void SystemMonitor_UpdateTimer(lv_timer_t *timer)
{
    (void)timer;
    
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
    
    // 更新CPU占用率（每秒计算一次，使用FreeRTOS任务统计功能）
    if (current_time - s_last_cpu_time >= 1000) {
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
}

void SystemMonitor_Create(void)
{
    // 检查是否已创建（避免重复创建）
    if (s_cpu_label != NULL || s_fps_label != NULL) {
        ESP_LOGW(TAG, "SystemMonitor already created");
        return;
    }
    
    ESP_LOGI(TAG, "Creating SystemMonitor...");
    
    // 在lv_layer_top()上创建，全局可见
    lv_obj_t *top_layer = lv_layer_top();
    
    // 创建CPU标签
    s_cpu_label = lv_label_create(top_layer);
    lv_label_set_text(s_cpu_label, "CPU: --%");
    lv_obj_set_style_text_color(s_cpu_label, lv_color_hex(0xFFAA00), 0);  // 橙色
    lv_obj_set_style_text_font(s_cpu_label, ResourcePool::GetFont("bahnschrift_13"), 0);
    lv_obj_align(s_cpu_label, LV_ALIGN_BOTTOM_RIGHT, -10, -5);  // 右下角
    lv_obj_clear_flag(s_cpu_label, LV_OBJ_FLAG_SCROLLABLE);  // 禁用滚动
    
    // 创建FPS标签
    s_fps_label = lv_label_create(top_layer);
    lv_label_set_text(s_fps_label, "FPS: --");
    lv_obj_set_style_text_color(s_fps_label, lv_color_hex(0xFFAA00), 0);  // 橙色
    lv_obj_set_style_text_font(s_fps_label, ResourcePool::GetFont("bahnschrift_13"), 0);
    lv_obj_align(s_fps_label, LV_ALIGN_BOTTOM_RIGHT, -10, -25);  // 右下角，在CPU上方
    lv_obj_clear_flag(s_fps_label, LV_OBJ_FLAG_SCROLLABLE);  // 禁用滚动
    
    // 初始化统计变量
    s_frame_count = 0;
    s_last_fps_time = esp_timer_get_time() / 1000;  // 转换为毫秒
    s_current_fps = 0.0f;
    s_last_cpu_time = esp_timer_get_time() / 1000;
    s_current_cpu = 0.0f;
    
    // 创建定时器（100ms更新一次）
    s_update_timer = lv_timer_create(SystemMonitor_UpdateTimer, 100, nullptr);
    lv_timer_ready(s_update_timer);
    
    ESP_LOGI(TAG, "SystemMonitor created successfully");
}

void SystemMonitor_Update(void)
{
    // 由定时器自动更新，此函数保留用于兼容性
}

void SystemMonitor_NotifyFrame(void)
{
    s_frame_count++;
}
