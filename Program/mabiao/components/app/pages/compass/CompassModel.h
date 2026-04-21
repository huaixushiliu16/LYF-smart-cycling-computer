#ifndef __COMPASS_MODEL_H
#define __COMPASS_MODEL_H

#include "dataproc.h"
#include "dataproc_def.h"
#include "Account.h"

namespace Page
{

class CompassModel
{
public:
    void Init();
    void Deinit();

    void GetMAGInfo(
        int *dir,
        int *x,
        int *y,
        int *z,
        bool *isCalibrated);

    // 暂时移除校准功能，留空实现
    void SetMAGCalibration(void);

    void SetStatusBarStyle(StatusBar_Style_t style);
    void SetStatusBarAppear(bool en);

private:
    Account *account;

private:
    static int onEvent(Account *account, Account::EventParam_t *param);
};

}

#endif
