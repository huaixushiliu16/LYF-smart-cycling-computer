#ifndef __RGB_CONTROL_VIEW_H
#define __RGB_CONTROL_VIEW_H

#include "PageBase.h"
#include "ResourcePool.h"

namespace Page
{

class RGBControlView
{
public:
    struct
    {
        struct
        {
            lv_obj_t *cont;
            lv_obj_t *title;
        } header;

        lv_obj_t *sw;
        lv_obj_t *label_status;

        lv_anim_timeline_t *anim_timeline;
    } ui;
    
    void Create(lv_obj_t *root);
    void Delete();
    void AppearAnimStart(bool reverse = false);
    
private:
    void Header_Create(lv_obj_t *par);
    void Switch_Create(lv_obj_t *par);
    void Status_Create(lv_obj_t *par);
};

}

#endif
