/**
 * @file bsp_touch.c
 * @brief FT6336G触摸屏驱动实现（BSP层）
 * @note 阶段2.5：软件分层架构搭建
 */

#include "bsp_touch.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BSP_TOUCH";

#define BSP_TOUCH_I2C_FREQ_HZ  100000  // 100kHz标准模式

// FT6336G寄存器地址
#define BSP_TOUCH_REG_DEVICE_MODE      0xA8  // 工作模式
#define BSP_TOUCH_REG_AUTO_SLEEP       0x80  // 自动休眠
#define BSP_TOUCH_REG_POINT_NUM        0x02  // 触摸点数量
#define BSP_TOUCH_REG_POINT1_XH        0x03  // 触摸点1 X坐标高位
#define BSP_TOUCH_REG_POINT1_XL        0x04  // 触摸点1 X坐标低位
#define BSP_TOUCH_REG_POINT1_YH        0x05  // 触摸点1 Y坐标高位
#define BSP_TOUCH_REG_POINT1_YL        0x06  // 触摸点1 Y坐标低位

static i2c_master_bus_handle_t s_i2c_bus_handle = NULL;
static i2c_master_dev_handle_t s_i2c_dev_handle = NULL;
static bool s_initialized = false;

/**
 * @brief I2C写入数据
 */
static esp_err_t bsp_touch_i2c_write(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_transmit(s_i2c_dev_handle, write_buf, 2, -1);
}

/**
 * @brief I2C读取数据
 */
static esp_err_t bsp_touch_i2c_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_i2c_dev_handle, &reg_addr, 1, data, len, -1);
}

esp_err_t bsp_touch_init(void)
{
    esp_err_t ret = ESP_OK;

    if (s_initialized) {
        ESP_LOGW(TAG, "Touch driver already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing FT6336G touch driver...");
    ESP_LOGI(TAG, "I2C Address: 0x%02X", BSP_TOUCH_I2C_ADDR);
    ESP_LOGI(TAG, "Pins: SCL=%d, SDA=%d, INT=%d, RST=%d",
             BSP_TOUCH_PIN_SCL, BSP_TOUCH_PIN_SDA, BSP_TOUCH_PIN_INT, BSP_TOUCH_PIN_RST);

    // 配置复位引脚
    gpio_config_t rst_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << BSP_TOUCH_PIN_RST,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    ret = gpio_config(&rst_gpio_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config RST GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    // 不使用 CTP_INT：采用轮询读取触摸点数据，不占用额外 GPIO

    // 复位触摸芯片
    ESP_LOGI(TAG, "Resetting touch chip...");
    gpio_set_level(BSP_TOUCH_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(BSP_TOUCH_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));  // 等待复位完成

    // 初始化I2C总线
    ESP_LOGI(TAG, "Initializing I2C bus...");
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = BSP_TOUCH_PIN_SDA,
        .scl_io_num = BSP_TOUCH_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        },
    };
    ret = i2c_new_master_bus(&i2c_bus_config, &s_i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // 添加I2C设备
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BSP_TOUCH_I2C_ADDR,
        .scl_speed_hz = BSP_TOUCH_I2C_FREQ_HZ,
    };
    ret = i2c_master_bus_add_device(s_i2c_bus_handle, &dev_config, &s_i2c_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        i2c_del_master_bus(s_i2c_bus_handle);
        s_i2c_bus_handle = NULL;
        return ret;
    }

    // 检测设备是否存在
    uint8_t test_data = 0;
    ret = bsp_touch_i2c_read(BSP_TOUCH_REG_POINT_NUM, &test_data, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to detect touch device");
        i2c_master_bus_rm_device(s_i2c_dev_handle);
        i2c_del_master_bus(s_i2c_bus_handle);
        s_i2c_dev_handle = NULL;
        s_i2c_bus_handle = NULL;
        return ret;
    }

    // FT6336G初始化序列
    ESP_LOGI(TAG, "Configuring FT6336G...");
    
    // 设置工作模式
    ret = bsp_touch_i2c_write(BSP_TOUCH_REG_DEVICE_MODE, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set device mode");
    }

    // 关闭自动休眠
    ret = bsp_touch_i2c_write(BSP_TOUCH_REG_AUTO_SLEEP, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to disable auto sleep");
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Touch driver initialized successfully");
    return ESP_OK;
}

esp_err_t bsp_touch_read(bsp_touch_point_t *point)
{
    if (!s_initialized || point == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t point_num = 0;
    uint8_t data[4] = {0};

    // 读取触摸点数量
    esp_err_t ret = bsp_touch_i2c_read(BSP_TOUCH_REG_POINT_NUM, &point_num, 1);
    if (ret != ESP_OK) {
        return ret;
    }

    point_num &= 0x0F;  // 低4位为触摸点数量

    if (point_num == 0) {
        point->pressed = false;
        point->x = 0;
        point->y = 0;
        return ESP_OK;
    }

    // 读取第一个触摸点的坐标
    ret = bsp_touch_i2c_read(BSP_TOUCH_REG_POINT1_XH, data, 4);
    if (ret != ESP_OK) {
        return ret;
    }

    // 解析坐标（12位分辨率）
    uint16_t raw_x = ((data[0] & 0x0F) << 8) | data[1];
    uint16_t raw_y = ((data[2] & 0x0F) << 8) | data[3];

    // 坐标变换（根据配置宏进行变换，对齐LCD显示方向）
    uint16_t x = raw_x;
    uint16_t y = raw_y;
    
    // 1. 交换XY轴（如果需要）
    #if BSP_TOUCH_SWAP_XY
    {
        uint16_t temp = x;
        x = y;
        y = temp;
    }
    #endif
    
    // 2. 反转X轴（如果需要）
    #if BSP_TOUCH_REVERSE_X
    x = BSP_TOUCH_WIDTH - 1 - x;
    #endif
    
    // 3. 反转Y轴（如果需要，默认启用以对齐version2经验）
    #if BSP_TOUCH_REVERSE_Y
    y = BSP_TOUCH_HEIGHT - 1 - y;
    #endif

    // 限制范围（防止溢出）
    if (x >= BSP_TOUCH_WIDTH) {
        x = BSP_TOUCH_WIDTH - 1;
    }
    if (y >= BSP_TOUCH_HEIGHT) {
        y = BSP_TOUCH_HEIGHT - 1;
    }

    point->x = x;
    point->y = y;
    point->pressed = true;

    return ESP_OK;
}

bool bsp_touch_is_touched(void)
{
    if (!s_initialized) {
        return false;
    }

    uint8_t point_num = 0;
    esp_err_t ret = bsp_touch_i2c_read(BSP_TOUCH_REG_POINT_NUM, &point_num, 1);
    if (ret != ESP_OK) {
        return false;
    }

    return (point_num & 0x0F) > 0;
}

esp_err_t bsp_touch_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    if (s_i2c_dev_handle != NULL) {
        i2c_master_bus_rm_device(s_i2c_dev_handle);
        s_i2c_dev_handle = NULL;
    }

    if (s_i2c_bus_handle != NULL) {
        i2c_del_master_bus(s_i2c_bus_handle);
        s_i2c_bus_handle = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Touch driver deinitialized");
    return ESP_OK;
}
