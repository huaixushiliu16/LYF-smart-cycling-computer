#include "DialplateView.h"
#include <stdarg.h>
#include <stdio.h>
#include "esp_log.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

using namespace Page;

// 动画包装函数
static void anim_opa_cb(void *var, int32_t v)
{
    lv_obj_t *obj = (lv_obj_t *)var;
    lv_obj_set_style_opa(obj, (lv_opa_t)v, 0);
}

static void anim_y_cb(void *var, int32_t v)
{
    lv_obj_t *obj = (lv_obj_t *)var;
    lv_obj_set_y(obj, (lv_coord_t)v);
}

static void anim_height_cb(void *var, int32_t v)
{
    lv_obj_t *obj = (lv_obj_t *)var;
    lv_obj_set_height(obj, (lv_coord_t)v);
}

void DialplateView::Create(lv_obj_t *root)
{
    BottomInfo_Create(root);
    TopInfo_Create(root);
    BtnCont_Create(root);

    // 确保所有对象都已创建
    if (!ui.topInfo.cont || !ui.bottomInfo.cont || !ui.btnCont.btnRec) {
        ESP_LOGE("DialplateView", "Failed to create UI objects");
        return;
    }

    // 强制更新布局，确保位置和尺寸正确
    lv_obj_update_layout(ui.topInfo.cont);
    lv_obj_update_layout(ui.bottomInfo.cont);
    lv_obj_update_layout(ui.btnCont.btnRec);
    
    // 确保顶部容器的滚动位置在顶部（防止初始化时滚动到底部）
    lv_obj_scroll_to_y(ui.topInfo.cont, 0, LV_ANIM_OFF);

    ui.anim_timeline = lv_anim_timeline_create();
    if (!ui.anim_timeline) {
        ESP_LOGE("DialplateView", "Failed to create animation timeline");
        return;
    }

    // 使用标准 LVGL API 创建动画
    lv_coord_t y_tar_top = lv_obj_get_y(ui.topInfo.cont);
    lv_coord_t y_tar_bottom = lv_obj_get_y(ui.bottomInfo.cont);
    lv_coord_t h_tar_btn = lv_obj_get_height(ui.btnCont.btnRec);

    // 顶部容器动画：从上方滑入
    lv_anim_t a1;
    lv_anim_init(&a1);
    lv_anim_set_var(&a1, ui.topInfo.cont);
    lv_anim_set_values(&a1, -lv_obj_get_height(ui.topInfo.cont), y_tar_top);
    lv_anim_set_exec_cb(&a1, anim_y_cb);
    lv_anim_set_time(&a1, 500);
    lv_anim_set_path_cb(&a1, lv_anim_path_ease_out);
    lv_anim_timeline_add(ui.anim_timeline, 0, &a1);

    // 底部容器动画：从上方滑入 + 淡入
    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_var(&a2, ui.bottomInfo.cont);
    lv_anim_set_values(&a2, -lv_obj_get_height(ui.bottomInfo.cont), y_tar_bottom);
    lv_anim_set_exec_cb(&a2, anim_y_cb);
    lv_anim_set_time(&a2, 500);
    lv_anim_set_path_cb(&a2, lv_anim_path_ease_out);
    lv_anim_timeline_add(ui.anim_timeline, 200, &a2);

    lv_anim_t a3;
    lv_anim_init(&a3);
    lv_anim_set_var(&a3, ui.bottomInfo.cont);
    lv_anim_set_values(&a3, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_exec_cb(&a3, anim_opa_cb);
    lv_anim_set_time(&a3, 500);
    lv_anim_set_path_cb(&a3, lv_anim_path_ease_out);
    lv_anim_timeline_add(ui.anim_timeline, 200, &a3);

    // 按钮动画：高度从0展开
    lv_anim_t a4;
    lv_anim_init(&a4);
    lv_anim_set_var(&a4, ui.btnCont.btnMap);
    lv_anim_set_values(&a4, 0, h_tar_btn);
    lv_anim_set_exec_cb(&a4, anim_height_cb);
    lv_anim_set_time(&a4, 500);
    lv_anim_set_path_cb(&a4, lv_anim_path_ease_out);
    lv_anim_timeline_add(ui.anim_timeline, 500, &a4);

    lv_anim_t a5;
    lv_anim_init(&a5);
    lv_anim_set_var(&a5, ui.btnCont.btnRec);
    lv_anim_set_values(&a5, 0, h_tar_btn);
    lv_anim_set_exec_cb(&a5, anim_height_cb);
    lv_anim_set_time(&a5, 500);
    lv_anim_set_path_cb(&a5, lv_anim_path_ease_out);
    lv_anim_timeline_add(ui.anim_timeline, 600, &a5);

    lv_anim_t a6;
    lv_anim_init(&a6);
    lv_anim_set_var(&a6, ui.btnCont.btnMenu);
    lv_anim_set_values(&a6, 0, h_tar_btn);
    lv_anim_set_exec_cb(&a6, anim_height_cb);
    lv_anim_set_time(&a6, 500);
    lv_anim_set_path_cb(&a6, lv_anim_path_ease_out);
    lv_anim_timeline_add(ui.anim_timeline, 700, &a6);
}

void DialplateView::Delete()
{
    if (ui.anim_timeline)
    {
        lv_anim_timeline_del(ui.anim_timeline);
        ui.anim_timeline = nullptr;
    }
}

void DialplateView::TopInfo_Create(lv_obj_t *par)
{
    lv_obj_t *cont = lv_obj_create(par);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_HOR_RES, 142);

    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    // 颜色反转：0x333333 (深灰) -> 白色
    lv_obj_set_style_bg_color(cont, lv_color_white(), 0);

    lv_obj_set_style_radius(cont, 27, 0);

    // 左右居中显示，Y = -36px（不可见区域36px）
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, -36);
    ui.topInfo.cont = cont;

    // 速度数字：左侧，Y位置50px（在可见区域内，容器可见区域0-106px）
    lv_obj_t *label = lv_label_create(cont);
    lv_obj_set_style_text_font(label, ResourcePool::GetFont("bahnschrift_65"), 0);
    // 颜色反转：白色 -> 黑色
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_label_set_text(label, "00");
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_set_y(label, 22);  // Y位置45px（在可见区域内）0
    ui.topInfo.labelSpeed = label;

    // 速度单位：跟随速度数字，左对齐
    label = lv_label_create(cont);
    lv_obj_set_style_text_font(label, ResourcePool::GetFont("bahnschrift_17"), 0);
    // 颜色反转：白色 -> 黑色
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_label_set_text(label, "km/h");
    lv_obj_align_to(label, ui.topInfo.labelSpeed, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
    ui.topInfo.labelUint = label;

    // AVG信息组：右上角，Y位置25px（在可见区域内）
    SubInfoGrp_Create(cont, &(ui.topInfo.avgInfo), "AVG");
    lv_obj_align(ui.topInfo.avgInfo.cont, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_set_y(ui.topInfo.avgInfo.cont, 22);  // Y位置25px（在可见区域内）

    // Trip信息组：右下角，Y位置65px（确保在可见区域内，容器底部106px，信息组高度30px，65+30=95px < 106px）
    SubInfoGrp_Create(cont, &(ui.topInfo.tripInfo), "Trip");
    lv_obj_align(ui.topInfo.tripInfo.cont, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_set_y(ui.topInfo.tripInfo.cont, 52);  // Y位置65px（确保在可见区域内）
}

void DialplateView::BottomInfo_Create(lv_obj_t *par)
{
    lv_obj_t *cont = lv_obj_create(par);
    lv_obj_remove_style_all(cont);
    // 颜色反转：黑色 -> 白色
    lv_obj_set_style_bg_color(cont, lv_color_white(), 0);
    // 底部区域高度设置为116px
    // 计算：顶部区域 Y = -36px，高度 142px，底部在 106px
    // 按钮区域在底部，高度 40px，从 280px 开始
    // 底部区域可用空间：280 - 106 = 174px（有足够空间）
    // 3行 × 30px = 90px，2个行间距 × 4px = 8px，上下外边距 4px × 2 = 8px，总共 106px
    // 设置为 116px 以提供更好的显示效果
    lv_obj_set_size(cont, LV_HOR_RES, 116);
    // 位置：顶部区域底部在 106px，底部区域从 110px 开始（留出4px间距）
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 110);

    // 使用两列三行布局：ROW_WRAP，每行2个，共3行
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(
        cont,
        LV_FLEX_ALIGN_SPACE_EVENLY,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START);
    // 设置列间距和行间距（减小间距以容纳更多内容）
    lv_obj_set_style_pad_row(cont, 4, 0);  // 行间距4px（减小）
    lv_obj_set_style_pad_column(cont, 4, 0);  // 列间距4px（减小）
    lv_obj_set_style_pad_all(cont, 4, 0);  // 外边距4px（减小）

    ui.bottomInfo.cont = cont;

    const char *unitText[6] =
        {
            "Altitude",   // 海拔（原AVG位置）
            "Time",
            "Temperature", // 温度（原Trip位置）
            "Calorie",
            "HR",      // 心率
            "Cadence"  // 踏频
        };

    for (int i = 0; i < ARRAY_SIZE(ui.bottomInfo.labelInfoGrp); i++)
    {
        SubInfoGrp_Create(
            cont,
            &(ui.bottomInfo.labelInfoGrp[i]),
            unitText[i]);
    }
}

void DialplateView::SubInfoGrp_Create(lv_obj_t *par, SubInfo_t *info, const char *unitText)
{
    lv_obj_t *cont = lv_obj_create(par);
    lv_obj_remove_style_all(cont);
    // 两列布局：屏幕240px - 外边距8px = 232px，两列各约110px，留出列间距
    // 高度调整为30px以适应底部区域空间（3行 × 30px = 90px，2个行间距8px，上下外边距8px，总共106px，刚好适配108px）
    lv_obj_set_size(cont, 110, 30);

    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        cont,
        LV_FLEX_ALIGN_SPACE_AROUND,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label = lv_label_create(cont);
    lv_obj_set_style_text_font(label, ResourcePool::GetFont("bahnschrift_17"), 0);
    // 颜色反转：白色 -> 黑色
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_label_set_text(label, "--");  // 设置初始文本
    info->lableValue = label;

    label = lv_label_create(cont);
    lv_obj_set_style_text_font(label, ResourcePool::GetFont("bahnschrift_13"), 0);
    // 颜色反转：0xb3b3b3 (浅灰) -> 0x4c4c4c (深灰)
    lv_obj_set_style_text_color(label, lv_color_hex(0x4c4c4c), 0);
    lv_label_set_text(label, unitText);
    info->lableUnit = label;

    info->cont = cont;
}

void DialplateView::BtnCont_Create(lv_obj_t *par)
{
    lv_obj_t *cont = lv_obj_create(par);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_HOR_RES, 40);
    // 将按钮容器对齐到屏幕底部
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, 0);

    ui.btnCont.cont = cont;

    ui.btnCont.btnMap = Btn_Create(cont, ResourcePool::GetImage("locate"), -80);
    ui.btnCont.btnRec = Btn_Create(cont, ResourcePool::GetImage("start"), 0);
    ui.btnCont.btnMenu = Btn_Create(cont, ResourcePool::GetImage("menu"), 80);
}

lv_obj_t *DialplateView::Btn_Create(lv_obj_t *par, const void *img_src, lv_coord_t x_ofs)
{
    lv_obj_t *obj = lv_obj_create(par);
    lv_obj_remove_style_all(obj);
    // 增大30%：40*1.3=52, 31*1.3=40
    lv_obj_set_size(obj, 52, 40);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(obj, LV_ALIGN_CENTER, x_ofs, 0);
    lv_obj_set_style_bg_img_src(obj, img_src, 0);

    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    // 按下状态也增大30%：45*1.3=58, 25*1.3=32
    lv_obj_set_style_width(obj, 58, LV_STATE_PRESSED);
    lv_obj_set_style_height(obj, 32, LV_STATE_PRESSED);
    // 颜色反转：0x666666 (灰色) -> 0x999999 (浅灰)
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x999999), 0);
    // 颜色反转：0xbbbbbb (浅灰，按下) -> 0x444444 (深灰)
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x444444), LV_STATE_PRESSED);
    // 颜色反转：0xff931e (橙色，聚焦) -> 0x006ce1 (蓝色)
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x006ce1), LV_STATE_FOCUSED);
    lv_obj_set_style_radius(obj, 9, 0);

    static lv_style_transition_dsc_t tran;
    static const lv_style_prop_t prop[] = {LV_STYLE_WIDTH, LV_STYLE_HEIGHT, LV_STYLE_PROP_INV};
    lv_style_transition_dsc_init(
        &tran,
        prop,
        lv_anim_path_ease_out,
        200,
        0,
        nullptr);
    lv_obj_set_style_transition(obj, &tran, LV_STATE_PRESSED);
    lv_obj_set_style_transition(obj, &tran, LV_STATE_FOCUSED);

    lv_obj_update_layout(obj);

    return obj;
}

void DialplateView::AppearAnimStart(bool reverse)
{
    lv_anim_timeline_set_reverse(ui.anim_timeline, reverse);
    lv_anim_timeline_start(ui.anim_timeline);
}
