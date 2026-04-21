#ifndef __SPEEDSENSOR_PRESENTER_H
#define __SPEEDSENSOR_PRESENTER_H

#include "PageBase.h"
#include "SpeedSensorModel.h"

namespace Page
{
    class SpeedSensor : public PageBase
    {
    public:
        SpeedSensor();
        virtual ~SpeedSensor();

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
        void CreateDeviceList();
        void CreateSettings();
        void CreateLiveData();
        void UpdateUI();
        void UpdateDeviceList();

        static void onTimerUpdate(lv_timer_t *timer);
        static void onEvent(lv_event_t *event);
        static void onRootEvent(lv_event_t *event);

        void OnScanClicked();
        void OnConnectClicked(int index);
        void OnDisconnectClicked();
        void OnModeClicked(bsp_ble_cscs_mode_t mode);
        void OnWheelDelta(int delta_mm);
        void OnSaveClicked();

    private:
        lv_obj_t *cont;
        lv_obj_t *title_label;
        lv_obj_t *scan_btn;
        lv_obj_t *disconnect_btn;
        lv_obj_t *device_list;
        lv_obj_t *wheel_value_label;
        lv_obj_t *wheel_minus_btn;
        lv_obj_t *wheel_plus_btn;
        lv_obj_t *save_btn;
        lv_obj_t *mode_speed_btn;
        lv_obj_t *mode_cadence_btn;
        lv_obj_t *live_label;
        lv_timer_t *timer;

        lv_obj_t *device_items[SpeedSensorModel::MAX_DEVICES];
        int device_item_count;

        SpeedSensorModel Model;
        uint16_t wheel_mm;
        bool dirty;
    };
}

#endif

