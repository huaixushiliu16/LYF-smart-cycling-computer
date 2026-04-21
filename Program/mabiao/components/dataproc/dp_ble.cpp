/**
 * @file dp_ble.cpp
 * @brief DataProc layer BLE data processing implementation
 * @note Phase 7: HAL layer encapsulation and DataProc layer implementation
 * @note Phase 8.5: Account mode integration
 */

#include "dataproc.h"
#include "hal.h"
#include "esp_log.h"
#include "utils/datacenter/Account.h"
#include "utils/datacenter/DataCenter.h"

static const char *TAG = "DP_BLE";
static bool s_initialized = false;
static Account* s_ble_account = nullptr;

// Account event callback function (C++ function, not in extern "C")
static int DP_BLE_OnEvent(Account* account, Account::EventParam_t* param)
{
    if (param->event == Account::EVENT_TIMER)
    {
        DP_BLE_Update();
        return Account::RES_OK;
    }
    
    if (param->event == Account::EVENT_SUB_PULL)
    {
        HAL::BLE_Info_t* info = (HAL::BLE_Info_t*)param->data_p;
        if (param->size != sizeof(HAL::BLE_Info_t))
        {
            return Account::RES_SIZE_MISMATCH;
        }
        HAL::BLE_GetInfo(info);
        return Account::RES_OK;
    }
    
    return Account::RES_UNSUPPORTED_REQUEST;
}

extern "C" {

void DP_BLE_Init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "DP_BLE already initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing BLE DataProc...");
    
    // Get DataCenter
    DataCenter* center = DataProc_GetDataCenter();
    if (!center) {
        ESP_LOGE(TAG, "DataCenter not initialized");
        return;
    }
    
    // Create BLE Account
    s_ble_account = new Account("BLE", center, sizeof(HAL::BLE_Info_t));
    s_ble_account->SetEventCallback(DP_BLE_OnEvent);
    s_ble_account->SetTimerPeriod(500);  // 500ms update period (BLE data updates faster)
    s_ble_account->SetTimerEnable(true);
    
    s_initialized = true;
    ESP_LOGI(TAG, "BLE DataProc initialized");
}

void DP_BLE_Update(void)
{
    if (!s_initialized) {
        return;
    }
    
    // Call HAL layer update
    HAL::BLE_Update();
    
    // Get BLE info and perform data validity check (HAL layer already checked, can verify again here)
    HAL::BLE_Info_t bleInfo;
    if (HAL::BLE_GetInfo(&bleInfo)) {
        // DataProc layer can perform additional business logic processing here
        // For example: data filtering, data fusion, etc.
        
        // Publish data to Account
        if (s_ble_account)
        {
            s_ble_account->Commit(&bleInfo, sizeof(bleInfo));
            s_ble_account->Publish();
        }
    }
}

bool DP_BLE_GetInfo(HAL::BLE_Info_t *info)
{
    if (!s_initialized || info == nullptr) {
        return false;
    }
    
    // Get BLE info from HAL layer (HAL layer has already performed data validity check)
    return HAL::BLE_GetInfo(info);
}

} // extern "C"
