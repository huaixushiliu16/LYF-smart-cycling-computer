/**
 * @file bsp_ble.h
 * @brief BSP层BLE驱动接口（基于NimBLE）
 * @note 阶段4：BLE Client驱动开发
 */

#ifndef __BSP_BLE_H
#define __BSP_BLE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BLE设备类型
 */
typedef enum {
    BSP_BLE_DEVICE_TYPE_HR,           // 心率设备（Heart Rate Service）
    BSP_BLE_DEVICE_TYPE_CSCS,         // CSCS设备（统一类型，根据实际数据判断模式）
    BSP_BLE_DEVICE_TYPE_UNKNOWN,      // 未知设备
} bsp_ble_device_type_t;

/**
 * @brief CSCS设备工作模式
 */
typedef enum {
    BSP_BLE_CSCS_MODE_UNKNOWN,        // 未知模式（未确定）
    BSP_BLE_CSCS_MODE_SPEED,          // 速度模式（只发送速度数据）
    BSP_BLE_CSCS_MODE_CADENCE,        // 踏频模式（只发送踏频数据）
    BSP_BLE_CSCS_MODE_BOTH,           // 同时支持速度和踏频（较少见）
} bsp_ble_cscs_mode_t;

/**
 * @brief BLE设备信息结构体
 */
typedef struct {
    uint8_t addr[6];                  // MAC地址
    uint8_t addr_type;                // 地址类型（BLE_ADDR_PUBLIC或BLE_ADDR_RANDOM）
    char name[32];                    // 设备名称
    int8_t rssi;                      // 信号强度
    bsp_ble_device_type_t type;        // 设备类型
    bool is_connected;                // 连接状态
} bsp_ble_device_info_t;

/**
 * @brief LVGL互斥锁函数指针类型（用于线程安全的LVGL操作）
 */
typedef bool (*bsp_ble_lvgl_lock_fn_t)(int timeout_ms);
typedef void (*bsp_ble_lvgl_unlock_fn_t)(void);

/**
 * @brief BLE配置结构体
 */
typedef struct {
    bool enable_scan;                 // 是否启用扫描
    uint32_t scan_interval_ms;        // 扫描间隔（毫秒）
    bsp_ble_lvgl_lock_fn_t lvgl_lock;    // LVGL互斥锁获取函数（可选，NULL表示不使用锁）
    bsp_ble_lvgl_unlock_fn_t lvgl_unlock; // LVGL互斥锁释放函数（可选，NULL表示不使用锁）
} bsp_ble_config_t;

/**
 * @brief 初始化BLE驱动
 * @param config BLE配置参数
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t bsp_ble_init(const bsp_ble_config_t *config);

/**
 * @brief 开始扫描BLE设备
 * @param duration_sec 扫描持续时间（秒），0表示持续扫描
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t bsp_ble_start_scan(uint32_t duration_sec);

/**
 * @brief 停止扫描
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t bsp_ble_stop_scan(void);

/**
 * @brief 连接设备
 * @param addr 设备MAC地址（6字节）
 * @param addr_type 地址类型（BLE_ADDR_PUBLIC或BLE_ADDR_RANDOM）
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t bsp_ble_connect(const uint8_t *addr, uint8_t addr_type);

/**
 * @brief 断开连接
 * @param conn_handle 连接句柄
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t bsp_ble_disconnect(uint16_t conn_handle);

/**
 * @brief 获取已扫描的设备列表
 * @param devices 设备信息数组（输出）
 * @param count 实际设备数量（输出）
 * @param max_count 最大设备数量（输入）
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t bsp_ble_get_scanned_devices(bsp_ble_device_info_t *devices,
                                       uint8_t *count,
                                       uint8_t max_count);

/**
 * @brief 获取当前心率（bpm）
 * @return 心率值，0表示无数据
 */
uint16_t bsp_ble_get_heart_rate(void);

/**
 * @brief 获取当前速度（km/h）
 * @return 速度值，0.0表示无数据
 */
float bsp_ble_get_speed(void);

/**
 * @brief 获取当前踏频（RPM）
 * @return 踏频值，0.0表示无数据
 */
float bsp_ble_get_cadence(void);

/**
 * @brief 设置轮周长（米）
 * @param circumference_m 轮周长（米）
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t bsp_ble_set_wheel_circumference(float circumference_m);

/**
 * @brief 获取轮周长（米）
 * @return 当前轮周长（米）
 */
float bsp_ble_get_wheel_circumference(void);

/**
 * @brief 获取设备连接状态
 * @param type 设备类型
 * @return true 已连接，false 未连接
 */
bool bsp_ble_is_device_connected(bsp_ble_device_type_t type);

/**
 * @brief 获取设备连接句柄（conn_handle）
 * @param type 设备类型
 * @return 连接句柄；未连接返回 0
 */
uint16_t bsp_ble_get_conn_handle(bsp_ble_device_type_t type);

/**
 * @brief 获取CSCS设备工作模式
 * @return CSCS设备模式，如果未连接或未确定则返回BSP_BLE_CSCS_MODE_UNKNOWN
 */
bsp_ble_cscs_mode_t bsp_ble_get_cscs_mode(void);

/**
 * @brief 获取CSCS设备是否支持速度
 * @return true 支持速度，false 不支持或未连接
 */
bool bsp_ble_cscs_supports_speed(void);

/**
 * @brief 获取CSCS设备是否支持踏频
 * @return true 支持踏频，false 不支持或未连接
 */
bool bsp_ble_cscs_supports_cadence(void);

/**
 * @brief 设置 CSCS 页面的"显示偏好"（大字体优先展示 Speed 还是 Cadence）。
 * @param mode 目标偏好（BSP_BLE_CSCS_MODE_SPEED 或 BSP_BLE_CSCS_MODE_CADENCE）。
 * @note **本函数不向传感器下发任何 BLE 命令**。XOSS ARENA 等"单感"传感器
 *       的硬件模式切换由厂家手机 APP 通过私有 NUS 通道完成；标准 CSCS SC
 *       Control Point 的 Update Sensor Location 命令在这类设备上会返回
 *       虚假的 Success 但实际不切换（实测），因此本驱动不再尝试硬件层切换。
 *       真实的 speed / cadence 数值始终由 CSC Measurement 的 flags 字节按位
 *       独立解析、两路并行更新，UI 按本函数设置的偏好决定突出显示哪个。
 *       若要真正切换传感器硬件模式，请使用厂家手机 APP。
 * @return ESP_OK 成功；ESP_ERR_INVALID_STATE 未连接 CSCS 设备；ESP_ERR_INVALID_ARG 非法模式。
 */
esp_err_t bsp_ble_set_cscs_mode(bsp_ble_cscs_mode_t mode);

/**
 * @brief 更新BLE驱动（需要在主循环中定期调用）
 * @note 主要用于处理内部状态更新，实际数据通过回调函数接收
 */
void bsp_ble_update(void);

/**
 * @brief 设置LVGL互斥锁函数指针（用于线程安全的LVGL操作）
 * @param lock_fn LVGL互斥锁获取函数
 * @param unlock_fn LVGL互斥锁释放函数
 * @note 此函数应在bsp_ble_init之后、bsp_ble_ui_init之前调用
 */
void bsp_ble_set_lvgl_lock_functions(bsp_ble_lvgl_lock_fn_t lock_fn, bsp_ble_lvgl_unlock_fn_t unlock_fn);

/**
 * @brief 初始化LVGL调试界面
 */
void bsp_ble_ui_init(void);

/**
 * @brief 更新LVGL调试界面（需要在主循环中定期调用）
 */
void bsp_ble_ui_update(void);

/**
 * @brief 通知帧刷新（在LVGL flush回调中调用，用于FPS统计）
 */
void bsp_ble_ui_notify_frame(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_BLE_H */
