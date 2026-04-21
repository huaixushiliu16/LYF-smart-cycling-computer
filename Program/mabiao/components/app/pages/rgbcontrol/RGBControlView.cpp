#include "RGBControlView.h"
#include "ResourcePool.h"
#include "esp_log.h"

static const char *TAG = "RGBControlView";

namespace Page
{

void RGBControlView::Create(lv_obj_t *root)
{
    ESP_LOGI(TAG, "Creating RGBControlView");
    
    Header_Create(root);
    Switch_Create(root);
    Status_Create(root);
    
    // 创建动画时间线
    ui.anim_timeline = lv_anim_timeline_create();
}

void RGBControlView::Delete()
{
    if (ui.anim_timeline) {
        lv_anim_timeline_del(ui.anim_timeline);
        ui.anim_timeline = nullptr;
    }
}

void RGBControlView::AppearAnimStart(bool reverse)
{
    if (ui.anim_timeline) {
        lv_anim_timeline_set_reverse(ui.anim_timeline, reverse);
        lv_anim_timeline_start(ui.anim_timeline);
    }
}

void RGBControlView::Header_Create(lv_obj_t *par)
{
    // Create header container
    ui.header.cont = lv_obj_create(par);
    lv_obj_remove_style_all(ui.header.cont);
    lv_obj_set_size(ui.header.cont, LV_HOR_RES, 40);
    lv_obj_align(ui.header.cont, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(ui.header.cont, LV_OPA_TRANSP, 0);
    
    // Create title
    ui.header.title = lv_label_create(ui.header.cont);
    lv_label_set_text(ui.header.title, "RGB");
    lv_obj_set_style_text_color(ui.header.title, lv_color_black(), 0);
    const lv_font_t* font = ResourcePool::GetFont("bahnschrift_13");
    if (font) {
        lv_obj_set_style_text_font(ui.header.title, font, 0);
    }
    lv_obj_align(ui.header.title, LV_ALIGN_CENTER, 0, 0);
}

void RGBControlView::Switch_Create(lv_obj_t *par)
{
    ui.sw = lv_switch_create(par);
    lv_obj_set_size(ui.sw, 120, 50);
    lv_obj_align(ui.sw, LV_ALIGN_CENTER, 0, -10);

    // 黑白风格：关闭时白底黑框；开启时黑底白点
    lv_obj_set_style_bg_color(ui.sw, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui.sw, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui.sw, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui.sw, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui.sw, lv_color_black(), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(ui.sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);

    lv_obj_set_style_bg_color(ui.sw, lv_color_black(), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui.sw, LV_OPA_COVER, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui.sw, lv_color_white(), LV_PART_KNOB | LV_STATE_CHECKED);
}

void RGBControlView::Status_Create(lv_obj_t *par)
{
    ui.label_status = lv_label_create(par);
    lv_label_set_text(ui.label_status, "Connect HR sensor first");
    lv_obj_set_style_text_color(ui.label_status, lv_color_black(), 0);
    const lv_font_t* font = ResourcePool::GetFont("bahnschrift_13");
    if (font) {
        lv_obj_set_style_text_font(ui.label_status, font, 0);
    }
    lv_obj_align(ui.label_status, LV_ALIGN_CENTER, 0, 50);
}

}
