#ifndef __BLEDEVICES_MODEL_H
#define __BLEDEVICES_MODEL_H

#include "bsp_ble.h"
#include "dataproc.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// 前向声明
class Account;

namespace Page
{

class BleDevicesModel
{
public:
    static const int MAX_DEVICES = 20;  // 最大设备数量

    void Init();
    void Deinit();

    // BLE操作
    esp_err_t StartScan(uint32_t duration_sec = 0);  // 0表示持续扫描
    esp_err_t StopScan();
    bool IsScanning();
    
    // 设备列表
    int GetDeviceCount();
    esp_err_t GetDevices(bsp_ble_device_info_t *devices, int *count, int max_count);
    
    // 设备连接
    esp_err_t ConnectDevice(const uint8_t *addr, uint8_t addr_type);
    esp_err_t DisconnectDevice(uint16_t conn_handle);
    bool IsDeviceConnected(bsp_ble_device_type_t type);
    
    // 状态栏
    void SetStatusBarStyle(int style);
    void SetStatusBarAppear(bool en);
    
    // 获取连接状态统计
    int GetConnectedDeviceCount();

private:
    Account *account;
    bool scanning;
};

}

#endif
