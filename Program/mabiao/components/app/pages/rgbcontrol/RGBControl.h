#ifndef __RGB_CONTROL_PRESENTER_H
#define __RGB_CONTROL_PRESENTER_H

#include "RGBControlView.h"
#include "RGBControlModel.h"
#include "PageBase.h"

namespace Page
{

class RGBControl : public PageBase
{
public:
    RGBControl();
    virtual ~RGBControl();

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
    void AttachEvent(lv_obj_t *obj);
    static void onTimerUpdate(lv_timer_t *timer);
    static void onEvent(lv_event_t *event);
    static void onRootEvent(lv_event_t *event);
    
    // 事件处理函数
    void OnSwitchToggled();

private:
    RGBControlView View;
    RGBControlModel Model;
    lv_timer_t *timer;
    RGB_Info_t current_rgb_info;
};

}

#endif
