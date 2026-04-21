/**
 * @file bsp_rgb_led.c
 * @brief RGB LED驱动实现（BSP层）
 * @note WS2812 数据脚默认 GPIO20（RGB_DIN，与硬件原理图一致）
 */

#include "bsp_rgb_led.h"
#include "led_strip.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <math.h>

static const char *TAG = "BSP_RGB_LED";

// 默认配置
#define RGB_LED_GPIO_DEFAULT    20
#define RGB_LED_MAX_LEDS_DEFAULT 1

// 更新周期配置
#define RGB_LED_UPDATE_PERIOD_MS    30  // 更新周期（毫秒），实现较快变化
#define RGB_LED_DEFAULT_BRIGHTNESS   60  // 默认亮度（0-100）
#define RGB_LED_DEFAULT_SPEED        50  // 默认速度（0-100）

// 渐变色关键点（HSV色相值，0-360）
#define RGB_LED_COLOR_COUNT 6
static const uint16_t s_color_points[RGB_LED_COLOR_COUNT] = {
    0,    // 红色
    60,   // 黄色
    120,  // 绿色
    180,  // 青色
    240,  // 蓝色
    300   // 紫色
};

static bool s_initialized = false;
static bool s_running = false;
static led_strip_handle_t s_led_strip = NULL;
static TaskHandle_t s_task_handle = NULL;

// 当前模式状态
static bsp_rgb_mode_t s_current_mode = BSP_RGB_MODE_OFF;
static uint8_t s_brightness = RGB_LED_DEFAULT_BRIGHTNESS;
static uint8_t s_speed = RGB_LED_DEFAULT_SPEED;
static bsp_rgb_color_t s_solid_color = {255, 255, 255};  // 默认白色
static uint16_t s_heart_rate_bpm = 0;  // 心率值（用于心率同步模式）

// 彩虹渐变状态
static uint8_t s_color_index = 0;  // 当前颜色索引
static uint16_t s_hue_progress = 0; // 颜色过渡进度（0-360）

// 其他模式的状态变量
static uint32_t s_mode_timer = 0;  // 模式内部计时器
static uint32_t s_random_seed = 0;  // 随机数种子

/**
 * @brief HSV转RGB颜色转换函数（参考RDB项目实现）
 * @param h 色相 (0-360)
 * @param s 饱和度 (0-100)
 * @param v 亮度 (0-100)
 * @param r 输出红色分量 (0-255)
 * @param g 输出绿色分量 (0-255)
 * @param b 输出蓝色分量 (0-255)
 */
static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t region, remainder;
    uint8_t p, q, t;
    uint8_t s_scaled, v_scaled;

    // 将s和v从0-100范围缩放到0-255范围
    s_scaled = (s * 255) / 100;
    v_scaled = (v * 255) / 100;

    if (s == 0) {
        *r = *g = *b = v_scaled;
        return;
    }

    // 将h限制在0-360范围内，然后转换为0-60的region
    h = h % 360;
    region = h / 60;
    remainder = ((h % 60) * 255) / 60;

    p = (v_scaled * (255 - s_scaled)) / 255;
    q = (v_scaled * (255 - ((s_scaled * remainder) / 255))) / 255;
    t = (v_scaled * (255 - ((s_scaled * (255 - remainder)) / 255))) / 255;

    switch (region) {
        case 0:
            *r = v_scaled; *g = t; *b = p;
            break;
        case 1:
            *r = q; *g = v_scaled; *b = p;
            break;
        case 2:
            *r = p; *g = v_scaled; *b = t;
            break;
        case 3:
            *r = p; *g = q; *b = v_scaled;
            break;
        case 4:
            *r = t; *g = p; *b = v_scaled;
            break;
        default:  // case 5
            *r = v_scaled; *g = p; *b = q;
            break;
    }
}

// 简单随机数生成器（线性同余法）
static uint32_t simple_random(void)
{
    s_random_seed = (s_random_seed * 1103515245 + 12345) & 0x7fffffff;
    return s_random_seed;
}

/**
 * @brief 根据心率区间获取颜色
 */
static void get_heart_rate_color(uint16_t bpm, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (bpm < 60) {
        *r = 0; *g = 0; *b = 100;  // 深蓝（过低）
    } else if (bpm < 80) {
        *r = 0; *g = 100; *b = 200;  // 青色（静息）
    } else if (bpm < 120) {
        *r = 0; *g = 200; *b = 100;  // 绿色（有氧）
    } else if (bpm < 150) {
        *r = 255; *g = 150; *b = 0;  // 橙色（燃脂）
    } else if (bpm < 180) {
        *r = 255; *g = 50; *b = 0;    // 红色（无氧）
    } else {
        *r = 255; *g = 255; *b = 255;  // 白色（极限）
    }
}

/**
 * @brief 彩虹渐变模式更新
 */
static void rgb_mode_rainbow_update(void)
{
    if (!s_led_strip || !s_running) {
        return;
    }

    uint8_t r, g, b;
    uint16_t target_hue;
    uint16_t start_hue, end_hue;
    
    // 获取当前颜色段的起始和结束色相
    start_hue = s_color_points[s_color_index];
    end_hue = s_color_points[(s_color_index + 1) % RGB_LED_COLOR_COUNT];
    
    // 计算目标色相（处理色相环绕）
    if (end_hue < start_hue) {
        // 跨越360度边界（如300->0）
        target_hue = start_hue + ((360 + end_hue - start_hue) * s_hue_progress) / 360;
        if (target_hue >= 360) {
            target_hue -= 360;
        }
    } else {
        // 正常过渡
        target_hue = start_hue + ((end_hue - start_hue) * s_hue_progress) / 360;
    }
    
    // HSV转RGB，应用亮度
    uint8_t brightness = (s_brightness * 255) / 100;
    hsv_to_rgb(target_hue, 100, brightness, &r, &g, &b);
    
    // 设置LED颜色
    led_strip_set_pixel(s_led_strip, 0, r, g, b);
    led_strip_refresh(s_led_strip);
    
    // 更新进度（根据速度调整）
    uint8_t speed_factor = (s_speed * 5) / 100 + 1;  // 1-5度/次
    s_hue_progress += speed_factor;
    
    // 检查是否完成当前颜色段
    if (s_hue_progress >= 360) {
        s_hue_progress = 0;
        s_color_index = (s_color_index + 1) % RGB_LED_COLOR_COUNT;
    }
}

/**
 * @brief 闪电闪烁模式更新
 */
static void rgb_mode_lightning_update(void)
{
    if (!s_led_strip || !s_running) {
        return;
    }

    uint8_t r, g, b;
    uint32_t random_val = simple_random();
    
    // 随机闪烁（约10%概率）
    if ((random_val % 100) < 10) {
        // 闪电效果：白色或蓝色
        if (random_val % 2) {
            r = 255; g = 255; b = 255;  // 白色
        } else {
            r = 100; g = 150; b = 255;  // 蓝色
        }
        // 应用亮度
        r = (r * s_brightness) / 100;
        g = (g * s_brightness) / 100;
        b = (b * s_brightness) / 100;
    } else {
        // 大部分时间保持较暗
        r = 0; g = 0; b = (20 * s_brightness) / 100;
    }
    
    led_strip_set_pixel(s_led_strip, 0, r, g, b);
    led_strip_refresh(s_led_strip);
}

/**
 * @brief 火焰效果模式更新
 */
static void rgb_mode_fire_update(void)
{
    if (!s_led_strip || !s_running) {
        return;
    }

    uint8_t r, g, b;
    uint32_t random_val = simple_random();
    
    // 火焰颜色：橙红色，带随机波动
    uint8_t base_r = 255;
    uint8_t base_g = 100;
    uint8_t base_b = 0;
    
    // 添加随机波动（模拟火焰跳动）
    int8_t variation = (random_val % 40) - 20;  // -20到+20
    r = base_r;
    g = base_g + variation;
    if (g > 200) g = 200;
    if (g < 50) g = 50;
    b = base_b;
    
    // 应用亮度
    r = (r * s_brightness) / 100;
    g = (g * s_brightness) / 100;
    b = (b * s_brightness) / 100;
    
    led_strip_set_pixel(s_led_strip, 0, r, g, b);
    led_strip_refresh(s_led_strip);
}

/**
 * @brief 星空闪烁模式更新
 */
static void rgb_mode_starry_update(void)
{
    if (!s_led_strip || !s_running) {
        return;
    }

    uint8_t r, g, b;
    uint32_t random_val = simple_random();
    
    // 随机闪烁（约5%概率）
    if ((random_val % 100) < 5) {
        // 随机颜色
        uint8_t hue = random_val % 360;
        uint8_t brightness = (s_brightness * 255) / 100;
        hsv_to_rgb(hue, 100, brightness, &r, &g, &b);
    } else {
        // 大部分时间保持较暗
        r = 0; g = 0; b = 0;
    }
    
    led_strip_set_pixel(s_led_strip, 0, r, g, b);
    led_strip_refresh(s_led_strip);
}

/**
 * @brief 波浪效果模式更新
 */
static void rgb_mode_wave_update(void)
{
    if (!s_led_strip || !s_running) {
        return;
    }

    uint8_t r, g, b;
    
    // 使用正弦波创建波浪效果
    // 速度影响波浪频率
    uint16_t wave_period = 360;
    uint16_t wave_pos = (s_mode_timer * (s_speed + 10)) % wave_period;
    
    // 计算正弦波值（0-360度色相）
    // 简化：使用线性近似
    uint16_t hue = (wave_pos * 360) / wave_period;
    
    uint8_t brightness = (s_brightness * 255) / 100;
    hsv_to_rgb(hue, 100, brightness, &r, &g, &b);
    
    led_strip_set_pixel(s_led_strip, 0, r, g, b);
    led_strip_refresh(s_led_strip);
    
    s_mode_timer++;
}

/**
 * @brief 烟花爆炸模式更新
 */
static void rgb_mode_fireworks_update(void)
{
    if (!s_led_strip || !s_running) {
        return;
    }

    uint8_t r, g, b;
    uint32_t random_val = simple_random();
    
    // 随机爆发（约3%概率）
    if ((random_val % 100) < 3) {
        // 随机颜色爆发
        uint8_t hue = random_val % 360;
        uint8_t brightness = (s_brightness * 255) / 100;
        hsv_to_rgb(hue, 100, brightness, &r, &g, &b);
    } else {
        // 快速衰减
        static uint8_t fade_r = 0, fade_g = 0, fade_b = 0;
        fade_r = (fade_r * 9) / 10;
        fade_g = (fade_g * 9) / 10;
        fade_b = (fade_b * 9) / 10;
        r = fade_r; g = fade_g; b = fade_b;
    }
    
    led_strip_set_pixel(s_led_strip, 0, r, g, b);
    led_strip_refresh(s_led_strip);
}

/**
 * @brief 心跳效果模式更新（固定频率）
 */
static void rgb_mode_heartbeat_update(void)
{
    if (!s_led_strip || !s_running) {
        return;
    }

    uint8_t r, g, b;
    
    // 默认心率：72 BPM
    uint16_t default_bpm = 72;
    uint32_t period_ms = 60000 / default_bpm;
    uint32_t elapsed = s_mode_timer * RGB_LED_UPDATE_PERIOD_MS;
    uint32_t progress_ms = elapsed % period_ms;
    float progress = (float)progress_ms / period_ms;
    
    // 心跳脉冲波形（快速上升，缓慢下降）
    float pulse = 0.0f;
    if (progress < 0.1f) {
        pulse = progress / 0.1f;  // 快速上升
    } else {
        pulse = 1.0f - (progress - 0.1f) / 0.9f;
        pulse = pulse * pulse;  // 平方衰减
    }
    
    // 红色心跳
    uint8_t base_brightness = (s_brightness * 255) / 100;
    uint16_t pulse_brightness_temp = base_brightness + (uint16_t)(base_brightness * pulse * 0.5f);
    uint8_t pulse_brightness = (pulse_brightness_temp > 255) ? 255 : (uint8_t)pulse_brightness_temp;
    
    r = pulse_brightness;
    g = (pulse_brightness * 20) / 100;  // 略带粉色
    b = (pulse_brightness * 10) / 100;
    
    led_strip_set_pixel(s_led_strip, 0, r, g, b);
    led_strip_refresh(s_led_strip);
    
    s_mode_timer++;
}

/**
 * @brief 心率同步模式更新
 */
static void rgb_mode_heart_rate_sync_update(void)
{
    if (!s_led_strip || !s_running) {
        return;
    }

    uint8_t r, g, b;
    
    // 检查心率数据有效性
    if (s_heart_rate_bpm < 30 || s_heart_rate_bpm > 220) {
        // 无效心率：直接熄灯，避免“开着但不亮/颜色不一致”的困惑
        r = 0; g = 0; b = 0;
        led_strip_set_pixel(s_led_strip, 0, r, g, b);
        led_strip_refresh(s_led_strip);
        return;
    }
    
    // 计算心跳周期（毫秒）
    uint32_t period_ms = 60000 / s_heart_rate_bpm;
    uint32_t elapsed = s_mode_timer * RGB_LED_UPDATE_PERIOD_MS;
    uint32_t progress_ms = elapsed % period_ms;
    float progress = (float)progress_ms / period_ms;
    
    // 心跳脉冲波形（快速上升，缓慢下降）
    float pulse = 0.0f;
    if (progress < 0.1f) {
        pulse = progress / 0.1f;  // 快速上升（0-10%周期）
    } else {
        pulse = 1.0f - (progress - 0.1f) / 0.9f;
        pulse = pulse * pulse;  // 平方衰减
    }
    
    // 固定红色（按 bpm 改变闪烁频率），不随心率区间变色
    uint8_t base_r = 255, base_g = 0, base_b = 0;
    
    // 应用脉冲亮度
    uint8_t base_brightness = (s_brightness * 255) / 100;
    uint8_t pulse_amplitude = (base_brightness * 50) / 100;  // 50%幅度
    uint16_t current_brightness_temp = base_brightness + (uint16_t)(pulse_amplitude * pulse);
    uint8_t current_brightness = (current_brightness_temp > 255) ? 255 : (uint8_t)current_brightness_temp;
    
    r = (base_r * current_brightness) / 255;
    g = (base_g * current_brightness) / 255;
    b = (base_b * current_brightness) / 255;
    
    led_strip_set_pixel(s_led_strip, 0, r, g, b);
    led_strip_refresh(s_led_strip);
    
    s_mode_timer++;
}

/**
 * @brief 螺旋旋转模式更新
 */
static void rgb_mode_spiral_update(void)
{
    if (!s_led_strip || !s_running) {
        return;
    }

    uint8_t r, g, b;
    
    // 螺旋效果：色相旋转
    uint16_t hue = (s_mode_timer * (s_speed + 5)) % 360;
    
    uint8_t brightness = (s_brightness * 255) / 100;
    hsv_to_rgb(hue, 100, brightness, &r, &g, &b);
    
    led_strip_set_pixel(s_led_strip, 0, r, g, b);
    led_strip_refresh(s_led_strip);
    
    s_mode_timer++;
}

/**
 * @brief 色块跳跃模式更新
 */
static void rgb_mode_color_block_update(void)
{
    if (!s_led_strip || !s_running) {
        return;
    }

    uint8_t r, g, b;
    
    // 根据速度切换颜色块
    uint16_t block_duration = 1000 - (s_speed * 8);  // 100-1000ms
    if (block_duration < 100) block_duration = 100;
    
    uint32_t block_index = (s_mode_timer * RGB_LED_UPDATE_PERIOD_MS) / block_duration;
    
    // 预定义颜色块
    static const bsp_rgb_color_t color_blocks[] = {
        {255, 0, 0},    // 红
        {0, 255, 0},    // 绿
        {0, 0, 255},    // 蓝
        {255, 255, 0},  // 黄
        {255, 0, 255},  // 紫
        {0, 255, 255},  // 青
    };
    uint8_t block_count = sizeof(color_blocks) / sizeof(color_blocks[0]);
    bsp_rgb_color_t color = color_blocks[block_index % block_count];
    
    // 应用亮度
    r = (color.r * s_brightness) / 100;
    g = (color.g * s_brightness) / 100;
    b = (color.b * s_brightness) / 100;
    
    led_strip_set_pixel(s_led_strip, 0, r, g, b);
    led_strip_refresh(s_led_strip);
}

/**
 * @brief 极光效果模式更新
 */
static void rgb_mode_aurora_update(void)
{
    if (!s_led_strip || !s_running) {
        return;
    }

    uint8_t r, g, b;
    
    // 极光效果：绿色/蓝色/紫色渐变流动
    uint16_t base_hue = 180;  // 青色起始
    uint16_t hue_variation = (s_mode_timer * (s_speed + 3)) % 120;  // 0-120度变化（青到紫）
    uint16_t hue = (base_hue + hue_variation) % 360;
    
    uint8_t brightness = (s_brightness * 255) / 100;
    hsv_to_rgb(hue, 80, brightness, &r, &g, &b);  // 降低饱和度，更柔和
    
    led_strip_set_pixel(s_led_strip, 0, r, g, b);
    led_strip_refresh(s_led_strip);
    
    s_mode_timer++;
}

/**
 * @brief 纯色模式更新
 */
static void rgb_mode_solid_color_update(void)
{
    if (!s_led_strip || !s_running) {
        return;
    }

    uint8_t r, g, b;
    
    // 应用亮度和颜色
    r = (s_solid_color.r * s_brightness) / 100;
    g = (s_solid_color.g * s_brightness) / 100;
    b = (s_solid_color.b * s_brightness) / 100;
    
    led_strip_set_pixel(s_led_strip, 0, r, g, b);
    led_strip_refresh(s_led_strip);
}

/**
 * @brief 根据当前模式调用相应的更新函数
 */
static void rgb_mode_update(void)
{
    switch (s_current_mode) {
        case BSP_RGB_MODE_OFF:
            // 关闭模式，清除LED
            if (s_led_strip) {
                led_strip_clear(s_led_strip);
            }
            break;
        case BSP_RGB_MODE_RAINBOW:
            rgb_mode_rainbow_update();
            break;
        case BSP_RGB_MODE_LIGHTNING:
            rgb_mode_lightning_update();
            break;
        case BSP_RGB_MODE_FIRE:
            rgb_mode_fire_update();
            break;
        case BSP_RGB_MODE_STARRY:
            rgb_mode_starry_update();
            break;
        case BSP_RGB_MODE_WAVE:
            rgb_mode_wave_update();
            break;
        case BSP_RGB_MODE_FIREWORKS:
            rgb_mode_fireworks_update();
            break;
        case BSP_RGB_MODE_HEARTBEAT:
            rgb_mode_heartbeat_update();
            break;
        case BSP_RGB_MODE_HEART_RATE_SYNC:
            rgb_mode_heart_rate_sync_update();
            break;
        case BSP_RGB_MODE_SPIRAL:
            rgb_mode_spiral_update();
            break;
        case BSP_RGB_MODE_COLOR_BLOCK:
            rgb_mode_color_block_update();
            break;
        case BSP_RGB_MODE_AURORA:
            rgb_mode_aurora_update();
            break;
        case BSP_RGB_MODE_SOLID_COLOR:
            rgb_mode_solid_color_update();
            break;
        default:
            break;
    }
}

/**
 * @brief RGB LED效果任务
 */
static void rgb_led_task(void *pvParameters)
{
    ESP_LOGI(TAG, "RGB LED task started");
    
    while (s_running) {
        rgb_mode_update();
        vTaskDelay(pdMS_TO_TICKS(RGB_LED_UPDATE_PERIOD_MS));
    }
    
    ESP_LOGI(TAG, "RGB LED task stopped");
    s_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t bsp_rgb_led_init(const bsp_rgb_led_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "RGB LED already initialized");
        return ESP_OK;
    }

    uint8_t gpio_num = config ? config->gpio_num : RGB_LED_GPIO_DEFAULT;
    uint8_t max_leds = config ? config->max_leds : RGB_LED_MAX_LEDS_DEFAULT;

    ESP_LOGI(TAG, "Initializing RGB LED on GPIO%d (WS2812)", gpio_num);

    // LED strip初始化配置
    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio_num,
        .max_leds = max_leds,
    };

    // RMT后端配置（ESP32-S3支持RMT）
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT device: %s", esp_err_to_name(ret));
        return ret;
    }

    // 清除所有LED
    led_strip_clear(s_led_strip);
    
    s_initialized = true;
    ESP_LOGI(TAG, "RGB LED initialized successfully (RMT backend)");
    
    return ESP_OK;
}

esp_err_t bsp_rgb_led_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    // 停止运行
    bsp_rgb_led_stop();

    // 等待任务结束
    if (s_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // 删除LED strip
    if (s_led_strip != NULL) {
        led_strip_del(s_led_strip);
        s_led_strip = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "RGB LED deinitialized");
    
    return ESP_OK;
}

esp_err_t bsp_rgb_led_start(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "RGB LED not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_running) {
        ESP_LOGW(TAG, "RGB LED already running");
        return ESP_OK;
    }

    s_running = true;
    
    // 重置模式状态
    s_color_index = 0;
    s_hue_progress = 0;
    s_mode_timer = 0;
    s_random_seed = (uint32_t)esp_timer_get_time();  // 使用时间戳作为随机种子

    // 创建任务
    BaseType_t ret = xTaskCreate(
        rgb_led_task,
        "rgb_led_task",
        2048,
        NULL,
        2,  // 较低优先级
        &s_task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RGB LED task");
        s_running = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "RGB LED effect started (mode: %d)", s_current_mode);
    return ESP_OK;
}

esp_err_t bsp_rgb_led_stop(void)
{
    if (!s_running) {
        return ESP_OK;
    }

    s_running = false;
    
    // 等待任务结束
    if (s_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // 清除LED
    if (s_led_strip != NULL) {
        led_strip_clear(s_led_strip);
    }

    ESP_LOGI(TAG, "RGB LED effect stopped");
    return ESP_OK;
}

esp_err_t bsp_rgb_led_set_color(const bsp_rgb_color_t *color)
{
    if (!s_initialized || !s_led_strip) {
        ESP_LOGE(TAG, "RGB LED not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (color == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 停止自动效果
    if (s_running) {
        bsp_rgb_led_stop();
    }

    // 设置颜色
    led_strip_set_pixel(s_led_strip, 0, color->r, color->g, color->b);
    led_strip_refresh(s_led_strip);

    return ESP_OK;
}

esp_err_t bsp_rgb_led_clear(void)
{
    if (!s_initialized || !s_led_strip) {
        return ESP_ERR_INVALID_STATE;
    }

    led_strip_clear(s_led_strip);
    return ESP_OK;
}

esp_err_t bsp_rgb_led_update(void)
{
    if (!s_initialized || !s_running) {
        return ESP_OK;
    }

    rgb_mode_update();
    return ESP_OK;
}

esp_err_t bsp_rgb_led_set_mode(bsp_rgb_mode_t mode)
{
    if (mode >= BSP_RGB_MODE_MAX) {
        ESP_LOGE(TAG, "Invalid RGB mode: %d", mode);
        return ESP_ERR_INVALID_ARG;
    }

    s_current_mode = mode;
    
    // 重置模式状态
    s_color_index = 0;
    s_hue_progress = 0;
    s_mode_timer = 0;
    
    // 如果模式是关闭，停止运行
    if (mode == BSP_RGB_MODE_OFF) {
        bsp_rgb_led_stop();
    }
    
    ESP_LOGI(TAG, "RGB mode set to: %d", mode);
    return ESP_OK;
}

esp_err_t bsp_rgb_led_set_brightness(uint8_t brightness)
{
    if (brightness > 100) {
        brightness = 100;
    }
    
    s_brightness = brightness;
    ESP_LOGD(TAG, "RGB brightness set to: %d%%", brightness);
    return ESP_OK;
}

esp_err_t bsp_rgb_led_set_speed(uint8_t speed)
{
    if (speed > 100) {
        speed = 100;
    }
    
    s_speed = speed;
    ESP_LOGD(TAG, "RGB speed set to: %d%%", speed);
    return ESP_OK;
}

esp_err_t bsp_rgb_led_set_heart_rate(uint16_t heart_rate_bpm)
{
    s_heart_rate_bpm = heart_rate_bpm;
    ESP_LOGD(TAG, "Heart rate set to: %d bpm", heart_rate_bpm);
    return ESP_OK;
}

esp_err_t bsp_rgb_led_set_solid_color(const bsp_rgb_color_t *color)
{
    if (color == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    s_solid_color = *color;
    ESP_LOGD(TAG, "Solid color set to: R=%d G=%d B=%d", color->r, color->g, color->b);
    return ESP_OK;
}
