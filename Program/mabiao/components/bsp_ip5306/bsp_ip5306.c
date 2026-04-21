/**
 * @file bsp_ip5306.c
 * @brief IP5306电源管理驱动实现（BSP层）
 * @note 阶段6：电源驱动开发
 */

#include "bsp_ip5306.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "BSP_IP5306";

// IP5306配置
#define IP5306_WAKE_GPIO          GPIO_NUM_21  // KEY引脚
#define IP5306_WAKE_INTERVAL_MS   10000        // 激活间隔：10秒
#define IP5306_WAKE_PULSE_MS      100          // 脉冲宽度：100ms

static TaskHandle_t ip5306_task_handle = NULL;
static volatile bool ip5306_enabled = false;

/**
 * @brief 触发一次激活脉冲
 */
static void ip5306_pulse_once(void)
{
    // IP5306 KEY为低有效（模拟按下=拉低）
    gpio_set_level(IP5306_WAKE_GPIO, 0);
    ESP_LOGD(TAG, "IP5306 wake pulse: GPIO%d -> LOW", IP5306_WAKE_GPIO);
    vTaskDelay(pdMS_TO_TICKS(IP5306_WAKE_PULSE_MS));
    gpio_set_level(IP5306_WAKE_GPIO, 1);
    ESP_LOGD(TAG, "IP5306 wake pulse: GPIO%d -> HIGH", IP5306_WAKE_GPIO);
}

/**
 * @brief IP5306周期激活任务
 */
static void ip5306_keepalive_task(void *param)
{
    (void)param;
    
    while (1) {
        if (ip5306_enabled) {
            // 每10秒激活一次
            vTaskDelay(pdMS_TO_TICKS(IP5306_WAKE_INTERVAL_MS));
            ESP_LOGD(TAG, "IP5306 periodic activation trigger");
            ip5306_pulse_once();
        } else {
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}

esp_err_t bsp_ip5306_init(void)
{
    // 配置GPIO为开漏输出 + 上拉：更贴近"按键到GND"的行为
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT_OD,
        .pin_bit_mask = (1ULL << IP5306_WAKE_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure IP5306 wake GPIO: %s", esp_err_to_name(err));
        return err;
    }
    
    // 默认保持高电平
    gpio_set_level(IP5306_WAKE_GPIO, 1);
    
    // 初始化阶段：立即执行一次"拉低 100ms -> 拉高"触发电源激活
    ESP_LOGI(TAG, "IP5306 initial activation trigger");
    ip5306_pulse_once();
    
    // 创建后台任务
    if (ip5306_task_handle == NULL) {
        BaseType_t ret = xTaskCreate(ip5306_keepalive_task, "IP5306_KeepAlive", 2048, NULL, 5, &ip5306_task_handle);
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create IP5306 keepalive task");
            ip5306_task_handle = NULL;
            return ESP_FAIL;
        }
    }
    
    ip5306_enabled = false;
    ESP_LOGI(TAG, "IP5306 keepalive initialized (GPIO%d, interval=%dms, pulse=%dms)",
             IP5306_WAKE_GPIO, IP5306_WAKE_INTERVAL_MS, IP5306_WAKE_PULSE_MS);
    
    return ESP_OK;
}

void bsp_ip5306_start(void)
{
    if (ip5306_task_handle == NULL) {
        ESP_LOGE(TAG, "IP5306 not initialized");
        return;
    }
    
    ip5306_enabled = true;
    ESP_LOGI(TAG, "IP5306 keepalive started");
}

void bsp_ip5306_stop(void)
{
    ip5306_enabled = false;
    ESP_LOGI(TAG, "IP5306 keepalive stopped");
}

void bsp_ip5306_trigger(void)
{
    // 手动触发一次脉冲（用于测试）
    ESP_LOGI(TAG, "IP5306 manual trigger");
    ip5306_pulse_once();
}
