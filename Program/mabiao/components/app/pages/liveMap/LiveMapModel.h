#ifndef __LIVEMAP_MODEL_H
#define __LIVEMAP_MODEL_H

#include "lvgl.h"
#include "hal_def.h"
#include "MapConv.h"
#include "TileConv.h"
#include "TrackFilter.h"
#include "dataproc.h"
#include "dataproc_def.h"
#include "Account.h"
#include <vector>

namespace Page
{

class LiveMapModel
{
public:
    LiveMapModel();
    ~LiveMapModel() {}
    void Init();
    void Deinit();
    void GetGPS_Info(HAL::GPS_Info_t* info);
    void GetArrowTheme(char* buf, uint32_t size);
    bool GetTrackFilterActive();
    void TrackReload(TrackPointFilter::Callback_t callback, void* userData);
    void SetStatusBarStyle(StatusBar_Style_t style);
    
    // 时间格式化函数（供外部调用）
    static const char* MakeTimeString(uint64_t seconds, char* buf, uint16_t len);

public:
    MapConv mapConv;
    TileConv tileConv;
    TrackPointFilter pointFilter;
    TrackLineFilter lineFilter;
    SportStatus_Info_t sportStatusInfo;

private:
    Account* account;

private:
    static int onEvent(Account* account, Account::EventParam_t* param);

private:
};

}

#endif
