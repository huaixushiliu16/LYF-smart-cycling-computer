#ifndef __DIALPLATE_MODEL_H
#define __DIALPLATE_MODEL_H

#include "dataproc.h"
#include "dataproc_def.h"
#include "Account.h"
#include "hal_def.h"

namespace Page
{

class DialplateModel
{
public:
    typedef enum
    {
        REC_START    = TRACK_FILTER_CMD_START,
        REC_PAUSE    = TRACK_FILTER_CMD_PAUSE,
        REC_CONTINUE = TRACK_FILTER_CMD_CONTINUE,
        REC_STOP     = TRACK_FILTER_CMD_STOP,
        REC_READY_STOP  // 特殊状态：准备停止（只更新UI，不发送TrackFilter命令）
    } RecCmd_t;

public:
    SportStatus_Info_t sportStatusInfo;

public:
    void Init();
    void Deinit();

    bool GetGPSReady();

    float GetSpeed()
    {
        return sportStatusInfo.speed_kph;
    }

    float GetAvgSpeed()
    {
        return sportStatusInfo.speed_avg_kph;
    }

    void TrackFilterCommand(RecCmd_t cmd);
    void SetStatusBarStyle(StatusBar_Style_t style);
    void SetStatusBarAppear(bool en);

    // 获取IMU数据
    bool GetIMUInfo(HAL::IMU_Info_t* info);

    // 本地实现时间格式化函数（供外部调用）
    static const char* MakeTimeString(uint64_t seconds, char* buf, uint16_t len);

private:
    Account *account;

    // onEvent 回调函数（在 cpp 中实现）
    static int onEvent(Account* account, Account::EventParam_t* param);
};

}

#endif
