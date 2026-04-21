#include "AppFactory.h"
#include "WaveTable.h"
#include "SystemInfos.h"
#include "Dialplate.h"
#include "LiveMap.h"
#include "BleDevices.h"
#include "speedsensor/SpeedSensor.h"
#include "heartrate/HeartRate.h"
#include "compass/Compass.h"
#include "rgbcontrol/RGBControl.h"
#include <string.h>

PageBase* AppFactory::CreatePage(const char* name)
{
    // 核心页面注册（与 app.cpp 中的 Install 调用保持一致）
    if (strcmp(name, "WaveTable") == 0) {
        return new Page::WaveTable();
    }
    else if (strcmp(name, "SystemInfos") == 0) {
        return new Page::SystemInfos();
    }
    else if (strcmp(name, "Dialplate") == 0) {
        return new Page::Dialplate();
    }
    else if (strcmp(name, "LiveMap") == 0) {
        return new Page::LiveMap();
    }
    else if (strcmp(name, "BleDevices") == 0) {
        return new Page::BleDevices();
    }
    else if (strcmp(name, "SpeedSensor") == 0) {
        return new Page::SpeedSensor();
    }
    else if (strcmp(name, "HeartRate") == 0) {
        return new Page::HeartRate();
    }
    else if (strcmp(name, "Compass") == 0) {
        return new Page::Compass();
    }
    else if (strcmp(name, "RGBControl") == 0) {
        return new Page::RGBControl();
    }
    // 后续扩展页面时，统一使用此命名风格
    
    return nullptr;
}
