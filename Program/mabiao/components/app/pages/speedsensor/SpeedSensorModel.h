#ifndef __SPEEDSENSOR_MODEL_H
#define __SPEEDSENSOR_MODEL_H

#include "bsp_ble.h"
#include "dataproc_def.h"
#include "esp_err.h"
#include <stdint.h>

class Account;

namespace Page
{
class SpeedSensorModel
{
public:
    static const int MAX_DEVICES = 20;

    void Init();
    void Deinit();

    esp_err_t StartScan(uint32_t duration_sec = 0);
    esp_err_t StopScan();
    bool IsScanning() const;

    esp_err_t GetDevicesCSCS(bsp_ble_device_info_t *devices, int *count, int max_count);

    esp_err_t ConnectDevice(const uint8_t *addr, uint8_t addr_type);
    esp_err_t DisconnectCSCS();
    bool IsConnected() const;

    bsp_ble_cscs_mode_t GetMode() const;
    esp_err_t SetMode(bsp_ble_cscs_mode_t mode);

    uint16_t GetWheelMM() const;
    void SetWheelMM(uint16_t mm);
    esp_err_t SaveWheelToSysConfig();

    void SetStatusBarStyle(int style);
    void SetStatusBarAppear(bool en);

private:
    Account *account;
    bool scanning;
    uint16_t wheel_mm;
};
}

#endif

