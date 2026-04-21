/**
 * @file dp_trackfilter.cpp
 * @brief 轨迹过滤数据处理实现
 * @note LiveMap移植：轨迹记录和过滤
 */

#include "dataproc.h"
#include "dataproc_def.h"
#include "hal_def.h"
#include "esp_log.h"
#include "utils/datacenter/Account.h"
#include "utils/datacenter/DataCenter.h"
#include "MapConv.h"
#include "TrackFilter.h"
#include "PointContainer.h"
#include "config.h"  // 使用app组件的config.h
#include <string.h>
#include <inttypes.h>  // 用于PRIu32宏

static const char *TAG = "DP_TRACKFILTER";
static bool s_initialized = false;
static Account* s_trackfilter_account = nullptr;

typedef struct
{
   MapConv mapConv;
   TrackPointFilter pointFilter;
   PointContainer* pointContainer;
   bool isStarted;
   bool isActive;
} TrackFilter_t;

static TrackFilter_t s_trackFilter = {};

static void onNotify(Account* account, TrackFilter_Info_t* info)
{
   switch (info->cmd)
   {
   case TRACK_FILTER_CMD_START:
       s_trackFilter.pointContainer = new PointContainer;
       s_trackFilter.pointFilter.Reset();
       s_trackFilter.isActive = true;
       s_trackFilter.isStarted = true;
       ESP_LOGI(TAG, "Track filter start");
       break;
   case TRACK_FILTER_CMD_PAUSE:
       s_trackFilter.isActive = false;
       ESP_LOGI(TAG, "Track filter pause");
       break;
   case TRACK_FILTER_CMD_CONTINUE:
       s_trackFilter.isActive = true;
       ESP_LOGI(TAG, "Track filter continue");
       break;
   case TRACK_FILTER_CMD_STOP:
   {
       s_trackFilter.isStarted = false;
       s_trackFilter.isActive = false;

       if (s_trackFilter.pointContainer)
       {
           delete s_trackFilter.pointContainer;
           s_trackFilter.pointContainer = nullptr;
       }

      uint32_t sum = 0, output = 0;
      s_trackFilter.pointFilter.GetCounts(&sum, &output);
      uint32_t filtered_percent = sum ? (100 - output * 100 / sum) : 0;
      ESP_LOGI(TAG, "Track filter stop, filted(%" PRIu32 "%%): sum = %" PRIu32 ", output = %" PRIu32,
          filtered_percent, sum, output);
       break;
   }
   default:
       break;
   }
}

static void onPublish(Account* account, HAL::GPS_Info_t* gps)
{
   int32_t mapX, mapY;
   s_trackFilter.mapConv.ConvertMapCoordinate(
       gps->longitude,
       gps->latitude,
       &mapX,
       &mapY
   );

   if (s_trackFilter.pointFilter.PushPoint(mapX, mapY))
   {
       s_trackFilter.pointContainer->PushPoint(mapX, mapY);
   }
}

// Account事件回调
static int DP_TrackFilter_OnEvent(Account* account, Account::EventParam_t* param)
{
   if (param->event == Account::EVENT_PUB_PUBLISH
           && param->size == sizeof(HAL::GPS_Info_t))
   {
       if (s_trackFilter.isActive)
       {
           onPublish(account, (HAL::GPS_Info_t*)param->data_p);
       }

       return Account::RES_OK;
   }

   if (param->size != sizeof(TrackFilter_Info_t))
   {
       return Account::RES_SIZE_MISMATCH;
   }

   switch (param->event)
   {
   case Account::EVENT_SUB_PULL:
   {
       TrackFilter_Info_t* info = (TrackFilter_Info_t*)param->data_p;
       info->pointCont = s_trackFilter.pointContainer;
       info->level = (uint8_t)s_trackFilter.mapConv.GetLevel();
       info->isActive = s_trackFilter.isStarted;
       break;
   }
   case Account::EVENT_NOTIFY:
       onNotify(account, (TrackFilter_Info_t*)param->data_p);
       break;

   default:
       break;
   }

    return Account::RES_OK;
}

extern "C" {

void DP_TrackFilter_Init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "TrackFilter already initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing TrackFilter DataProc...");
    
    // 获取DataCenter
    DataCenter* center = DataProc_GetDataCenter();
    if (!center) {
        ESP_LOGE(TAG, "DataCenter not initialized");
        return;
    }
    
    // 创建TrackFilter Account
    s_trackfilter_account = new Account("TrackFilter", center, 0);
    s_trackfilter_account->Subscribe("GPS");
    s_trackfilter_account->SetEventCallback(DP_TrackFilter_OnEvent);
    
    // 初始化
    s_trackFilter.pointContainer = nullptr;
    s_trackFilter.isActive = false;
    s_trackFilter.isStarted = false;

    s_trackFilter.mapConv.SetLevel(CONFIG_LIVE_MAP_LEVEL_DEFAULT);
    s_trackFilter.pointFilter.SetOffsetThreshold(CONFIG_TRACK_FILTER_OFFSET_THRESHOLD);
    
    s_initialized = true;
    ESP_LOGI(TAG, "TrackFilter DataProc initialized");
}

} // extern "C"
