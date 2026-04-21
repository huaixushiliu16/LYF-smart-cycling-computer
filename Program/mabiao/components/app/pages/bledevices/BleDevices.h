#ifndef __BLEDEVICES_PRESENTER_H
#define __BLEDEVICES_PRESENTER_H

#include "PageBase.h"
#include "BleDevicesModel.h"

namespace Page
{
    class BleDevices : public PageBase
    {
    public:
        BleDevices();
        virtual ~BleDevices();

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
        void CreateUI();
        void CreateTitleBar();
        void CreateMenuButtons();
        static void onEvent(lv_event_t *event);
        static void onRootEvent(lv_event_t *event);
        
    private:
        lv_obj_t *cont;              // 主容器
        lv_obj_t *title_label;      // 标题标签
        lv_obj_t *btn_speed;        // 速度/踏频入口
        lv_obj_t *btn_hr;           // 心率入口
        
        BleDevicesModel Model;
    };
}

#endif
