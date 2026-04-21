#include "WaveTable.h"
#include "PageManager.h"
#include "ResourcePool.h"
#include "esp_log.h"

static const char *TAG = "WaveTable";

using namespace Page;

WaveTable::WaveTable()
    : cont(nullptr)
    , btnSystemInfos(nullptr)
    , btnDialplate(nullptr)
    , btnMap(nullptr)
    , btnBleDevices(nullptr)
    , btnCompass(nullptr)
    , btnRGBControl(nullptr)
    , timer(nullptr)
{
    Model.Init();
}

WaveTable::~WaveTable()
{
    Model.Deinit();
}

void WaveTable::onCustomAttrConfig()
{
    SetCustomLoadAnimType(PageManager::LOAD_ANIM_NONE);
}

void WaveTable::onViewLoad()
{
    ESP_LOGI(TAG, "onViewLoad - 创建全屏白色容器");
    
    // 步骤1: 确保_root的尺寸和位置正确
    lv_obj_set_size(_root, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(_root, 0, 0);
    lv_obj_set_style_border_width(_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // 步骤2: 创建全屏容器（240×320），参考version2的做法
    cont = lv_obj_create(_root);
    if (!cont) {
        ESP_LOGE(TAG, "Failed to create container");
        return;
    }
    
    // 清除容器的所有默认样式
    lv_obj_remove_style_all(cont);
    
    // 设置容器尺寸为全屏（240×320）
    lv_obj_set_size(cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(cont, 0, 0);
    
    // 清除边框和圆角，避免白边
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // 设置容器背景色为白色，不透明
    lv_obj_set_style_bg_color(cont, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    ESP_LOGI(TAG, "Container created: %dx%d, white background", LV_HOR_RES, LV_VER_RES);
    
    // Define button size and spacing constants
    const int button_size = 86;      // Square button size (width = height), further reduce by ~5%
    const int button_spacing = 11;   // Tighten spacing: total 3-row height ~ -10%
    const int top_margin = 35;       // Top margin (for status bar)
    const int side_margin = 10;      // Left and right margins
    
    // 创建系统信息入口按钮 - 左上角，不居中
    btnSystemInfos = lv_btn_create(cont);
    if (!btnSystemInfos) {
        ESP_LOGE(TAG, "Failed to create SystemInfos button");
        return;
    }
    
    // 清除按钮的默认样式
    lv_obj_remove_style_all(btnSystemInfos);
    
    // Set button size (square)
    lv_obj_set_size(btnSystemInfos, button_size, button_size);
    
    // Place at top-left corner
    lv_obj_align(btnSystemInfos, LV_ALIGN_TOP_LEFT, side_margin, top_margin);
    
    // 设置按钮样式：黑色背景，白色字体
    lv_obj_set_style_bg_color(btnSystemInfos, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btnSystemInfos, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btnSystemInfos, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btnSystemInfos, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btnSystemInfos, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btnSystemInfos, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btnSystemInfos, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    // 启用变换中心点，用于缩放动画
    lv_obj_set_style_transform_pivot_x(btnSystemInfos, 50, 0);
    lv_obj_set_style_transform_pivot_y(btnSystemInfos, 50, 0);
    
    // 创建按钮标签
    lv_obj_t *label = lv_label_create(btnSystemInfos);
    lv_label_set_text(label, "SystemInfos");
    lv_obj_center(label);
    
    const lv_font_t* font = ResourcePool::GetFont("bahnschrift_13");
    if (font) {
        lv_obj_set_style_text_font(label, font, 0);
    }
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    AttachEvent(btnSystemInfos);
    
    ESP_LOGI(TAG, "SystemInfos button created at top-left");
    
    // 创建 Dialplate 入口按钮 - SystemInfos 右侧
    btnDialplate = lv_btn_create(cont);
    if (!btnDialplate) {
        ESP_LOGE(TAG, "Failed to create Dialplate button");
        return;
    }
    
    lv_obj_remove_style_all(btnDialplate);
    lv_obj_set_size(btnDialplate, button_size, button_size);
    lv_obj_align(btnDialplate, LV_ALIGN_TOP_RIGHT, -side_margin, top_margin);
    
    lv_obj_set_style_bg_color(btnDialplate, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btnDialplate, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btnDialplate, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btnDialplate, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btnDialplate, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btnDialplate, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btnDialplate, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_transform_pivot_x(btnDialplate, 50, 0);
    lv_obj_set_style_transform_pivot_y(btnDialplate, 50, 0);
    
    lv_obj_t *labelDialplate = lv_label_create(btnDialplate);
    lv_label_set_text(labelDialplate, "Dialplate");
    lv_obj_center(labelDialplate);
    
    const lv_font_t* fontDialplate = ResourcePool::GetFont("bahnschrift_13");
    if (fontDialplate) {
        lv_obj_set_style_text_font(labelDialplate, fontDialplate, 0);
    }
    lv_obj_set_style_text_color(labelDialplate, lv_color_white(), 0);
    AttachEvent(btnDialplate);
    
    ESP_LOGI(TAG, "Dialplate button created");
    
    // 创建 MAP 入口按钮 - SystemInfos 下方
    btnMap = lv_btn_create(cont);
    if (!btnMap) {
        ESP_LOGE(TAG, "Failed to create Map button");
        return;
    }
    
    lv_obj_remove_style_all(btnMap);
    lv_obj_set_size(btnMap, button_size, button_size);
    // Align below SystemInfos, left aligned
    lv_obj_align(btnMap, LV_ALIGN_TOP_LEFT, side_margin, top_margin + button_size + button_spacing);
    
    lv_obj_set_style_bg_color(btnMap, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btnMap, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btnMap, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btnMap, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btnMap, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btnMap, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btnMap, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_transform_pivot_x(btnMap, 50, 0);
    lv_obj_set_style_transform_pivot_y(btnMap, 50, 0);
    
    lv_obj_t *labelMap = lv_label_create(btnMap);
    lv_label_set_text(labelMap, "LiveMap");
    lv_obj_center(labelMap);
    
    const lv_font_t* fontMap = ResourcePool::GetFont("bahnschrift_13");
    if (fontMap) {
        lv_obj_set_style_text_font(labelMap, fontMap, 0);
    }
    lv_obj_set_style_text_color(labelMap, lv_color_white(), 0);
    AttachEvent(btnMap);
    
    ESP_LOGI(TAG, "Map button created");
    
    // 创建 BLE设备 入口按钮 - Dialplate 下方
    btnBleDevices = lv_btn_create(cont);
    if (!btnBleDevices) {
        ESP_LOGE(TAG, "Failed to create BleDevices button");
        return;
    }
    
    lv_obj_remove_style_all(btnBleDevices);
    lv_obj_set_size(btnBleDevices, button_size, button_size);
    // Align below Dialplate, right aligned
    lv_obj_align(btnBleDevices, LV_ALIGN_TOP_RIGHT, -side_margin, top_margin + button_size + button_spacing);
    
    lv_obj_set_style_bg_color(btnBleDevices, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btnBleDevices, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btnBleDevices, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btnBleDevices, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btnBleDevices, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btnBleDevices, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btnBleDevices, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_transform_pivot_x(btnBleDevices, 50, 0);
    lv_obj_set_style_transform_pivot_y(btnBleDevices, 50, 0);
    
    lv_obj_t *labelBleDevices = lv_label_create(btnBleDevices);
    lv_label_set_text(labelBleDevices, "BLE");
    const lv_font_t* fontBleDevices = ResourcePool::GetFont("bahnschrift_13");
    if (fontBleDevices) {
        lv_obj_set_style_text_font(labelBleDevices, fontBleDevices, 0);
    }
    lv_obj_center(labelBleDevices);
    lv_obj_set_style_text_color(labelBleDevices, lv_color_white(), 0);
    AttachEvent(btnBleDevices);
    
    ESP_LOGI(TAG, "BleDevices button created");
    
    // 创建 Compass 入口按钮 - Map 下方
    btnCompass = lv_btn_create(cont);
    if (!btnCompass) {
        ESP_LOGE(TAG, "Failed to create Compass button");
        return;
    }
    
    lv_obj_remove_style_all(btnCompass);
    lv_obj_set_size(btnCompass, button_size, button_size);
    // Align below Map, left aligned
    lv_obj_align(btnCompass, LV_ALIGN_TOP_LEFT, side_margin, top_margin + (button_size + button_spacing) * 2);
    
    lv_obj_set_style_bg_color(btnCompass, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btnCompass, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btnCompass, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btnCompass, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btnCompass, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btnCompass, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btnCompass, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_transform_pivot_x(btnCompass, 50, 0);
    lv_obj_set_style_transform_pivot_y(btnCompass, 50, 0);
    
    lv_obj_t *labelCompass = lv_label_create(btnCompass);
    lv_label_set_text(labelCompass, "Compass");
    const lv_font_t* fontCompass = ResourcePool::GetFont("bahnschrift_13");
    if (fontCompass) {
        lv_obj_set_style_text_font(labelCompass, fontCompass, 0);
    }
    lv_obj_center(labelCompass);
    lv_obj_set_style_text_color(labelCompass, lv_color_white(), 0);
    AttachEvent(btnCompass);
    
    ESP_LOGI(TAG, "Compass button created");
    
    // 创建 RGB Control 入口按钮 - BLE 下方（右侧）
    btnRGBControl = lv_btn_create(cont);
    if (!btnRGBControl) {
        ESP_LOGE(TAG, "Failed to create RGBControl button");
        return;
    }
    
    lv_obj_remove_style_all(btnRGBControl);
    lv_obj_set_size(btnRGBControl, button_size, button_size);
    // Align below BLE, right aligned
    lv_obj_align(btnRGBControl, LV_ALIGN_TOP_RIGHT, -side_margin, top_margin + (button_size + button_spacing) * 2);
    
    lv_obj_set_style_bg_color(btnRGBControl, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btnRGBControl, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btnRGBControl, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btnRGBControl, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btnRGBControl, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btnRGBControl, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btnRGBControl, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_transform_pivot_x(btnRGBControl, 50, 0);
    lv_obj_set_style_transform_pivot_y(btnRGBControl, 50, 0);
    
    lv_obj_t *labelRGBControl = lv_label_create(btnRGBControl);
    lv_label_set_text(labelRGBControl, "RGB");
    const lv_font_t* fontRGBControl = ResourcePool::GetFont("bahnschrift_13");
    if (fontRGBControl) {
        lv_obj_set_style_text_font(labelRGBControl, fontRGBControl, 0);
    }
    lv_obj_center(labelRGBControl);
    lv_obj_set_style_text_color(labelRGBControl, lv_color_white(), 0);
    AttachEvent(btnRGBControl);
    
    ESP_LOGI(TAG, "RGBControl button created");
}

void WaveTable::onViewDidLoad()
{
    ESP_LOGI(TAG, "onViewDidLoad");
}

void WaveTable::onViewWillAppear()
{
    ESP_LOGI(TAG, "onViewWillAppear");
    
    // 设置状态栏样式为透明模式（黑色文字，反色后显示为白色）
    Model.SetStatusBarStyle(0);  // 0 = 透明模式
    // 显示状态栏
    Model.SetStatusBarAppear(true);
    
    // 容器已经在onViewLoad中设置了白色背景，这里不需要再次设置
    Update();
    
    // 启动按钮出现动画
    StartButtonAppearAnim();
}

void WaveTable::onViewDidAppear()
{
    ESP_LOGI(TAG, "onViewDidAppear");
    timer = lv_timer_create(onTimerUpdate, 1000, this);
}

void WaveTable::onViewWillDisappear()
{
    ESP_LOGI(TAG, "onViewWillDisappear");
    if (timer)
    {
        lv_timer_del(timer);
        timer = nullptr;
    }
}

void WaveTable::onViewDidDisappear()
{
    ESP_LOGI(TAG, "onViewDidDisappear");
}

void WaveTable::onViewUnload()
{
    ESP_LOGI(TAG, "onViewUnload");
    // 按钮会在容器删除时自动删除，只需要删除容器
    if (cont)
    {
        lv_obj_del(cont);
        cont = nullptr;
        btnSystemInfos = nullptr;  // 按钮已随容器删除
        btnDialplate = nullptr;    // 按钮已随容器删除
        btnMap = nullptr;          // 按钮已随容器删除
        btnBleDevices = nullptr;   // 按钮已随容器删除
        btnCompass = nullptr;      // 按钮已随容器删除
        btnRGBControl = nullptr;    // 按钮已随容器删除
    }
}

void WaveTable::onViewDidUnload()
{
    ESP_LOGI(TAG, "onViewDidUnload");
}

void WaveTable::Update()
{
    ESP_LOGD(TAG, "Update");
}

void WaveTable::StartButtonAppearAnim()
{
    // 按钮列表（按出现顺序）
    lv_obj_t *buttons[] = {
        btnSystemInfos,
        btnDialplate,
        btnMap,
        btnBleDevices,
        btnCompass,
        btnRGBControl
    };
    
    const int button_count = sizeof(buttons) / sizeof(buttons[0]);
    const int delay_per_button = 80;  // 每个按钮延迟80ms
    
    for (int i = 0; i < button_count; i++)
    {
        if (buttons[i] == nullptr) continue;
        
        // 初始状态：透明且缩小
        lv_obj_set_style_opa(buttons[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_transform_zoom(buttons[i], 205, 0);  // 80%大小 (256 * 0.8 = 205)
        
        // 创建动画：淡入 + 放大
        lv_anim_t a_opa;
        lv_anim_init(&a_opa);
        lv_anim_set_var(&a_opa, buttons[i]);
        lv_anim_set_values(&a_opa, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_time(&a_opa, 300);
        lv_anim_set_delay(&a_opa, i * delay_per_button);
        lv_anim_set_exec_cb(&a_opa, [](void *var, int32_t v) {
            lv_obj_t *obj = (lv_obj_t *)var;
            lv_obj_set_style_opa(obj, (lv_opa_t)v, 0);
        });
        lv_anim_set_path_cb(&a_opa, lv_anim_path_ease_out);
        lv_anim_start(&a_opa);
        
        lv_anim_t a_scale;
        lv_anim_init(&a_scale);
        lv_anim_set_var(&a_scale, buttons[i]);
        lv_anim_set_values(&a_scale, 205, 256);  // 从80%放大到100% (256 * 0.8 = 205, 256 * 1.0 = 256)
        lv_anim_set_time(&a_scale, 300);
        lv_anim_set_delay(&a_scale, i * delay_per_button);
        lv_anim_set_exec_cb(&a_scale, [](void *var, int32_t v) {
            lv_obj_t *obj = (lv_obj_t *)var;
            lv_obj_set_style_transform_zoom(obj, v, 0);
        });
        lv_anim_set_path_cb(&a_scale, lv_anim_path_ease_out);
        lv_anim_start(&a_scale);
    }
}

void WaveTable::AttachEvent(lv_obj_t *obj)
{
    lv_obj_add_event_cb(obj, onEvent, LV_EVENT_ALL, this);
}

void WaveTable::onTimerUpdate(lv_timer_t *timer)
{
    WaveTable *instance = (WaveTable *)timer->user_data;
    instance->Update();
}

void WaveTable::onEvent(lv_event_t *event)
{
    WaveTable *instance = (WaveTable *)lv_event_get_user_data(event);
    lv_obj_t *obj = lv_event_get_target(event);  // 使用 get_target 而不是 get_current_target
    lv_event_code_t code = lv_event_get_code(event);

    // 处理按钮按下动画
    if (code == LV_EVENT_PRESSED)
    {
        lv_obj_t *btn = obj;
        while (btn != nullptr && btn != instance->cont)
        {
            if (btn == instance->btnSystemInfos || btn == instance->btnDialplate ||
                btn == instance->btnMap || btn == instance->btnBleDevices ||
                btn == instance->btnCompass || btn == instance->btnRGBControl)
            {
                // 按下时缩小按钮
                lv_anim_t a;
                lv_anim_init(&a);
                lv_anim_set_var(&a, btn);
                lv_anim_set_values(&a, 256, 243);  // 从100%缩小到95% (256 * 1.0 = 256, 256 * 0.95 = 243)
                lv_anim_set_time(&a, 100);
                lv_anim_set_exec_cb(&a, [](void *var, int32_t v) {
                    lv_obj_t *obj = (lv_obj_t *)var;
                    lv_obj_set_style_transform_zoom(obj, v, 0);
                });
                lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
                lv_anim_start(&a);
                break;
            }
            btn = lv_obj_get_parent(btn);
        }
    }
    // 处理按钮释放动画
    else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST)
    {
        lv_obj_t *btn = obj;
        while (btn != nullptr && btn != instance->cont)
        {
            if (btn == instance->btnSystemInfos || btn == instance->btnDialplate ||
                btn == instance->btnMap || btn == instance->btnBleDevices ||
                btn == instance->btnCompass || btn == instance->btnRGBControl)
            {
                // 释放时恢复按钮大小
                lv_anim_t a;
                lv_anim_init(&a);
                lv_anim_set_var(&a, btn);
                lv_anim_set_values(&a, 243, 256);  // 从95%恢复到100% (256 * 0.95 = 243, 256 * 1.0 = 256)
                lv_anim_set_time(&a, 100);
                lv_anim_set_exec_cb(&a, [](void *var, int32_t v) {
                    lv_obj_t *obj = (lv_obj_t *)var;
                    lv_obj_set_style_transform_zoom(obj, v, 0);
                });
                lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
                lv_anim_start(&a);
                break;
            }
            btn = lv_obj_get_parent(btn);
        }
    }
    else if (code == LV_EVENT_CLICKED)
    {
        // 如果点击的是按钮内的子对象（如 label），需要找到父按钮
        // 检查是否是按钮的直接子对象，如果是则使用父对象
        lv_obj_t *btn = obj;
        while (btn != nullptr && btn != instance->cont)
        {
            // 检查是否是已知的按钮
            if (btn == instance->btnSystemInfos)
            {
                ESP_LOGI(TAG, "SystemInfos button clicked, navigating to SystemInfos");
                instance->_Manager->Push("SystemInfos");
                return;
            }
            else if (btn == instance->btnDialplate)
            {
                ESP_LOGI(TAG, "Dialplate button clicked, navigating to Dialplate");
                instance->_Manager->Push("Dialplate");
                return;
            }
            else if (btn == instance->btnMap)
            {
                ESP_LOGI(TAG, "LiveMap button clicked, navigating to LiveMap");
                instance->_Manager->Push("LiveMap");
                return;
            }
            else if (btn == instance->btnBleDevices)
            {
                ESP_LOGI(TAG, "BleDevices button clicked, navigating to BleDevices");
                instance->_Manager->Push("BleDevices");
                return;
            }
            else if (btn == instance->btnCompass)
            {
                ESP_LOGI(TAG, "Compass button clicked, navigating to Compass");
                instance->_Manager->Push("Compass");
                return;
            }
            else if (btn == instance->btnRGBControl)
            {
                ESP_LOGI(TAG, "RGBControl button clicked, navigating to RGBControl");
                instance->_Manager->Push("RGBControl");
                return;
            }
            // 向上查找父对象
            btn = lv_obj_get_parent(btn);
        }
        
        ESP_LOGW(TAG, "Unknown button clicked: obj=0x%p", obj);
    }
}
