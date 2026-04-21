#ifndef __WAVETABLE_PRESENTER_H
#define __WAVETABLE_PRESENTER_H

#include "PageBase.h"
#include "WaveTableModel.h"

namespace Page
{
    class WaveTable : public PageBase
    {
    public:
        WaveTable();
        virtual ~WaveTable();

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
        void StartButtonAppearAnim();
        static void onTimerUpdate(lv_timer_t *timer);
        static void onEvent(lv_event_t *event);

    private:
        lv_obj_t *cont;
        lv_obj_t *btnSystemInfos;
        lv_obj_t *btnDialplate;
        lv_obj_t *btnMap;  // MAP入口按钮
        lv_obj_t *btnBleDevices;  // BLE设备入口按钮
        lv_obj_t *btnCompass;  // 指南针入口按钮
        lv_obj_t *btnRGBControl;  // RGB控制入口按钮
        lv_timer_t *timer;
        WaveTableModel Model;
    };
}

#endif
