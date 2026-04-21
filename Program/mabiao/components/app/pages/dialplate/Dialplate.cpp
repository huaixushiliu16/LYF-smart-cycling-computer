#include "Dialplate.h"
#include "PageManager.h"
#include "ResourcePool.h"
#include "hal_def.h"
#include "esp_log.h"
#include <cstdio>
#include <cmath>

static const char *TAG = "Dialplate";

using namespace Page;

Dialplate::Dialplate()
    : recState(RECORD_STATE_READY),
      lastFocus(nullptr)
{
}

Dialplate::~Dialplate()
{
}

void Dialplate::onCustomAttrConfig()
{
    ESP_LOGI(TAG, "onCustomAttrConfig");

    SetCustomCacheEnable(false);
    SetCustomLoadAnimType(PageManager::LOAD_ANIM_NONE);
}

void Dialplate::onViewLoad()
{
    ESP_LOGI(TAG, "onViewLoad");
    Model.Init();
    View.Create(_root);

    AttachEvent(lv_scr_act());
    AttachEvent(View.ui.btnCont.btnMap);
    AttachEvent(View.ui.btnCont.btnRec);
    AttachEvent(View.ui.btnCont.btnMenu);
}

void Dialplate::onViewDidLoad()
{
    ESP_LOGI(TAG, "onViewDidLoad");
}

void Dialplate::onViewWillAppear()
{
    ESP_LOGI(TAG, "onViewWillAppear");

    lv_group_t *group = lv_group_get_default();
    if (group) {
        lv_group_set_wrap(group, false);

        lv_group_add_obj(group, View.ui.btnCont.btnMap);
        lv_group_add_obj(group, View.ui.btnCont.btnRec);
        lv_group_add_obj(group, View.ui.btnCont.btnMenu);
    }

    Model.SetStatusBarStyle(STATUS_BAR_STYLE_TRANSP);
    Model.SetStatusBarAppear(true);
    Update();

    View.AppearAnimStart();
}

void Dialplate::onViewDidAppear()
{
    ESP_LOGI(TAG, "onViewDidAppear");

    timer = lv_timer_create(onTimerUpdate, 1000, this);
}

void Dialplate::onViewWillDisappear()
{
    ESP_LOGI(TAG, "onViewWillDisappear");

    lv_group_t *group = lv_group_get_default();
    if (group) {
        lastFocus = lv_group_get_focused(group);
        lv_group_remove_all_objs(group);
    }
    
    if (timer) {
        lv_timer_del(timer);
        timer = nullptr;
    }
}

void Dialplate::onViewDidDisappear()
{
    ESP_LOGI(TAG, "onViewDidDisappear");
}

void Dialplate::onViewUnload()
{
    ESP_LOGI(TAG, "onViewUnload");

    Model.Deinit();
    View.Delete();
    lv_obj_remove_event_cb(lv_scr_act(), onEvent);
}

void Dialplate::onViewDidUnload()
{
    ESP_LOGI(TAG, "onViewDidUnload");
}

void Dialplate::AttachEvent(lv_obj_t *obj)
{
    lv_obj_add_event_cb(obj, onEvent, LV_EVENT_ALL, this);
}

void Dialplate::Update()
{
    // 首帧保护：刚进入 Dialplate 时 Model.sportStatusInfo 还没被 Account
    // 第一次 Pull 填充，整块内存是未初始化的（能看到 spd=2748411 / hr=60098 /
    // cal=-5.9e+14 这种垃圾值）。让 UI 跳过明显异常的那一帧，避免屏幕上闪
    // 出"inf"/"nan"/"-0.0"的残影（之前看到的 AVG/TRIP 显示单字母 "f" 就是这来的）。
    {
        const SportStatus_Info_t &g = Model.sportStatusInfo;
        bool bad = false;
        if (!std::isfinite(g.speed_kph) || g.speed_kph < 0.0f || g.speed_kph > 200.0f) bad = true;
        if (!std::isfinite(g.speed_avg_kph) || g.speed_avg_kph < 0.0f || g.speed_avg_kph > 200.0f) bad = true;
        if (!std::isfinite(g.single_distance) || g.single_distance < 0.0f || g.single_distance > 10000.0f) bad = true;
        if (!std::isfinite(g.single_calorie) || g.single_calorie < 0.0f || g.single_calorie > 1e6f) bad = true;
        if (g.heart_rate_bpm > 250) bad = true;
        if (bad) {
            static uint32_t s_bad_frame_log = 0;
            if ((s_bad_frame_log++ % 20) == 0) {
                ESP_LOGW(TAG, "skip bad frame: spd=%.1f avg=%.1f dist=%.2f cal=%.1f hr=%u",
                         g.speed_kph, g.speed_avg_kph, g.single_distance,
                         g.single_calorie, (unsigned)g.heart_rate_bpm);
            }
            return;
        }
    }

    // 骑行界面数据日志（每秒一行）。
    // 便于排查"时间跳跃/速度抖动/距离不涨"这类现象：
    //   - single_time 正常应当是每秒 +1（只在 speed>0 或 GPS 信号中断时累加）；
    //   - 若短时间内猛增，说明 dp_sport 里的 tick/秒单位换算又出错。
    {
        const SportStatus_Info_t &s = Model.sportStatusInfo;
        uint32_t t_sec = (uint32_t)s.single_time;
        uint32_t hh = t_sec / 3600;
        uint32_t mm = (t_sec / 60) % 60;
        uint32_t ss = t_sec % 60;
        ESP_LOGI(TAG,
                 "ride: t=%02lu:%02lu:%02lu(%lus) spd=%.1f avg=%.1f dist=%.2fkm "
                 "cal=%.1fkcal hr=%u cad=%.0f alt=%.1fm",
                 (unsigned long)hh, (unsigned long)mm, (unsigned long)ss, (unsigned long)t_sec,
                 s.speed_kph, s.speed_avg_kph, s.single_distance,
                 s.single_calorie,
                 (unsigned)s.heart_rate_bpm, s.cadence_rpm,
                 s.altitude_m);
    }

    lv_label_set_text_fmt(View.ui.topInfo.labelSpeed, "%02d", (int)Model.GetSpeed());

    // 顶部AVG速度：确保有数据时才显示，避免显示"f"
    float avgSpeed = Model.GetAvgSpeed();
    if (avgSpeed >= 0.0f && avgSpeed <= 200.0f) {
        lv_label_set_text_fmt(View.ui.topInfo.avgInfo.lableValue, "%.1f", avgSpeed);
    } else {
        lv_label_set_text(View.ui.topInfo.avgInfo.lableValue, "--");
    }
    

    
    // 顶部Trip距离：single_distance单位已经是km，不需要除以1000
    float tripDistance = Model.sportStatusInfo.single_distance;
    if (tripDistance >= 0.0f && tripDistance <= 10000.0f) {
        lv_label_set_text_fmt(
            View.ui.topInfo.tripInfo.lableValue,
            "%.1f",
            tripDistance);
    } else {
        lv_label_set_text(View.ui.topInfo.tripInfo.lableValue, "--");
    }
    
    // 使用 Model 中的 MakeTimeString 方法（静态方法）
    char timeBuf[16];
    DialplateModel::MakeTimeString(Model.sportStatusInfo.single_time, timeBuf, sizeof(timeBuf));
    lv_label_set_text(View.ui.bottomInfo.labelInfoGrp[1].lableValue, timeBuf);
    
    // 底部Altitude和Temperature：从IMU获取数据
    HAL::IMU_Info_t imuInfo;
    if (Model.GetIMUInfo(&imuInfo)) {
        // Altitude：从IMU获取height字段
        float height = imuInfo.height;
        if (View.ui.bottomInfo.labelInfoGrp[0].lableValue == nullptr) {
            ESP_LOGE(TAG, "Altitude label is nullptr!");
        } else {
            if (height >= -500.0f && height <= 9000.0f) {  // 合理范围：-500m到9000m
                char height_str[16];
                snprintf(height_str, sizeof(height_str), "%.0f", height);
                lv_label_set_text(View.ui.bottomInfo.labelInfoGrp[0].lableValue, height_str);
                lv_obj_invalidate(View.ui.bottomInfo.labelInfoGrp[0].lableValue);  // 强制刷新
            } else {
                ESP_LOGW(TAG, "IMU height out of range: %.2f m", height);
                lv_label_set_text(View.ui.bottomInfo.labelInfoGrp[0].lableValue, "--");
            }
        }
        
        // Temperature：从IMU获取temperature字段
        float temperature = imuInfo.temperature;
        if (View.ui.bottomInfo.labelInfoGrp[2].lableValue == nullptr) {
            ESP_LOGE(TAG, "Temperature label is nullptr!");
        } else {
            if (temperature >= -40.0f && temperature <= 85.0f) {  // 合理范围：-40°C到85°C
                char temp_str[16];
                snprintf(temp_str, sizeof(temp_str), "%.1f", temperature);
                lv_label_set_text(View.ui.bottomInfo.labelInfoGrp[2].lableValue, temp_str);
                lv_obj_invalidate(View.ui.bottomInfo.labelInfoGrp[2].lableValue);  // 强制刷新
            } else {
                ESP_LOGW(TAG, "IMU temperature out of range: %.2f C", temperature);
                lv_label_set_text(View.ui.bottomInfo.labelInfoGrp[2].lableValue, "--");
            }
        }
    } else {
        // IMU数据获取失败，显示"--"
        ESP_LOGW(TAG, "Failed to get IMU info");
        if (View.ui.bottomInfo.labelInfoGrp[0].lableValue) {
            lv_label_set_text(View.ui.bottomInfo.labelInfoGrp[0].lableValue, "--");
        }
        if (View.ui.bottomInfo.labelInfoGrp[2].lableValue) {
            lv_label_set_text(View.ui.bottomInfo.labelInfoGrp[2].lableValue, "--");
        }
    }
    
    // 卡路里：确保数据有效
    float calorie = Model.sportStatusInfo.single_calorie;
    if (calorie >= 0.0f && calorie <= 100000.0f) {
        lv_label_set_text_fmt(
            View.ui.bottomInfo.labelInfoGrp[3].lableValue,
            "%d k",
            int(calorie));
    } else {
        lv_label_set_text(View.ui.bottomInfo.labelInfoGrp[3].lableValue, "-- k");
    }
    
    // 心率和踏频显示（根据设备连接状态）
    // 心率：设备连接时显示数值，未连接时显示 "--"
    if (Model.sportStatusInfo.heart_rate_bpm > 0) {
        lv_label_set_text_fmt(
            View.ui.bottomInfo.labelInfoGrp[4].lableValue,
            "%d",
            Model.sportStatusInfo.heart_rate_bpm);
    } else {
        lv_label_set_text(View.ui.bottomInfo.labelInfoGrp[4].lableValue, "--");
    }
    lv_obj_clear_flag(View.ui.bottomInfo.labelInfoGrp[4].cont, LV_OBJ_FLAG_HIDDEN);
    
    // 踏频：设备连接时显示数值，未连接时显示 "--"
    float cadence = Model.sportStatusInfo.cadence_rpm;
    if (cadence > 0.0f && cadence <= 200.0f) {
        lv_label_set_text_fmt(
            View.ui.bottomInfo.labelInfoGrp[5].lableValue,
            "%.0f",
            cadence);
    } else {
        lv_label_set_text(View.ui.bottomInfo.labelInfoGrp[5].lableValue, "--");
    }
    lv_obj_clear_flag(View.ui.bottomInfo.labelInfoGrp[5].cont, LV_OBJ_FLAG_HIDDEN);
}

void Dialplate::onTimerUpdate(lv_timer_t *timer)
{
    Dialplate *instance = (Dialplate *)timer->user_data;
    if (instance) {
        instance->Update();
    }
}

void Dialplate::onBtnClicked(lv_obj_t *btn)
{
    if (btn == View.ui.btnCont.btnMap)
    {
        if (lv_obj_has_state(btn, LV_STATE_FOCUSED))
        {
            // 导航到地图页面
            _Manager->Push("LiveMap");
            ESP_LOGI(TAG, "Map button clicked, navigating to LiveMap");
        }
    }
    else if (btn == View.ui.btnCont.btnMenu)
    {
        _Manager->Push("SystemInfos");
    }
    // btnRec 按钮的事件在 onEvent 中单独处理（支持长按和短按）
}

void Dialplate::onRecord(bool longPress)
{
    switch (recState)
    {
    case RECORD_STATE_READY:
        if (longPress)
        {
            if (!Model.GetGPSReady())
            {
                ESP_LOGW(TAG, "GPS has not ready, can't start record");
                return;
            }

            Model.TrackFilterCommand(Model.REC_START);
            SetBtnRecImgSrc("pause");
            recState = RECORD_STATE_RUN;
            ESP_LOGI(TAG, "Recording started");
        }
        break;
    case RECORD_STATE_RUN:
        if (!longPress)
        {
            Model.TrackFilterCommand(Model.REC_PAUSE);
            SetBtnRecImgSrc("start");
            recState = RECORD_STATE_PAUSE;
            ESP_LOGI(TAG, "Recording paused");
        }
        break;
    case RECORD_STATE_PAUSE:
        if (longPress)
        {
            SetBtnRecImgSrc("stop");
            Model.TrackFilterCommand(Model.REC_READY_STOP);
            recState = RECORD_STATE_STOP;
            ESP_LOGI(TAG, "Recording ready to stop");
        }
        else
        {
            Model.TrackFilterCommand(Model.REC_CONTINUE);
            SetBtnRecImgSrc("pause");
            recState = RECORD_STATE_RUN;
            ESP_LOGI(TAG, "Recording continued");
        }
        break;
    case RECORD_STATE_STOP:
        if (longPress)
        {
            Model.TrackFilterCommand(Model.REC_STOP);
            SetBtnRecImgSrc("start");
            recState = RECORD_STATE_READY;
            ESP_LOGI(TAG, "Recording stopped");
        }
        else
        {
            Model.TrackFilterCommand(Model.REC_CONTINUE);
            SetBtnRecImgSrc("pause");
            recState = RECORD_STATE_RUN;
            ESP_LOGI(TAG, "Recording continued from stop state");
        }
        break;
    default:
        break;
    }
}

void Dialplate::SetBtnRecImgSrc(const char *srcName)
{
    const void* img = ResourcePool::GetImage(srcName);
    if (img == nullptr) {
        ESP_LOGW(TAG, "Image resource '%s' not found, using 'start' as fallback", srcName);
        img = ResourcePool::GetImage("start");
    }
    lv_obj_set_style_bg_img_src(View.ui.btnCont.btnRec, img, 0);
}

void Dialplate::onEvent(lv_event_t *event)
{
    Dialplate *instance = (Dialplate *)lv_event_get_user_data(event);
    if (instance == nullptr) {
        return;
    }

    lv_obj_t *obj = lv_event_get_current_target(event);
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_SHORT_CLICKED)
    {
        // btnRec 按钮的短按事件单独处理
        if (obj == instance->View.ui.btnCont.btnRec)
        {
            instance->onRecord(false);
        }
        else
        {
            instance->onBtnClicked(obj);
        }
    }

    // 处理 btnRec 的长按事件
    if (obj == instance->View.ui.btnCont.btnRec)
    {
        if (code == LV_EVENT_LONG_PRESSED)
        {
            instance->onRecord(true);
        }
    }

    if (code == LV_EVENT_GESTURE)
    {
        lv_indev_wait_release(lv_indev_get_act());

        switch (lv_indev_get_gesture_dir(lv_indev_get_act()))
        {
        case LV_DIR_LEFT:
            instance->_Manager->Pop();
            break;
        case LV_DIR_RIGHT:
            instance->_Manager->Pop();
            break;
        case LV_DIR_TOP:
            break;
        case LV_DIR_BOTTOM:
            break;
        default:
            break;
        }
    }
}
