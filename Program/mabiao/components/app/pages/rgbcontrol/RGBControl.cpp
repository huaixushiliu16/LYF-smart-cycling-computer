#include "RGBControl.h"
#include "PageManager.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "RGBControl";

namespace Page
{

RGBControl::RGBControl()
    : timer(nullptr)
{
    memset(&current_rgb_info, 0, sizeof(current_rgb_info));
}

RGBControl::~RGBControl()
{
}

void RGBControl::onCustomAttrConfig()
{
    SetCustomCacheEnable(false);
    // 禁用 PageManager Root drag（它会导致页面出现上下拖拽/滑动感）
    // 页面返回改用 LV_EVENT_GESTURE 的左右滑动（见 onRootEvent）
    SetCustomLoadAnimType(PageManager::LOAD_ANIM_NONE);
}

void RGBControl::onViewLoad()
{
    ESP_LOGI(TAG, "onViewLoad");
    
    Model.Init();
    
    // 固定为不滚动页面 + 黑白界面
    lv_obj_set_size(_root, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(_root, 0, 0);
    lv_obj_set_style_border_width(_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(_root, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(_root, LV_DIR_NONE);

    View.Create(_root);
    AttachEvent(_root);

    // 左右滑动返回首页（与 BLE 页面一致）
    lv_obj_add_event_cb(lv_scr_act(), onRootEvent, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(_root, onRootEvent, LV_EVENT_ALL, this);
}

void RGBControl::onViewDidLoad()
{
    ESP_LOGI(TAG, "onViewDidLoad");
}

void RGBControl::onViewWillAppear()
{
    ESP_LOGI(TAG, "onViewWillAppear");
    
    Model.SetStatusBarStyle((StatusBar_Style_t)0);
    
    // 获取当前RGB状态
    Model.GetRGBInfo(&current_rgb_info);

    // 进入页面固定为心率同步模式 + 固定亮度（不在 UI 暴露）
    current_rgb_info.mode = RGB_MODE_HEART_RATE_SYNC;
    current_rgb_info.brightness = 80;
    Model.SetMode(current_rgb_info.mode);
    Model.SetBrightness(current_rgb_info.brightness);
    
    // 更新UI
    Update();
    
    // 创建定时器（100ms更新一次，用于实时预览）
    timer = lv_timer_create(onTimerUpdate, 100, this);

    View.AppearAnimStart();
}

void RGBControl::onViewDidAppear()
{
    ESP_LOGI(TAG, "onViewDidAppear");
}

void RGBControl::onViewWillDisappear()
{
    ESP_LOGI(TAG, "onViewWillDisappear");
}

void RGBControl::onViewDidDisappear()
{
    ESP_LOGI(TAG, "onViewDidDisappear");
}

void RGBControl::onViewUnload()
{
    ESP_LOGI(TAG, "onViewUnload");
    
    if (timer) {
        lv_timer_del(timer);
        timer = nullptr;
    }

    lv_obj_remove_event_cb(lv_scr_act(), onRootEvent);
    lv_obj_remove_event_cb(_root, onRootEvent);
    
    View.Delete();
    Model.Deinit();
}

void RGBControl::onViewDidUnload()
{
    ESP_LOGI(TAG, "onViewDidUnload");
}

void RGBControl::Update()
{
    // 心率决定：是否允许操作开关 + 状态文本
    uint16_t hr = 0;
    Model.GetHeartRate(&hr);
    bool hr_valid = (hr >= 30 && hr <= 220);

    if (View.ui.sw) {
        if (hr_valid) {
            lv_obj_clear_state(View.ui.sw, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(View.ui.sw, LV_STATE_DISABLED);
            // 未连接/无效心率时，强制关闭，避免“开着但没反应”
            if (current_rgb_info.enabled) {
                current_rgb_info.enabled = false;
                if (lv_obj_has_state(View.ui.sw, LV_STATE_CHECKED)) {
                    lv_obj_clear_state(View.ui.sw, LV_STATE_CHECKED);
                }
                Model.SetEnabled(false);
            } else {
                if (lv_obj_has_state(View.ui.sw, LV_STATE_CHECKED)) {
                    lv_obj_clear_state(View.ui.sw, LV_STATE_CHECKED);
                }
            }
        }

        if (current_rgb_info.enabled) {
            lv_obj_add_state(View.ui.sw, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(View.ui.sw, LV_STATE_CHECKED);
        }
    }

    if (View.ui.label_status) {
        if (hr_valid) {
            char buf[32];
            snprintf(buf, sizeof(buf), "HR: %u bpm", (unsigned)hr);
            lv_label_set_text(View.ui.label_status, buf);
        } else {
            lv_label_set_text(View.ui.label_status, "Connect HR sensor first");
        }
    }
}

void RGBControl::AttachEvent(lv_obj_t *obj)
{
    (void)obj;
    if (View.ui.sw) {
        lv_obj_add_event_cb(View.ui.sw, onEvent, LV_EVENT_VALUE_CHANGED, this);
    }
}

void RGBControl::onTimerUpdate(lv_timer_t *timer)
{
    RGBControl *instance = (RGBControl *)timer->user_data;
    if (instance) {
        instance->Update();
    }
}

void RGBControl::onEvent(lv_event_t *event)
{
    RGBControl *instance = (RGBControl *)lv_event_get_user_data(event);
    if (!instance) {
        return;
    }
    
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *obj = lv_event_get_target(event);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        if (obj == instance->View.ui.sw) {
            instance->OnSwitchToggled();
        }
    }
}

void RGBControl::OnSwitchToggled()
{
    if (!View.ui.sw) {
        return;
    }
    bool checked = lv_obj_has_state(View.ui.sw, LV_STATE_CHECKED);
    current_rgb_info.enabled = checked;
    Model.SetEnabled(checked);
}

void RGBControl::onRootEvent(lv_event_t *event)
{
    RGBControl *instance = (RGBControl *)lv_event_get_user_data(event);
    if (instance == nullptr) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_GESTURE)
    {
        lv_indev_wait_release(lv_indev_get_act());
        switch (lv_indev_get_gesture_dir(lv_indev_get_act()))
        {
        case LV_DIR_LEFT:
        case LV_DIR_RIGHT:
            instance->_Manager->Pop();
            break;
        default:
            break;
        }
    }
}

}
