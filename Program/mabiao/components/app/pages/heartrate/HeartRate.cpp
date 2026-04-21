#include "HeartRate.h"
#include "PageManager.h"
#include "bsp_ble.h"
#include "esp_log.h"

static const char *TAG = "HeartRate";
static void tag_keep(void) { (void)TAG; }

using namespace Page;

static void set_btn_enabled(lv_obj_t* btn, bool en)
{
    if (!btn) return;
    if (en) lv_obj_clear_state(btn, LV_STATE_DISABLED);
    else lv_obj_add_state(btn, LV_STATE_DISABLED);
}

HeartRate::HeartRate()
    : cont(nullptr)
    , title_label(nullptr)
    , scan_btn(nullptr)
    , disconnect_btn(nullptr)
    , device_list(nullptr)
    , weight_value_label(nullptr)
    , weight_minus_btn(nullptr)
    , weight_plus_btn(nullptr)
    , save_btn(nullptr)
    , live_label(nullptr)
    , timer(nullptr)
    , device_item_count(0)
    , weight_kg(70)
    , dirty(false)
{
    memset(device_items, 0, sizeof(device_items));
    Model.Init();
    tag_keep();
    weight_kg = Model.GetWeightKG();
}

HeartRate::~HeartRate()
{
    Model.Deinit();
}

void HeartRate::onCustomAttrConfig()
{
    SetCustomLoadAnimType(PageManager::LOAD_ANIM_NONE);
}

void HeartRate::onViewLoad()
{
    lv_obj_set_size(_root, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(_root, 0, 0);
    lv_obj_set_style_border_width(_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    CreateUI();

    lv_obj_add_event_cb(lv_scr_act(), onRootEvent, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(_root, onRootEvent, LV_EVENT_ALL, this);
}

void HeartRate::onViewDidLoad()
{
}

void HeartRate::onViewWillAppear()
{
    Model.SetStatusBarStyle(0);
    Model.SetStatusBarAppear(true);
    UpdateUI();
}

void HeartRate::onViewDidAppear()
{
    timer = lv_timer_create(onTimerUpdate, 500, this);
    if (timer) lv_timer_ready(timer);
}

void HeartRate::onViewWillDisappear()
{
    if (timer) {
        lv_timer_del(timer);
        timer = nullptr;
    }
}

void HeartRate::onViewDidDisappear()
{
}

void HeartRate::onViewUnload()
{
    lv_obj_remove_event_cb(lv_scr_act(), onRootEvent);
    lv_obj_remove_event_cb(_root, onRootEvent);
    if (cont) {
        lv_obj_del(cont);
        cont = nullptr;
    }
    memset(device_items, 0, sizeof(device_items));
    device_item_count = 0;
}

void HeartRate::onViewDidUnload()
{
}

void HeartRate::CreateUI()
{
    cont = lv_obj_create(_root);
    lv_obj_set_size(cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0xF0F0F0), LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    CreateTitleBar();
    CreateDeviceList();
    CreateSettings();
    CreateLiveData();
}

void HeartRate::CreateTitleBar()
{
    // Title on the left, Scan button on the right.
    // Disconnect is handled per-row in the device list (button text toggles
    // between "Connect" / "Disconnect") to avoid a crowded 240 px title bar.
    title_label = lv_label_create(cont);
    lv_label_set_text(title_label, "Heart Rate");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 10, 32);

    scan_btn = lv_btn_create(cont);
    lv_obj_set_size(scan_btn, 64, 28);
    lv_obj_align(scan_btn, LV_ALIGN_TOP_RIGHT, -10, 30);
    lv_obj_set_style_radius(scan_btn, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0x0066CC), LV_PART_MAIN);
    lv_obj_add_event_cb(scan_btn, onEvent, LV_EVENT_CLICKED, this);
    lv_obj_t *scan_label = lv_label_create(scan_btn);
    lv_label_set_text(scan_label, "Scan");
    lv_obj_center(scan_label);

    // Kept as a nullptr sentinel so onEvent's equality checks remain safe.
    disconnect_btn = nullptr;
}

void HeartRate::CreateDeviceList()
{
    device_list = lv_obj_create(cont);
    lv_obj_set_size(device_list, LV_HOR_RES - 20, 92);
    lv_obj_align(device_list, LV_ALIGN_TOP_MID, 0, 68);
    lv_obj_set_style_radius(device_list, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(device_list, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_pad_all(device_list, 6, LV_PART_MAIN);
    lv_obj_set_scroll_dir(device_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(device_list, LV_SCROLLBAR_MODE_AUTO);
}

void HeartRate::CreateSettings()
{
    // Settings box: 220 wide, 72 high. pad_all=8 => content 204x56.
    // Row 1 (y=0): single-line "Weight: 70 kg" label.
    // Row 2 (y=26): [-] [+]             [Save].
    lv_obj_t* box = lv_obj_create(cont);
    lv_obj_set_size(box, LV_HOR_RES - 20, 72);
    lv_obj_align(box, LV_ALIGN_TOP_MID, 0, 166);
    lv_obj_set_style_radius(box, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(box, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_pad_all(box, 8, LV_PART_MAIN);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    weight_value_label = lv_label_create(box);
    lv_obj_align(weight_value_label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_font(weight_value_label, &lv_font_montserrat_14, LV_PART_MAIN);

    weight_minus_btn = lv_btn_create(box);
    lv_obj_set_size(weight_minus_btn, 40, 28);
    lv_obj_align(weight_minus_btn, LV_ALIGN_TOP_LEFT, 0, 26);
    lv_obj_add_event_cb(weight_minus_btn, onEvent, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(weight_minus_btn, onEvent, LV_EVENT_LONG_PRESSED_REPEAT, this);
    lv_obj_t* wminus = lv_label_create(weight_minus_btn);
    lv_label_set_text(wminus, "-");
    lv_obj_center(wminus);

    weight_plus_btn = lv_btn_create(box);
    lv_obj_set_size(weight_plus_btn, 40, 28);
    lv_obj_align(weight_plus_btn, LV_ALIGN_TOP_LEFT, 48, 26);
    lv_obj_add_event_cb(weight_plus_btn, onEvent, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(weight_plus_btn, onEvent, LV_EVENT_LONG_PRESSED_REPEAT, this);
    lv_obj_t* wplus = lv_label_create(weight_plus_btn);
    lv_label_set_text(wplus, "+");
    lv_obj_center(wplus);

    save_btn = lv_btn_create(box);
    lv_obj_set_size(save_btn, 60, 28);
    lv_obj_align(save_btn, LV_ALIGN_TOP_RIGHT, 0, 26);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x0066CC), LV_PART_MAIN);
    lv_obj_add_event_cb(save_btn, onEvent, LV_EVENT_CLICKED, this);
    lv_obj_t* s = lv_label_create(save_btn);
    lv_label_set_text(s, "Save");
    lv_obj_center(s);
}

void HeartRate::CreateLiveData()
{
    // Prominent single metric. Placed well below the settings box and just
    // above the FPS/CPU overlay at the bottom-right.
    live_label = lv_label_create(cont);
    lv_obj_set_style_text_font(live_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(live_label, LV_ALIGN_BOTTOM_LEFT, 12, -10);
    lv_label_set_text(live_label, "HR: -- bpm");
}

void HeartRate::UpdateUI()
{
    if (scan_btn) {
        lv_obj_t* lbl = lv_obj_get_child(scan_btn, 0);
        if (lbl) lv_label_set_text(lbl, Model.IsScanning() ? "Stop" : "Scan");
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "Weight: %d kg", weight_kg);
    lv_label_set_text(weight_value_label, buf);

    set_btn_enabled(weight_minus_btn, weight_kg > 30);
    set_btn_enabled(weight_plus_btn, weight_kg < 200);
    set_btn_enabled(save_btn, dirty);

    if (title_label) {
        lv_label_set_text(title_label, dirty ? "Heart Rate *" : "Heart Rate");
    }

    UpdateDeviceList();
}

void HeartRate::UpdateDeviceList()
{
    if (!device_list) return;

    bsp_ble_device_info_t devices[HeartRateModel::MAX_DEVICES];
    int count = 0;
    if (Model.GetDevicesHR(devices, &count, HeartRateModel::MAX_DEVICES) != ESP_OK) {
        return;
    }

    lv_obj_clean(device_list);
    device_item_count = 0;
    for (int i = 0; i < count; i++) {
        lv_obj_t* item = lv_obj_create(device_list);
        lv_obj_set_width(item, LV_PCT(100));
        lv_obj_set_style_radius(item, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(item, 6, LV_PART_MAIN);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* name = lv_label_create(item);
        lv_label_set_text(name, devices[i].name);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, 0, 0);

        char line2[64];
        snprintf(line2, sizeof(line2), "RSSI %d  %s", (int)devices[i].rssi,
                 devices[i].is_connected ? "Connected" : "Available");
        lv_obj_t* meta = lv_label_create(item);
        lv_label_set_text(meta, line2);
        lv_obj_align(meta, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        lv_obj_t* btn = lv_btn_create(item);
        lv_obj_set_size(btn, 82, 28);
        lv_obj_align(btn, LV_ALIGN_RIGHT_MID, -2, 0);
        lv_obj_set_style_bg_color(btn,
            devices[i].is_connected ? lv_color_hex(0x8E2424) : lv_color_hex(0x0066CC),
            LV_PART_MAIN);
        lv_obj_add_event_cb(btn, onEvent, LV_EVENT_CLICKED, this);
        lv_obj_t* bl = lv_label_create(btn);
        lv_label_set_text(bl, devices[i].is_connected ? "Disconnect" : "Connect");
        lv_obj_center(bl);

        device_items[i] = btn;
        device_item_count++;
    }
}

void HeartRate::onTimerUpdate(lv_timer_t *timer)
{
    HeartRate *instance = (HeartRate *)timer->user_data;
    if (!instance) return;

    uint16_t hr = bsp_ble_get_heart_rate();
    char buf[48];
    snprintf(buf, sizeof(buf), "HR: %u bpm", (unsigned)hr);
    lv_label_set_text(instance->live_label, buf);

    instance->UpdateUI();
}

void HeartRate::onEvent(lv_event_t *event)
{
    HeartRate *instance = (HeartRate *)lv_event_get_user_data(event);
    if (!instance) return;

    lv_obj_t *target = lv_event_get_target(event);

    if (target == instance->scan_btn) {
        instance->OnScanClicked();
        return;
    }
    if (target == instance->disconnect_btn) {
        instance->OnDisconnectClicked();
        return;
    }
    if (target == instance->weight_minus_btn) {
        instance->OnWeightDelta((lv_event_get_code(event) == LV_EVENT_LONG_PRESSED_REPEAT) ? -5 : -1);
        return;
    }
    if (target == instance->weight_plus_btn) {
        instance->OnWeightDelta((lv_event_get_code(event) == LV_EVENT_LONG_PRESSED_REPEAT) ? +5 : +1);
        return;
    }
    if (target == instance->save_btn) {
        instance->OnSaveClicked();
        return;
    }

    for (int i = 0; i < instance->device_item_count; i++) {
        if (target == instance->device_items[i]) {
            instance->OnConnectClicked(i);
            return;
        }
    }
}

void HeartRate::OnScanClicked()
{
    if (Model.IsScanning()) {
        Model.StopScan();
    } else {
        Model.StartScan(0);
    }
    UpdateUI();
}

void HeartRate::OnConnectClicked(int index)
{
    // The per-row button acts as a toggle: if the device is already connected
    // it disconnects, otherwise it initiates a new connection.
    bsp_ble_device_info_t devices[HeartRateModel::MAX_DEVICES];
    int count = 0;
    if (Model.GetDevicesHR(devices, &count, HeartRateModel::MAX_DEVICES) != ESP_OK) {
        return;
    }
    if (index < 0 || index >= count) return;

    if (devices[index].is_connected) {
        Model.DisconnectHR();
    } else {
        Model.ConnectDevice(devices[index].addr, devices[index].addr_type);
    }
}

void HeartRate::OnDisconnectClicked()
{
    Model.DisconnectHR();
}

void HeartRate::OnWeightDelta(int delta_kg)
{
    int v = weight_kg + delta_kg;
    if (v < 30) v = 30;
    if (v > 200) v = 200;
    weight_kg = v;
    Model.SetWeightKG(weight_kg);
    dirty = true;
    UpdateUI();
}

void HeartRate::OnSaveClicked()
{
    Model.SaveWeightToSysConfig();
    dirty = false;
    UpdateUI();
}

void HeartRate::onRootEvent(lv_event_t *event)
{
    HeartRate *instance = (HeartRate *)lv_event_get_user_data(event);
    if (!instance) return;

    if (lv_event_get_code(event) == LV_EVENT_GESTURE)
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

