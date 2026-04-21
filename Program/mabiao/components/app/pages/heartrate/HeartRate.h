#ifndef __HEARTRATE_PRESENTER_H
#define __HEARTRATE_PRESENTER_H

#include "PageBase.h"
#include "HeartRateModel.h"

namespace Page
{
    class HeartRate : public PageBase
    {
    public:
        HeartRate();
        virtual ~HeartRate();

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
        void OnWeightDelta(int delta_kg);
        void OnSaveClicked();

    private:
        lv_obj_t *cont;
        lv_obj_t *title_label;
        lv_obj_t *scan_btn;
        lv_obj_t *disconnect_btn;
        lv_obj_t *device_list;
        lv_obj_t *weight_value_label;
        lv_obj_t *weight_minus_btn;
        lv_obj_t *weight_plus_btn;
        lv_obj_t *save_btn;
        lv_obj_t *live_label;
        lv_timer_t *timer;

        lv_obj_t *device_items[HeartRateModel::MAX_DEVICES];
        int device_item_count;

        HeartRateModel Model;
        int weight_kg;
        bool dirty;
    };
}

#endif

