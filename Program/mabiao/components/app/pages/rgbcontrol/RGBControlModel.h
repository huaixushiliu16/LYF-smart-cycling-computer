#ifndef __RGB_CONTROL_MODEL_H
#define __RGB_CONTROL_MODEL_H

#include "dataproc.h"
#include "dataproc_def.h"
#include "Account.h"

namespace Page
{

class RGBControlModel
{
public:
    void Init();
    void Deinit();

    void GetRGBInfo(RGB_Info_t *info);
    void SetMode(RGB_Mode_t mode);
    void SetBrightness(uint8_t brightness);
    void SetSpeed(uint8_t speed);
    void SetEnabled(bool enabled);
    void SetSolidColor(const RGB_Color_t *color);
    
    void GetHeartRate(uint16_t *heart_rate_bpm);
    
    void SetStatusBarStyle(StatusBar_Style_t style);
    void SetStatusBarAppear(bool en);

private:
    Account *rgb_account;
    Account *sport_account;
};

}

#endif
