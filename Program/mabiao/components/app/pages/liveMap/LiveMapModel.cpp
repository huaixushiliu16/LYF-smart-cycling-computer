#include "LiveMapModel.h"
#include "config.h"
#include "PointContainer.h"
#include <cstdio>
#include <string.h>
#include <inttypes.h>

using namespace Page;

LiveMapModel::LiveMapModel()
{

}

void LiveMapModel::Init()
{
    DataCenter* center = DataProc_GetDataCenter();
    if (center == nullptr) {
        return;
    }
    account = new Account("LiveMapModel", center, 0, this);
    account->Subscribe("GPS");
    account->Subscribe("SportStatus");
    account->Subscribe("TrackFilter");
    account->Subscribe("SysConfig");
    account->Subscribe("StatusBar");
    account->SetEventCallback(onEvent);
}

void LiveMapModel::Deinit()
{
    if (account)
    {
        delete account;
        account = nullptr;
    }
}

void LiveMapModel::GetGPS_Info(HAL::GPS_Info_t* info)
{
    memset(info, 0, sizeof(HAL::GPS_Info_t));
    if(account->Pull("GPS", info, sizeof(HAL::GPS_Info_t)) != Account::RES_OK)
    {
        return;
    }

    /* Use default location */
    if (!info->isVaild)
    {
        SysConfig_Info_t sysConfig;
        if(account->Pull("SysConfig", &sysConfig, sizeof(sysConfig)) == Account::RES_OK)
        {
            info->longitude = sysConfig.longitude;
            info->latitude = sysConfig.latitude;

#if CONFIG_LIVE_MAP_DEBUG_ENABLE
            printf("[Model] SysConfigLongitude:%.2f\n", sysConfig.longitude);
            printf("[Model] SysConfiglatitude:%.2f\n", sysConfig.latitude);
#endif
        }
    }
}

void LiveMapModel::GetArrowTheme(char* buf, uint32_t size)
{
    SysConfig_Info_t sysConfig;
    if(account->Pull("SysConfig", &sysConfig, sizeof(sysConfig)) != Account::RES_OK)
    {
        buf[0] = '\0';
        return;
    }
    strncpy(buf, sysConfig.arrowTheme, size);
    buf[size - 1] = '\0';
}

bool LiveMapModel::GetTrackFilterActive()
{
    TrackFilter_Info_t info;
    if(account->Pull("TrackFilter", &info, sizeof(info)) != Account::RES_OK)
    {
        return false;
    }

    return info.isActive;
}

int LiveMapModel::onEvent(Account* account, Account::EventParam_t* param)
{
    if (param->event != Account::EVENT_PUB_PUBLISH)
    {
        return Account::RES_UNSUPPORTED_REQUEST;
    }

    if (strcmp(param->tran->ID, "SportStatus") != 0
            || param->size != sizeof(SportStatus_Info_t))
    {
        return Account::RES_PARAM_ERROR;
    }

    LiveMapModel* instance = (LiveMapModel*)account->UserData;
    memcpy(&(instance->sportStatusInfo), param->data_p, param->size);

    return Account::RES_OK;
}

void LiveMapModel::TrackReload(TrackPointFilter::Callback_t callback, void* userData)
{
    TrackFilter_Info_t info;
    if(account->Pull("TrackFilter", &info, sizeof(info)) != Account::RES_OK)
    {
        return;
    }

    if (!info.isActive || info.pointCont == nullptr)
    {
        return;
    }

    PointContainer* pointContainer = (PointContainer*)info.pointCont;

    pointContainer->PopStart();
    pointFilter.Reset();

    TrackPointFilter ptFilter;

    ptFilter.SetOffsetThreshold(CONFIG_TRACK_FILTER_OFFSET_THRESHOLD);
    ptFilter.SetOutputPointCallback(callback);
    ptFilter.SetSecondFilterModeEnable(true);
    ptFilter.userData = userData;

    int32_t pointX, pointY;
    while (pointContainer->PopPoint(&pointX, &pointY))
    {
        int32_t mapX, mapY;
        mapConv.ConvertMapLevelPos(
            &mapX, &mapY,
            pointX, pointY,
            info.level
        );

        ptFilter.PushPoint(mapX, mapY);
    }
    ptFilter.PushEnd();
}

void LiveMapModel::SetStatusBarStyle(StatusBar_Style_t style)
{
    StatusBar_Info_t info;
    memset(&info, 0, sizeof(info));

    info.cmd = STATUS_BAR_CMD_SET_STYLE;
    info.param.style = style;

    account->Notify("StatusBar", &info, sizeof(info));
}

const char* LiveMapModel::MakeTimeString(uint64_t seconds, char* buf, uint16_t len)
{
    if (buf == nullptr || len == 0) {
        return buf;
    }
    
    // 当前项目的 single_time 单位是秒
    uint64_t ss = seconds;
    uint64_t mm = ss / 60;
    uint32_t hh = (uint32_t)(mm / 60);

    snprintf(
        buf, len,
        "%" PRIu32 ":%02" PRIu32 ":%02" PRIu32,
        hh,
        (uint32_t)(mm % 60),
        (uint32_t)(ss % 60)
    );

    return buf;
}
