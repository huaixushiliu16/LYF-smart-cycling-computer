#ifndef __APP_FACTORY_H
#define __APP_FACTORY_H

#include "PageFactory.h"

// PageBase在PageFactory.h中已包含

class AppFactory : public PageFactory
{
public:
    virtual ~AppFactory() {}
    virtual PageBase* CreatePage(const char* name);
};

#endif
