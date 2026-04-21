/**
 * @file bsp_gps.c
 * @brief GPS驱动实现（BSP层）
 * @note 阶段5：GPS驱动开发 - DMA模式 + NMEA解析
 */

#include "bsp_gps.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "BSP_GPS";

// UART配置（参考version2：GPS_RX_PIN=38接收GPS_TX，GPS_TX_PIN=39发送到GPS_RX）
#define GPS_UART_NUM           UART_NUM_2
#define GPS_UART_RX_PIN        GPIO_NUM_38  // ESP32 RX引脚，接收GPS模块TX数据
#define GPS_UART_TX_PIN        GPIO_NUM_39  // ESP32 TX引脚，发送数据到GPS模块RX
#define GPS_UART_BAUDRATE      9600
#define GPS_UART_RX_BUF_SIZE   2048  // DMA模式（≥128字节自动启用），增大缓冲区避免溢出
#define GPS_UART_TX_BUF_SIZE    0     // GPS通常不需要发送
#define GPS_UART_QUEUE_SIZE     20

// NMEA解析缓冲区
#define NMEA_BUFFER_SIZE       256
#define NMEA_MAX_FIELD_LEN     32

// 状态变量
static bool s_initialized = false;
static QueueHandle_t s_uart_queue = NULL;
static bsp_gps_data_t s_gps_data = {0};
static bool s_data_valid = false;

// GPS接收缓冲区（静态变量，避免栈溢出）
#define GPS_RX_BUF_SIZE 256   // GPS接收缓冲区大小
static uint8_t s_gps_rx_buffer[GPS_RX_BUF_SIZE];  // GPS接收缓冲区（静态变量，避免栈溢出）

// NMEA解析状态机
typedef enum {
    NMEA_STATE_IDLE,
    NMEA_STATE_RECEIVING,
    NMEA_STATE_CHECKSUM
} nmea_state_t;

static nmea_state_t s_nmea_state = NMEA_STATE_IDLE;
static char s_nmea_buffer[NMEA_BUFFER_SIZE];
static uint16_t s_nmea_index = 0;
static uint8_t s_nmea_checksum = 0;
static bool s_nmea_in_checksum = false;

// 抑制未使用函数的警告（nmea_calculate_checksum保留以备将来扩展）
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

/**
 * @brief 计算NMEA校验和
 * 注意：当前未使用，但保留以备将来扩展（如需要验证校验和）
 */
static uint8_t nmea_calculate_checksum(const char *sentence)
{
    uint8_t checksum = 0;
    // 跳过$符号，计算到*之前的所有字符的XOR
    if (*sentence == '$') {
        sentence++;
    }
    while (*sentence && *sentence != '*') {
        checksum ^= *sentence++;
    }
    return checksum;
}

/**
 * @brief 解析NMEA字段（逗号分隔）
 */
static const char* nmea_get_field(const char *sentence, int field_index, char *buffer, int buffer_size)
{
    if (sentence == NULL || buffer == NULL || buffer_size <= 0) {
        return NULL;
    }
    
    int current_field = 0;
    const char *start = sentence;
    const char *end = sentence;
    
    // 跳过$符号
    if (*start == '$') {
        start++;
        end++;
    }
    
    while (*end != '\0' && current_field <= field_index) {
        if (*end == ',' || *end == '*' || *end == '\r' || *end == '\n') {
            if (current_field == field_index) {
                int len = end - start;
                if (len >= buffer_size) {
                    len = buffer_size - 1;
                }
                memcpy(buffer, start, len);
                buffer[len] = '\0';
                return buffer;
            }
            if (*end == ',' || *end == '*') {
                start = end + 1;
            } else {
                break;
            }
            current_field++;
        }
        end++;
    }
    
    return NULL;
}

/**
 * @brief 将DDMM.MMMM格式转换为度
 */
static double nmea_ddmm_to_degrees(const char *ddmm_str, char direction)
{
    if (ddmm_str == NULL || strlen(ddmm_str) == 0) {
        return 0.0;
    }
    
    double value = atof(ddmm_str);
    int degrees = (int)(value / 100);
    double minutes = value - (degrees * 100);
    double result = degrees + (minutes / 60.0);
    
    if (direction == 'S' || direction == 'W') {
        result = -result;
    }
    
    return result;
}

/**
 * @brief 解析$GPGGA帧
 * $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
 */
static bool parse_gpgga(const char *sentence)
{
    char field[NMEA_MAX_FIELD_LEN];
    
    // 字段1：UTC时间（HHMMSS）
    const char *time_str = nmea_get_field(sentence, 1, field, sizeof(field));
    if (time_str != NULL && strlen(time_str) >= 6) {
        s_gps_data.hour = (time_str[0] - '0') * 10 + (time_str[1] - '0');
        s_gps_data.minute = (time_str[2] - '0') * 10 + (time_str[3] - '0');
        s_gps_data.second = (time_str[4] - '0') * 10 + (time_str[5] - '0');
    }
    
    // 字段2：纬度（DDMM.MMMM）
    const char *lat_str = nmea_get_field(sentence, 2, field, sizeof(field));
    // 字段3：纬度方向（N/S）
    char lat_dir_buf[NMEA_MAX_FIELD_LEN];
    const char *lat_dir = nmea_get_field(sentence, 3, lat_dir_buf, sizeof(lat_dir_buf));
    // 检查字段是否存在且非空（GPS未定位时字段为空）
    if (lat_str != NULL && lat_dir != NULL && strlen(lat_str) > 0 && strlen(lat_dir) > 0) {
        s_gps_data.latitude = nmea_ddmm_to_degrees(lat_str, lat_dir[0]);
    }
    
    // 字段4：经度（DDDMM.MMMM）
    char lon_buf[NMEA_MAX_FIELD_LEN];
    const char *lon_str = nmea_get_field(sentence, 4, lon_buf, sizeof(lon_buf));
    // 字段5：经度方向（E/W）
    char lon_dir_buf[NMEA_MAX_FIELD_LEN];
    const char *lon_dir = nmea_get_field(sentence, 5, lon_dir_buf, sizeof(lon_dir_buf));
    // 检查字段是否存在且非空（GPS未定位时字段为空）
    if (lon_str != NULL && lon_dir != NULL && strlen(lon_str) > 0 && strlen(lon_dir) > 0) {
        s_gps_data.longitude = nmea_ddmm_to_degrees(lon_str, lon_dir[0]);
    }
    
    // 字段6：定位质量（0=无效，1=GPS，2=DGPS）
    const char *quality_str = nmea_get_field(sentence, 6, field, sizeof(field));
    int quality = (quality_str != NULL) ? atoi(quality_str) : 0;
    s_gps_data.is_valid = (quality > 0);
    
    // 字段7：卫星数
    const char *sat_str = nmea_get_field(sentence, 7, field, sizeof(field));
    if (sat_str != NULL) {
        s_gps_data.satellites = atoi(sat_str);
    }
    
    // 字段9：海拔高度（米）
    const char *alt_str = nmea_get_field(sentence, 9, field, sizeof(field));
    if (alt_str != NULL) {
        s_gps_data.altitude = atof(alt_str);
    }
    
    return true;
}

/**
 * @brief 解析$GPRMC帧
 * $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
 */
static bool parse_gprmc(const char *sentence)
{
    char field[NMEA_MAX_FIELD_LEN];
    
    // 字段1：UTC时间（HHMMSS）
    const char *time_str = nmea_get_field(sentence, 1, field, sizeof(field));
    if (time_str != NULL && strlen(time_str) >= 6) {
        s_gps_data.hour = (time_str[0] - '0') * 10 + (time_str[1] - '0');
        s_gps_data.minute = (time_str[2] - '0') * 10 + (time_str[3] - '0');
        s_gps_data.second = (time_str[4] - '0') * 10 + (time_str[5] - '0');
    }
    
    // 字段2：状态（A=有效，V=无效）
    const char *status_str = nmea_get_field(sentence, 2, field, sizeof(field));
    if (status_str != NULL) {
        s_gps_data.is_valid = (status_str[0] == 'A');
    }
    
    // 字段3：纬度（DDMM.MMMM）
    const char *lat_str = nmea_get_field(sentence, 3, field, sizeof(field));
    // 字段4：纬度方向（N/S）
    char lat_dir_buf[NMEA_MAX_FIELD_LEN];
    const char *lat_dir = nmea_get_field(sentence, 4, lat_dir_buf, sizeof(lat_dir_buf));
    // 检查字段是否存在且非空（GPS未定位时字段为空）
    if (lat_str != NULL && lat_dir != NULL && strlen(lat_str) > 0 && strlen(lat_dir) > 0) {
        s_gps_data.latitude = nmea_ddmm_to_degrees(lat_str, lat_dir[0]);
    }
    
    // 字段5：经度（DDDMM.MMMM）
    char lon_buf[NMEA_MAX_FIELD_LEN];
    const char *lon_str = nmea_get_field(sentence, 5, lon_buf, sizeof(lon_buf));
    // 字段6：经度方向（E/W）
    char lon_dir_buf[NMEA_MAX_FIELD_LEN];
    const char *lon_dir = nmea_get_field(sentence, 6, lon_dir_buf, sizeof(lon_dir_buf));
    // 检查字段是否存在且非空（GPS未定位时字段为空）
    if (lon_str != NULL && lon_dir != NULL && strlen(lon_str) > 0 && strlen(lon_dir) > 0) {
        s_gps_data.longitude = nmea_ddmm_to_degrees(lon_str, lon_dir[0]);
    }
    
    // 字段7：速度（节）
    const char *speed_str = nmea_get_field(sentence, 7, field, sizeof(field));
    if (speed_str != NULL) {
        float speed_knots = atof(speed_str);
        s_gps_data.speed = speed_knots * 1.852f;  // 节转换为km/h
    }
    
    // 字段8：航向（度）
    const char *course_str = nmea_get_field(sentence, 8, field, sizeof(field));
    if (course_str != NULL) {
        s_gps_data.course = atof(course_str);
    }
    
    // 字段9：日期（DDMMYY）
    const char *date_str = nmea_get_field(sentence, 9, field, sizeof(field));
    if (date_str != NULL && strlen(date_str) >= 6) {
        int day = (date_str[0] - '0') * 10 + (date_str[1] - '0');
        int month = (date_str[2] - '0') * 10 + (date_str[3] - '0');
        int year = (date_str[4] - '0') * 10 + (date_str[5] - '0');
        s_gps_data.day = day;
        s_gps_data.month = month;
        s_gps_data.year = 2000 + year;  // 假设是20xx年
    }
    
    return true;
}

/**
 * @brief 处理NMEA字符（状态机）
 */
static void nmea_process_char(char c)
{
    switch (s_nmea_state) {
        case NMEA_STATE_IDLE:
            if (c == '$') {
                s_nmea_state = NMEA_STATE_RECEIVING;
                s_nmea_index = 0;
                s_nmea_buffer[0] = '$';
                s_nmea_index = 1;
                s_nmea_checksum = 0;
                s_nmea_in_checksum = false;
            }
            break;
            
        case NMEA_STATE_RECEIVING:
            if (c == '*') {
                s_nmea_state = NMEA_STATE_CHECKSUM;
                s_nmea_buffer[s_nmea_index] = '\0';
            } else if (c == '\r' || c == '\n') {
                // 没有校验和的句子（不完整）
                s_nmea_state = NMEA_STATE_IDLE;
                s_nmea_index = 0;
            } else {
                if (s_nmea_index < NMEA_BUFFER_SIZE - 1) {
                    s_nmea_buffer[s_nmea_index++] = c;
                } else {
                    // 缓冲区溢出
                    s_nmea_state = NMEA_STATE_IDLE;
                    s_nmea_index = 0;
                }
            }
            break;
            
        case NMEA_STATE_CHECKSUM:
            if (c == '\r' || c == '\n') {
                // 完成一个NMEA句子（跳过校验和，直接解析）
                // 注意：s_nmea_buffer在遇到*时已经添加了'\0'，所以这里直接解析
                
                // 解析句子（支持GP和GN开头的句子）
                if (strncmp(s_nmea_buffer, "$GPGGA", 6) == 0 || strncmp(s_nmea_buffer, "$GNGGA", 6) == 0) {
                    parse_gpgga(s_nmea_buffer);
                    s_data_valid = s_gps_data.is_valid;
                } else if (strncmp(s_nmea_buffer, "$GPRMC", 6) == 0 || strncmp(s_nmea_buffer, "$GNRMC", 6) == 0) {
                    parse_gprmc(s_nmea_buffer);
                    s_data_valid = s_gps_data.is_valid;
                }
                
                s_nmea_state = NMEA_STATE_IDLE;
                s_nmea_index = 0;
            } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
                // 校验和字符（跳过，不存储）
                // 可以在这里添加校验和验证逻辑
            } else if (c == '$') {
                // 遇到新的句子开始，重置状态机
                s_nmea_state = NMEA_STATE_RECEIVING;
                s_nmea_index = 0;
                s_nmea_buffer[0] = '$';
                s_nmea_index = 1;
                s_nmea_checksum = 0;
                s_nmea_in_checksum = false;
            } else {
                // 遇到其他字符，可能是格式错误，重置状态机
                s_nmea_state = NMEA_STATE_IDLE;
                s_nmea_index = 0;
            }
            break;
    }
}

esp_err_t bsp_gps_init(const bsp_gps_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "GPS already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing GPS driver (UART2, DMA mode)...");
    
    // 配置UART参数
    uart_config_t uart_config = {
        .baud_rate = (config != NULL && config->baud_rate > 0) ? config->baud_rate : GPS_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    // 安装UART驱动（DMA模式：RX缓冲区≥128字节自动启用DMA）
    esp_err_t ret = uart_driver_install(GPS_UART_NUM,
                                         GPS_UART_RX_BUF_SIZE,  // RX buffer（DMA自动管理）
                                         GPS_UART_TX_BUF_SIZE,  // TX buffer
                                         GPS_UART_QUEUE_SIZE,    // Queue size
                                         &s_uart_queue,          // Queue handle（用于DMA完成通知）
                                         0);                     // Interrupt flags
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 配置UART参数
    ret = uart_param_config(GPS_UART_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART parameters: %s", esp_err_to_name(ret));
        uart_driver_delete(GPS_UART_NUM);
        return ret;
    }
    
    // 配置UART引脚
    ret = uart_set_pin(GPS_UART_NUM,
                       GPS_UART_TX_PIN,  // TX
                       GPS_UART_RX_PIN,  // RX
                       UART_PIN_NO_CHANGE,  // RTS
                       UART_PIN_NO_CHANGE); // CTS
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(ret));
        uart_driver_delete(GPS_UART_NUM);
        return ret;
    }
    
    // 清空接收缓冲区
    uart_flush_input(GPS_UART_NUM);
    
    // 初始化状态
    memset(&s_gps_data, 0, sizeof(s_gps_data));
    s_data_valid = false;
    s_nmea_state = NMEA_STATE_IDLE;
    s_nmea_index = 0;
    
    s_initialized = true;
    ESP_LOGI(TAG, "GPS driver initialized (DMA mode, RX buffer=%d bytes)", GPS_UART_RX_BUF_SIZE);
    
    return ESP_OK;
}

esp_err_t bsp_gps_read(bsp_gps_data_t *data)
{
    if (!s_initialized || data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // 复制当前GPS数据
    memcpy(data, &s_gps_data, sizeof(bsp_gps_data_t));
    
    return ESP_OK;
}

esp_err_t bsp_gps_update(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // 使用UART事件队列（DMA模式）
    uart_event_t event;
    int processed_events = 0;
    const int MAX_EVENTS_PER_CALL = 50;  // 限制每次处理的事件数，避免阻塞太久
    
    while (processed_events < MAX_EVENTS_PER_CALL && 
           xQueueReceive(s_uart_queue, &event, 0) == pdTRUE) {
        processed_events++;
        
        switch (event.type) {
            case UART_DATA:
                // DMA传输完成，读取数据（使用静态缓冲区，避免栈溢出）
                {
                    int len = uart_read_bytes(GPS_UART_NUM, s_gps_rx_buffer,
                                             (event.size < sizeof(s_gps_rx_buffer)) ? event.size : sizeof(s_gps_rx_buffer),
                                             0);
                    if (len > 0) {
                        // 逐字节喂给NMEA解析器
                        for (int i = 0; i < len; i++) {
                            nmea_process_char((char)s_gps_rx_buffer[i]);
                        }
                    }
                }
                break;
                
            case UART_FIFO_OVF:
                // FIFO溢出（DMA缓冲区满），清空缓冲区并继续处理
                uart_flush_input(GPS_UART_NUM);
                // 移除警告日志，避免刷屏
                break;
                
            case UART_BUFFER_FULL:
                // 缓冲区满，清空缓冲区并继续处理
                uart_flush_input(GPS_UART_NUM);
                // 移除警告日志，避免刷屏
                break;
                
            case UART_BREAK:
                // 检测到BREAK信号
                ESP_LOGD(TAG, "GPS UART break detected");
                break;
                
            case UART_PARITY_ERR:
                // 校验错误
                ESP_LOGW(TAG, "GPS UART parity error");
                break;
                
            case UART_FRAME_ERR:
                // 帧错误
                ESP_LOGW(TAG, "GPS UART frame error");
                break;
                
            default:
                break;
        }
    }
    
    // 处理完队列事件后，继续读取UART中剩余的数据（如果有）
    // 这样可以更及时地清空缓冲区
    int remaining_len = uart_read_bytes(GPS_UART_NUM, s_gps_rx_buffer,
                                       sizeof(s_gps_rx_buffer), 0);
    if (remaining_len > 0) {
        for (int i = 0; i < remaining_len; i++) {
            nmea_process_char((char)s_gps_rx_buffer[i]);
        }
    }
    
    // 调试：检查NMEA状态机是否卡住
    static TickType_t last_state_check = 0;
    TickType_t now = xTaskGetTickCount();
    if (pdMS_TO_TICKS(30000) < (now - last_state_check)) { // 每30秒检查一次
        if (s_nmea_state != NMEA_STATE_IDLE) {
            // 减少警告频率（每100次才打印一次）
            static uint32_t stuck_warn_count = 0;
            if (++stuck_warn_count % 100 == 0) {
                ESP_LOGW(TAG, "NMEA state machine stuck in state %d, resetting (count=%lu)", s_nmea_state, stuck_warn_count);
            }
            s_nmea_state = NMEA_STATE_IDLE;
            s_nmea_index = 0;
        }
        last_state_check = now;
    }
    
    return ESP_OK;
}

esp_err_t bsp_gps_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }
    
    // 删除UART驱动
    uart_driver_delete(GPS_UART_NUM);
    
    s_uart_queue = NULL;
    s_initialized = false;
    s_data_valid = false;
    
    ESP_LOGI(TAG, "GPS driver deinitialized");
    return ESP_OK;
}

#pragma GCC diagnostic pop
