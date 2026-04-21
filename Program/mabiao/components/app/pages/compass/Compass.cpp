#include "Compass.h"
#include "PageManager.h"
#include "esp_log.h"

static const char *TAG = "Compass";

using namespace Page;

Compass::Compass()
    : timer(nullptr)
    , angleLast(0)
{
}

Compass::~Compass()
{
}

void Compass::onCustomAttrConfig()
{
    SetCustomCacheEnable(false);
    SetCustomLoadAnimType(PageManager::LOAD_ANIM_NONE);
}

void Compass::onViewLoad()
{
    ESP_LOGI(TAG, "onViewLoad");
    Model.Init();
    View.Create(_root);

    AttachEvent(lv_scr_act());
    if (View.ui.btnCont.btn) {
        AttachEvent(View.ui.btnCont.btn);
    }
    
    // Enable gesture for back navigation (same as Dialplate)
    lv_obj_add_flag(_root, LV_OBJ_FLAG_GESTURE_BUBBLE);
}

void Compass::onViewDidLoad()
{
    ESP_LOGI(TAG, "onViewDidLoad");
}

void Compass::onViewWillAppear()
{
    ESP_LOGI(TAG, "onViewWillAppear");

    lv_group_t *group = lv_group_get_default();
    if (!group) {
        ESP_LOGW(TAG, "lv_group_get_default() returned NULL");
    }

    Model.SetStatusBarStyle(STATUS_BAR_STYLE_TRANSP);

    // 初始化角度，避免第一次更新时出现问题
    angleLast = 0;
    
    // 先更新一次UI，但不强制更新数据（避免阻塞）
    // 实际数据更新由定时器处理
    // 注意：Update() 应该快速返回，不应该阻塞
    Update();

    lv_obj_fade_in(_root, 350, 0);

    View.AppearAnimStart();
    
    ESP_LOGI(TAG, "onViewWillAppear completed");
}

void Compass::onViewDidAppear()
{
    ESP_LOGI(TAG, "onViewDidAppear");
    timer = lv_timer_create(onTimerUpdate, 100, this);
}

void Compass::onViewWillDisappear()
{
    ESP_LOGI(TAG, "onViewWillDisappear");
    lv_group_t *group = lv_group_get_default();
    if (group) {
        lv_group_remove_all_objs(group);
    }

    if (timer) {
        lv_timer_del(timer);
        timer = nullptr;
    }
}

void Compass::onViewDidDisappear()
{
    ESP_LOGI(TAG, "onViewDidDisappear");
}

void Compass::onViewUnload()
{
    ESP_LOGI(TAG, "onViewUnload");

    View.Delete();
    Model.Deinit();

    lv_obj_remove_event_cb(lv_scr_act(), onEvent);
}

void Compass::onViewDidUnload()
{
    ESP_LOGI(TAG, "onViewDidUnload");
}

void Compass::AttachEvent(lv_obj_t *obj)
{
    lv_obj_add_event_cb(obj, onEvent, LV_EVENT_ALL, this);
}

void Compass::Update()
{
    // 快速检查，避免阻塞
    if (!View.ui.mainCont.img && !View.ui.mainCont.label[label_angle]) {
        // UI 还未完全创建，跳过更新
        return;
    }
    
    int angle = 0;
    int x = 0;
    int y = 0;
    int z = 0;
    bool isCalibrated = false;

    Model.GetMAGInfo(&angle, &x, &y, &z, &isCalibrated);

    // 如果数据有效，更新UI
    if (isCalibrated && View.ui.mainCont.img)
    {
        if (View.ui.btnCont.cont) {
            lv_obj_move_background(View.ui.btnCont.cont);
        }
        
        // 角度平滑过渡算法
        if (angleLast < angle)
        {
            int16_t tmp = angle - angleLast;

            if (tmp < 180)
            {
                int16_t cnt = tmp / 10;
                if (tmp == 0)
                    tmp++;
                angleLast += cnt;
            }
            else
            {
                tmp = 180 + 179 - angle + angleLast;
                int16_t cnt = tmp / 10;
                if (tmp == 0)
                    tmp++;
                angleLast -= cnt;
            }
        }
        else
        {
            int16_t tmp = angleLast - angle;
            if (tmp < 180)
            {
                int16_t cnt = tmp / 10;
                angleLast -= cnt;
            }
            else
            {
                tmp = 180 + 179 + angle - angleLast;
                int16_t cnt = tmp / 10;
                if (tmp == 0)
                    tmp++;
                angleLast += cnt;
            }
        }

        // 角度范围限制
        if (angleLast > 180)
            angleLast = -179 + (angleLast - 180);
        if (angleLast < -179)
            angleLast = 180 - (-179 - angleLast);

        // 设置图片旋转角度（十分之一度）
        if (View.ui.mainCont.img) {
            lv_img_set_angle(View.ui.mainCont.img, angleLast * 10);
        }

        // 根据角度确定方向标签索引
        // 将角度转换为0-360范围
        int16_t angle_normalized = angleLast;
        if (angle_normalized < 0) {
            angle_normalized += 360;
        }
        
        // 根据角度范围确定方向（8个方向，每个45度）
        uint8_t tmp = 0;
        if (angle_normalized >= 337 || angle_normalized < 22) {
            tmp = 3;  // 北
        } else if (angle_normalized >= 22 && angle_normalized < 67) {
            tmp = 2;  // 东北
        } else if (angle_normalized >= 67 && angle_normalized < 112) {
            tmp = 1;  // 东
        } else if (angle_normalized >= 112 && angle_normalized < 157) {
            tmp = 0;  // 东南
        } else if (angle_normalized >= 157 && angle_normalized < 202) {
            tmp = 7;  // 南
        } else if (angle_normalized >= 202 && angle_normalized < 247) {
            tmp = 6;  // 西南
        } else if (angle_normalized >= 247 && angle_normalized < 292) {
            tmp = 5;  // 西
        } else {  // 292-337
            tmp = 4;  // 西北
        }

        // 计算显示角度（0-360度）
        int16_t angle_t = angleLast;
        if (angle_t < 0)
            angle_t += 360;
        angle_t = -angle_t + 360;

        // 更新方向标签
        if (View.ui.mainCont.label[label_angle]) {
            lv_label_set_text_fmt(View.ui.mainCont.label[label_angle], rotation_ch[tmp], angle_t);
        }
    } else {
        // 数据无效时，显示默认状态，避免界面无响应
        if (View.ui.mainCont.label[label_angle]) {
            lv_label_set_text(View.ui.mainCont.label[label_angle], "等待数据...");
        }
        // 确保按钮容器显示（用于校准提示）
        if (View.ui.btnCont.cont) {
            lv_obj_move_foreground(View.ui.btnCont.cont);
        }
    }
}

void Compass::onTimerUpdate(lv_timer_t *timer)
{
    Compass *instance = (Compass *)timer->user_data;
    if (instance) {
        instance->Update();
    }
}

void Compass::onEvent(lv_event_t *event)
{
    Compass *instance = (Compass *)lv_event_get_user_data(event);
    LV_ASSERT_NULL(instance);

    lv_obj_t *obj = lv_event_get_current_target(event);
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_SHORT_CLICKED)
    {
        if (instance->View.ui.btnCont.btn && obj == instance->View.ui.btnCont.btn)
        {
            // 暂时跳过校准功能
            ESP_LOGW(TAG, "Calibration button clicked but not implemented");
            // lv_label_set_text_fmt(instance->View.ui.btnCont.label, "%s", "开始校准");
            // instance->View.AppearAnimClick();
            // instance->Model.SetMAGCalibration();
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
