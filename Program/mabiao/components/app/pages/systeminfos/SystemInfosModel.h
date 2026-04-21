#ifndef __SYSTEM_INFOS_MODEL_H
#define __SYSTEM_INFOS_MODEL_H

#include "dataproc.h"

// 前向声明（Account类在全局命名空间中）
class Account;

namespace Page
{

class SystemInfosModel
{
public:
    void Init();
    void Deinit();

    void GetSportInfo(
        float* trip,
        char* time, uint32_t len,
        float* maxSpd
    );

    void GetGPSInfo(
        float* lat,
        float* lng,
        float* alt,
        char* utc, uint32_t len,
        float* course,
        float* speed
    );

    void GetMAGInfo(
        float* dir,
        int* x,
        int* y,
        int* z
    );

    void GetIMUInfo(
        int* step,
        char* info, uint32_t len
    );

    // GetRTCInfo已移除

    void GetBatteryInfo(
        int* usage,
        float* voltage,
        char* state, uint32_t len
    );

    void GetStorageInfo(
        bool* detect,
        const char** type,
        char* size, uint32_t len
    );

    void GetSystemInfo(
        char* firmVer, uint32_t len1,
        char* authorName, uint32_t len2,
        char* lvglVer, uint32_t len3
    );

    void SetStatusBarStyle(int style);  // 简化，不使用DataProc命名空间
    void SetStatusBarAppear(bool en);

private:
    Account *account;

private:

};

}

#endif
