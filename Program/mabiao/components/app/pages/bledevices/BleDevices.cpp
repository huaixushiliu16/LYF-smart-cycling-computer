#include "BleDevices.h"
#include "PageManager.h"
#include "esp_log.h"

static const char *TAG = "BleDevices";
static void tag_keep(void) { (void)TAG; }

using namespace Page;

BleDevices::BleDevices()
    : cont(nullptr)
    , title_label(nullptr)
    , btn_speed(nullptr)
    , btn_hr(nullptr)
{
    Model.Init();
}

BleDevices::~BleDevices()
{
    Model.Deinit();
}

void BleDevices::onCustomAttrConfig()
{
    // Use no animation type to support gesture return
    SetCustomLoadAnimType(PageManager::LOAD_ANIM_NONE);
}

void BleDevices::onViewLoad()
{
    tag_keep();
    ESP_LOGI(TAG, "onViewLoad - Creating BLE hub interface");

    lv_obj_set_size(_root, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(_root, 0, 0);
    lv_obj_set_style_border_width(_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    CreateUI();

    lv_obj_add_event_cb(lv_scr_act(), onRootEvent, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(_root, onRootEvent, LV_EVENT_ALL, this);
}

void BleDevices::onViewDidLoad()
{
    ESP_LOGI(TAG, "onViewDidLoad");
}

void BleDevices::onViewWillAppear()
{
    ESP_LOGI(TAG, "onViewWillAppear");

    Model.SetStatusBarStyle(0);
    Model.SetStatusBarAppear(true);
}

void BleDevices::onViewDidAppear()
{
    ESP_LOGI(TAG, "onViewDidAppear");
}

void BleDevices::onViewWillDisappear()
{
    ESP_LOGI(TAG, "onViewWillDisappear");
}

void BleDevices::onViewDidDisappear()
{
    ESP_LOGI(TAG, "onViewDidDisappear");
}

void BleDevices::onViewUnload()
{
    ESP_LOGI(TAG, "onViewUnload");

    lv_obj_remove_event_cb(lv_scr_act(), onRootEvent);
    lv_obj_remove_event_cb(_root, onRootEvent);

    if (cont)
    {
        lv_obj_del(cont);
        cont = nullptr;
        title_label = nullptr;
        btn_speed = nullptr;
        btn_hr = nullptr;
    }
}

void BleDevices::onViewDidUnload()
{
}

void BleDevices::CreateUI()
{
    cont = lv_obj_create(_root);
    lv_obj_set_size(cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(cont, 0, 0);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0xF0F0F0), LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont, 0, LV_PART_MAIN);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    CreateTitleBar();
    CreateMenuButtons();
}

void BleDevices::CreateTitleBar()
{
    title_label = lv_label_create(cont);
    lv_label_set_text(title_label, "BLE");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 12, 10);
}

void BleDevices::CreateMenuButtons()
{
    const int side_pad = 16;
    const int top_y = 72;
    const int btn_w = LV_HOR_RES - side_pad * 2;
    const int btn_h = 84;
    const int gap = 16;

    btn_speed = lv_btn_create(cont);
    lv_obj_set_size(btn_speed, btn_w, btn_h);
    lv_obj_align(btn_speed, LV_ALIGN_TOP_MID, 0, top_y);
    lv_obj_set_style_radius(btn_speed, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_speed, lv_color_hex(0x2E7D32), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_speed, onEvent, LV_EVENT_CLICKED, this);

    lv_obj_t* speed_label = lv_label_create(btn_speed);
    lv_label_set_text(speed_label, "Speed / Cadence");
    lv_obj_set_style_text_color(speed_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(speed_label);

    btn_hr = lv_btn_create(cont);
    lv_obj_set_size(btn_hr, btn_w, btn_h);
    lv_obj_align(btn_hr, LV_ALIGN_TOP_MID, 0, top_y + btn_h + gap);
    lv_obj_set_style_radius(btn_hr, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_hr, lv_color_hex(0x1565C0), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_hr, onEvent, LV_EVENT_CLICKED, this);

    lv_obj_t* hr_label = lv_label_create(btn_hr);
    lv_label_set_text(hr_label, "Heart Rate");
    lv_obj_set_style_text_color(hr_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(hr_label);
}

void BleDevices::onEvent(lv_event_t *event)
{
    BleDevices *instance = (BleDevices *)lv_event_get_user_data(event);
    if (instance == nullptr) {
        return;
    }

    lv_obj_t *target = lv_event_get_target(event);

    if (target == instance->btn_speed) {
        instance->_Manager->Push("SpeedSensor");
        return;
    }
    if (target == instance->btn_hr) {
        instance->_Manager->Push("HeartRate");
        return;
    }
}

void BleDevices::onRootEvent(lv_event_t *event)
{
    BleDevices *instance = (BleDevices *)lv_event_get_user_data(event);
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

