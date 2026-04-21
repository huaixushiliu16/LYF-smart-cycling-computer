#include "ResourcePool.h"
#include "ResourceManager.h"
#include "esp_log.h"

static ResourceManager Font_;
static ResourceManager Image_;

extern "C"
{
#define IMPORT_FONT(name)                               \
    do                                                  \
    {                                                   \
        LV_FONT_DECLARE(font_##name)                    \
        Font_.AddResource(#name, (void *)&font_##name); \
    } while (0)

#define IMPORT_IMG(name)                                    \
    do                                                      \
    {                                                       \
        LV_IMG_DECLARE(img_src_##name)                      \
        Image_.AddResource(#name, (void *)&img_src_##name); \
    } while (0)

    // Declare copyFontFromSD2SPIRAM function (for smileysd_16 font)
    extern void copyFontFromSD2SPIRAM(void);
    
    static void Resource_Init()
    {
        // Initialize smileysd_16 font (load from SD card to SPIRAM)
        // Note: Ensure SD card is mounted and /font_smileysd_16.bin file exists
        // If SD card is not mounted, font will use default behavior (may display abnormally)
        copyFontFromSD2SPIRAM();
        
        // Import all font resources
        IMPORT_FONT(bahnschrift_13);
        IMPORT_FONT(bahnschrift_17);
        IMPORT_FONT(bahnschrift_32);
        IMPORT_FONT(bahnschrift_65);
        IMPORT_FONT(agencyb_36);
        IMPORT_FONT(smiley_36);
        IMPORT_FONT(compass_24);
        IMPORT_FONT(smileysd_16);
        
        // 导入所有图像资源
        IMPORT_IMG(alarm);
        IMPORT_IMG(battery);
        IMPORT_IMG(battery_info);
        IMPORT_IMG(bicycle);
        IMPORT_IMG(compass);
        IMPORT_IMG(compass_bg);
        IMPORT_IMG(gps_arrow_default);
        IMPORT_IMG(gps_arrow_dark);
        IMPORT_IMG(gps_arrow_light);
        IMPORT_IMG(gps_pin);
        IMPORT_IMG(gyroscope);
        IMPORT_IMG(locate);
        IMPORT_IMG(map_location);
        IMPORT_IMG(menu);
        IMPORT_IMG(origin_point);
        IMPORT_IMG(pause);
        IMPORT_IMG(popcat_close);
        IMPORT_IMG(popcat_open);
        IMPORT_IMG(satellite);
        IMPORT_IMG(sd_card);
        IMPORT_IMG(start);
        IMPORT_IMG(stop);
        IMPORT_IMG(storage);
        IMPORT_IMG(system_info);
        IMPORT_IMG(time_info);
        IMPORT_IMG(trip);
        
        ESP_LOGI("ResourcePool", "Fonts and images imported successfully");
    }

} /* extern "C" */

void ResourcePool::Init()
{
    Resource_Init();
    Font_.SetDefault((void *)LV_FONT_DEFAULT);
}

lv_font_t *ResourcePool::GetFont(const char *name)
{
    return (lv_font_t *)Font_.GetResource(name);
}

const void *ResourcePool::GetImage(const char *name)
{
    return Image_.GetResource(name);
}

// C接口包装（供C文件使用）
extern "C" {
lv_font_t* ResourcePool_GetFont(const char* name)
{
    return ResourcePool::GetFont(name);
}
}
