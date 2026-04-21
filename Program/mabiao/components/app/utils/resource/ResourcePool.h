#ifndef __RESOURCE_POOL
#define __RESOURCE_POOL

#include "lvgl.h"

namespace ResourcePool
{

void Init();
lv_font_t* GetFont(const char* name);
const void* GetImage(const char* name);

}

// C接口（供C文件使用）
#ifdef __cplusplus
extern "C" {
#endif
lv_font_t* ResourcePool_GetFont(const char* name);
#ifdef __cplusplus
}
#endif

#endif
