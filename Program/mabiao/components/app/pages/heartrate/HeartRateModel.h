#ifndef __HEARTRATE_MODEL_H
#define __HEARTRATE_MODEL_H

#include "bsp_ble.h"
#include "dataproc_def.h"
#include "esp_err.h"
#include <stdint.h>

class Account;

namespace Page
{
class HeartRateModel
{
public:
    static const int MAX_DEVICES = 20;

    void Init();
    void Deinit();

    esp_err_t StartScan(uint32_t duration_sec = 0);
    esp_err_t StopScan();
    bool IsScanning() const;

    esp_err_t GetDevicesHR(bsp_ble_device_info_t *devices, int *count, int max_count);

    esp_err_t ConnectDevice(const uint8_t *addr, uint8_t addr_type);
    esp_err_t DisconnectHR();
    bool IsConnected() const;

    int GetWeightKG() const;
    void SetWeightKG(int kg);
    esp_err_t SaveWeightToSysConfig();

    void SetStatusBarStyle(int style);
    void SetStatusBarAppear(bool en);

private:
    Account *account;
    bool scanning;
    int weight_kg;
};
}

#endif

