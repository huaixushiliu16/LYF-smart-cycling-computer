/**
 * @file Account.h
 * @brief Account类公开接口（供App层使用）
 * @note 此文件是utils/datacenter/Account.h的副本，用于暴露给依赖dataproc组件的其他组件
 */

#ifndef __DATAPROC_ACCOUNT_H
#define __DATAPROC_ACCOUNT_H

#include <stdint.h>
#include <vector>
#include "PingPongBuffer/PingPongBuffer.h"
#include "lvgl.h"

#define nullptr NULL

class DataCenter;

class Account
{
public:

    /* Event type enumeration */
    typedef enum
    {
        EVENT_NONE,
        EVENT_PUB_PUBLISH, // Publisher posted information
        EVENT_SUB_PULL,    // Subscriber data pull request
        EVENT_NOTIFY,      // Subscribers send notifications to publishers
        EVENT_TIMER,       // Timed event
        _EVENT_LAST
    } EventCode_t;

    /* Error type enumeration */
    typedef enum
    {
        RES_OK                  =  0,
        RES_UNKNOW              = -1,
        RES_SIZE_MISMATCH       = -2,
        RES_UNSUPPORTED_REQUEST = -3,
        RES_NO_CALLBACK         = -4,
        RES_NO_CACHE            = -5,
        RES_NO_COMMITED         = -6,
        RES_NOT_FOUND           = -7,
        RES_PARAM_ERROR         = -8
    } ResCode_t;

    /* Event parameter structure */
    typedef struct
    {
        EventCode_t event; // Event type
        Account* tran;     // Pointer to sender
        Account* recv;     // Pointer to receiver
        void* data_p;      // Pointer to data
        uint32_t size;     // The length of the data
    } EventParam_t;

    /* Event callback function pointer */
    typedef int(*EventCallback_t)(Account* account, EventParam_t* param);

    typedef std::vector<Account*> AccountVector_t;

public:
    Account(
        const char* id,
        DataCenter* center,
        uint32_t bufSize = 0,
        void* userData = nullptr
    );
    ~Account();

    Account* Subscribe(const char* pubID);
    bool Unsubscribe(const char* pubID);
    bool Commit(const void* data_p, uint32_t size);
    int Publish();
    int Pull(const char* pubID, void* data_p, uint32_t size);
    int Pull(Account* pub, void* data_p, uint32_t size);
    int Notify(const char* pubID, const void* data_p, uint32_t size);
    int Notify(Account* pub, const void* data_p, uint32_t size);
    void SetEventCallback(EventCallback_t callback);
    void SetTimerPeriod(uint32_t period);
    void SetTimerEnable(bool en);
    size_t GetPublishersSize();
    size_t GetSubscribersSize();

public:
    const char* ID;      /* Unique account ID */
    DataCenter* Center;  /* Pointer to the data center */
    void* UserData;

    AccountVector_t publishers;  /* Followed publishers */
    AccountVector_t subscribers; /* Followed subscribers */

    struct
    {
        EventCallback_t eventCallback;
        lv_timer_t* timer;
        PingPongBuffer_t BufferManager;
        uint32_t BufferSize;
    } priv;

private:
    static void TimerCallbackHandler(lv_timer_t* task);
};

#endif /* __DATAPROC_ACCOUNT_H */
