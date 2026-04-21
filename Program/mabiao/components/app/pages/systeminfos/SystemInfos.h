#ifndef __SYSTEM_INFOS_PRESENTER_H
#define __SYSTEM_INFOS_PRESENTER_H

#include "SystemInfosView.h"
#include "SystemInfosModel.h"
#include "PageBase.h"

namespace Page
{

class SystemInfos : public PageBase
{
public:

public:
    SystemInfos();
    virtual ~SystemInfos();

    virtual void onCustomAttrConfig();
    virtual void onViewLoad();
    virtual void onViewDidLoad();
    virtual void onViewWillAppear();
    virtual void onViewDidAppear();
    virtual void onViewWillDisappear();
    virtual void onViewDidDisappear();
    virtual void onViewUnload();
    virtual void onViewDidUnload();

private:
    void Update();
    void AttachEvent(lv_obj_t* obj);
    static void onTimerUpdate(lv_timer_t* timer);
    static void onEvent(lv_event_t* event);
    static void onRootEvent(lv_event_t* event);

private:
    SystemInfosView View;
    SystemInfosModel Model;
    lv_timer_t* timer;
    
    // 双击检测相关
    static lv_obj_t* s_last_clicked_obj;
    static uint32_t s_last_click_time;
    static const uint32_t DOUBLE_CLICK_TIME_MS = 300;  // 双击时间间隔（毫秒）
};

}

#endif
