/**
 * @file dataproc.cpp
 * @brief DataProc layer main implementation
 * @note Phase 8: Account mode integration
 */

#include "dataproc.h"
#include "esp_log.h"
#include "utils/datacenter/DataCenter.h"
#include "utils/datacenter/Account.h"

static const char *TAG = "DATAPROC";
static DataCenter* s_datacenter = nullptr;

// Forward declaration
extern int DP_Sport_OnEvent(Account* account, Account::EventParam_t* param);
// Note: GPS, IMU, Power, Storage Account event callback functions are static,
// Accounts are created in their respective Init functions, no need to declare here

extern "C" {

void DataProc_Init(void)
{
    ESP_LOGI(TAG, "Initializing DataProc layer...");
    
    // Initialize DataCenter
    s_datacenter = new DataCenter("DataProc");
    
    // Create SportStatus Account
    Account* sport_account = new Account("SportStatus", s_datacenter, sizeof(SportStatus_Info_t));
    sport_account->SetEventCallback(DP_Sport_OnEvent);
    // Timer will be set in DP_Sport_Init
    
    // Initialize each DataProc module (must be after DataCenter initialization)
    DP_GPS_Init();      // Will create GPS Account internally
    DP_IMU_Init();      // Will create IMU and MAG Account internally
    DP_Sport_Init();
    DP_Power_Init();    // Will create Power Account internally
    DP_BLE_Init();      // Will create BLE Account internally (BLE Account created at system startup, but BLE hardware initialization is immediate now)
    DP_Storage_Init();  // Will create Storage Account internally
    DP_StatusBar_Init(); // Will create StatusBar Account internally
    DP_SysConfig_Init(); // Will create SysConfig Account internally
    DP_TrackFilter_Init(); // Will create TrackFilter Account internally
    DP_RGB_Init(); // Will create RGB Account internally
    
    ESP_LOGI(TAG, "DataProc layer initialized");
}

} // extern "C"

// 这些函数在extern "C"块外实现，但使用C链接
DataCenter* DataProc_GetDataCenter(void)
{
    return s_datacenter;
}

Account* DataProc_SearchAccount(const char* name)
{
    if (s_datacenter == nullptr || name == nullptr) {
        return nullptr;
    }
    return s_datacenter->SearchAccount(name);
}
