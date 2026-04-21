#ifndef __DIALPLATE_VIEW_H
#define __DIALPLATE_VIEW_H

#include "PageBase.h"
#include "ResourcePool.h"

namespace Page
{

class DialplateView
{

public:
    typedef struct
    {
        lv_obj_t* cont;
        lv_obj_t* lableValue;
        lv_obj_t* lableUnit;
    } SubInfo_t;

public:
    struct
    {
        struct
        {
            lv_obj_t* cont;
            lv_obj_t* labelSpeed;
            lv_obj_t* labelUint;
            SubInfo_t avgInfo;
            SubInfo_t tripInfo;
        } topInfo;

        struct
        {
            lv_obj_t* cont;
            SubInfo_t labelInfoGrp[6];  // 4个原有 + 2个新增（心率和踏频）
        } bottomInfo;

        struct
        {
            lv_obj_t* cont;
            lv_obj_t* btnMap;
            lv_obj_t* btnRec;
            lv_obj_t* btnMenu;
        } btnCont;

        lv_anim_timeline_t* anim_timeline;
    } ui;

    void Create(lv_obj_t* root);
    void Delete();
    void AppearAnimStart(bool reverse = false);

private:
    void TopInfo_Create(lv_obj_t* par);
    void BottomInfo_Create(lv_obj_t* par);
    void SubInfoGrp_Create(lv_obj_t* par, SubInfo_t* info, const char* unitText);
    void BtnCont_Create(lv_obj_t* par);
    lv_obj_t* Btn_Create(lv_obj_t* par, const void* img_src, lv_coord_t x_ofs);
};

}

#endif // !__DIALPLATE_VIEW_H
