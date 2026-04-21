/**
 * @file app.cpp
 * @brief App层主实现
 * @note 阶段8：数据融合和UI界面开发
 */

#include "app.h"
#include "esp_log.h"
#include "lvgl.h"
#include "PageManager.h"
#include "AppFactory.h"
#include "ResourcePool.h"
#include "StatusBar.h"
// SystemMonitor（右下角 FPS/CPU 调试显示）已禁用，不再创建；
// 保留头文件是为了以后调试可以快速重启用。
// #include "SystemMonitor.h"
#include <stdbool.h>

static const char *TAG = "APP";
static bool s_initialized = false;
static AppFactory* s_factory = nullptr;
static PageManager* s_page_manager = nullptr;

esp_err_t App_Init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "App already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing App layer...");
    
    // 创建AppFactory
    s_factory = new AppFactory();
    
    // 创建PageManager
    s_page_manager = new PageManager(s_factory);
    
    // 设置全局动画类型
    s_page_manager->SetGlobalLoadAnimType(PageManager::LOAD_ANIM_OVER_TOP);
    
    // 设置根默认样式
    static lv_style_t root_style;
    lv_style_init(&root_style);
    lv_style_set_width(&root_style, LV_HOR_RES);
    lv_style_set_height(&root_style, LV_VER_RES);
    lv_style_set_bg_opa(&root_style, LV_OPA_COVER);
    lv_style_set_bg_color(&root_style, lv_color_white());
    s_page_manager->SetRootDefaultStyle(&root_style);
    
    // 初始化ResourcePool
    ResourcePool::Init();
    
    // 创建StatusBar
    Page::StatusBar_Create(lv_layer_top());

    // SystemMonitor（右下角 FPS/CPU 显示）已禁用，避免遮挡 Dialplate 的 HR/Cadence 栏
    // 如需重新启用用于性能调试，解开 app.cpp 顶部的 #include 和下面一行即可。
    // SystemMonitor_Create();

    // 安装核心页面（保持核心页面集不扩范围）
    // 命名风格：使用简洁的页面名称，与 AppFactory 保持一致
    s_page_manager->Install("WaveTable", "WaveTable");
    s_page_manager->Install("SystemInfos", "SystemInfos");
    s_page_manager->Install("Dialplate", "Dialplate");
    s_page_manager->Install("LiveMap", "LiveMap");
    s_page_manager->Install("BleDevices", "BleDevices");
    s_page_manager->Install("SpeedSensor", "SpeedSensor");
    s_page_manager->Install("HeartRate", "HeartRate");
    s_page_manager->Install("Compass", "Compass");
    s_page_manager->Install("RGBControl", "RGBControl");
    // 后续扩展页面时，统一使用此命名风格
    
    // 推送WaveTable作为首页
    s_page_manager->Push("WaveTable");
    
    s_initialized = true;
    ESP_LOGI(TAG, "App layer initialized");
    return ESP_OK;
}

esp_err_t App_Uninit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Uninitializing App layer...");
    
    if (s_page_manager) {
        delete s_page_manager;
        s_page_manager = nullptr;
    }
    
    if (s_factory) {
        delete s_factory;
        s_factory = nullptr;
    }
    
    s_initialized = false;
    ESP_LOGI(TAG, "App layer uninitialized");
    return ESP_OK;
}
