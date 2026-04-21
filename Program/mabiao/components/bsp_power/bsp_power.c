/**
 * @file bsp_power.c
 * @brief 电源管理驱动实现（BSP层）
 * @note 阶段6：电源驱动开发
 * @note 2026-04-20 重构：修正分压比；统一百分比计算；加 ADC 饱和检测与诊断日志；
 *                        加 EMA 低通滤波 + 百分比死区防抖；分离 read/update/get_cached 语义
 */

#include "bsp_power.h"
#include "driver/adc.h"
// Note: esp_adc_cal.h is provided by esp_adc component (declared in CMakeLists.txt)
#include "esp_adc_cal.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "BSP_POWER";

// ================= 硬件配置 =================
#define POWER_ADC_CHANNEL          ADC2_CHANNEL_3  // GPIO14
#define POWER_CTRL_PIN             GPIO_NUM_13     // 分压电路 P-MOS 使能（经 N-MOS 反相）
#define POWER_ADC_SAMPLE_COUNT      32              // 采样次数
#define POWER_ADC_SAMPLE_DELAY_MS   2               // 采样间隔（ms）
#define POWER_DIVIDER_STABLE_MS     5               // 分压稳定时间（ms）

// ================= 电池参数（Li-ion 单节）=================
#define POWER_BATTERY_VOLTAGE_MIN_MV   3000        // 截止电压（0%）
#define POWER_BATTERY_VOLTAGE_MAX_MV   4200        // 满电电压（100%）

// ================= 分压电阻 =================
// 原理图（假设硬件实际装配为 R13=10K，R15=1K，与原理图标注相反）：
//   VBAT -- [P-MOS] -- R13(10K) --+-- R14(100R) -- BAT_VALUE(ADC)
//                                 |
//                                R15(1K)
//                                 |
//                                GND
// Vdiv = Vbat * R15 / (R13 + R15) = Vbat * 1 / 11 ≈ 0.091 * Vbat
// VBAT=4.2V → Vdiv ≈ 0.38V；VBAT=3.0V → Vdiv ≈ 0.27V；全部落在 ADC 线性区。
#define POWER_DIVIDER_RTOP_OHM         10000
#define POWER_DIVIDER_RBOT_OHM         1000

// ================= 限幅（防止 ADC 漂移/饱和导致百分比异常）=================
#define POWER_BATTERY_VOLTAGE_CLAMP_MIN_MV  2500
#define POWER_BATTERY_VOLTAGE_CLAMP_MAX_MV  4500

// ================= ADC 饱和检测（诊断用）=================
// ESP32-S3 ADC_ATTEN_DB_12 下 12bit raw 最大 4095，线性区约 150~2450mV；
// raw > 4080 或 校准后 > 2800mV 认为已饱和，此时电池读数不可信。
#define POWER_ADC_RAW_SATURATED     4080
#define POWER_ADC_PIN_SATURATED_MV  2800

// ================= 滤波与防抖 =================
// EMA 低通：new = old + (sample - old) / POWER_EMA_DENOM
// DENOM 越大越平滑但响应越慢。4 对 1s 采样周期而言约 4~5s 到达 90%。
#define POWER_EMA_DENOM             4
// 百分比变化死区：新百分比与上次相差 < 该值时保持上次值，避免 UI 个位数反复跳
#define POWER_PERCENT_DEADBAND      1

// ================= 内部状态 =================
static bool s_initialized = false;
static esp_adc_cal_characteristics_t s_adc_chars;
static bool s_adc_calibrated = false;

static bsp_power_info_t s_power_info = {0};     // 最近一次有效结果（供 get_cached）
static uint32_t s_filtered_vbat_mv = 0;         // EMA 滤波后的电池电压
static uint8_t  s_last_percent = 0;             // 上次百分比（用于死区）
static bool s_first_sample = true;              // 首次采样标志（跳过死区 & 预填 EMA）
static bool s_last_saturated_warn = false;      // 上次是否打过饱和告警（防刷屏）

static inline uint32_t clamp_u32(uint32_t v, uint32_t lo, uint32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline uint32_t battery_mv_from_div_mv(uint32_t v_div_mv)
{
    // Vbat = Vdiv * (Rtop + Rbot) / Rbot
    return (uint32_t)((uint64_t)v_div_mv * (POWER_DIVIDER_RTOP_OHM + POWER_DIVIDER_RBOT_OHM) / POWER_DIVIDER_RBOT_OHM);
}

/**
 * @brief 由电池电压（mV）计算电量百分比（线性插值）
 */
static uint8_t calculate_battery_percent(uint32_t voltage_mv)
{
    if (voltage_mv >= POWER_BATTERY_VOLTAGE_MAX_MV) {
        return 100;
    } else if (voltage_mv <= POWER_BATTERY_VOLTAGE_MIN_MV) {
        return 0;
    }
    uint32_t range = voltage_mv - POWER_BATTERY_VOLTAGE_MIN_MV;
    uint8_t percent = (uint8_t)((range * 100) / (POWER_BATTERY_VOLTAGE_MAX_MV - POWER_BATTERY_VOLTAGE_MIN_MV));
    return percent;
}

esp_err_t bsp_power_init(const bsp_power_config_t *config)
{
    (void)config;  // 当前使用编译期宏，未使用运行期配置
    if (s_initialized) {
        ESP_LOGW(TAG, "Power already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing power management driver...");
    ESP_LOGI(TAG, "  ADC channel: ADC2_CH3 (GPIO14), atten=DB_12, width=12bit");
    ESP_LOGI(TAG, "  Divider: Rtop=%d ohm, Rbot=%d ohm, Vdiv = Vbat * %d / %d",
             POWER_DIVIDER_RTOP_OHM, POWER_DIVIDER_RBOT_OHM,
             POWER_DIVIDER_RBOT_OHM, POWER_DIVIDER_RTOP_OHM + POWER_DIVIDER_RBOT_OHM);
    ESP_LOGI(TAG, "  Battery range: %d mV (0%%) ~ %d mV (100%%)",
             POWER_BATTERY_VOLTAGE_MIN_MV, POWER_BATTERY_VOLTAGE_MAX_MV);

    // 1. 配置分压电路控制引脚（GPIO13 输出，默认 LOW = 分压断开省电）
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << POWER_CTRL_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(POWER_CTRL_PIN, 0);

    // 2. 配置 ADC2（GPIO14 -> ADC2_CH3），12 dB 衰减，12 bit
    ESP_ERROR_CHECK(adc2_config_channel_atten(POWER_ADC_CHANNEL, ADC_ATTEN_DB_12));

    // 3. ADC 校准
    esp_adc_cal_value_t cal_type = esp_adc_cal_characterize(
        ADC_UNIT_2,
        ADC_ATTEN_DB_12,
        ADC_WIDTH_BIT_12,
        1100,
        &s_adc_chars
    );
    if (cal_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGI(TAG, "ADC calibrated using Two Point values");
    } else if (cal_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGI(TAG, "ADC calibrated using eFuse Vref");
    } else {
        ESP_LOGI(TAG, "ADC calibrated using default Vref=1100mV (low accuracy)");
    }

    s_adc_calibrated = true;
    s_first_sample = true;
    s_last_percent = 0;
    s_last_saturated_warn = false;
    s_filtered_vbat_mv = 0;
    s_initialized = true;

    ESP_LOGI(TAG, "Power management driver initialized");
    return ESP_OK;
}

esp_err_t bsp_power_read(bsp_power_info_t *info)
{
    if (!s_initialized || info == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // 1. 使能分压电路（GPIO13=HIGH → N-MOS on → P-MOS on → VBAT 接入分压）
    gpio_set_level(POWER_CTRL_PIN, 1);

    // 2. 等待分压稳定
    vTaskDelay(pdMS_TO_TICKS(POWER_DIVIDER_STABLE_MS));

    // 3. 采集 ADC 样本（只保留读取成功的样本）
    uint32_t adc_samples[POWER_ADC_SAMPLE_COUNT];
    int valid_cnt = 0;
    int fail_cnt = 0;
    for (int i = 0; i < POWER_ADC_SAMPLE_COUNT; i++) {
        int raw = 0;
        esp_err_t adc_ret = adc2_get_raw(POWER_ADC_CHANNEL, ADC_WIDTH_BIT_12, &raw);
        if (adc_ret == ESP_OK) {
            adc_samples[valid_cnt++] = (uint32_t)raw;
        } else {
            fail_cnt++;
        }
        if (i < POWER_ADC_SAMPLE_COUNT - 1) {
            vTaskDelay(pdMS_TO_TICKS(POWER_ADC_SAMPLE_DELAY_MS));
        }
    }

    // 4. 关闭分压电路（省电）
    gpio_set_level(POWER_CTRL_PIN, 0);

    if (valid_cnt <= 0) {
        ESP_LOGW(TAG, "ADC read failed: %d/%d samples failed", fail_cnt, POWER_ADC_SAMPLE_COUNT);
        return ESP_FAIL;
    }
    if (fail_cnt > 0) {
        ESP_LOGD(TAG, "ADC partial failure: %d/%d samples failed", fail_cnt, POWER_ADC_SAMPLE_COUNT);
    }

    // 5. 中值滤波（冒泡排序，保留中间 50%）
    for (int i = 0; i < valid_cnt - 1; i++) {
        for (int j = i + 1; j < valid_cnt; j++) {
            if (adc_samples[i] > adc_samples[j]) {
                uint32_t temp = adc_samples[i];
                adc_samples[i] = adc_samples[j];
                adc_samples[j] = temp;
            }
        }
    }
    int start_idx = valid_cnt / 4;
    int end_idx = valid_cnt * 3 / 4;
    if (end_idx <= start_idx) {
        start_idx = 0;
        end_idx = valid_cnt;
    }
    uint32_t adc_sum = 0;
    for (int i = start_idx; i < end_idx; i++) {
        adc_sum += adc_samples[i];
    }
    uint32_t adc_avg = adc_sum / (end_idx - start_idx);
    uint32_t raw_min = adc_samples[0];
    uint32_t raw_max = adc_samples[valid_cnt - 1];

    // 6. ADC 饱和告警（仅在状态翻转时打印，避免刷屏）
    bool saturated = (adc_avg >= POWER_ADC_RAW_SATURATED);
    if (saturated && !s_last_saturated_warn) {
        ESP_LOGW(TAG, "ADC saturated! raw_avg=%lu (>=%d). "
                      "Likely divider ratio wrong or hardware mismatch. Battery reading unreliable.",
                 (unsigned long)adc_avg, POWER_ADC_RAW_SATURATED);
        s_last_saturated_warn = true;
    } else if (!saturated && s_last_saturated_warn) {
        ESP_LOGI(TAG, "ADC returned to linear region: raw_avg=%lu", (unsigned long)adc_avg);
        s_last_saturated_warn = false;
    }

    // 7. 转为引脚电压（使用校准参数）
    uint32_t adc_pin_mv = 0;
    if (s_adc_calibrated) {
        adc_pin_mv = esp_adc_cal_raw_to_voltage(adc_avg, &s_adc_chars);
    } else {
        adc_pin_mv = (adc_avg * 3300) / 4095;
    }

    // 8. 反推电池电压 + 限幅
    uint32_t battery_voltage_raw_mv = battery_mv_from_div_mv(adc_pin_mv);
    uint32_t battery_voltage_mv = clamp_u32(battery_voltage_raw_mv,
                                            POWER_BATTERY_VOLTAGE_CLAMP_MIN_MV,
                                            POWER_BATTERY_VOLTAGE_CLAMP_MAX_MV);

    // 9. EMA 低通滤波（首次采样直接预填，避免启动时一秒内从 0 渐近）
    if (s_first_sample || s_filtered_vbat_mv == 0) {
        s_filtered_vbat_mv = battery_voltage_mv;
    } else {
        // new = old + (sample - old) / DENOM，使用有符号差值避免下溢
        int32_t delta = (int32_t)battery_voltage_mv - (int32_t)s_filtered_vbat_mv;
        s_filtered_vbat_mv = (uint32_t)((int32_t)s_filtered_vbat_mv + delta / (int32_t)POWER_EMA_DENOM);
    }

    // 10. 百分比（用滤波后的电压）+ 死区防抖
    uint8_t battery_percent_raw = calculate_battery_percent(s_filtered_vbat_mv);
    uint8_t battery_percent = battery_percent_raw;
    if (!s_first_sample) {
        int diff = (int)battery_percent_raw - (int)s_last_percent;
        if (diff >= -POWER_PERCENT_DEADBAND && diff <= POWER_PERCENT_DEADBAND) {
            // 变化太小，保持上次读数（消除 ±1% 抖动）
            battery_percent = s_last_percent;
        }
    }
    s_last_percent = battery_percent;
    s_first_sample = false;

    // 11. 充电检测：当前硬件未接 IP5306 STAT 引脚，且用户暂不做软检测。
    bool is_charging = false;

    // 12. 诊断日志（DEBUG 级别，默认 INFO 不打印；menuconfig 切换 DEBUG 即显示）
    ESP_LOGD(TAG, "[diag] raw_avg=%lu min=%lu max=%lu | adc_pin=%lu mV | vbat_raw=%lu mV -> clamp=%lu mV | filt=%lu mV | pct=%u%%%s",
             (unsigned long)adc_avg, (unsigned long)raw_min, (unsigned long)raw_max,
             (unsigned long)adc_pin_mv,
             (unsigned long)battery_voltage_raw_mv, (unsigned long)battery_voltage_mv,
             (unsigned long)s_filtered_vbat_mv, (unsigned)battery_percent,
             saturated ? " [SAT]" : "");

    // 13. 填充输出与缓存
    info->voltage_mv = (uint16_t)s_filtered_vbat_mv;
    info->battery_percent = battery_percent;
    info->is_charging = is_charging;
    s_power_info = *info;

    return ESP_OK;
}

esp_err_t bsp_power_update(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    // 只执行一次采样并更新内部缓存（~70ms 阻塞）
    bsp_power_info_t tmp;
    return bsp_power_read(&tmp);
}

esp_err_t bsp_power_get_cached(bsp_power_info_t *info)
{
    if (!s_initialized || info == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    // 仅返回最近一次 update/read 的结果，不触发硬件采样（无阻塞）
    *info = s_power_info;
    return ESP_OK;
}

esp_err_t bsp_power_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }
    gpio_set_level(POWER_CTRL_PIN, 0);
    s_initialized = false;
    ESP_LOGI(TAG, "Power management driver deinitialized");
    return ESP_OK;
}
