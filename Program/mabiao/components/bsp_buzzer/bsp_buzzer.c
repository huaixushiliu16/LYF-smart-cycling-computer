/**
 * @file bsp_buzzer.c
 * @brief Passive buzzer on GPIO19 — LEDC ~4kHz square wave, active high (per schematic).
 */

#include "bsp_buzzer.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
static const char *TAG = "BSP_BUZZ";

/* 原理图：IO19 → BUZZER → NPN 低端开关，高电平导通 */
#define BUZZER_GPIO           GPIO_NUM_19
#define BUZZER_FREQ_HZ        4000

#define LEDC_TIMER            LEDC_TIMER_2
#define LEDC_MODE             LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL          LEDC_CHANNEL_5
#define LEDC_DUTY_RES         LEDC_TIMER_10_BIT
#define LEDC_DUTY_HALF        (1 << (10 - 1)) /* 50% */

#define BUZZ_QUEUE_LEN        6
#define BUZZ_TASK_STACK       2048
#define BUZZ_TASK_PRIO        3

static bool s_inited = false;
static QueueHandle_t s_queue;

static void buzzer_pwm_on(void)
{
    if (!s_inited) {
        return;
    }
    esp_err_t e = ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY_HALF);
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "ledc_set_duty on failed: %s", esp_err_to_name(e));
        return;
    }
    e = ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "ledc_update_duty on failed: %s", esp_err_to_name(e));
    }
}

static void buzzer_pwm_off(void)
{
    if (!s_inited) {
        return;
    }
    esp_err_t e = ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
    if (e != ESP_OK) {
        return;
    }
    (void)ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

static void play_pattern(bsp_buzz_pattern_t pattern)
{
    switch (pattern) {
    case BSP_BUZZ_PATTERN_CONNECT:
        buzzer_pwm_on();
        vTaskDelay(pdMS_TO_TICKS(70));
        buzzer_pwm_off();
        break;
    case BSP_BUZZ_PATTERN_DISCONNECT:
        buzzer_pwm_on();
        vTaskDelay(pdMS_TO_TICKS(45));
        buzzer_pwm_off();
        vTaskDelay(pdMS_TO_TICKS(90));
        buzzer_pwm_on();
        vTaskDelay(pdMS_TO_TICKS(45));
        buzzer_pwm_off();
        break;
    default:
        buzzer_pwm_off();
        break;
    }
}

static void buzzer_task(void *arg)
{
    (void)arg;
    bsp_buzz_pattern_t pattern;

    for (;;) {
        if (xQueueReceive(s_queue, &pattern, portMAX_DELAY) == pdTRUE) {
            play_pattern(pattern);
        }
    }
}

esp_err_t bsp_buzzer_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    ledc_timer_config_t tcfg = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = BUZZER_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&tcfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ledc_channel_config_t ccfg = {
        .gpio_num = BUZZER_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ret = ledc_channel_config(&ccfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_queue = xQueueCreate(BUZZ_QUEUE_LEN, sizeof(bsp_buzz_pattern_t));
    if (s_queue == NULL) {
        ESP_LOGE(TAG, "queue create failed");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ok = xTaskCreate(buzzer_task, "buzzer", BUZZ_TASK_STACK, NULL, BUZZ_TASK_PRIO, NULL);
    if (ok != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        ESP_LOGE(TAG, "task create failed");
        return ESP_ERR_NO_MEM;
    }

    s_inited = true;
    ESP_LOGI(TAG, "Passive buzzer init OK (GPIO%d, %dHz PWM)", (int)BUZZER_GPIO, BUZZER_FREQ_HZ);
    return ESP_OK;
}

void bsp_buzzer_request(bsp_buzz_pattern_t pattern)
{
    if (!s_inited || s_queue == NULL) {
        return;
    }
    (void)xQueueSend(s_queue, &pattern, 0);
}
