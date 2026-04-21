#ifndef __WAVETABLE_MODEL_H
#define __WAVETABLE_MODEL_H

#include "dataproc.h"

// 前向声明
class Account;

namespace Page
{

class WaveTableModel
{
public:
    void Init();
    void Deinit();

    void SetStatusBarStyle(int style);
    void SetStatusBarAppear(bool en);

private:
    Account *account;
};

}

#endif
