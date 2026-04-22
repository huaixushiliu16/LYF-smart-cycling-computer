/**
 * @file main.c
 * @brief ESP32-S3 Smart Cycling Computer - Main Application Entry
 * @note Phase 2.5: Software Layered Architecture Setup
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"
#include "bsp_lcd.h"
#include "bsp_touch.h"
#include "bsp_sd.h"
#include "bsp_ble.h"
#include "bsp_imu.h"
#include "bsp_ip5306.h"
#include "bsp_rgb_led.h"
#include "bsp_buzzer.h"
#include "driver/spi_master.h"  // For SPI2_HOST definition
#include "hal.h"
#include "app.h"
#include "dataproc.h"

#include "lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "MAIN";

// Core mode switch (enabled by default, disables high-noise debug tasks)
// Can be disabled via compile option -DCORE_MODE_ENABLED=0
#ifndef CORE_MODE_ENABLED
#define CORE_MODE_ENABLED 1
#endif

// Serial debug related variables
static bsp_ble_device_info_t s_debug_device_list[20];
static uint8_t s_debug_device_count = 0;
static bsp_ble_device_type_t s_current_filter_type = BSP_BLE_DEVICE_TYPE_UNKNOWN;

/**
 * @brief Print device list
 */
static void print_device_list(void)
{
    printf("\n========== Scanned Device List ==========\n");
    if (s_debug_device_count == 0) {
        printf("No devices found\n");
    } else {
        for (int i = 0; i < s_debug_device_count; i++) {
            const char *type_str = "Unknown";
            switch (s_debug_device_list[i].type) {
                case BSP_BLE_DEVICE_TYPE_HR:
                    type_str = "HR Device";
                    break;
                case BSP_BLE_DEVICE_TYPE_CSCS:
                    // If connected, show CSCS mode information
                    if (s_debug_device_list[i].is_connected) {
                        bsp_ble_cscs_mode_t mode = bsp_ble_get_cscs_mode();
                        switch (mode) {
                            case BSP_BLE_CSCS_MODE_SPEED:
                                type_str = "CSCS Device (Speed Mode)";
                                break;
                            case BSP_BLE_CSCS_MODE_CADENCE:
                                type_str = "CSCS Device (Cadence Mode)";
                                break;
                            case BSP_BLE_CSCS_MODE_BOTH:
                                type_str = "CSCS Device (Speed+Cadence)";
                                break;
                            default:
                                type_str = "CSCS Device (Mode Unknown)";
                                break;
                        }
                    } else {
                        type_str = "CSCS Device (Speed/Cadence)";
                    }
                    break;
                default:
                    type_str = "Unknown Device";
                    break;
            }
            printf("[%d] %s\n", i + 1, s_debug_device_list[i].name);
            printf("     MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                   s_debug_device_list[i].addr[5], s_debug_device_list[i].addr[4],
                   s_debug_device_list[i].addr[3], s_debug_device_list[i].addr[2],
                   s_debug_device_list[i].addr[1], s_debug_device_list[i].addr[0]);
            printf("     RSSI: %d dBm, Type: %s\n", 
                   s_debug_device_list[i].rssi, type_str);
        }
    }
    printf("========================================\n");
    printf("Enter device number to connect (1-%d), or 0 to return to main menu\n", s_debug_device_count);
}

/**
 * @brief Update device list (based on current filter type)
 */
static void update_debug_device_list(void)
{
    bsp_ble_device_info_t all_devices[20];
    uint8_t all_count = 0;
    
    esp_err_t ret = bsp_ble_get_scanned_devices(all_devices, &all_count, 20);
    if (ret != ESP_OK) {
        s_debug_device_count = 0;
        return;
    }
    
    s_debug_device_count = 0;
    for (int i = 0; i < all_count && s_debug_device_count < 20; i++) {
        // If filter type is set, only show matching devices
        bool match = false;
        if (s_current_filter_type == BSP_BLE_DEVICE_TYPE_UNKNOWN) {
            // Show all devices
            match = true;
        } else if (all_devices[i].type == s_current_filter_type) {
            // Exact match
            match = true;
        } else if (all_devices[i].type == BSP_BLE_DEVICE_TYPE_CSCS) {
            // CSCS devices unified type, match CSCS filter
            if (s_current_filter_type == BSP_BLE_DEVICE_TYPE_CSCS) {
                match = true;
            }
        }
        
        if (match) {
            memcpy(&s_debug_device_list[s_debug_device_count], &all_devices[i], 
                   sizeof(bsp_ble_device_info_t));
            s_debug_device_count++;
        }
    }
}

/**
 * @brief Connect device
 */
static void connect_device(int index)
{
    if (index < 1 || index > s_debug_device_count) {
        printf("Error: Invalid device number\n");
        return;
    }
    
    int device_idx = index - 1;
    printf("\nConnecting to device: %s\n", s_debug_device_list[device_idx].name);
    printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           s_debug_device_list[device_idx].addr[5], s_debug_device_list[device_idx].addr[4],
           s_debug_device_list[device_idx].addr[3], s_debug_device_list[device_idx].addr[2],
           s_debug_device_list[device_idx].addr[1], s_debug_device_list[device_idx].addr[0]);
    
    esp_err_t ret = bsp_ble_connect(s_debug_device_list[device_idx].addr, 
                                     s_debug_device_list[device_idx].addr_type);
    if (ret == ESP_OK) {
        printf("Connection request sent, waiting for connection to complete...\n");
    } else {
        printf("Connection failed: %s\n", esp_err_to_name(ret));
    }
}

/**
 * @brief Print sensor data
 */
static void print_sensor_data(void)
{
    uint16_t heart_rate = bsp_ble_get_heart_rate();
    float speed = bsp_ble_get_speed();
    float cadence = bsp_ble_get_cadence();
    
    // GPS data
    GPS_Info_t gps_info;
    bool gps_valid = GPS_GetInfo_C(&gps_info);
    
    // IMU data
    IMU_Info_t imu_info;
    bool imu_valid = IMU_GetInfo_C(&imu_info);
    float slope = bsp_imu_get_slope();
    
    printf("\n========== Sensor Data ==========\n");
    
    // BLE data
    printf("--- BLE Data ---\n");
    if (heart_rate > 0) {
        printf("Heart Rate: %d bpm\n", heart_rate);
    } else {
        printf("Heart Rate: --\n");
    }
    
    if (speed > 0.0f) {
        printf("Speed: %.1f km/h\n", speed);
    } else {
        printf("Speed: --\n");
    }
    
    if (cadence > 0.0f) {
        printf("Cadence: %.1f RPM\n", cadence);
    } else {
        printf("Cadence: --\n");
    }
    
    // GPS data
    printf("\n--- GPS Data ---\n");
    if (gps_valid && gps_info.isVaild) {
        printf("Fix Status: Valid\n");
        printf("Longitude: %.6f°\n", gps_info.longitude);
        printf("Latitude: %.6f°\n", gps_info.latitude);
        printf("Altitude: %.2f m\n", gps_info.altitude);
        printf("Speed: %.2f km/h\n", gps_info.speed);
        printf("Course: %.2f°\n", gps_info.course);
        printf("Satellites: %d\n", gps_info.satellites);
        if (gps_info.clock.year > 0) {
            printf("UTC Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                   gps_info.clock.year, gps_info.clock.month, gps_info.clock.day,
                   gps_info.clock.hour, gps_info.clock.minute, gps_info.clock.second);
        }
    } else {
        printf("Fix Status: Invalid (Waiting for fix...)\n");
    }
    
    // IMU data
    printf("\n--- IMU Data ---\n");
    if (imu_valid) {
        printf("Attitude: Roll=%.2f°, Pitch=%.2f°, Yaw=%.2f°\n",
               imu_info.roll, imu_info.pitch, imu_info.yaw);
        printf("Acceleration: X=%d, Y=%d, Z=%d\n",
               imu_info.ax, imu_info.ay, imu_info.az);
        printf("Gyroscope: X=%d, Y=%d, Z=%d\n",
               imu_info.gx, imu_info.gy, imu_info.gz);
        printf("Slope: %.2f%%\n", slope);
    } else {
        printf("IMU Data: Invalid\n");
    }
    
    // Power data
    printf("\n--- Power Data ---\n");
    Power_Info_t power_info;
    if (Power_GetInfo_C(&power_info)) {
        printf("Voltage: %d mV (%.2f V)\n", power_info.voltage, power_info.voltage / 1000.0f);
        printf("Battery: %d%%\n", power_info.usage);
        if (power_info.isCharging) {
            printf("Charging Status: Charging\n");
        } else {
            printf("Charging Status: Not Charging\n");
        }
    } else {
        printf("Power Data: Invalid\n");
    }
    
    printf("===============================\n");
}

/**
 * @brief GPS data processing task (high frequency processing to avoid buffer overflow)
 */
static void gps_process_task(void *pvParameters)
{
    // Add current task to watchdog monitoring list
    esp_task_wdt_add(NULL);
    
    while (1) {
        // High frequency GPS data update (every 100ms) to clear buffer in time
        GPS_Update_C();
        esp_task_wdt_reset();  // Feed watchdog
        vTaskDelay(pdMS_TO_TICKS(100));  // Process every 100ms
    }
}

/**
 * @brief IMU data processing task (high frequency processing to avoid buffer overflow)
 */
static void imu_process_task(void *pvParameters)
{
    // Add current task to watchdog monitoring list
    esp_task_wdt_add(NULL);
    
    while (1) {
        // High frequency IMU data update (every 50ms) to clear buffer in time
        IMU_Update_C();
        esp_task_wdt_reset();  // Feed watchdog
        vTaskDelay(pdMS_TO_TICKS(50));  // Process every 50ms (IMU data has higher frequency)
    }
}

/**
 * @brief Sensor data monitoring task (automatically prints data after connection)
 */
static void sensor_monitor_task(void *pvParameters)
{
    // Add current task to watchdog monitoring list
    esp_task_wdt_add(NULL);
    
    bool last_hr_connected = false;
    bool last_speed_connected = false;
    bool last_cadence_connected = false;
    
    while (1) {
        // IMU data is processed by dedicated imu_process_task, no update here
        
        bool hr_connected = bsp_ble_is_device_connected(BSP_BLE_DEVICE_TYPE_HR);
        bool cscs_connected = bsp_ble_is_device_connected(BSP_BLE_DEVICE_TYPE_CSCS);
        bool speed_supported = bsp_ble_cscs_supports_speed();
        bool cadence_supported = bsp_ble_cscs_supports_cadence();
        
        // Detect connection status changes
        if (hr_connected && !last_hr_connected) {
            printf("\n[INFO] HR device connected!\n");
            printf("Starting to receive heart rate data...\n");
        }
        if (cscs_connected && !last_speed_connected && !last_cadence_connected) {
            printf("\n[INFO] CSCS device connected!\n");
            bsp_ble_cscs_mode_t mode = bsp_ble_get_cscs_mode();
            switch (mode) {
                case BSP_BLE_CSCS_MODE_SPEED:
                    printf("Device Mode: Speed Mode\n");
                    break;
                case BSP_BLE_CSCS_MODE_CADENCE:
                    printf("Device Mode: Cadence Mode\n");
                    break;
                case BSP_BLE_CSCS_MODE_BOTH:
                    printf("Device Mode: Speed+Cadence\n");
                    break;
                default:
                    printf("Device Mode: Identifying...\n");
                    break;
            }
        }
        
        // HR device connected, but don't auto-print data (avoid screen flooding)
        // User can manually view sensor data via command E
        
        // If CSCS device connected and supports speed, periodically print speed data
        if (cscs_connected && speed_supported) {
            float speed = bsp_ble_get_speed();
            if (speed > 0.0f) {
                printf("[Speed] %.1f km/h\n", speed);
            }
        }
        
        // If CSCS device connected and supports cadence, periodically print cadence data
        if (cscs_connected && cadence_supported) {
            float cadence = bsp_ble_get_cadence();
            if (cadence > 0.0f) {
                printf("[Cadence] %.1f RPM\n", cadence);
            }
        }
        
        // GPS data is already displayed on screen, no longer print to serial
        
        // IMU data is already displayed on screen, no longer print to serial (removed screen flooding logs)
        
        last_hr_connected = hr_connected;
        last_speed_connected = (cscs_connected && speed_supported);
        last_cadence_connected = (cscs_connected && cadence_supported);
        
        esp_task_wdt_reset();  // Feed watchdog
        vTaskDelay(pdMS_TO_TICKS(2000));  // Check every 2 seconds
    }
}

/**
 * @brief Serial debug task
 */
static void serial_debug_task(void *pvParameters)
{
    // Add current task to watchdog monitoring list
    esp_task_wdt_add(NULL);
    
    char input[32];
    char cmd_char;
    int device_num;
    bool is_waiting_for_device = false;  // Whether waiting for device number input
    
    vTaskDelay(pdMS_TO_TICKS(2000));  // Wait for system initialization to complete
    
    printf("\n\n");
    printf("========================================\n");
    printf("    BLE Serial Debug Menu\n");
    printf("========================================\n");
    printf("A - Scan HR devices\n");
    printf("B - Scan CSCS devices (Speed/Cadence, auto-detect mode after connection)\n");
    printf("C - Scan all devices (HR+CSCS)\n");
    printf("D - Show all scanned devices\n");
    printf("E - Show sensor data\n");
    printf("F - Stop scanning\n");
    printf("========================================\n");
    printf("Enter command (A-F) or device number (1-N): ");
    
    while (1) {
        if (fgets(input, sizeof(input), stdin) != NULL) {
            // Remove newline and spaces
            size_t len = strlen(input);
            if (len > 0 && input[len - 1] == '\n') {
                input[len - 1] = '\0';
                len--;
            }
            // Remove leading spaces
            while (len > 0 && input[0] == ' ') {
                memmove(input, input + 1, len);
                len--;
            }
            
            if (len == 0) {
                continue;
            }
            
            // Check if it's a letter command (A-F)
            cmd_char = toupper(input[0]);
            if (cmd_char >= 'A' && cmd_char <= 'F') {
                is_waiting_for_device = false;
                
                switch (cmd_char) {
                    case 'A':  // Scan HR devices
                        printf("\nStarting to scan HR devices (10 seconds)...\n");
                        s_current_filter_type = BSP_BLE_DEVICE_TYPE_HR;
                        bsp_ble_stop_scan();  // Stop current scan first
                        vTaskDelay(pdMS_TO_TICKS(500));
                        bsp_ble_start_scan(10);
                        vTaskDelay(pdMS_TO_TICKS(11000));  // Wait for scan to complete
                        update_debug_device_list();
                        print_device_list();
                        is_waiting_for_device = (s_debug_device_count > 0);
                        break;
                        
                    case 'B':  // Scan CSCS devices (unified scan, auto-detect mode after connection)
                        printf("\nStarting to scan CSCS devices (10 seconds)...\n");
                        printf("Note: CSCS devices unified type, auto-detect working mode (Speed/Cadence/Both) after connection\n");
                        s_current_filter_type = BSP_BLE_DEVICE_TYPE_CSCS;
                        bsp_ble_stop_scan();  // Stop current scan first
                        vTaskDelay(pdMS_TO_TICKS(500));
                        bsp_ble_start_scan(10);
                        vTaskDelay(pdMS_TO_TICKS(11000));  // Wait for scan to complete
                        update_debug_device_list();
                        print_device_list();
                        is_waiting_for_device = (s_debug_device_count > 0);
                        break;
                        
                    case 'C':  // Scan all devices (HR+CSCS)
                        printf("\nStarting to scan all devices (HR+CSCS, 10 seconds)...\n");
                        printf("Note: CSCS devices auto-detect working mode after connection\n");
                        s_current_filter_type = BSP_BLE_DEVICE_TYPE_UNKNOWN;
                        bsp_ble_stop_scan();  // Stop current scan first
                        vTaskDelay(pdMS_TO_TICKS(500));
                        bsp_ble_start_scan(10);
                        vTaskDelay(pdMS_TO_TICKS(11000));  // Wait for scan to complete
                        update_debug_device_list();
                        print_device_list();
                        is_waiting_for_device = (s_debug_device_count > 0);
                        break;
                        
                    case 'D':  // Show all devices
                        s_current_filter_type = BSP_BLE_DEVICE_TYPE_UNKNOWN;
                        update_debug_device_list();
                        print_device_list();
                        is_waiting_for_device = (s_debug_device_count > 0);
                        break;
                        
                    case 'E':  // Show sensor data
                        print_sensor_data();
                        break;
                        
                    case 'F':  // Stop scanning
                        printf("\nStopping scan...\n");
                        bsp_ble_stop_scan();
                        is_waiting_for_device = false;
                        break;
                }
            } else {
                // Try to process as device number
                device_num = atoi(input);
                if (device_num >= 1 && device_num <= 20) {
                    if (s_debug_device_count > 0) {
                        connect_device(device_num);
                        is_waiting_for_device = false;
                    } else {
                        printf("Error: No available device list, please scan devices first\n");
                    }
                } else {
                    printf("Invalid command, please enter A-F or device number 1-%d\n", s_debug_device_count);
                }
            }
            
            if (is_waiting_for_device) {
                printf("\nEnter device number to connect (1-%d) or command (A-F): ", s_debug_device_count);
            } else {
                printf("\nEnter command (A-F) or device number (1-N): ");
            }
        }
        
        esp_task_wdt_reset();  // Feed watchdog
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Main task function (watchdog task)
 * @note Phase 8: DP_Sport_Update is automatically triggered by Account mode timer
 */
static void main_task(void *pvParameters)
{
    // Add current task to watchdog monitoring list
    esp_task_wdt_add(NULL);
    
    // DP_Sport_Update is now automatically triggered by Account mode timer (100ms period)
    // Main task can be used for other system-level tasks
    
    while (1) {
        // Execute other tasks when system is idle
        esp_task_wdt_reset();  // Feed watchdog
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second period
    }
}

/**
 * @brief Application main entry function
 */
void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32-S3 Smart Cycling Computer");
    ESP_LOGI(TAG, "Project: mabiao");
    ESP_LOGI(TAG, "Framework: ESP-IDF v5.3.1");
    ESP_LOGI(TAG, "Chip: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "========================================");

    // Initialize NVS (for configuration storage later)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    // PSRAM information (ESP32-S3-WROOM-1-N16R8 contains 8MB PSRAM)
    // PSRAM is automatically initialized at startup (CONFIG_SPIRAM_BOOT_INIT=y)
    #ifdef CONFIG_SPIRAM
    ESP_LOGI(TAG, "PSRAM enabled (configured in sdkconfig)");
    #else
    ESP_LOGW(TAG, "PSRAM not enabled in sdkconfig");
    #endif

    // Print system information
    ESP_LOGI(TAG, "Free heap: %" PRIu32 " bytes", (uint32_t)esp_get_free_heap_size());
    ESP_LOGI(TAG, "Minimum free heap: %" PRIu32 " bytes", (uint32_t)esp_get_minimum_free_heap_size());

    // 1. Initialize BSP layer
    ESP_LOGI(TAG, "Initializing BSP layer...");
    // Use LovyanGFX to drive LCD (reference version2 project)
    ret = bsp_lcd_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BSP LCD: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "BSP LCD initialized (LovyanGFX)");

    ret = bsp_touch_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BSP Touch: %s", esp_err_to_name(ret));
        // Continue execution, touch functionality may be unavailable
    } else {
        ESP_LOGI(TAG, "BSP Touch initialized");
    }

    // Initialize SD card driver
    ESP_LOGI(TAG, "Initializing SD card...");
    bsp_sd_config_t sd_config = {
        .spi_host = SPI2_HOST,
        .cs_pin = 1,  // GPIO1
    };
    ret = bsp_sd_init(&sd_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BSP SD: %s", esp_err_to_name(ret));
        // Continue execution, SD card functionality may be unavailable
    } else {
        ESP_LOGI(TAG, "BSP SD initialized");

        // Mount SD card filesystem
        ret = bsp_sd_mount("/sdcard");
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
            ESP_LOGW(TAG, "SD card functionality will be unavailable");
        } else {
            ESP_LOGI(TAG, "SD card mounted successfully");

            // Get SD card information
            bsp_sd_info_t sd_info;
            ret = bsp_sd_get_info(&sd_info);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "SD card info: %llu MB total, %llu MB free, mounted: %s",
                         sd_info.total_size_mb, sd_info.free_size_mb,
                         sd_info.is_mounted ? "yes" : "no");
            }
        }
    }

    // 2. Initialize HAL layer
    ESP_LOGI(TAG, "Initializing HAL layer...");
    HAL_Init_C();  // Use C-compatible interface
    ESP_LOGI(TAG, "HAL layer initialized");
    
    // Initialize IP5306 keepalive (must start early in system initialization)
    ESP_LOGI(TAG, "Initializing IP5306 keepalive...");
    ret = bsp_ip5306_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize IP5306: %s", esp_err_to_name(ret));
        // Continue execution, but IP5306 functionality may be unavailable
    } else {
        ESP_LOGI(TAG, "IP5306 initialized");
        bsp_ip5306_start();  // Start periodic keepalive
        ESP_LOGI(TAG, "IP5306 keepalive started");
    }

    // Initialize RGB LED（GPIO20 RGB_DIN；HAL_Init_C 内已 init则此处直接返回已初始化）
    ESP_LOGI(TAG, "Initializing RGB LED...");
    ret = bsp_rgb_led_init(NULL);  // 默认 GPIO20，与原理图 RGB_DIN 一致
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize RGB LED: %s", esp_err_to_name(ret));
        // Continue execution, RGB LED functionality may be unavailable
    } else {
        ESP_LOGI(TAG, "RGB LED initialized");
        // 启动渐变色效果（系统启动时自动运行）
        ret = bsp_rgb_led_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start RGB LED effect: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "RGB LED gradient effect started");
        }
    }

    // 3. Initialize LVGL (based on LovyanGFX)
    ESP_LOGI(TAG, "Initializing LVGL port...");
    ret = lvgl_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LVGL port: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "LVGL port initialized (LovyanGFX backend)");

    /* 无源蜂鸣器：GPIO19 + LEDC 4kHz（先于 BLE，供连接/断开提示） */
    ret = bsp_buzzer_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Buzzer init failed: %s (continuing without beep)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Buzzer initialized");
    }

    // Initialize BLE driver (BSP and HAL layers, must be after LVGL initialization because lvgl_port_lock/unlock is needed)
    ESP_LOGI(TAG, "Initializing BLE driver...");
    bsp_ble_config_t ble_config = {
        .lvgl_lock = lvgl_port_lock,
        .lvgl_unlock = lvgl_port_unlock,
        .enable_scan = false,  // Don't auto-scan, manual control
    };
    ret = bsp_ble_init(&ble_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BSP BLE driver: %s", esp_err_to_name(ret));
        // Continue execution, BLE functionality may be unavailable
    } else {
        ESP_LOGI(TAG, "BSP BLE driver initialized");
        // Initialize HAL layer BLE module (use C-compatible interface)
        BLE_Init_C();
        ESP_LOGI(TAG, "HAL BLE initialized");
        ESP_LOGI(TAG, "BLE driver initialization completed");
    }
    // Note: DataProc layer BLE module is already initialized in DataProc_Init()

    // 4. Initialize DataProc layer
    ESP_LOGI(TAG, "Initializing DataProc layer...");
    DataProc_Init();
    ESP_LOGI(TAG, "DataProc layer initialized");
    
    // Load persisted configuration (weight / wheel circumference) and apply to modules.
    DP_SysConfig_RequestLoad();

    // 5. Initialize App layer (requires LVGL mutex protection to prevent LVGL task from rendering during object creation causing crash)
    ESP_LOGI(TAG, "Initializing App layer...");
    lvgl_port_lock(-1);  // Lock LVGL, prevent LVGL task from rendering
    ret = App_Init();
    lvgl_port_unlock();  // Unlock LVGL, allow rendering
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize App layer: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "App layer initialized");

    // Wait for LVGL first frame rendering to complete
    vTaskDelay(pdMS_TO_TICKS(100));

    // Create basic tasks (specific functional tasks will be added in later phases)
    ESP_LOGI(TAG, "Creating main task...");
    
    xTaskCreate(
        main_task,
        "main_task",
        4096,
        NULL,
        5,
        NULL
    );

    // Create GPS processing task (high frequency processing to avoid buffer overflow)
    ESP_LOGI(TAG, "Creating GPS process task...");
    xTaskCreate(
        gps_process_task,
        "gps_process",
        3072,  // GPS processing task stack size
        NULL,
        4,     // Higher priority to ensure timely processing
        NULL
    );
    
    ESP_LOGI(TAG, "Creating IMU process task...");
    xTaskCreate(
        imu_process_task,
        "imu_process",
        3072,  // IMU processing task stack size
        NULL,
        4,     // Higher priority to ensure timely processing
        NULL
    );

#if CORE_MODE_ENABLED
    // Core mode: don't create debug tasks, focus on core UI functionality
    ESP_LOGI(TAG, "Core mode enabled: skipping debug tasks");
#else
    // Debug mode: create serial debug task and sensor monitoring task
    ESP_LOGI(TAG, "Debug mode enabled: creating debug tasks");
    
    // Create serial debug task
    ESP_LOGI(TAG, "Creating serial debug task...");
    xTaskCreate(
        serial_debug_task,
        "serial_debug",
        4096,
        NULL,
        5,
        NULL
    );

    // Create sensor data monitoring task (increase stack size to avoid stack overflow)
    ESP_LOGI(TAG, "Creating sensor monitor task...");
    xTaskCreate(
        sensor_monitor_task,
        "sensor_monitor",
        4096,  // Increased from 2048 to 4096 to avoid stack overflow
        NULL,
        3,
        NULL
    );
#endif

    ESP_LOGI(TAG, "System initialized successfully");
    bsp_buzzer_request(BSP_BUZZ_PATTERN_BOOT);
}
