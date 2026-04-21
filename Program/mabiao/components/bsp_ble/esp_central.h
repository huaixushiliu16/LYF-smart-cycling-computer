/**
 * @file esp_central.h
 * @brief BLE Central工具函数（从ESP-IDF示例中提取）
 * @note 这些函数需要从ESP-IDF示例中复制peer.c实现
 */

#ifndef H_ESP_CENTRAL_
#define H_ESP_CENTRAL_

// 包含必要的NimBLE头文件
// 这些头文件通过 bt 组件的包含路径提供
// 当组件声明了 REQUIRES bt 时，这些路径应该可用
#include "host/ble_hs.h"  // 包含GATT client函数 (ble_gattc_*)
#include "host/ble_uuid.h"  // UUID相关函数和类型定义
#include "host/ble_gap.h"
// SLIST 宏定义在 sys/queue.h 中（BSD queue macros）
#include <sys/queue.h>

#ifdef __cplusplus
extern "C" {
#endif

// Peer结构体定义
struct peer_dsc {
    SLIST_ENTRY(peer_dsc) next;
    struct ble_gatt_dsc dsc;
};
SLIST_HEAD(peer_dsc_list, peer_dsc);

struct peer_chr {
    SLIST_ENTRY(peer_chr) next;
    struct ble_gatt_chr chr;
    struct peer_dsc_list dscs;
};
SLIST_HEAD(peer_chr_list, peer_chr);

struct peer_svc {
    SLIST_ENTRY(peer_svc) next;
    struct ble_gatt_svc svc;
    struct peer_chr_list chrs;
};
SLIST_HEAD(peer_svc_list, peer_svc);

struct peer;
typedef void peer_disc_fn(const struct peer *peer, int status, void *arg);

/**
 * @brief The callback function for the devices traversal.
 */
typedef int peer_traverse_fn(const struct peer *peer, void *arg);

struct peer {
    SLIST_ENTRY(peer) next;
    uint16_t conn_handle;
    uint8_t peer_addr[6];
    struct peer_svc_list svcs;
    uint16_t disc_prev_chr_val;
    struct peer_svc *cur_svc;
    peer_disc_fn *disc_cb;
    void *disc_cb_arg;
};

// Peer管理函数声明
int peer_init(int max_peers, int max_svcs, int max_chrs, int max_dscs);
int peer_add(uint16_t conn_handle);
struct peer *peer_find(uint16_t conn_handle);
int peer_delete(uint16_t conn_handle);
int peer_disc_svc_by_uuid(uint16_t conn_handle, const ble_uuid_t *uuid, 
                          peer_disc_fn *disc_cb, void *disc_cb_arg);
int peer_disc_all(uint16_t conn_handle, peer_disc_fn *disc_cb, void *disc_cb_arg);
const struct peer_dsc *peer_dsc_find_uuid(const struct peer *peer,
                                          const ble_uuid_t *svc_uuid,
                                          const ble_uuid_t *chr_uuid,
                                          const ble_uuid_t *dsc_uuid);
const struct peer_chr *peer_chr_find_uuid(const struct peer *peer,
                                          const ble_uuid_t *svc_uuid,
                                          const ble_uuid_t *chr_uuid);
const struct peer_svc *peer_svc_find_uuid(const struct peer *peer,
                                          const ble_uuid_t *uuid);
void peer_traverse_all(peer_traverse_fn *trav_cb, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* H_ESP_CENTRAL_ */
