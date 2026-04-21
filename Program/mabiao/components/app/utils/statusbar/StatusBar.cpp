/*
 * MIT License
 * Copyright (c) 2021 _VIFEXTech
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "StatusBar.h"
#include "hal.h"
#include "esp_log.h"
#include "ResourcePool.h"
#include "dataproc_def.h"
#include "dataproc.h"
// Account 和 DataCenter 通过 dataproc 组件的包含路径访问
// 注意：不要同时包含 include/Account.h 和 utils/datacenter/Account.h，会导致重复定义
// 使用 DataCenter.h，它会自动包含正确的 Account.h
#include "DataCenter.h"
#include "../lv_anim_label/lv_anim_label.h"
#include "bsp_ble.h"

#define STATUS_BAR_HEIGHT 25

static const char *TAG = "StatusBar";

static Account *actStatusBar;

static lv_coord_t s_batt_usage_height = 0;
static lv_coord_t s_batt_usage_width = 0;

static void StatusBar_AnimCreate(lv_obj_t *contBatt);

struct
{
    lv_obj_t *cont;

    struct
    {
        lv_obj_t *img;
        lv_obj_t *label;
    } satellite;

    lv_obj_t *imgSD;

    lv_obj_t *labelClock;

    lv_obj_t *labelRec;

    struct
    {
        lv_obj_t *labelS;   // Speed sensor (CSCS in SPEED mode)
        lv_obj_t *labelC;   // Cadence sensor (CSCS in CADENCE mode)
        lv_obj_t *labelH;   // Heart rate sensor
    } ble;

    struct
    {
        lv_obj_t *img;
        lv_obj_t *objUsage;
        lv_obj_t *label;
    } battery;
} ui;

static void StatusBar_ConBattSetOpa(lv_obj_t *obj, int32_t opa)
{
    lv_obj_set_style_opa(obj, opa, 0);
}

static void StatusBar_onAnimOpaFinish(lv_anim_t *a)
{
    lv_obj_t *obj = (lv_obj_t *)a->var;
    StatusBar_ConBattSetOpa(obj, LV_OPA_COVER);
    StatusBar_AnimCreate(obj);
}

static void StatusBar_onAnimHeightFinish(lv_anim_t *a)
{
    lv_anim_t a_opa;
    lv_anim_init(&a_opa);
    lv_anim_set_var(&a_opa, a->var);
    lv_anim_set_exec_cb(&a_opa, (lv_anim_exec_xcb_t)StatusBar_ConBattSetOpa);
    lv_anim_set_ready_cb(&a_opa, StatusBar_onAnimOpaFinish);
    lv_anim_set_values(&a_opa, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_early_apply(&a_opa, true);
    lv_anim_set_delay(&a_opa, 500);
    lv_anim_set_time(&a_opa, 500);
    lv_anim_start(&a_opa);
}

static void StatusBar_AnimCreate(lv_obj_t *contBatt)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, contBatt);
    lv_anim_set_exec_cb(&a, [](void *var, int32_t v)
                        { lv_obj_set_height((lv_obj_t *)var, v); });
    lv_anim_set_values(&a, 0, s_batt_usage_height);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_ready_cb(&a, StatusBar_onAnimHeightFinish);
    lv_anim_start(&a);
}

static lv_obj_t *StatusBar_RecAnimLabelCreate(lv_obj_t *par)
{
    static lv_style_t style_label;
    lv_style_init(&style_label);
    // 反色文字：设置为黑色（反色后显示为白色，在白色背景上可见）
    lv_style_set_text_color(&style_label, lv_color_black());
    lv_style_set_text_font(&style_label, ResourcePool::GetFont("bahnschrift_13"));

    lv_obj_t *alabel = lv_anim_label_create(par);
    // 缩短容器宽度 50->40，给右侧的 S/C/H BLE 状态让出位置
    lv_obj_set_size(alabel, 40, STATUS_BAR_HEIGHT - 4);
    lv_anim_label_set_enter_dir(alabel, LV_DIR_TOP);
    lv_anim_label_set_exit_dir(alabel, LV_DIR_BOTTOM);
    lv_anim_label_set_path(alabel, lv_anim_path_ease_out);
    lv_anim_label_set_time(alabel, 500);
    lv_anim_label_add_style(alabel, &style_label);

    // 整体左移：right=156, left=116，与右侧 S/C/H(161~) 保留 5px 间距
    lv_obj_align(alabel, LV_ALIGN_RIGHT_MID, -84, 0);

    lv_anim_t a_enter;
    lv_anim_init(&a_enter);
    lv_anim_set_early_apply(&a_enter, true);
    lv_anim_set_values(&a_enter, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_exec_cb(&a_enter, [](void *var, int32_t v)
                        { lv_obj_set_style_opa((lv_obj_t *)var, v, 0); });
    lv_anim_set_time(&a_enter, 300);

    lv_anim_t a_exit = a_enter;
    lv_anim_set_values(&a_exit, LV_OPA_COVER, LV_OPA_TRANSP);

    lv_anim_label_set_custom_enter_anim(alabel, &a_enter);
    lv_anim_label_set_custom_exit_anim(alabel, &a_exit);

    return alabel;
}

static void StatusBar_Update(lv_timer_t *timer)
{
    if (!actStatusBar)
    {
        return;
    }

    /* GPS - 只Pull一次，获取卫星数量和时间 */
    HAL::GPS_Info_t gps;
    memset(&gps, 0, sizeof(gps));  // 初始化GPS数据
    if (actStatusBar->Pull("GPS", &gps, sizeof(gps)) == Account::RES_OK)
    {
        /* satellite */
        lv_label_set_text_fmt(ui.satellite.label, "%d", gps.satellites);

        /* clock - 从GPS的clock字段获取时间 */
        if (gps.isVaild && gps.clock.year > 0)
        {
            int hour = gps.clock.hour + 8;  // UTC+8
            if (hour >= 24) hour -= 24;
            lv_label_set_text_fmt(ui.labelClock, "%02d:%02d", hour, gps.clock.minute);
        }
        else
        {
            lv_label_set_text(ui.labelClock, "--:--");
        }
    }
    else
    {
        // Pull失败，显示默认值
        lv_label_set_text(ui.satellite.label, "0");
        lv_label_set_text(ui.labelClock, "--:--");
    }

    /* storage */
    Storage_Basic_Info_t sdInfo;
    memset(&sdInfo, 0, sizeof(sdInfo));  // 初始化存储信息
    if (actStatusBar->Pull("Storage", &sdInfo, sizeof(sdInfo)) == Account::RES_OK)
    {
        sdInfo.isDetect ? lv_obj_clear_state(ui.imgSD, LV_STATE_DISABLED) : lv_obj_add_state(ui.imgSD, LV_STATE_DISABLED);
    }

    /* battery */
    HAL::Power_Info_t power = {0, 0, false};
    if (actStatusBar->Pull("Power", &power, sizeof(power)) == Account::RES_OK)
    {
        lv_label_set_text_fmt(ui.battery.label, "%d", power.usage);
    }

    /* BLE connection status (S / C / H)
     * 三个字母从左到右：S=速度传感器, C=踏频设备, H=心率设备
     * 已连接显示黑色（反色后白色，清晰可见），未连接则降低透明度作为占位提示。
     * 说明：CSCS 设备硬件上是"单感"，同一时刻只会工作在 SPEED 或 CADENCE 一种模式，
     *       所以 S 和 C 不会同时点亮；BOTH/UNKNOWN 情况下保守地不点亮任一个。 */
    bool hr_connected = bsp_ble_is_device_connected(BSP_BLE_DEVICE_TYPE_HR);
    bool cscs_connected = bsp_ble_is_device_connected(BSP_BLE_DEVICE_TYPE_CSCS);
    bsp_ble_cscs_mode_t cscs_mode = cscs_connected ? bsp_ble_get_cscs_mode() : BSP_BLE_CSCS_MODE_UNKNOWN;
    bool show_S = cscs_connected && (cscs_mode == BSP_BLE_CSCS_MODE_SPEED);
    bool show_C = cscs_connected && (cscs_mode == BSP_BLE_CSCS_MODE_CADENCE);
    bool show_H = hr_connected;

    if (ui.ble.labelS) {
        lv_obj_set_style_text_opa(ui.ble.labelS, show_S ? LV_OPA_COVER : LV_OPA_20, 0);
    }
    if (ui.ble.labelC) {
        lv_obj_set_style_text_opa(ui.ble.labelC, show_C ? LV_OPA_COVER : LV_OPA_20, 0);
    }
    if (ui.ble.labelH) {
        lv_obj_set_style_text_opa(ui.ble.labelH, show_H ? LV_OPA_COVER : LV_OPA_20, 0);
    }

    bool Is_BattCharging = power.isCharging;
    lv_obj_t *contBatt = ui.battery.objUsage;
    static bool Is_BattChargingAnimActive = false;
    if (Is_BattCharging)
    {
        if (!Is_BattChargingAnimActive)
        {
            StatusBar_AnimCreate(contBatt);
            Is_BattChargingAnimActive = true;
        }
    }
    else
    {
        if (Is_BattChargingAnimActive)
        {
            lv_anim_del(contBatt, nullptr);
            StatusBar_ConBattSetOpa(contBatt, LV_OPA_COVER);
            Is_BattChargingAnimActive = false;
        }
        lv_coord_t height = (power.usage * s_batt_usage_height) / 100;
        if (height > s_batt_usage_height) height = s_batt_usage_height;
        lv_obj_set_height(contBatt, height);
    }
}

static void StatusBar_StyleInit(lv_obj_t *cont)
{
    /* style1 - 透明模式 */
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x333333), LV_STATE_DEFAULT);

    /* style2 - 白色模式（原黑色模式，颜色反转） */
    lv_obj_set_style_bg_opa(cont, LV_OPA_60, LV_STATE_USER_1);
    lv_obj_set_style_bg_color(cont, lv_color_white(), LV_STATE_USER_1);
    lv_obj_set_style_shadow_color(cont, lv_color_hex(0xCCCCCC), LV_STATE_USER_1);
    lv_obj_set_style_shadow_width(cont, 10, LV_STATE_USER_1);

    static lv_style_transition_dsc_t tran;
    static const lv_style_prop_t prop[] =
        {
            LV_STYLE_BG_COLOR,
            LV_STYLE_OPA,
            LV_STYLE_PROP_INV};
    lv_style_transition_dsc_init(
        &tran,
        prop,
        lv_anim_path_ease_out,
        200,
        0,
        nullptr);
    lv_obj_set_style_transition(cont, &tran, LV_STATE_USER_1);
}

static lv_obj_t *StatusBar_SdCardImage_Create(lv_obj_t *par)
{
    lv_obj_t *img = lv_img_create(par);
    lv_img_set_src(img, ResourcePool::GetImage("sd_card"));
    // 反色图标：设置重着色为黑色，不透明度255（反色后显示为白色）
    lv_obj_set_style_img_recolor(img, lv_color_black(), 0);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_align(img, LV_ALIGN_LEFT_MID, 55, -1);

    lv_obj_set_style_translate_y(img, -STATUS_BAR_HEIGHT, LV_STATE_DISABLED);

    static lv_style_transition_dsc_t tran;
    static const lv_style_prop_t prop[] =
        {
            LV_STYLE_TRANSLATE_Y,
            LV_STYLE_PROP_INV};
    lv_style_transition_dsc_init(
        &tran,
        prop,
        lv_anim_path_overshoot,
        100,
        0,
        nullptr);
    lv_obj_set_style_transition(img, &tran, LV_STATE_DISABLED);
    lv_obj_set_style_transition(img, &tran, LV_STATE_DEFAULT);

    return img;
}

static void StatusBar_SetStyle(StatusBar_Style_t style)
{
    lv_obj_t *cont = ui.cont;
    switch (style)
    {
    case STATUS_BAR_STYLE_TRANSP:
        lv_obj_add_state(cont, LV_STATE_DEFAULT);
        lv_obj_clear_state(cont, LV_STATE_USER_1);
        
        // 透明模式：黑色文字，黑色电量条（反色后显示为白色，在白色背景上可见）
        if (ui.satellite.label) {
            lv_obj_set_style_text_color(ui.satellite.label, lv_color_black(), 0);
        }
        if (ui.labelClock) {
            lv_obj_set_style_text_color(ui.labelClock, lv_color_black(), 0);
        }
        if (ui.battery.label) {
            lv_obj_set_style_text_color(ui.battery.label, lv_color_black(), 0);
        }
        if (ui.battery.objUsage) {
            lv_obj_set_style_bg_color(ui.battery.objUsage, lv_color_black(), 0);
        }
        if (ui.ble.labelS) lv_obj_set_style_text_color(ui.ble.labelS, lv_color_black(), 0);
        if (ui.ble.labelC) lv_obj_set_style_text_color(ui.ble.labelC, lv_color_black(), 0);
        if (ui.ble.labelH) lv_obj_set_style_text_color(ui.ble.labelH, lv_color_black(), 0);
        // 录制标签：需要更新内部两个 label 的颜色
        if (ui.labelRec) {
            lv_anim_label_t *alabel = (lv_anim_label_t *)ui.labelRec;
            lv_obj_set_style_text_color(alabel->label_1, lv_color_black(), 0);
            lv_obj_set_style_text_color(alabel->label_2, lv_color_black(), 0);
        }
        break;
        
    case STATUS_BAR_STYLE_BLACK:  // 现在实际是白色模式
        lv_obj_add_state(cont, LV_STATE_USER_1);
        
        // 白色模式：黑色文字，黑色电量条（反色后显示为白色）
        if (ui.satellite.label) {
            lv_obj_set_style_text_color(ui.satellite.label, lv_color_black(), 0);
        }
        if (ui.labelClock) {
            lv_obj_set_style_text_color(ui.labelClock, lv_color_black(), 0);
        }
        if (ui.battery.label) {
            lv_obj_set_style_text_color(ui.battery.label, lv_color_black(), 0);
        }
        if (ui.battery.objUsage) {
            lv_obj_set_style_bg_color(ui.battery.objUsage, lv_color_black(), 0);
        }
        if (ui.ble.labelS) lv_obj_set_style_text_color(ui.ble.labelS, lv_color_black(), 0);
        if (ui.ble.labelC) lv_obj_set_style_text_color(ui.ble.labelC, lv_color_black(), 0);
        if (ui.ble.labelH) lv_obj_set_style_text_color(ui.ble.labelH, lv_color_black(), 0);
        // 录制标签：需要更新内部两个 label 的颜色
        if (ui.labelRec) {
            lv_anim_label_t *alabel = (lv_anim_label_t *)ui.labelRec;
            lv_obj_set_style_text_color(alabel->label_1, lv_color_black(), 0);
            lv_obj_set_style_text_color(alabel->label_2, lv_color_black(), 0);
        }
        break;
        
    default:
        break;
    }
}

lv_obj_t *Page::StatusBar_Create(lv_obj_t *par)
{
    // 获取DataCenter和Account
    DataCenter *center = DataProc_GetDataCenter();
    if (!center)
    {
        ESP_LOGE(TAG, "DataCenter not initialized");
        return nullptr;
    }

    actStatusBar = center->SearchAccount("StatusBar");
    if (!actStatusBar)
    {
        ESP_LOGE(TAG, "StatusBar Account not found");
        return nullptr;
    }

    // 订阅GPS、Power、Storage Account
    actStatusBar->Subscribe("GPS");
    actStatusBar->Subscribe("Power");
    actStatusBar->Subscribe("Storage");

    lv_obj_t *cont = lv_obj_create(par);
    lv_obj_remove_style_all(cont);

    lv_obj_set_size(cont, LV_HOR_RES, STATUS_BAR_HEIGHT);
    // 直接定位到顶部，不需要动画（像SystemMonitor一样）
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 0);
    // 禁用滚动
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    StatusBar_StyleInit(cont);
    ui.cont = cont;

    static lv_style_t style_label;
    lv_style_init(&style_label);
    // 反色文字：设置为黑色（反色后显示为白色，在白色背景上可见）
    lv_style_set_text_color(&style_label, lv_color_black());
    lv_style_set_text_font(&style_label, ResourcePool::GetFont("bahnschrift_17"));

    /* satellite */
    lv_obj_t *img = lv_img_create(cont);
    lv_img_set_src(img, ResourcePool::GetImage("satellite"));
    // 反色图标：设置重着色为黑色，不透明度255（反色后显示为白色）
    lv_obj_set_style_img_recolor(img, lv_color_black(), 0);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_align(img, LV_ALIGN_LEFT_MID, 14, 0);
    ui.satellite.img = img;

    lv_obj_t *label = lv_label_create(cont);
    lv_obj_add_style(label, &style_label, 0);
    lv_obj_align_to(label, ui.satellite.img, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_label_set_text(label, "0");
    ui.satellite.label = label;

    /* sd card */
    ui.imgSD = StatusBar_SdCardImage_Create(cont);

    /* clock */
    // 左移时钟：从居中改成靠左对齐，给中间腾出位置放 REC 标签，避免与 REC / S/C/H 冲突
    label = lv_label_create(cont);
    lv_obj_add_style(label, &style_label, 0);
    lv_label_set_text(label, "00:00");
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 68, 0);
    ui.labelClock = label;

    /* recorder */
    ui.labelRec = StatusBar_RecAnimLabelCreate(cont);

    /* battery */
    img = lv_img_create(cont);
    lv_img_set_src(img, ResourcePool::GetImage("battery"));
    // 反色图标：设置重着色为黑色，不透明度255（反色后显示为白色）
    lv_obj_set_style_img_recolor(img, lv_color_black(), 0);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_align(img, LV_ALIGN_RIGHT_MID, -35, 0);
    lv_img_t *img_ext = (lv_img_t *)img;
    lv_obj_set_size(img, img_ext->w, img_ext->h);
    ui.battery.img = img;

    // 计算电量条尺寸
    s_batt_usage_height = lv_obj_get_style_height(img, 0) - 6;
    s_batt_usage_width = lv_obj_get_style_width(img, 0) - 4;

    lv_obj_t *obj = lv_obj_create(img);
    lv_obj_remove_style_all(obj);
    // 反色电量条：设置为黑色（反色后显示为白色）
    lv_obj_set_style_bg_color(obj, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_size(obj, s_batt_usage_width, s_batt_usage_height);
    lv_obj_align(obj, LV_ALIGN_BOTTOM_MID, 0, -2);
    ui.battery.objUsage = obj;

    label = lv_label_create(cont);
    lv_obj_add_style(label, &style_label, 0);
    lv_obj_align_to(label, ui.battery.img, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_label_set_text(label, "99%");
    ui.battery.label = label;

    /* BLE connection status (S C H), 放在电池图标左侧，占用三个字母的宽度。
     * 字母从左到右：S=速度, C=踏频, H=心率。
     * 使用 bahnschrift_13 小字号，避免与中间的 REC 录制标签冲突。
     * 初始创建为"未连接"样式（文字黑色 + 低 opa 作为占位灰），运行时由
     * StatusBar_Update 根据 bsp_ble 的连接状态将已连接字母切到 OPA_COVER。*/
    {
        static lv_style_t style_ble_label;
        lv_style_init(&style_ble_label);
        lv_style_set_text_color(&style_ble_label, lv_color_black());
        lv_style_set_text_font(&style_ble_label, ResourcePool::GetFont("bahnschrift_13"));

        lv_obj_t *lblH = lv_label_create(cont);
        lv_obj_add_style(lblH, &style_ble_label, 0);
        lv_label_set_text(lblH, "H");
        lv_obj_align_to(lblH, ui.battery.img, LV_ALIGN_OUT_LEFT_MID, -4, 0);
        lv_obj_set_style_text_opa(lblH, LV_OPA_20, 0);
        ui.ble.labelH = lblH;

        lv_obj_t *lblC = lv_label_create(cont);
        lv_obj_add_style(lblC, &style_ble_label, 0);
        lv_label_set_text(lblC, "C");
        lv_obj_align_to(lblC, lblH, LV_ALIGN_OUT_LEFT_MID, -2, 0);
        lv_obj_set_style_text_opa(lblC, LV_OPA_20, 0);
        ui.ble.labelC = lblC;

        lv_obj_t *lblS = lv_label_create(cont);
        lv_obj_add_style(lblS, &style_ble_label, 0);
        lv_label_set_text(lblS, "S");
        lv_obj_align_to(lblS, lblC, LV_ALIGN_OUT_LEFT_MID, -2, 0);
        lv_obj_set_style_text_opa(lblS, LV_OPA_20, 0);
        ui.ble.labelS = lblS;
    }

    StatusBar_SetStyle(STATUS_BAR_STYLE_TRANSP);

    // 确保状态栏在顶层显示，不被页面容器遮挡
    lv_obj_move_foreground(cont);

    lv_timer_t *timer = lv_timer_create(StatusBar_Update, 1000, nullptr);
    lv_timer_ready(timer);

    return ui.cont;
}

static void StatusBar_Appear(bool en)
{
    if (!ui.cont) {
        return;
    }
    
    // 确保状态栏在顶层显示
    lv_obj_move_foreground(ui.cont);
    
    // 直接显示/隐藏，不使用动画（像SystemMonitor一样）
    if (en) {
        lv_obj_clear_flag(ui.cont, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(ui.cont, LV_ALIGN_TOP_MID, 0, 0);
    } else {
        lv_obj_add_flag(ui.cont, LV_OBJ_FLAG_HIDDEN);
    }
}

void Page::StatusBar_SetAppear(bool en)
{
    StatusBar_Appear(en);
}

void Page::StatusBar_SetStyle(int style)
{
    StatusBar_SetStyle((StatusBar_Style_t)style);
}

void Page::StatusBar_SetLabelRec(bool show, const char *str)
{
    if (ui.labelRec)
    {
        lv_anim_label_push_text(ui.labelRec, show ? str : " ");
    }
}
