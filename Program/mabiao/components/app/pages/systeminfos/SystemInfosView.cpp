#include "SystemInfosView.h"
#include "ResourcePool.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

// 禁用格式截断警告（我们使用 strncat 手动构建字符串，确保安全）
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

using namespace Page;

#define ITEM_HEIGHT_MIN 100
// 移除ITEM_PAD，让内容从顶部开始显示
// #define ITEM_PAD ((LV_VER_RES - ITEM_HEIGHT_MIN) / 2)

/**
 * @brief 安全格式化多行文本（统一格式化辅助函数）
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @param format 格式化字符串（支持 %s, %d, %f 等）
 * @param ... 可变参数
 * @return 成功返回0，失败返回-1
 */
static int SafeFormatText(char* buf, size_t buf_size, const char* format, ...)
{
    if (buf == nullptr || buf_size == 0 || format == nullptr) {
        return -1;
    }
    
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buf, buf_size, format, args);
    va_end(args);
    
    // 确保字符串以 \0 结尾
    if (buf_size > 0) {
        buf[buf_size - 1] = '\0';
    }
    
    return (ret >= 0 && (size_t)ret < buf_size) ? 0 : -1;
}

void SystemInfosView::Create(lv_obj_t *root)
{
    // 先移除所有样式，包括可能来自PageManager的默认样式
    // 使用LV_PART_MAIN和所有状态来彻底清除样式
    lv_obj_remove_style_all(root);
    lv_obj_remove_style(root, NULL, LV_PART_MAIN);
    lv_obj_remove_style(root, NULL, LV_PART_SCROLLBAR);
    
    // 设置尺寸
    lv_obj_set_size(root, LV_HOR_RES, LV_VER_RES);
    
    lv_obj_set_style_bg_color(root, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    
    // 移除垂直padding，让内容从顶部开始
    lv_obj_set_style_pad_ver(root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // 确保容器可以滚动（关键：必须启用滚动标志）
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLL_ELASTIC);  // 禁用弹性滚动
    lv_obj_add_flag(root, LV_OBJ_FLAG_SCROLLABLE);  // 启用滚动
    lv_obj_set_scroll_dir(root, LV_DIR_VER);  // 设置垂直滚动方向
    lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);  // 隐藏滚动条 

    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        root,
        LV_FLEX_ALIGN_START,  // 主轴上从顶部开始
        LV_FLEX_ALIGN_START,  // 交叉轴上从左边开始
        LV_FLEX_ALIGN_START); // 交叉轴对齐方式改为START，确保内容从顶部开始

    Style_Init();

    /* Item Sport */
    Item_Create(
        &ui.sport,
        root,
        "Sport",
        "bicycle",

        "Total trip\n"
        "Total time\n"
        "Max speed");

    /* Item GPS */
    Item_Create(
        &ui.gps,
        root,
        "GPS",
        "map_location",

        "Latitude\n"
        "Longitude\n"
        "Altitude\n"
        "UTC Time\n\n"
        "Course\n"
        "Speed");

    /* Item MAG */
    Item_Create(
        &ui.mag,
        root,
        "MAG",
        "compass",

        "Compass\n"
        "X\n"
        "Y\n"
        "Z");

    /* Item IMU */
    Item_Create(
        &ui.imu,
        root,
        "IMU",
        "gyroscope",

        "Step\n"
        "Ax\n"
        "Ay\n"
        "Az\n"
        "Gx\n"
        "Gy\n"
        "Gz");

    /* Item RTC已移除 */

    /* Item Battery */
    Item_Create(
        &ui.battery,
        root,
        "Battery",
        "battery_info",

        "Usage\n"
        "Voltage\n"
        "Status");

    /* Item Storage */
    Item_Create(
        &ui.storage,
        root,
        "Storage",
        "storage",

        "Status\n"
        "Size\n"
        "Type");

    /* Item System */
    Item_Create(
        &ui.system,
        root,
        "System",
        "system_info",

        "Firmware\n"
        "Author\n"
        "LVGL");

    Group_Init();
}

void SystemInfosView::Group_Init()
{
    lv_group_t *group = lv_group_get_default();
    if (group) {
        lv_group_set_wrap(group, true);
        lv_group_set_focus_cb(group, onFocus);

        item_t *item_grp = ((item_t *)&ui);

        /* Reverse adding to group makes encoder operation more comfortable */
        for (int i = sizeof(ui) / sizeof(item_t) - 1; i >= 0; i--)
        {
            if (item_grp[i].icon != nullptr) {
                lv_group_add_obj(group, item_grp[i].icon);
            }
        }

        if (item_grp[0].icon != nullptr) {
            lv_group_focus_obj(item_grp[0].icon);
        }
    }
}

void SystemInfosView::Delete()
{
    Style_Reset();
}

void SystemInfosView::SetScrollToY(lv_obj_t *obj, lv_coord_t y, lv_anim_enable_t en)
{
    lv_coord_t scroll_y = lv_obj_get_scroll_y(obj); 
    lv_coord_t diff = -y + scroll_y;

    lv_obj_scroll_by(obj, 0, diff, en);
}

void SystemInfosView::onFocus(lv_group_t *g)
{
    if (!g) return;
    lv_obj_t *icon = lv_group_get_focused(g);
    if (!icon) return;
    lv_obj_t *cont = lv_obj_get_parent(icon);
    if (!cont) return;
    lv_coord_t y = lv_obj_get_y(cont);
    lv_obj_scroll_to_y(lv_obj_get_parent(cont), y, LV_ANIM_ON);
}

void SystemInfosView::Style_Init()
{
    // 参考 version2：icon 使用 bahnschrift_17，info/data 使用 bahnschrift_13
    lv_style_init(&style.icon);
    lv_style_set_width(&style.icon, 240);  // 图标区域宽度设置为240像素
    lv_style_set_bg_color(&style.icon, lv_color_white());  // 图标区域采用白色背景（反色后显示为黑色）
    lv_style_set_bg_opa(&style.icon, LV_OPA_COVER);
    lv_style_set_text_font(&style.icon, ResourcePool::GetFont("bahnschrift_17"));
    lv_style_set_text_color(&style.icon, lv_color_black());  // 图标文字改为黑色（反色后显示为白色）

    lv_style_init(&style.focus);
    lv_style_set_width(&style.focus, 70);
    lv_style_set_border_side(&style.focus, LV_BORDER_SIDE_RIGHT);
    lv_style_set_border_width(&style.focus, 2);
    lv_style_set_border_color(&style.focus, lv_color_hex(0xff931e));

    static const lv_style_prop_t style_prop[] =
        {
            LV_STYLE_WIDTH,
            LV_STYLE_PROP_INV};

    static lv_style_transition_dsc_t trans;
    lv_style_transition_dsc_init(
        &trans,
        style_prop,
        lv_anim_path_overshoot,
        200,
        0,
        nullptr);
    lv_style_set_transition(&style.focus, &trans);
    lv_style_set_transition(&style.icon, &trans);

    // 参考 version2：info 和 data 使用 bahnschrift_13
    lv_style_init(&style.info);
    lv_style_set_text_font(&style.info, ResourcePool::GetFont("bahnschrift_13"));
    lv_style_set_text_color(&style.info, lv_color_hex(0xFF0000));  // 信息标签红色文字

    lv_style_init(&style.data);
    lv_style_set_text_font(&style.data, ResourcePool::GetFont("bahnschrift_13"));
    lv_style_set_text_color(&style.data, lv_color_black());  // 数据标签黑色文字
}

void SystemInfosView::Style_Reset()
{
    lv_style_reset(&style.icon);
    lv_style_reset(&style.info);
    lv_style_reset(&style.data);
    lv_style_reset(&style.focus);
}

void SystemInfosView::Item_Create(
    item_t *item,
    lv_obj_t *par,
    const char *name,
    const char *img_src,
    const char *infos)
{
    lv_obj_t *cont = lv_obj_create(par);
    lv_obj_enable_style_refresh(false);
    lv_obj_remove_style_all(cont);
    // 容器宽度设置为240像素，与图标区域宽度一致
    lv_obj_set_width(cont, 240);

    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    item->cont = cont;

    /* icon */
    lv_obj_t *icon = lv_obj_create(cont);
    lv_obj_enable_style_refresh(false);
    lv_obj_remove_style_all(icon);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_style(icon, &style.icon, 0);
    lv_obj_add_style(icon, &style.focus, LV_STATE_FOCUSED);
    lv_obj_set_style_align(icon, LV_ALIGN_LEFT_MID, 0);

    // 参考 version2：使用 SPACE_AROUND 布局
    lv_obj_set_flex_flow(icon, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        icon,
        LV_FLEX_ALIGN_SPACE_AROUND,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER);

    lv_obj_t *img = lv_img_create(icon);
    lv_obj_enable_style_refresh(false);
    const void *icon_src = ResourcePool::GetImage(img_src);
    if (icon_src) {
        lv_img_set_src(img, icon_src);
        // 反色图标：设置重着色为黑色，不透明度255（完全反色）
        lv_obj_set_style_img_recolor(img, lv_color_black(), 0);
        lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    } else {
        lv_obj_del(img);
        img = lv_label_create(icon);
        lv_label_set_text(img, LV_SYMBOL_WARNING);
        lv_obj_set_style_text_color(img, lv_color_black(), 0);  // 改为黑色（反色后显示为白色）
    }

    lv_obj_t *label = lv_label_create(icon);
    lv_obj_enable_style_refresh(false);
    lv_label_set_text(label, name);
    item->icon = icon;

    /* infos */
    label = lv_label_create(cont);
    lv_obj_enable_style_refresh(false);
    lv_label_set_text(label, infos);
    lv_obj_add_style(label, &style.info, 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 75, 0);
    item->labelInfo = label;

    /* datas */
    label = lv_label_create(cont);
    lv_obj_enable_style_refresh(false);
    lv_label_set_text(label, "-");
    lv_obj_add_style(label, &style.data, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 60, 0);
    item->labelData = label;

    lv_obj_move_foreground(icon);
    lv_obj_enable_style_refresh(true);

    /* get real max height */
    lv_obj_update_layout(item->labelInfo);
    lv_coord_t height = lv_obj_get_height(item->labelInfo);
    height = LV_MAX(height, ITEM_HEIGHT_MIN);
    lv_obj_set_height(cont, height);
    lv_obj_set_height(icon, height);
}

void SystemInfosView::SetSport(
    float trip,
    const char *time,
    float maxSpd)
{
    if (ui.sport.labelData == nullptr || time == nullptr) {
        return;
    }
    
    // 安全格式化：无效数据显示 "--"
    char tripStr[32] = {0};
    const char* timeStr = (time[0] == '\0') ? "--" : time;
    char maxSpdStr[32] = {0};
    
    if (trip < 0) {
        strncpy(tripStr, "--", sizeof(tripStr) - 1);
    } else {
        snprintf(tripStr, sizeof(tripStr), "%0.2fkm", trip);
    }
    
    if (maxSpd < 0) {
        strncpy(maxSpdStr, "--", sizeof(maxSpdStr) - 1);
    } else {
        snprintf(maxSpdStr, sizeof(maxSpdStr), "%0.1fkm/h", maxSpd);
    }
    
    // 统一格式化输出
    char buf[256] = {0};
    SafeFormatText(buf, sizeof(buf), "%s\n%s\n%s", tripStr, timeStr, maxSpdStr);
    
    lv_label_set_text(ui.sport.labelData, buf);
}

void SystemInfosView::SetGPS(
    float lat,
    float lng,
    float alt,
    const char *utc,
    float course,
    float speed)
{
    if (ui.gps.labelData == nullptr || utc == nullptr) {
        return;
    }
    
    // 安全格式化：无效数据显示 "--"
    const char* utcStr = (utc[0] == '\0') ? "--" : utc;
    char latStr[32] = {0}, lngStr[32] = {0}, altStr[32] = {0};
    char courseStr[32] = {0}, speedStr[32] = {0};
    
    // 格式化每个字段
    if (lat < -900) {
        strncpy(latStr, "--", sizeof(latStr) - 1);
    } else {
        snprintf(latStr, sizeof(latStr), "%0.6f", lat);
    }
    
    if (lng < -900) {
        strncpy(lngStr, "--", sizeof(lngStr) - 1);
    } else {
        snprintf(lngStr, sizeof(lngStr), "%0.6f", lng);
    }
    
    if (alt < -900) {
        strncpy(altStr, "--", sizeof(altStr) - 1);
    } else {
        snprintf(altStr, sizeof(altStr), "%0.2fm", alt);
    }
    
    if (course < 0) {
        strncpy(courseStr, "--", sizeof(courseStr) - 1);
    } else {
        snprintf(courseStr, sizeof(courseStr), "%0.1f deg", course);
    }
    
    if (speed < 0) {
        strncpy(speedStr, "--", sizeof(speedStr) - 1);
    } else {
        snprintf(speedStr, sizeof(speedStr), "%0.1fkm/h", speed);
    }
    
    // 统一格式化输出
    char buf[256] = {0};
    SafeFormatText(buf, sizeof(buf), "%s\n%s\n%s\n%s\n%s\n%s",
        latStr, lngStr, altStr, utcStr, courseStr, speedStr);
    
    lv_label_set_text(ui.gps.labelData, buf);
}

void SystemInfosView::SetMAG(
    float dir,
    int x,
    int y,
    int z)
{
    if (ui.mag.labelData == nullptr) {
        return;
    }
    
    // 安全格式化：无效数据显示 "--"
    char dirStr[32] = {0}, xStr[32] = {0}, yStr[32] = {0}, zStr[32] = {0};
    
    if (dir < 0) {
        strncpy(dirStr, "--", sizeof(dirStr) - 1);
    } else {
        snprintf(dirStr, sizeof(dirStr), "%0.1f deg", dir);
    }
    
    if (x == -999) {
        strncpy(xStr, "--", sizeof(xStr) - 1);
    } else {
        snprintf(xStr, sizeof(xStr), "%d", x);
    }
    
    if (y == -999) {
        strncpy(yStr, "--", sizeof(yStr) - 1);
    } else {
        snprintf(yStr, sizeof(yStr), "%d", y);
    }
    
    if (z == -999) {
        strncpy(zStr, "--", sizeof(zStr) - 1);
    } else {
        snprintf(zStr, sizeof(zStr), "%d", z);
    }
    
    // 统一格式化输出
    char buf[256] = {0};
    SafeFormatText(buf, sizeof(buf), "%s\n%s\n%s\n%s", dirStr, xStr, yStr, zStr);
    
    lv_label_set_text(ui.mag.labelData, buf);
}

void SystemInfosView::SetIMU(
    int step,
    const char *info)
{
    if (ui.imu.labelData == nullptr || info == nullptr) {
        return;
    }
    
    // 安全格式化：无效数据显示 "--"
    char stepStr[32] = {0};
    const char* infoStr = (info[0] == '\0') ? "--" : info;
    
    if (step < 0) {
        strncpy(stepStr, "--", sizeof(stepStr) - 1);
    } else {
        snprintf(stepStr, sizeof(stepStr), "%d", step);
    }
    
    // 统一格式化输出
    char buf[256] = {0};
    SafeFormatText(buf, sizeof(buf), "%s\n%s", stepStr, infoStr);
    
    lv_label_set_text(ui.imu.labelData, buf);
}

// SetRTC已移除

void SystemInfosView::SetBattery(
    int usage,
    float voltage,
    const char *state)
{
    if (ui.battery.labelData == nullptr || state == nullptr) {
        return;
    }
    
    // 安全格式化：无效数据显示 "--"
    char usageStr[32] = {0}, voltageStr[32] = {0};
    const char* stateStr = (state[0] == '\0') ? "--" : state;
    
    if (usage < 0) {
        strncpy(usageStr, "--", sizeof(usageStr) - 1);
    } else {
        snprintf(usageStr, sizeof(usageStr), "%d%%", usage);
    }
    
    if (voltage < 0) {
        strncpy(voltageStr, "--", sizeof(voltageStr) - 1);
    } else {
        snprintf(voltageStr, sizeof(voltageStr), "%0.2fV", voltage);
    }
    
    // 统一格式化输出
    char buf[256] = {0};
    SafeFormatText(buf, sizeof(buf), "%s\n%s\n%s", usageStr, voltageStr, stateStr);
    
    lv_label_set_text(ui.battery.labelData, buf);
}

void SystemInfosView::SetStorage(
    const char *detect,
    const char *size,
    const char *type,
    const char *version)
{
    if (ui.storage.labelData == nullptr) {
        return;
    }
    
    // 安全格式化：空指针显示 "--"
    const char* detectStr = (detect != nullptr) ? detect : "--";
    const char* sizeStr = (size != nullptr) ? size : "--";
    const char* typeStr = (type != nullptr) ? type : "--";
    
    // 统一格式化输出
    char buf[128] = {0};
    SafeFormatText(buf, sizeof(buf), "%s\n%s\n%s", detectStr, sizeStr, typeStr);
    
    lv_label_set_text(ui.storage.labelData, buf);
    // 移除version参数显示
}

void SystemInfosView::SetSystem(
    const char *firmVer,
    const char *authorName,
    const char *lvglVer)
{
    if (ui.system.labelData == nullptr) {
        return;
    }
    
    // 安全格式化：空指针显示 "--"
    const char* firmVerStr = (firmVer != nullptr) ? firmVer : "--";
    const char* authorNameStr = (authorName != nullptr) ? authorName : "--";
    const char* lvglVerStr = (lvglVer != nullptr) ? lvglVer : "--";
    
    // 统一格式化输出
    char buf[128] = {0};
    SafeFormatText(buf, sizeof(buf), "%s\n%s\n%s", firmVerStr, authorNameStr, lvglVerStr);
    
    lv_label_set_text(ui.system.labelData, buf);
}

#pragma GCC diagnostic pop
