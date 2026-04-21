/**
 * @file bsp_imu.c
 * @brief IMU驱动实现（BSP层）
 * @note 阶段5：IMU驱动开发 - DMA模式 + 帧解析 + 坡度计算
 */

#include "bsp_imu.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 抑制未使用函数的警告（combine32保留以备将来扩展）
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

static const char *TAG = "BSP_IMU";

// UART配置（TX/RX已交换）
#define IMU_UART_NUM           UART_NUM_1
#define IMU_UART_RX_PIN        GPIO_NUM_5  // 交换：原TX改为RX
#define IMU_UART_TX_PIN        GPIO_NUM_4  // 交换：原RX改为TX
#define IMU_UART_RST_PIN       GPIO_NUM_6
#define IMU_UART_BAUDRATE      115200
#define IMU_UART_RX_BUF_SIZE   2048  // DMA模式（≥128字节自动启用），增加到2048避免溢出
#define IMU_UART_TX_BUF_SIZE   0     // IMU通常不需要发送
#define IMU_UART_QUEUE_SIZE    20

// HLK-AS201帧格式定义
#define AS201_HEAD_HIGH         0xFA
#define AS201_HEAD_LOW          0xFB
#define AS201_TAIL_HIGH         0xFC
#define AS201_TAIL_LOW          0xFD

// 帧解析缓冲区（增加到1024，避免溢出）
#define IMU_BUFFER_SIZE         1024

// 状态变量
static bool s_initialized = false;
static QueueHandle_t s_uart_queue = NULL;
static bsp_imu_data_t s_imu_data = {0};
static bool s_data_valid = false;

// 帧解析缓冲区
static uint8_t s_frame_buffer[IMU_BUFFER_SIZE];
static uint16_t s_frame_buffer_len = 0;

// IMU接收缓冲区（静态变量，避免栈溢出）
#define IMU_RX_BUF_SIZE 256
static uint8_t s_imu_rx_buffer[IMU_RX_BUF_SIZE];

/**
 * @brief 组合16位数据（小端序）
 */
static int16_t combine16(uint8_t low, uint8_t high, bool signed_val)
{
    uint16_t v = ((uint16_t)high << 8) | low;
    if (signed_val && (v & 0x8000)) {
        return (int16_t)(v - 0x10000);
    }
    return (int16_t)v;
}

/**
 * @brief 组合32位数据（小端序）
 * 注意：当前未使用，但保留以备将来扩展（如气压、高度等32位数据）
 */
static int32_t combine32(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, bool signed_val)
{
    uint32_t v = ((uint32_t)b3 << 24) | ((uint32_t)b2 << 16) | 
                 ((uint32_t)b1 << 8) | b0;
    if (signed_val && (v & 0x80000000)) {
        return (int32_t)(v - 0x100000000);
    }
    return (int32_t)v;
}

/**
 * @brief 解析IMU帧（参考ChappieIMU.hpp的parseFrame函数）
 */
static bool parse_imu_frame(const uint8_t *frame, uint16_t len)
{
    // 空指针检查
    if (frame == NULL) {
        ESP_LOGE(TAG, "parse_imu_frame: frame is NULL");
        return false;
    }
    
    // 检查帧长度（至少10字节：帧头2 + 长度1 + 命令1 + 类型1 + 数据1 + 校验1 + 帧尾2）
    if (len < 10) {
        ESP_LOGW(TAG, "Frame too short: %d bytes", len);
        return false;
    }
    
    // 检查长度是否超出缓冲区大小
    if (len > IMU_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Frame too long: %d bytes (max %d)", len, IMU_BUFFER_SIZE);
        return false;
    }
    
    // 检查帧头
    if (frame[0] != AS201_HEAD_HIGH || frame[1] != AS201_HEAD_LOW) {
        ESP_LOGW(TAG, "Bad frame header: got 0x%02X 0x%02X, expected 0x%02X 0x%02X", 
                 frame[0], frame[1], AS201_HEAD_HIGH, AS201_HEAD_LOW);
        return false;
    }
    
    // 检查帧尾
    if (frame[len - 2] != AS201_TAIL_HIGH || frame[len - 1] != AS201_TAIL_LOW) {
        ESP_LOGW(TAG, "Bad frame tail: got 0x%02X 0x%02X, expected 0x%02X 0x%02X", 
                 frame[len - 2], frame[len - 1], AS201_TAIL_HIGH, AS201_TAIL_LOW);
        return false;
    }
    
    // 检查长度字段
    uint8_t payload_len = frame[2];
    if (payload_len < 3 || payload_len + 4 > len) {
        ESP_LOGW(TAG, "Bad payload length: %d (frame len=%d)", payload_len, len);
        return false;
    }
    
    // 校验和验证（从命令字节开始到数据结束）
    uint8_t checksum = 0;
    for (uint16_t i = 3; i < len - 3; i++) {
        checksum += frame[i];
    }
    if (checksum != frame[len - 3]) {
        ESP_LOGW(TAG, "Checksum error: calculated=0x%02X, received=0x%02X", checksum, frame[len - 3]);
        return false;
    }
    
    // 检查命令（通常为0x00）
    uint8_t cmd = frame[3];
    if (cmd != 0x00) {
        ESP_LOGW(TAG, "Bad command: 0x%02X (expected 0x00)", cmd);
        return false;
    }
    
    // 移除帧信息打印，避免刷屏
    
    // 解析数据（参考ChappieIMU.hpp的数据格式）
    uint16_t idx = 5;  // 跳过 cmd + type
    
    // 加速度（3×2字节），单位转换系数0.00478515625
    // 注意：转换为int16_t时，使用缩放因子（100倍）以保持精度
    // 例如：1.5 m/s² -> 150 (int16_t，单位0.01 m/s²)
    // 修复序列点错误：先读取字节，再递增索引
    {
        uint8_t ax_low = frame[idx++];
        uint8_t ax_high = frame[idx++];
        float ax_float = (float)combine16(ax_low, ax_high, true) * 0.00478515625f;
        s_imu_data.ax = (int16_t)(ax_float * 100.0f);
    }
    {
        uint8_t ay_low = frame[idx++];
        uint8_t ay_high = frame[idx++];
        float ay_float = (float)combine16(ay_low, ay_high, true) * 0.00478515625f;
        s_imu_data.ay = (int16_t)(ay_float * 100.0f);
    }
    {
        uint8_t az_low = frame[idx++];
        uint8_t az_high = frame[idx++];
        float az_float = (float)combine16(az_low, az_high, true) * 0.00478515625f;
        s_imu_data.az = (int16_t)(az_float * 100.0f);
    }
    
    // 角速度（3×2字节），单位转换系数0.0625
    // 注意：转换为int16_t时，使用缩放因子（100倍）以保持精度，与加速度数据一致
    // 例如：1.5 °/s -> 150 (int16_t，单位0.01 °/s)
    {
        uint8_t gx_low = frame[idx++];
        uint8_t gx_high = frame[idx++];
        int16_t gx_raw = combine16(gx_low, gx_high, true);
        float gx_float = (float)gx_raw * 0.0625f;  // 转换为°/s
        // 使用缩放因子100倍，与加速度数据格式一致
        s_imu_data.gx = (int16_t)(gx_float * 100.0f);
    }
    {
        uint8_t gy_low = frame[idx++];
        uint8_t gy_high = frame[idx++];
        int16_t gy_raw = combine16(gy_low, gy_high, true);
        float gy_float = (float)gy_raw * 0.0625f;  // 转换为°/s
        // 使用缩放因子100倍，与加速度数据格式一致
        s_imu_data.gy = (int16_t)(gy_float * 100.0f);
    }
    {
        uint8_t gz_low = frame[idx++];
        uint8_t gz_high = frame[idx++];
        int16_t gz_raw = combine16(gz_low, gz_high, true);
        float gz_float = (float)gz_raw * 0.0625f;  // 转换为°/s
        // 使用缩放因子100倍，与加速度数据格式一致
        s_imu_data.gz = (int16_t)(gz_float * 100.0f);
    }
    
    // 姿态角（3×2字节），单位转换系数0.0054931640625
    {
        uint8_t roll_low = frame[idx++];
        uint8_t roll_high = frame[idx++];
        float roll_raw = (float)combine16(roll_low, roll_high, true);
        s_imu_data.roll = roll_raw * 0.0054931640625f;
    }
    {
        uint8_t pitch_low = frame[idx++];
        uint8_t pitch_high = frame[idx++];
        float pitch_raw = (float)combine16(pitch_low, pitch_high, true);
        s_imu_data.pitch = pitch_raw * 0.0054931640625f;
    }
    {
        uint8_t yaw_low = frame[idx++];
        uint8_t yaw_high = frame[idx++];
        float yaw_raw = (float)combine16(yaw_low, yaw_high, true);
        s_imu_data.yaw = yaw_raw * 0.0054931640625f;
    }
    
    // 磁力计（3×2字节），单位转换系数0.006103515625
    {
        uint8_t mx_low = frame[idx++];
        uint8_t mx_high = frame[idx++];
        float mx_raw = (float)combine16(mx_low, mx_high, true);
        s_imu_data.mx = mx_raw * 0.006103515625f;
    }
    {
        uint8_t my_low = frame[idx++];
        uint8_t my_high = frame[idx++];
        float my_raw = (float)combine16(my_low, my_high, true);
        s_imu_data.my = my_raw * 0.006103515625f;
    }
    {
        uint8_t mz_low = frame[idx++];
        uint8_t mz_high = frame[idx++];
        float mz_raw = (float)combine16(mz_low, mz_high, true);
        s_imu_data.mz = mz_raw * 0.006103515625f;
    }
    
    // 四元数（4×2字节），单位转换系数0.000030517578125
    {
        uint8_t q0_low = frame[idx++];
        uint8_t q0_high = frame[idx++];
        float q0_raw = (float)combine16(q0_low, q0_high, true);
        s_imu_data.q0 = q0_raw * 0.000030517578125f;
    }
    {
        uint8_t q1_low = frame[idx++];
        uint8_t q1_high = frame[idx++];
        float q1_raw = (float)combine16(q1_low, q1_high, true);
        s_imu_data.q1 = q1_raw * 0.000030517578125f;
    }
    {
        uint8_t q2_low = frame[idx++];
        uint8_t q2_high = frame[idx++];
        float q2_raw = (float)combine16(q2_low, q2_high, true);
        s_imu_data.q2 = q2_raw * 0.000030517578125f;
    }
    {
        uint8_t q3_low = frame[idx++];
        uint8_t q3_high = frame[idx++];
        float q3_raw = (float)combine16(q3_low, q3_high, true);
        s_imu_data.q3 = q3_raw * 0.000030517578125f;
    }
    
    // 温度（2字节），单位转换系数0.01
    {
        uint8_t temp_low = frame[idx++];
        uint8_t temp_high = frame[idx++];
        float temp_raw = (float)combine16(temp_low, temp_high, true);
        s_imu_data.temperature = temp_raw * 0.01f;
    }
    
    // 气压（4字节），单位转换系数0.0002384185791
    {
        uint8_t p0 = frame[idx++];
        uint8_t p1 = frame[idx++];
        uint8_t p2 = frame[idx++];
        uint8_t p3 = frame[idx++];
        int32_t pressure_bits = combine32(p0, p1, p2, p3, false);
        s_imu_data.pressure = pressure_bits * 0.0002384185791f;
    }
    
    // 高度（4字节），单位转换系数0.0010728836
    uint8_t s_h0 = 0, s_h1 = 0, s_h2 = 0, s_h3 = 0;
    int32_t s_height_bits = 0;
    {
        s_h0 = frame[idx++];
        s_h1 = frame[idx++];
        s_h2 = frame[idx++];
        s_h3 = frame[idx++];
        s_height_bits = combine32(s_h0, s_h1, s_h2, s_h3, false);
        s_imu_data.height = s_height_bits * 0.0010728836f;
    }
    
    // 海凌科 AS201 固件不填高度字段（实测 4 字节恒为 00 00 00 00），
    // 所以只要硬件 height_bits=0，我们就基于气压自行计算"相对开机点"的高度差：
    //     h = 44330 × (1 − (P / P0)^(1/5.255))
    // P0 取第一帧的平稳气压作为零点，之后爬楼 +10m / 下楼 -10m 都能看出来。
    // 气压本身有 ±50Pa 级别的抖动，先做一阶低通（EMA, alpha=0.08）再换算高度，
    // 否则显示会乱跳 ±4m。
    if (s_height_bits == 0) {
        static bool  s_alt_init = false;
        static float s_press_ema = 0.0f;   // 平滑后的当前气压
        static float s_press_ref = 0.0f;   // 开机参考气压 (P0)
        float p = s_imu_data.pressure;
        if (p >= 50000.0f && p <= 120000.0f) {
            if (!s_alt_init) {
                s_press_ema = p;
                s_press_ref = p;
                s_alt_init  = true;
                s_imu_data.height = 0.0f;
            } else {
                s_press_ema = s_press_ema * 0.92f + p * 0.08f;
                float ratio = s_press_ema / s_press_ref;
                s_imu_data.height = 44330.0f * (1.0f - powf(ratio, 0.19022256f));
            }
        }
    }
    
    // 诊断：首次成功解析时把帧长度、原始 height 字节/位值/气压/温度全打出来，
    // 判定 altitude=0 是硬件完全不发（4字节=00 00 00 00）
    // 还是解析正常、当前绝对海拔接近 0（非零字节但算出接近 0）。
    {
        static uint32_t s_parse_ok_count = 0;
        if (s_parse_ok_count == 0 || (++s_parse_ok_count % 500) == 0) {
            ESP_LOGI(TAG, "IMU #%lu len=%u P=%.1fPa H=%.2fm(raw=%02X %02X %02X %02X bits=%ld) T=%.2fC pitch=%.2f",
                     (unsigned long)s_parse_ok_count, (unsigned)len,
                     s_imu_data.pressure, s_imu_data.height,
                     s_h0, s_h1, s_h2, s_h3, (long)s_height_bits,
                     s_imu_data.temperature, s_imu_data.pitch);
        }
        if (s_parse_ok_count == 0) s_parse_ok_count = 1;
    }
    
    s_data_valid = true;
    return true;
}

esp_err_t bsp_imu_init(const bsp_imu_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "IMU already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing IMU driver (UART1, DMA mode)...");
    
    // 1. 配置复位引脚（GPIO6）
    gpio_reset_pin(IMU_UART_RST_PIN);
    gpio_set_direction(IMU_UART_RST_PIN, GPIO_MODE_OUTPUT);
    
    // 2. 复位IMU（拉低10ms，再拉高）
    gpio_set_level(IMU_UART_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(IMU_UART_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 3. 配置UART参数
    uart_config_t uart_config = {
        .baud_rate = (config != NULL && config->baud_rate > 0) ? config->baud_rate : IMU_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    // 4. 安装UART驱动（DMA模式：RX缓冲区≥128字节自动启用DMA）
    esp_err_t ret = uart_driver_install(IMU_UART_NUM,
                                         IMU_UART_RX_BUF_SIZE,  // RX buffer（DMA自动管理）
                                         IMU_UART_TX_BUF_SIZE,  // TX buffer
                                         IMU_UART_QUEUE_SIZE,   // Queue size
                                         &s_uart_queue,          // Queue handle（用于DMA完成通知）
                                         0);                     // Interrupt flags
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 5. 配置UART参数
    ret = uart_param_config(IMU_UART_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART parameters: %s", esp_err_to_name(ret));
        uart_driver_delete(IMU_UART_NUM);
        return ret;
    }
    
    // 6. 配置UART引脚
    ret = uart_set_pin(IMU_UART_NUM,
                       IMU_UART_TX_PIN,  // TX
                       IMU_UART_RX_PIN,  // RX
                       UART_PIN_NO_CHANGE,  // RTS
                       UART_PIN_NO_CHANGE); // CTS
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(ret));
        uart_driver_delete(IMU_UART_NUM);
        return ret;
    }
    
    // 7. 清空接收缓冲区
    uart_flush_input(IMU_UART_NUM);
    
    // 8. 等待500ms，让IMU开始发送数据
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 9. 再次清空可能存在的初始数据
    uart_flush_input(IMU_UART_NUM);
    
    // 10. 初始化状态
    memset(&s_imu_data, 0, sizeof(s_imu_data));
    s_data_valid = false;
    s_frame_buffer_len = 0;
    
    s_initialized = true;
    ESP_LOGI(TAG, "IMU driver initialized (DMA mode, RX buffer=%d bytes)", IMU_UART_RX_BUF_SIZE);
    
    return ESP_OK;
}

esp_err_t bsp_imu_read(bsp_imu_data_t *data)
{
    if (!s_initialized || data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // 复制当前IMU数据
    memcpy(data, &s_imu_data, sizeof(bsp_imu_data_t));
    
    return ESP_OK;
}

esp_err_t bsp_imu_update(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // 使用UART事件队列（DMA模式）
    // 限制每次处理的事件数，避免阻塞太久
    uart_event_t event;
    int processed_events = 0;
    const int MAX_EVENTS_PER_CALL = 50;  // 限制每次处理的事件数
    
    while (processed_events < MAX_EVENTS_PER_CALL && 
           xQueueReceive(s_uart_queue, &event, 0) == pdTRUE) {
        processed_events++;
        switch (event.type) {
            case UART_DATA:
                // DMA传输完成，读取数据（使用静态缓冲区，避免栈溢出）
                {
                    int len = uart_read_bytes(IMU_UART_NUM, s_imu_rx_buffer,
                                             (event.size < sizeof(s_imu_rx_buffer)) ? event.size : sizeof(s_imu_rx_buffer),
                                             0);
                    if (len > 0) {
                        // 移除原始数据打印，避免刷屏
                        
                        // 检查缓冲区是否溢出
                        if (s_frame_buffer_len + len > sizeof(s_frame_buffer)) {
                            // 缓冲区满，丢弃并重新开始
                            s_frame_buffer_len = 0;
                            ESP_LOGW(TAG, "IMU buffer overflow, reset");
                        } else {
                            // 追加到缓冲区
                            memcpy(s_frame_buffer + s_frame_buffer_len, s_imu_rx_buffer, len);
                            s_frame_buffer_len += len;
                        }
                    }
                }
                break;
                
            case UART_FIFO_OVF:
                // FIFO溢出（DMA缓冲区满），清空缓冲区
                uart_flush_input(IMU_UART_NUM);
                s_frame_buffer_len = 0;
                ESP_LOGW(TAG, "IMU UART FIFO overflow");
                break;
                
            case UART_BUFFER_FULL:
                // 缓冲区满，清空缓冲区
                uart_flush_input(IMU_UART_NUM);
                s_frame_buffer_len = 0;
                ESP_LOGW(TAG, "IMU UART buffer full");
                break;
                
            case UART_BREAK:
                // 检测到BREAK信号
                ESP_LOGD(TAG, "IMU UART break detected");
                break;
                
            case UART_PARITY_ERR:
                // 校验错误
                ESP_LOGW(TAG, "IMU UART parity error");
                break;
                
            case UART_FRAME_ERR:
                // 帧错误
                ESP_LOGW(TAG, "IMU UART frame error");
                break;
                
            default:
                break;
        }
    }
    
    // 查找帧头
    uint16_t start = 0;
    while (start + 1 < s_frame_buffer_len) {
        if (s_frame_buffer[start] == AS201_HEAD_HIGH && s_frame_buffer[start + 1] == AS201_HEAD_LOW) {
            break;
        }
        start++;
    }
    
    // 移除帧头前的无效数据
    if (start > 0) {
        // 移除调试日志，避免刷屏
        memmove(s_frame_buffer, s_frame_buffer + start, s_frame_buffer_len - start);
        s_frame_buffer_len -= start;
        start = 0;
    }
    
    // 查找并解析完整帧（循环处理所有完整帧）
    // 参考Python示例：先验证帧头，然后根据length字段计算总长度
    bool found = true;
    while (found && s_frame_buffer_len >= 10) {
        found = false;
        
        // 验证帧头（必须从缓冲区开头开始）
        if (s_frame_buffer[0] != AS201_HEAD_HIGH || s_frame_buffer[1] != AS201_HEAD_LOW) {
            // 帧头不匹配，丢弃第一个字节，继续查找
            if (s_frame_buffer_len > 1) {
                memmove(s_frame_buffer, s_frame_buffer + 1, s_frame_buffer_len - 1);
                s_frame_buffer_len--;
            } else {
                s_frame_buffer_len = 0;
            }
            continue;
        }
        
        // 检查是否有足够的数据读取length字段
        if (s_frame_buffer_len < 3) {
            break;  // 数据不足，等待更多数据
        }
        
        // 获取payload长度（不包括帧头帧尾）
        uint8_t payload_len = s_frame_buffer[2];
        
        // 计算总帧长度：帧头(2) + length(1) + payload(payload_len) + 校验(1) + 帧尾(2) = payload_len + 5
        uint16_t expected_frame_len = payload_len + 5;
        
        // 检查长度是否合理
        if (payload_len < 3 || expected_frame_len > IMU_BUFFER_SIZE) {
            // 长度字段无效，丢弃第一个字节，重新开始
            memmove(s_frame_buffer, s_frame_buffer + 1, s_frame_buffer_len - 1);
            s_frame_buffer_len--;
            continue;
        }
        
        // 检查是否有完整的帧
        if (s_frame_buffer_len < expected_frame_len) {
            break;  // 数据不足，等待更多数据
        }
        
        // 验证帧尾
        if (s_frame_buffer[expected_frame_len - 2] != AS201_TAIL_HIGH || 
            s_frame_buffer[expected_frame_len - 1] != AS201_TAIL_LOW) {
            // 帧尾不匹配，丢弃第一个字节，重新开始
            memmove(s_frame_buffer, s_frame_buffer + 1, s_frame_buffer_len - 1);
            s_frame_buffer_len--;
            continue;
        }
        
        // 解析帧
        if (parse_imu_frame(s_frame_buffer, expected_frame_len)) {
            // 解析成功，更新数据
            // 移除解析成功日志，避免刷屏
        } else {
            // 解析失败，打印错误信息（每100次打印一次，避免刷屏）
            static uint32_t failed_count = 0;
            if (++failed_count % 100 == 0) {
                ESP_LOGW(TAG, "IMU frame parse failed (len=%d, payload_len=%d)", expected_frame_len, payload_len);
            }
        }
        
        // 移除已处理的帧
        if (expected_frame_len <= s_frame_buffer_len) {
            uint16_t remaining = s_frame_buffer_len - expected_frame_len;
            if (remaining > 0) {
                memmove(s_frame_buffer, s_frame_buffer + expected_frame_len, remaining);
            }
            s_frame_buffer_len = remaining;
        } else {
            // 不应该发生，但为了安全起见
            ESP_LOGE(TAG, "memmove boundary error: frame_len=%d > buffer_len=%d", expected_frame_len, s_frame_buffer_len);
            s_frame_buffer_len = 0;
        }
        found = true;
    }
    
    return ESP_OK;
}

float bsp_imu_get_slope(void)
{
    if (!s_data_valid) {
        return 0.0f;
    }
    
    // 将度转换为弧度
    float pitch_rad = s_imu_data.pitch * M_PI / 180.0f;
    
    // 计算坡度（百分比）
    float slope = tanf(pitch_rad) * 100.0f;
    
    // 限制范围（-100% ~ +100%）
    if (slope > 100.0f) {
        slope = 100.0f;
    }
    if (slope < -100.0f) {
        slope = -100.0f;
    }
    
    return slope;
}

esp_err_t bsp_imu_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }
    
    // 删除UART驱动
    uart_driver_delete(IMU_UART_NUM);
    
    s_uart_queue = NULL;
    s_initialized = false;
    s_data_valid = false;
    s_frame_buffer_len = 0;
    
    ESP_LOGI(TAG, "IMU driver deinitialized");
    return ESP_OK;
}

#pragma GCC diagnostic pop
