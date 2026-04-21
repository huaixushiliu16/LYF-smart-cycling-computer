#include "CompassView.h"
#include "ResourcePool.h"
#include <stdarg.h>
#include <stdio.h>
#include "esp_log.h"

using namespace Page;

// 动画回调函数
static void anim_img_opa_cb(void *var, int32_t v)
{
    lv_obj_t *obj = (lv_obj_t *)var;
    lv_obj_set_style_img_opa(obj, (lv_opa_t)v, 0);
}

// 动画回调函数 - 暂时注释，因为校准功能已禁用
// static void anim_x_cb(void *var, int32_t v)
// {
//     lv_obj_t *obj = (lv_obj_t *)var;
//     lv_obj_set_x(obj, (lv_coord_t)v);
// }

// static void anim_y_cb(void *var, int32_t v)
// {
//     lv_obj_t *obj = (lv_obj_t *)var;
//     lv_obj_set_y(obj, (lv_coord_t)v);
// }

// static void anim_shadow_opa_cb(void *var, int32_t v)
// {
//     lv_obj_t *obj = (lv_obj_t *)var;
//     lv_obj_set_style_shadow_opa(obj, (lv_opa_t)v, 0);
// }

void CompassView::Create(lv_obj_t *root)
{
    // 画布的创建
    MainCont_Create(root);
    BtnCont_Create(root);

    // 动画的创建
    ui.anim_timeline = lv_anim_timeline_create();
    ui.anim_timelineClick = lv_anim_timeline_create();

    if (!ui.anim_timeline || !ui.anim_timelineClick) {
        ESP_LOGE("CompassView", "Failed to create animation timeline");
        return;
    }

    // 主容器图片淡入动画（确保图片对象已创建）
    if (ui.mainCont.img) {
        lv_anim_t a1;
        lv_anim_init(&a1);
        lv_anim_set_var(&a1, ui.mainCont.img);
        lv_anim_set_values(&a1, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_exec_cb(&a1, anim_img_opa_cb);
        lv_anim_set_time(&a1, 500);
        lv_anim_set_path_cb(&a1, lv_anim_path_ease_out);
        lv_anim_timeline_add(ui.anim_timeline, 500, &a1);
    } else {
        ESP_LOGW("CompassView", "ui.mainCont.img is NULL, skipping animation");
    }

    // 按钮点击动画（爆炸效果）- 暂时简化，因为校准功能已禁用
    // 如果需要启用校准功能，可以在这里添加动画代码
    // 注意：lv_anim_timeline_add 会复制动画结构，所以可以安全地重用变量
}

void CompassView::Delete()
{
    if (ui.anim_timeline)
    {
        lv_anim_timeline_del(ui.anim_timeline);
        ui.anim_timeline = nullptr;
    }
    if (ui.anim_timelineClick)
    {
        lv_anim_timeline_del(ui.anim_timelineClick);
        ui.anim_timelineClick = nullptr;
    }
}

void CompassView::AppearAnimStart(bool reverse)
{
    if (ui.anim_timeline) {
        lv_anim_timeline_set_reverse(ui.anim_timeline, reverse);
        lv_anim_timeline_start(ui.anim_timeline);
    }
}

void CompassView::AppearAnimClick(bool reverse)
{
    if (ui.anim_timelineClick) {
        lv_anim_timeline_set_reverse(ui.anim_timelineClick, reverse);
        lv_anim_timeline_start(ui.anim_timelineClick);
    }
}

void CompassView::MainCont_Create(lv_obj_t *par)
{
    lv_obj_t *cont = lv_obj_create(par);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_center(cont);
    ui.mainCont.cont = cont;

    lv_obj_t *img = lv_img_create(cont);
    lv_obj_set_size(img, 240, 240);
    lv_img_set_src(img, ResourcePool::GetImage("compass_bg"));
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_img_opa(img, LV_OPA_COVER, 0);
    ui.mainCont.img = img;

    lv_obj_t *label = lv_label_create(cont);
    lv_obj_set_style_text_font(label, ResourcePool::GetFont("compass_24"), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xA0A6A5), 0);
    lv_obj_center(label);
    lv_label_set_text(label, "北0°");
    ui.mainCont.label[label_angle] = label;
}

void CompassView::CalibrationCont_Create(lv_obj_t *par)
{
    lv_obj_t *cont = lv_obj_create(par);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_HOR_RES, LV_VER_RES);

    lv_obj_set_style_bg_img_src(cont, ResourcePool::GetImage("compass_bg"), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x000000), 0);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);

    ui.calibrationCont.cont = cont;
}

void CompassView::BtnCont_Create(lv_obj_t *par)
{
    lv_obj_t *cont = lv_obj_create(par);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(70), lv_pct(70));

    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x6a8d6d), 0);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);

    lv_obj_set_style_radius(cont, 16, LV_PART_MAIN);

    lv_obj_set_style_shadow_width(cont, 10, 0);
    lv_obj_set_style_shadow_ofs_x(cont, 5, 0);
    lv_obj_set_style_shadow_ofs_y(cont, 5, 0);
    lv_obj_set_style_shadow_color(cont, lv_color_hex(0x5d8c3d), 0);
    lv_obj_set_style_shadow_spread(cont, 0, 0);
    lv_obj_set_style_shadow_opa(cont, LV_OPA_COVER, 0);

    ui.btnCont.cont = cont;

    ui.btnCont.btn = Btn_Create(cont, ResourcePool::GetImage("start"), 30);

    /* Render octagon explode - 暂时注释，因为校准功能已禁用 */
    // lv_obj_t *roundRect_1 = RoundRect_Create(cont, 0, 30);
    // lv_obj_t *roundRect_2 = RoundRect_Create(cont, 0, 30);
    // lv_obj_t *roundRect_3 = RoundRect_Create(cont, 0, 30);
    // lv_obj_t *roundRect_4 = RoundRect_Create(cont, 0, 30);
    // lv_obj_t *roundRect_5 = RoundRect_Create(cont, 0, 30);
    // lv_obj_t *roundRect_6 = RoundRect_Create(cont, 0, 30);
    // lv_obj_t *roundRect_7 = RoundRect_Create(cont, 0, 30);
    // lv_obj_t *roundRect_8 = RoundRect_Create(cont, 0, 30);

    lv_obj_t *label = lv_label_create(cont);
    lv_obj_set_style_text_font(label, ResourcePool::GetFont("compass_24"), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x236952), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 30);
    lv_label_set_text(label, "软件角校准");

    ui.btnCont.label = label;
}

lv_obj_t *CompassView::Btn_Create(lv_obj_t *par, const void *img_src, lv_coord_t y_ofs)
{
    lv_obj_t *obj = lv_obj_create(par);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, 105, 31);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_align(obj, LV_ALIGN_CENTER, 0, y_ofs);
    lv_obj_set_style_bg_img_src(obj, img_src, 0);

    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_width(obj, 65, LV_STATE_PRESSED);
    lv_obj_set_style_height(obj, 25, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x356b8c), 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x242947), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xf2daaa), LV_STATE_FOCUSED);
    lv_obj_set_style_radius(obj, 9, 0);

    static lv_style_transition_dsc_t tran;
    static const lv_style_prop_t prop[] = {LV_STYLE_WIDTH, LV_STYLE_HEIGHT, LV_STYLE_PROP_INV};
    lv_style_transition_dsc_init(
        &tran,
        prop,
        lv_anim_path_ease_out,
        150,
        0,
        nullptr);
    lv_obj_set_style_transition(obj, &tran, LV_STATE_PRESSED);
    lv_obj_set_style_transition(obj, &tran, LV_STATE_FOCUSED);

    lv_obj_update_layout(obj);

    return obj;
}

lv_obj_t *CompassView::RoundRect_Create(lv_obj_t *par, lv_coord_t x_ofs, lv_coord_t y_ofs)
{
    lv_obj_t *roundRect = lv_obj_create(par);
    lv_obj_remove_style_all(roundRect);
    lv_obj_set_size(roundRect, 10, 10);
    lv_obj_set_style_radius(roundRect, 2, 0);

    lv_obj_set_style_shadow_width(roundRect, 10, 0);
    lv_obj_set_style_shadow_ofs_x(roundRect, 1, 0);
    lv_obj_set_style_shadow_ofs_y(roundRect, 1, 0);
    lv_obj_set_style_shadow_color(roundRect, lv_color_hex(0x5d8c3d), 0);
    lv_obj_set_style_shadow_spread(roundRect, 1, 0);
    lv_obj_set_style_shadow_opa(roundRect, LV_OPA_TRANSP, 0);

    lv_obj_set_style_bg_color(roundRect, lv_color_hex(0x88d35e), 0);
    lv_obj_set_style_bg_opa(roundRect, LV_OPA_TRANSP, 0);
    lv_obj_align(roundRect, LV_ALIGN_CENTER, x_ofs, y_ofs);

    return roundRect;
}
