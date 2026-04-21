#include "SystemInfosModel.h"
#include "dataproc.h"
#include "dataproc_def.h"
#include "Account.h"  // 使用dataproc组件公开的Account.h
#include "hal_def.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <string.h>  // for memset
#include "lvgl.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace Page;

// 时间格式化辅助函数
static void FormatTimeString(uint64_t seconds, char* buf, uint32_t len)
{
    // 确保 buf 总是被正确初始化，即使参数无效
    if (buf == nullptr || len == 0) {
        return;
    }
    
    // 确保 buf 总是以 \0 结尾
    buf[0] = '\0';
    
    if (len > 1) {
        uint64_t hours = seconds / 3600;
        uint64_t minutes = (seconds % 3600) / 60;
        uint64_t secs = seconds % 60;
        snprintf(buf, len, "%02llu:%02llu:%02llu", hours, minutes, secs);
    }
    
    // 确保字符串以 \0 结尾（snprintf 应该已经做了，但为了安全再确保一次）
    if (len > 0) {
        buf[len - 1] = '\0';
    }
}

void SystemInfosModel::Init()
{
    ESP_LOGI("SystemInfosModel", "Initializing SystemInfosModel...");
    
    DataCenter* center = DataProc_GetDataCenter();
    if (center == nullptr) {
        ESP_LOGE("SystemInfosModel", "DataCenter is nullptr, cannot create Account");
        return;  // DataCenter未初始化，无法创建Account
    }
    
    account = new Account("SystemInfos", center);
    if (account == nullptr) {
        ESP_LOGE("SystemInfosModel", "Failed to create Account");
        return;  // Account创建失败
    }
    
    ESP_LOGI("SystemInfosModel", "Account created successfully");

    account->Subscribe("SportStatus");
    account->Subscribe("GPS");
    account->Subscribe("MAG");
    account->Subscribe("IMU");
    account->Subscribe("Power");
    account->Subscribe("Storage");
    // 移除：account->Subscribe("Clock");
    
    ESP_LOGI("SystemInfosModel", "SystemInfosModel initialized successfully");
}

void SystemInfosModel::Deinit()
{
    if (account)
    {
        delete account;
        account = nullptr;
    }
}

void SystemInfosModel::GetSportInfo(
    float *trip,
    char *time, uint32_t len,
    float *maxSpd)
{
    if (trip == nullptr || time == nullptr || maxSpd == nullptr || len == 0) {
        return;
    }
    
    if (account == nullptr) {
        *trip = -1.0f;  // 使用-1表示数据无效
        strncpy(time, "--", len);
        time[len - 1] = '\0';
        *maxSpd = -1.0f;
        return;
    }
    
    SportStatus_Info_t sport;
    memset(&sport, 0, sizeof(sport));
    int ret = account->Pull("SportStatus", &sport, sizeof(sport));
    if (ret != Account::RES_OK) {
        // Pull失败，数据未准备好
        *trip = -1.0f;
        if (len > 0) {
            strncpy(time, "--", len);
            time[len - 1] = '\0';
        }
        *maxSpd = -1.0f;
        return;
    }
    
    *trip = sport.total_distance;
    FormatTimeString(sport.total_time, time, len);
    // 确保 time 总是以 \0 结尾
    if (len > 0) {
        time[len - 1] = '\0';
    }
    *maxSpd = sport.speed_max_kph;
}

void SystemInfosModel::GetGPSInfo(
    float *lat,
    float *lng,
    float *alt,
    char *utc, uint32_t len,
    float *course,
    float *speed)
{
    if (account == nullptr) {
        *lat = -999.0f;  // 使用-999表示数据无效
        *lng = -999.0f;
        *alt = -999.0f;
        if (len > 0) {
            strncpy(utc, "--", len);
            utc[len - 1] = '\0';
        }
        *course = -1.0f;
        *speed = -1.0f;
        return;
    }
    
    HAL::GPS_Info_t gps;
    memset(&gps, 0, sizeof(gps));
    int ret = account->Pull("GPS", &gps, sizeof(gps));
    if (ret != Account::RES_OK) {
        // Pull失败，数据未准备好
        *lat = -999.0f;
        *lng = -999.0f;
        *alt = -999.0f;
        if (len > 0) {
            strncpy(utc, "--", len);
            utc[len - 1] = '\0';
        }
        *course = -1.0f;
        *speed = -1.0f;
        return;
    }
    
    *lat = (float)gps.latitude;
    *lng = (float)gps.longitude;
    *alt = gps.altitude;
    if (len > 0) {
        snprintf(
            utc, len,
            "%d-%02d-%02d\n%02d:%02d:%02d",
            gps.clock.year,
            gps.clock.month,
            gps.clock.day,
            gps.clock.hour,
            gps.clock.minute,
            gps.clock.second);
        // 确保字符串以 \0 结尾
        utc[len - 1] = '\0';
    }
    *course = gps.course;
    *speed = gps.speed;
}

void SystemInfosModel::GetMAGInfo(
    float *dir,
    int *x,
    int *y,
    int *z)
{
    if (account == nullptr) {
        *dir = -1.0f;  // 使用-1表示数据无效
        *x = -999;
        *y = -999;
        *z = -999;
        return;
    }
    
    // 使用公共类型 MAG_Info_t
    MAG_Info_t mag;
    memset(&mag, 0, sizeof(mag));
    int ret = account->Pull("MAG", &mag, sizeof(mag));
    if (ret != Account::RES_OK) {
        // Pull失败，数据未准备好
        *dir = -1.0f;
        *x = -999;
        *y = -999;
        *z = -999;
        return;
    }

    // 计算方向（度），基于magX和magY
    *dir = atan2f(mag.magY, mag.magX) * 180.0f / M_PI;
    if (*dir < 0) {
        *dir += 360.0f;
    }
    
    // 转换为整数（μT转换为整数，保留1位小数精度）
    *x = (int)(mag.magX * 10);
    *y = (int)(mag.magY * 10);
    *z = (int)(mag.magZ * 10);
}

void SystemInfosModel::GetIMUInfo(
    int *step,
    char *info, uint32_t len)
{
    if (account == nullptr) {
        *step = -1;  // 使用-1表示数据无效
        if (len > 0) {
            strncpy(info, "--\n--\n--\n--\n--\n--", len);
            info[len - 1] = '\0';
        }
        return;
    }
    
    HAL::IMU_Info_t imu;
    memset(&imu, 0, sizeof(imu));

    int ret = account->Pull("IMU", &imu, sizeof(imu));
    if (ret != Account::RES_OK) {
        // Pull失败，数据未准备好
        *step = -1;
        if (len > 0) {
            strncpy(info, "--\n--\n--\n--\n--\n--", len);
            info[len - 1] = '\0';
        }
        return;
    }
    
    *step = imu.steps;
    if (len > 0) {
        snprintf(
            info,
            len,
            "%d\n%d\n%d\n%d\n%d\n%d",
            imu.ax,
            imu.ay,
            imu.az,
            imu.gx,
            imu.gy,
            imu.gz);
        // 确保字符串以 \0 结尾
        info[len - 1] = '\0';
    }
}

// GetRTCInfo已移除

void SystemInfosModel::GetBatteryInfo(
    int *usage,
    float *voltage,
    char *state, uint32_t len)
{
    if (account == nullptr) {
        *usage = -1;  // 使用-1表示数据无效
        *voltage = -1.0f;
        if (len > 0) {
            strncpy(state, "--", len);
            state[len - 1] = '\0';
        }
        return;
    }
    
    HAL::Power_Info_t power;
    memset(&power, 0, sizeof(power));
    int ret = account->Pull("Power", &power, sizeof(power));
    if (ret != Account::RES_OK) {
        // Pull失败，数据未准备好
        *usage = -1;
        *voltage = -1.0f;
        if (len > 0) {
            strncpy(state, "--", len);
            state[len - 1] = '\0';
        }
        return;
    }
    
    *usage = power.usage;
    *voltage = (float)power.voltage / 1000.0;
    if (len > 0) {
        strncpy(state, power.isCharging ? "CHARGE" : "DISCHARGE", len);
        state[len - 1] = '\0';
    }
}

void SystemInfosModel::GetStorageInfo(
    bool *detect,
    const char **type,
    char *size, uint32_t len)
{
    if (account == nullptr) {
        *detect = false;
        *type = "--";
        if (len > 0) {
            strncpy(size, "--", len);
            size[len - 1] = '\0';
        }
        return;
    }
    
    // 使用公共类型 Storage_Basic_Info_t
    Storage_Basic_Info_t storage;
    memset(&storage, 0, sizeof(storage));
    int ret = account->Pull("Storage", &storage, sizeof(storage));
    if (ret != Account::RES_OK) {
        // Pull失败，数据未准备好
        *detect = false;
        *type = "--";
        if (len > 0) {
            strncpy(size, "--", len);
            size[len - 1] = '\0';
        }
        return;
    }
    
    *detect = storage.isDetect;
    *type = (storage.type != nullptr) ? storage.type : "--";
    if (len > 0) {
        if (storage.totalSizeMB > 0) {
            snprintf(size, len, "%.1f GB", storage.totalSizeMB / 1024.0f);
        } else {
            strncpy(size, "--", len);
        }
        // 确保字符串以 \0 结尾
        size[len - 1] = '\0';
    }
}

void SystemInfosModel::GetSystemInfo(
    char *firmVer, uint32_t len1,
    char *authorName, uint32_t len2,
    char *lvglVer, uint32_t len3)
{
    snprintf(firmVer, len1, "V1.0");
    snprintf(authorName, len2, "槐序十六");
    snprintf(lvglVer, len3, "%d.%d", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR);
}

void SystemInfosModel::SetStatusBarStyle(int style)
{
    if (account == nullptr) {
        return;
    }
    
    StatusBar_Info_t info;
    memset(&info, 0, sizeof(info));
    info.cmd = STATUS_BAR_CMD_SET_STYLE;
    info.param.style = (StatusBar_Style_t)style;
    
    account->Notify("StatusBar", &info, sizeof(info));
}

void SystemInfosModel::SetStatusBarAppear(bool en)
{
    if (account == nullptr) {
        return;
    }
    
    StatusBar_Info_t info;
    memset(&info, 0, sizeof(info));
    info.cmd = STATUS_BAR_CMD_APPEAR;
    info.param.appear = en;
    
    account->Notify("StatusBar", &info, sizeof(info));
}
