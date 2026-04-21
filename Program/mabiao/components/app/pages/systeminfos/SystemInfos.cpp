#include "SystemInfos.h"
#include "PageManager.h"
#include "ResourcePool.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "SystemInfos";

using namespace Page;

// 双击检测静态变量
lv_obj_t* SystemInfos::s_last_clicked_obj = nullptr;
uint32_t SystemInfos::s_last_click_time = 0;

SystemInfos::SystemInfos()
    : timer(nullptr)
{
}

SystemInfos::~SystemInfos()
{
}

void SystemInfos::onCustomAttrConfig()
{
    ESP_LOGI(TAG, "onCustomAttrConfig");
    // 使用无动画类型，支持手势返回
    SetCustomLoadAnimType(PageManager::LOAD_ANIM_NONE);
}

void SystemInfos::onViewLoad()
{
    ESP_LOGI(TAG, "onViewLoad");
    
    // 确保屏幕背景色是白色
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    }

    ESP_LOGI(TAG, "Initializing Model...");
    Model.Init();
    ESP_LOGI(TAG, "Model initialized");

    ESP_LOGI(TAG, "Creating View...");
    View.Create(_root);
    ESP_LOGI(TAG, "View created");

    ESP_LOGI(TAG, "Attaching events...");
    // 为root和屏幕附加手势事件（用于手势返回）
    lv_obj_add_event_cb(lv_scr_act(), onRootEvent, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(_root, onRootEvent, LV_EVENT_ALL, this);
    
    // 为每个图标添加双击事件处理
    SystemInfosView::item_t *item_grp = ((SystemInfosView::item_t *)&View.ui);
    int item_count = sizeof(View.ui) / sizeof(SystemInfosView::item_t);
    ESP_LOGI(TAG, "Item count: %d", item_count);
    for (int i = 0; i < item_count; i++)
    {
        if (item_grp[i].icon != nullptr) {
            AttachEvent(item_grp[i].icon);
        }
    }
    
    // 初始化所有数据为默认值（"--"），确保界面能立即显示
    // 这样即使数据未准备好，界面也能正常显示
    ESP_LOGI(TAG, "Initializing default values...");
    View.SetSport(-1.0f, "--", -1.0f);
    View.SetGPS(-999.0f, -999.0f, -999.0f, "--", -1.0f, -1.0f);
    View.SetMAG(-1.0f, -999, -999, -999);
    View.SetIMU(-1, "--\n--\n--\n--\n--\n--");
    View.SetBattery(-1, -1.0f, "--");
    View.SetStorage("ERROR", "--", "--", "");
    char firmVer[16] = "V1.0";
    char authorName[32] = "槐序十六";
    char lvglVer[16] = "8.3";
    View.SetSystem(firmVer, authorName, lvglVer);
    ESP_LOGI(TAG, "Default values initialized");
    
    ESP_LOGI(TAG, "onViewLoad completed");
}

void SystemInfos::onViewDidLoad()
{
    ESP_LOGI(TAG, "onViewDidLoad");
}

void SystemInfos::onViewWillAppear()
{
    ESP_LOGI(TAG, "onViewWillAppear");

    Model.SetStatusBarStyle(0);  // 简化实现
    Model.SetStatusBarAppear(true);
    
    // 先更新一次数据，然后再启动定时器
    // 即使数据未准备好，也要确保界面能显示
    Update();
    
    timer = lv_timer_create(onTimerUpdate, 1000, this);
    if (timer) {
        lv_timer_ready(timer);
        ESP_LOGI(TAG, "Timer created and started");
    } else {
        ESP_LOGE(TAG, "Failed to create timer");
    }

    // 确保界面可见，即使数据未准备好
    // 先设置为不透明，确保界面可见（即使数据未准备好）
    lv_obj_set_style_opa(_root, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    // 确保滚动位置在顶部，让内容可见
    lv_obj_scroll_to_y(_root, 0, LV_ANIM_OFF);
    // 强制刷新布局，确保内容正确显示
    lv_obj_update_layout(_root);
    // 然后再淡入（如果数据准备好了）
    lv_obj_fade_in(_root, 250, 0);
    
    ESP_LOGI(TAG, "onViewWillAppear completed");
}

void SystemInfos::onViewDidAppear()
{
    ESP_LOGI(TAG, "onViewDidAppear");
    lv_group_t *group = lv_group_get_default();
    if (group) {
        View.onFocus(group);
    }
}

void SystemInfos::onViewWillDisappear()
{
    ESP_LOGI(TAG, "onViewWillDisappear");
    lv_group_t *group = lv_group_get_default();
    if (group) {
        lv_group_focus_obj(nullptr);
        lv_group_remove_all_objs(group);
        lv_group_set_focus_cb(group, nullptr);
    }
    lv_obj_fade_out(_root, 250, 0);
}

void SystemInfos::onViewDidDisappear()
{
    ESP_LOGI(TAG, "onViewDidDisappear");

    if (timer)
    {
        lv_timer_del(timer);
        timer = nullptr;
    }
}

void SystemInfos::onViewUnload()
{
    ESP_LOGI(TAG, "onViewUnload");

    // 移除事件回调
    lv_obj_remove_event_cb(lv_scr_act(), onRootEvent);
    lv_obj_remove_event_cb(_root, onRootEvent);

    View.Delete();
    Model.Deinit();
}

void SystemInfos::onViewDidUnload()
{
    ESP_LOGI(TAG, "onViewDidUnload");
}

void SystemInfos::AttachEvent(lv_obj_t *obj)
{
    // 监听点击事件，用于双击检测
    lv_obj_add_event_cb(obj, onEvent, LV_EVENT_CLICKED, this);
}

void SystemInfos::Update()
{
    ESP_LOGD(TAG, "Update() called");
    
    char buf[64] = {0};

    // 使用 try-catch 风格的错误处理，确保即使某个数据获取失败也能继续
    // Sport
    float trip = 0;
    float maxSpd = 0;
    Model.GetSportInfo(&trip, buf, sizeof(buf), &maxSpd);
    View.SetSport(trip, buf, maxSpd);

    // GPS
    float lat = 0;
    float lng = 0;
    float alt = 0;
    float course = 0;
    float speed = 0;
    Model.GetGPSInfo(&lat, &lng, &alt, buf, sizeof(buf), &course, &speed);
    View.SetGPS(lat, lng, alt, buf, course, speed);

    // MAG
    float dir = 0;
    int x = 0;
    int y = 0;
    int z = 0;
    Model.GetMAGInfo(&dir, &x, &y, &z);
    View.SetMAG(dir, x, y, z);

    // IMU
    int steps = 0;
    Model.GetIMUInfo(&steps, buf, sizeof(buf));
    View.SetIMU(steps, buf);

    // Power
    int usage = 0;
    float voltage = 0;
    Model.GetBatteryInfo(&usage, &voltage, buf, sizeof(buf));
    View.SetBattery(usage, voltage, buf);

    // Storage
    bool detect = false;
    const char *type = "-";
    Model.GetStorageInfo(&detect, &type, buf, sizeof(buf));
    View.SetStorage(
        detect ? "OK" : "ERROR",
        buf,
        type,
        "");  // version参数保留但不使用

    // System
    char firmVer[16], authorName[32], lvglVer[16];
    Model.GetSystemInfo(firmVer, sizeof(firmVer), 
                       authorName, sizeof(authorName),
                       lvglVer, sizeof(lvglVer));
    View.SetSystem(firmVer, authorName, lvglVer);
    
    ESP_LOGD(TAG, "Update() completed");
}

void SystemInfos::onTimerUpdate(lv_timer_t *timer)
{
    SystemInfos *instance = (SystemInfos *)timer->user_data;
    if (instance) {
        instance->Update();
    }
}

void SystemInfos::onEvent(lv_event_t *event)
{
    SystemInfos *instance = (SystemInfos *)lv_event_get_user_data(event);
    lv_obj_t *obj = lv_event_get_current_target(event);
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_CLICKED)
    {
        uint32_t current_time = esp_timer_get_time() / 1000;  // 转换为毫秒
        
        // 检查是否是双击（同一对象，且在时间间隔内）
        if (s_last_clicked_obj == obj && 
            (current_time - s_last_click_time) < DOUBLE_CLICK_TIME_MS)
        {
            // 双击：切换焦点
            ESP_LOGI(TAG, "Double click detected on icon");
            lv_group_t *group = lv_group_get_default();
            if (group) {
                lv_obj_t *focused = lv_group_get_focused(group);
                if (focused == obj) {
                    // 当前已获得焦点，移除焦点（恢复图标区域宽度）
                    lv_group_focus_obj(nullptr);
                } else {
                    // 未获得焦点，设置焦点（缩小图标区域显示右侧信息）
                    lv_group_focus_obj(obj);
                }
            }
            
            // 重置双击检测
            s_last_clicked_obj = nullptr;
            s_last_click_time = 0;
        }
        else
        {
            // 第一次点击，记录时间和对象
            s_last_clicked_obj = obj;
            s_last_click_time = current_time;
        }
    }
}

void SystemInfos::onRootEvent(lv_event_t *event)
{
    SystemInfos *instance = (SystemInfos *)lv_event_get_user_data(event);
    if (instance == nullptr) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(event);

    // 处理手势返回
    if (code == LV_EVENT_GESTURE)
    {
        lv_indev_wait_release(lv_indev_get_act());

        switch (lv_indev_get_gesture_dir(lv_indev_get_act()))
        {
        case LV_DIR_LEFT:
            instance->_Manager->Pop();
            ESP_LOGI(TAG, "Left gesture detected, returning to previous page");
            break;
        case LV_DIR_RIGHT:
            instance->_Manager->Pop();
            ESP_LOGI(TAG, "Right gesture detected, returning to previous page");
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
