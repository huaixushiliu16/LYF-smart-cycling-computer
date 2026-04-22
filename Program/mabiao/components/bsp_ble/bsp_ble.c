/**
 * @file bsp_ble.c
 * @brief BSP?BLE???????NimBLE??
 * @note ??4?BLE Client?????
 */

#include "bsp_ble.h"
#include "bsp_buzzer.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"  // ??GATT client?? (ble_gattc_*)
#include "host/ble_uuid.h"  // ???????????ble_uuid_u16??
#include "host/ble_gap.h"
#include "esp_central.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
// ble_store functions are provided by ESP-IDF bt component, no header needed
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ????????????ESP-IDF bt??????
// ble_store functions are provided by ESP-IDF bt component
void ble_store_config_init(void);
int ble_store_util_status_rr(struct ble_store_status_event *event, void *arg);

static const char *TAG = "BSP_BLE";

// BLE??UUID?????CSCS??????????
// ????
#define BLE_UUID_HR_SERVICE           0x180D  // Heart Rate Service
#define BLE_UUID_HR_MEASUREMENT_CHAR   0x2A37  // Heart Rate Measurement

// CSCS???Cycling Speed and Cadence Service??
#define BLE_UUID_CSCS_SERVICE         0x1816  // Cycling Speed and Cadence Service
#define BLE_UUID_CSC_MEASUREMENT_CHAR 0x2A5B  // CSC Measurement
#define BLE_UUID_CSC_FEATURE_CHAR     0x2A5C  // CSC Feature
#define BLE_UUID_CSC_SENSOR_LOCATION_CHAR 0x2A5D  // Sensor Location????????
#define BLE_UUID_CSC_CONTROL_POINT_CHAR 0x2A55    // SC Control Point??????

// ---- BLE connection / discovery capacity ----
#define MAX_BLE_DEVICES 3
#define MAX_SCANNED_DEVICES 20

// IMPORTANT: peer_init(max_peers, max_svcs, max_chrs, max_dscs) configures
// GLOBAL shared mempools in peer.c, NOT per-peer quotas. The totals below must
// be large enough to hold the *sum* of discovered svcs/chrs/dscs across ALL
// currently connected peers; otherwise service discovery of the 2nd/3rd peer
// fails with BLE_HS_ENOMEM (=6) and NimBLE auto-terminates that link.
//
// Sizing rationale for MAX_BLE_DEVICES=3:
//   - A typical CSCS sensor (e.g. XOSS ARENA) reports ~6 svcs, ~17 chrs, ~5 dscs.
//   - A typical HR strap (Polar/Decathlon) reports ~5-8 svcs, ~15-25 chrs, ~5-10 dscs.
//   - Keep ~30% headroom for peripherals with extra proprietary/DFU services.
#define MAX_PEER_SERVICES 32   // total across all peers (~10 per peer budget)
#define MAX_PEER_CHARS    96   // total across all peers (~32 per peer budget)
#define MAX_PEER_DESCS    96   // total across all peers (~32 per peer budget)

// ??????
typedef struct {
    uint16_t conn_handle;
    bsp_ble_device_type_t device_type;
    bsp_ble_cscs_mode_t cscs_mode;    // CSCS???????CSCS??????
    bool is_connected;
    uint8_t addr[6];
    uint8_t addr_type;
    bool csc_feature_read;             // ?????CSC Feature
    uint16_t csc_feature_value;        // CSC Feature??2????
    uint16_t sensor_location_handle;  // Sensor Location 值 handle（可读，不写）
    uint16_t control_point_handle;    // SC Control Point 值 handle（Write With Response）
    uint16_t control_point_cccd_handle; // SC Control Point CCCD handle（用于订阅 Indicate）
    bool     sc_cp_indicate_enabled;  // 是否已订阅 SC CP 的 Indicate
    uint8_t  pending_sc_cp_op;        // 最近一次通过 SC CP 发出的 Op Code（仅用于 Request Supported Locations 查询）
    // 本设备通过 Request Supported Sensor Locations 查询回来的支持位置列表
    // （仅作为诊断信息记录到日志；不再用于驱动 Update Sensor Location 切换，
    // 因为 XOSS 等设备对该命令的 Success 响应是假的）
    uint8_t  supported_locations[16];
    uint8_t  supported_locations_count;
    bool     supported_locations_known; // 是否已经收到支持列表响应
    bool     mode_manually_set;       // 用户是否主动选过显示偏好（保护 flags 自动判定不反向覆盖）
} device_connection_t;

// ????????
typedef struct {
    bsp_ble_device_info_t devices[MAX_SCANNED_DEVICES];
    uint8_t count;
    SemaphoreHandle_t mutex;
} scanned_devices_t;

// ???????
static bool s_ble_initialized = false;
static bool s_scanning = false;
static device_connection_t s_device_connections[MAX_BLE_DEVICES];
static scanned_devices_t s_scanned_devices;
static uint16_t s_current_heart_rate = 0;
static float s_current_speed = 0.0f;
static float s_current_cadence = 0.0f;
static float s_wheel_circumference = 2.096f;  // ??????????00C????

// CSCS?????????????????
static uint32_t s_last_wheel_revs = 0;
static uint16_t s_last_wheel_time = 0;
static uint16_t s_last_crank_revs = 0;
static uint16_t s_last_crank_time = 0;

// ---- BLE diagnostic helpers (for connected-but-no-data debug) ----
static uint32_t s_cscs_subscribe_tick = 0;
static bool     s_cscs_waiting_data   = false;
static uint16_t s_cscs_conn_handle    = 0;
static uint32_t s_cscs_notify_count   = 0;

// Idle-zero helpers. CSCS sensors keep re-sending the last snapshot at ~1Hz
// even when the wheel/crank is still; without this we would display the last
// non-zero speed/cadence forever. Stamp the tick when we actually observe a
// new revolution, then zero-out in bsp_ble_update() after a quiet window.
#define BSP_BLE_IDLE_ZERO_MS 3000
static uint32_t s_last_wheel_motion_tick = 0;
static uint32_t s_last_crank_motion_tick = 0;

// 关于 XOSS ARENA S1610 等"单感"传感器的硬件模式切换：
// 实测它对标准 SC Control Point Op=0x03 (Update Sensor Location) 会回
// "Success" Indication，但内部状态机并不真正切换——厂家手机 APP 用的是
// 另一条私有 NUS 通道 (6e400001-…) 来驱动真实的模式切换。
// 因此本驱动不再尝试通过 SC CP 做硬件层切换；bsp_ble_set_cscs_mode 只更改
// 本地"显示偏好"，CSC Measurement 里的 flags byte 才是真相，wheel / crank
// 两路数据一直独立解析更新。用户若需要切换传感器硬件模式，请用原厂 APP。

static const char *ble_uuid_to_str_safe(const ble_uuid_t *uuid, char *dst, size_t dst_len)
{
    if (uuid == NULL || dst == NULL || dst_len < BLE_UUID_STR_LEN) {
        if (dst && dst_len > 0) dst[0] = 0;
        return dst ? dst : "";
    }
    return ble_uuid_to_str(uuid, dst);
}

static void log_hex_dump(const char *prefix, const uint8_t *data, int len)
{
    if (data == NULL || len <= 0) return;
    char buf[64 * 3 + 8];
    int max = (len > 64) ? 64 : len;
    int pos = 0;
    for (int i = 0; i < max; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%02X ", data[i]);
        if (pos >= (int)sizeof(buf) - 4) break;
    }
    if (pos > 0 && buf[pos - 1] == ' ') buf[pos - 1] = 0;
    // Use ESP_LOGD: hex dumps are debug-only. Bump log level via menuconfig
    // (Component config -> Log output -> Default log verbosity = Debug) if you
    // want to see the raw bytes again. Keeps steady-state output quiet.
    ESP_LOGD(TAG, "%s len=%d | %s%s",
             prefix, len, buf, (len > max) ? " ...(truncated)" : "");
}

// forward decl: discovery kickoff after MTU negotiation
static void bsp_ble_start_service_discovery(uint16_t conn_handle);

/**
 * MTU exchange callback. Some sensors require an MTU exchange handshake before
 * they will respond to subsequent GATT operations; we do this early after connect.
 *
 * 重要：我们把"启动服务发现"从 CONNECT 事件挪到这里来，原因是对 XOSS
 * ARENA S1610 这类传感器，如果 MTU 交换还没完成就并行发起 discover all svcs，
 * 对端会在发现到一半直接 terminate（hci_reason=19）——必须等 MTU 交换结束
 * 再开始服务发现，链路才稳。
 */
static int bsp_ble_mtu_exchange_cb(uint16_t conn_handle,
                                   const struct ble_gatt_error *error,
                                   uint16_t mtu, void *arg)
{
    (void)arg;
    if (error == NULL || error->status == 0) {
        ESP_LOGI(TAG, "MTU exchanged OK: conn_handle=%u MTU=%u",
                 (unsigned)conn_handle, (unsigned)mtu);
    } else {
        ESP_LOGW(TAG, "MTU exchange failed: conn_handle=%u status=%d (continuing with default MTU)",
                 (unsigned)conn_handle, error != NULL ? error->status : -1);
    }

    // MTU 阶段结束，现在启动服务发现。注意：本回调在 NimBLE host task 上下文，
    // 不能长时间 vTaskDelay，因此这里只做一次"登记"，真正的 peer_disc_all
    // 由 bsp_ble_start_service_discovery 发起。
    bsp_ble_start_service_discovery(conn_handle);
    return 0;
}

/**
 * @brief ????????
 */
static device_connection_t *find_device_connection(uint16_t conn_handle)
{
    for (int i = 0; i < MAX_BLE_DEVICES; i++) {
        if (s_device_connections[i].conn_handle == conn_handle) {
            return &s_device_connections[i];
        }
    }
    return NULL;
}

/**
 * @brief ??????????
 */
static device_connection_t *find_free_device_slot(void)
{
    for (int i = 0; i < MAX_BLE_DEVICES; i++) {
        if (!s_device_connections[i].is_connected) {
            return &s_device_connections[i];
        }
    }
    return NULL;
}

/**
 * @brief ??????????
 */
static device_connection_t *find_device_by_type(bsp_ble_device_type_t type)
{
    for (int i = 0; i < MAX_BLE_DEVICES; i++) {
        if (s_device_connections[i].is_connected && 
            s_device_connections[i].device_type == type) {
            return &s_device_connections[i];
        }
    }
    return NULL;
}

/**
 * @brief ???????????????UUID
 */
static bool check_target_service(struct ble_hs_adv_fields *fields)
{
    // ??????UUID??x180D
    for (int i = 0; i < fields->num_uuids16; i++) {
        if (ble_uuid_u16(&fields->uuids16[i].u) == BLE_UUID_HR_SERVICE) {
            return true;
        }
    }

    // ??CSCS??UUID??x1816
    for (int i = 0; i < fields->num_uuids16; i++) {
        if (ble_uuid_u16(&fields->uuids16[i].u) == BLE_UUID_CSCS_SERVICE) {
            return true;
        }
    }

    return false;
}

/**
 * @brief ??????
 */
static bsp_ble_device_type_t identify_device_type(struct ble_hs_adv_fields *fields)
{
    // ???????
    for (int i = 0; i < fields->num_uuids16; i++) {
        if (ble_uuid_u16(&fields->uuids16[i].u) == BLE_UUID_HR_SERVICE) {
            return BSP_BLE_DEVICE_TYPE_HR;
        }
    }

    // ??CSCS??
    for (int i = 0; i < fields->num_uuids16; i++) {
        if (ble_uuid_u16(&fields->uuids16[i].u) == BLE_UUID_CSCS_SERVICE) {
            // CSCS???????????????CSC Feature????????
            return BSP_BLE_DEVICE_TYPE_CSCS;
        }
    }

    return BSP_BLE_DEVICE_TYPE_UNKNOWN;
}

/**
 * @brief ????????
 * @note ??????????????
 */
static void save_scanned_device(const struct ble_gap_disc_desc *disc,
                                struct ble_hs_adv_fields *fields)
{
    if (xSemaphoreTake(s_scanned_devices.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }

    // ?????????????????????????????????
    bool is_already_connected = false;
    for (int i = 0; i < MAX_BLE_DEVICES; i++) {
        if (s_device_connections[i].is_connected &&
            memcmp(s_device_connections[i].addr, disc->addr.val, 6) == 0) {
            is_already_connected = true;
            break;
        }
    }

    // ???????????????MAC????
    for (int i = 0; i < s_scanned_devices.count; i++) {
        if (memcmp(s_scanned_devices.devices[i].addr, disc->addr.val, 6) == 0) {
            // ???????????RSSI??????
            s_scanned_devices.devices[i].rssi = disc->rssi;
            s_scanned_devices.devices[i].is_connected = is_already_connected;
            xSemaphoreGive(s_scanned_devices.mutex);
            return;
        }
    }

    // ??????
    if (s_scanned_devices.count < MAX_SCANNED_DEVICES) {
        bsp_ble_device_info_t *dev = &s_scanned_devices.devices[s_scanned_devices.count];
        memcpy(dev->addr, disc->addr.val, 6);
        dev->addr_type = disc->addr.type;  // ??????
        dev->rssi = disc->rssi;
        dev->type = identify_device_type(fields);
        dev->is_connected = is_already_connected;  // ????????

        // ??????
        if (fields->name != NULL && fields->name_len > 0) {
            int len = fields->name_len < 31 ? fields->name_len : 31;
            memcpy(dev->name, fields->name, len);
            dev->name[len] = '\0';
        } else {
            snprintf(dev->name, sizeof(dev->name), "Unknown");
        }

        s_scanned_devices.count++;
        
        // Log scanned device details
        const char *type_str = "Unknown";
        if (dev->type == BSP_BLE_DEVICE_TYPE_HR) {
            type_str = "HR (Heart Rate)";
        } else if (dev->type == BSP_BLE_DEVICE_TYPE_CSCS) {
            type_str = "CSCS (Speed/Cadence)";
        }
        
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "Device scanned #%d:", s_scanned_devices.count);
        ESP_LOGI(TAG, "  Name: %s", dev->name);
        ESP_LOGI(TAG, "  MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 dev->addr[5], dev->addr[4], dev->addr[3],
                 dev->addr[2], dev->addr[1], dev->addr[0]);
        ESP_LOGI(TAG, "  Address Type: %s (%d)",
                 dev->addr_type == BLE_ADDR_PUBLIC ? "Public" : 
                 dev->addr_type == BLE_ADDR_RANDOM ? "Random" : "Unknown",
                 dev->addr_type);
        ESP_LOGI(TAG, "  Type: %s (%d)", type_str, dev->type);
        ESP_LOGI(TAG, "  RSSI: %d dBm", dev->rssi);
        ESP_LOGI(TAG, "  Status: %s", is_already_connected ? "Already Connected" : "Available");

        if (fields->num_uuids16 > 0) {
            char uuid_buf_16[64] = {0};
            int wpos = 0;
            for (int i = 0; i < fields->num_uuids16; i++) {
                wpos += snprintf(uuid_buf_16 + wpos, sizeof(uuid_buf_16) - wpos,
                                 "0x%04X%s", ble_uuid_u16(&fields->uuids16[i].u),
                                 (i + 1 < fields->num_uuids16) ? ", " : "");
                if (wpos >= (int)sizeof(uuid_buf_16) - 8) break;
            }
            ESP_LOGI(TAG, "  Adv UUID16 (%s): [%s]",
                     fields->uuids16_is_complete ? "complete" : "partial", uuid_buf_16);
        } else {
            ESP_LOGI(TAG, "  Adv UUID16: none");
        }
        if (fields->num_uuids128 > 0) {
            char uuid_str128[BLE_UUID_STR_LEN];
            for (int i = 0; i < fields->num_uuids128; i++) {
                ble_uuid_to_str_safe(&fields->uuids128[i].u, uuid_str128, sizeof(uuid_str128));
                ESP_LOGI(TAG, "  Adv UUID128[%d]: %s%s", i, uuid_str128,
                         (i == 0 && !fields->uuids128_is_complete) ? " (partial list)" : "");
            }
        }
        ESP_LOGI(TAG, "========================================");
    }

    xSemaphoreGive(s_scanned_devices.mutex);
}

/**
 * @brief ????????
 */
static void parse_heart_rate_measurement(struct os_mbuf *om)
{
    uint8_t data[20];
    int len = os_mbuf_len(om);
    
    if (len < 2) {
        ESP_LOGW(TAG, "Heart rate data too short: %d bytes", len);
        return;
    }

    // ????
    os_mbuf_copydata(om, 0, len, data);

    uint8_t flags = data[0];
    bool hr_format_16bit = (flags & 0x01) != 0;  // Bit 0???????
    bool sensor_contact = (flags & 0x02) != 0;   // Bit 1?????????
    (void)sensor_contact;  // ??????????????

    uint16_t heart_rate = 0;
    if (hr_format_16bit && len >= 3) {
        // 16?????????
        heart_rate = data[1] | (data[2] << 8);
    } else if (len >= 2) {
        // 8?????
        heart_rate = data[1];
    }

    // ????????????30-220 bpm??
    if (heart_rate >= 30 && heart_rate <= 220) {
        s_current_heart_rate = heart_rate;
        // ???????????????????????
        ESP_LOGD(TAG, "Heart rate: %d bpm", heart_rate);
    } else {
        ESP_LOGW(TAG, "Invalid heart rate: %d bpm (valid range: 30-220)", heart_rate);
    }
}

/**
 * @brief ????????
 */
static void calculate_speed_and_cadence(uint32_t wheel_revs, uint16_t wheel_time,
                                        uint16_t crank_revs, uint16_t crank_time)
{
    // ?????m/s?? ?????????
    if (wheel_revs > 0 && wheel_time > 0) {
        if (s_last_wheel_revs > 0 && s_last_wheel_time > 0 && wheel_revs >= s_last_wheel_revs) {
            uint32_t rev_diff = wheel_revs - s_last_wheel_revs;
            uint16_t time_diff = wheel_time - s_last_wheel_time;

            // ????????5535 -> 0??
            if (time_diff > 32767) {
                time_diff = 65535 - s_last_wheel_time + wheel_time;
            }

            if (time_diff > 0 && rev_diff > 0) {
                // ?? = (???? ? ???? / ??????
                float speed_ms = (rev_diff * s_wheel_circumference) / (time_diff / 1024.0f);
                float speed_kmh = speed_ms * 3.6f;

                if (speed_kmh >= 0.0f && speed_kmh <= 100.0f) {
                    s_current_speed = speed_kmh;
                    s_last_wheel_motion_tick = xTaskGetTickCount();
                    // ???????????????????????
                    ESP_LOGD(TAG, "Speed: %.2f km/h", speed_kmh);
                } else {
                    ESP_LOGW(TAG, "Invalid speed: %.2f km/h (valid range: 0-100)", speed_kmh);
                }
            }
        }
        s_last_wheel_revs = wheel_revs;
        s_last_wheel_time = wheel_time;
    }

    // ?????RPM?? ????????????????parse_csc_measurement??????
    // ??????????parse_csc_measurement??????????????????
    // ???????????
}

/**
 * @brief ??CSC????
 * @note ??CSCS??????????.2??CSC Measurement (0x2A5B)
 *       ??????
 *       - ??0: Flags??????
 *         * Bit 0: Wheel Revolution Data Present (0x01) - ??????
 *         * Bit 1: Crank Revolution Data Present (0x02) - ??????
 *       - ???????Bit 0 = 1??
 *         * ??1-4: ??????(uint32_t, ????
 *         * ??5-6: ???????? (uint16_t, ???? ??: 1/1024??
 *       - ???????Bit 1 = 1??
 *         * ??N-N+1: ?????? (uint16_t, ????
 *         * ??N+2-N+3: ???????? (uint16_t, ???? ??: 1/1024??
 */
static void parse_csc_measurement(struct os_mbuf *om, uint16_t conn_handle)
{
    uint8_t data[20];
    int len = os_mbuf_len(om);
    
    if (len < 3) {
        ESP_LOGW(TAG, "CSC data too short: %d bytes", len);
        return;
    }

    // ????
    os_mbuf_copydata(om, 0, len, data);

    log_hex_dump("[CSC raw]", data, len > (int)sizeof(data) ? (int)sizeof(data) : len);
    s_cscs_notify_count++;
    s_cscs_waiting_data = false;

    uint8_t flags = data[0];
    bool wheel_rev_present = (flags & 0x01) != 0;  // Bit 0: ????
    bool crank_rev_present = (flags & 0x02) != 0;  // Bit 1: ????
    
    // ????????????????????
    // Per-notification summary at LOGD: mode is logged once via
    // "CSCS device mode: ..." below; printing this every second is noisy.
    ESP_LOGD(TAG, "CSC Measurement received: flags=0x%02X (Speed:%s Cadence:%s), len=%d bytes",
             flags, wheel_rev_present ? "Yes" : "No", crank_rev_present ? "Yes" : "No", len);

    device_connection_t *dev_conn = find_device_connection(conn_handle);
    if (dev_conn == NULL) {
        ESP_LOGW(TAG, "Device connection not found");
        return;
    }

    // ??????flags????????????Feature??????????
    if (dev_conn->cscs_mode == BSP_BLE_CSCS_MODE_UNKNOWN) {
        if (wheel_rev_present && crank_rev_present) {
            dev_conn->cscs_mode = BSP_BLE_CSCS_MODE_BOTH;
            ESP_LOGI(TAG, "CSCS device mode: BOTH (Speed + Cadence)");
        } else if (wheel_rev_present) {
            dev_conn->cscs_mode = BSP_BLE_CSCS_MODE_SPEED;
            ESP_LOGI(TAG, "CSCS device mode: SPEED_ONLY");
        } else if (crank_rev_present) {
            dev_conn->cscs_mode = BSP_BLE_CSCS_MODE_CADENCE;
            ESP_LOGI(TAG, "CSCS device mode: CADENCE_ONLY");
        } else {
            ESP_LOGW(TAG, "CSCS device mode: UNKNOWN (no wheel or crank data)");
        }
    }

    // ?????Feature?????????
    if (dev_conn->csc_feature_read) {
        bool feature_supports_speed = (dev_conn->csc_feature_value & 0x01) != 0;
        bool feature_supports_cadence = (dev_conn->csc_feature_value & 0x02) != 0;
        
        if (feature_supports_speed && !wheel_rev_present) {
            ESP_LOGW(TAG, "CSC Feature indicates speed support, but data has no wheel_rev");
        }
        if (feature_supports_cadence && !crank_rev_present) {
            ESP_LOGW(TAG, "CSC Feature indicates cadence support, but data has no crank_rev");
        }
    }

    uint32_t cumulative_wheel_revs = 0;
    uint16_t last_wheel_event_time = 0;
    uint16_t cumulative_crank_revs = 0;
    uint16_t last_crank_event_time = 0;

    int offset = 1;

    // ??????4???????? ???????????
    if (wheel_rev_present && len >= offset + 6) {
        cumulative_wheel_revs = data[offset] | 
                               (data[offset + 1] << 8) |
                               (data[offset + 2] << 16) |
                               (data[offset + 3] << 24);
        last_wheel_event_time = data[offset + 4] | (data[offset + 5] << 8);
        offset += 6;
        ESP_LOGD(TAG, "Wheel: revs=%lu, time=%d", cumulative_wheel_revs, last_wheel_event_time);
    } else if (wheel_rev_present) {
        ESP_LOGW(TAG, "  Wheel data expected but insufficient data (need 6 bytes, got %d)", len - offset);
    }

    // ???????????????? ???????????
    if (crank_rev_present && len >= offset + 4) {
        cumulative_crank_revs = data[offset] | (data[offset + 1] << 8);
        last_crank_event_time = data[offset + 2] | (data[offset + 3] << 8);
        offset += 4;
        ESP_LOGD(TAG, "Crank: revs=%d, time=%d", cumulative_crank_revs, last_crank_event_time);
        
        // ?????????????????CSCS????
        // ???????????????????????????
        if (s_last_crank_revs > 0 && s_last_crank_time > 0 && cumulative_crank_revs >= s_last_crank_revs) {
            uint16_t rev_diff = cumulative_crank_revs - s_last_crank_revs;
            uint16_t time_diff = last_crank_event_time - s_last_crank_time;
            
            // ??????
            if (time_diff > 32767) {
                time_diff = 65535 - s_last_crank_time + last_crank_event_time;
            }
            
            if (time_diff > 0 && rev_diff > 0) {
                // ?????????? ? 60??/ ??????
                float cadence_rpm = (rev_diff * 60.0f) / (time_diff / 1024.0f);
                
                if (cadence_rpm >= 0.0f && cadence_rpm <= 200.0f) {
                    s_current_cadence = cadence_rpm;
                    s_last_crank_motion_tick = xTaskGetTickCount();
                    ESP_LOGD(TAG, "Cadence: %.2f RPM", cadence_rpm);
                } else {
                    ESP_LOGW(TAG, "Invalid cadence: %.2f RPM (valid range: 0-200)", cadence_rpm);
                }
            }
        }
        
        // ????????????
        s_last_crank_revs = cumulative_crank_revs;
        s_last_crank_time = last_crank_event_time;
    } else if (crank_rev_present) {
        ESP_LOGW(TAG, "  Crank data expected but insufficient data (need 4 bytes, got %d)", len - offset);
    }
    
    ESP_LOGD(TAG, "Device mode: %d", dev_conn->cscs_mode);

    // ??????????????
    if (dev_conn->cscs_mode == BSP_BLE_CSCS_MODE_SPEED || 
        dev_conn->cscs_mode == BSP_BLE_CSCS_MODE_BOTH) {
        // ????
        calculate_speed_and_cadence(cumulative_wheel_revs, last_wheel_event_time,
                                    0, 0);  // ??????
    }
}

// ????
static void bsp_ble_on_hr_disc_complete(const struct peer *peer, int status, void *arg);
static void bsp_ble_on_cscs_disc_complete(const struct peer *peer, int status, void *arg);
static int bsp_ble_csc_feature_read_cb(uint16_t conn_handle,
                                       const struct ble_gatt_error *error,
                                       struct ble_gatt_attr *attr, void *arg);
static void bsp_ble_handle_sc_cp_indication(device_connection_t *dev_conn,
                                            const uint8_t *data, int len);
static int  bsp_ble_sc_cp_request_supported_locations(device_connection_t *dev_conn);
static void bsp_ble_on_disc_complete(const struct peer *peer, int status, void *arg);

/**
 * @brief CSC Feature????
 * @note ??CSCS??????????.3??CSC Feature (0x2A5C)
 *       ?????uint16_t??????????
 *       ??????
 *       - Bit 0: Wheel Revolution Data Supported (0x01) - ????
 *       - Bit 1: Crank Revolution Data Supported (0x02) - ????
 *       - Bit 2: Multiple Sensor Locations Supported (0x04) - ????????
 *       - Bit 3-15: ??
 *       ??????0x07 = ??????????????
 */
static int bsp_ble_csc_feature_read_cb(uint16_t conn_handle,
                                       const struct ble_gatt_error *error,
                                       struct ble_gatt_attr *attr, void *arg)
{
    if (error->status != 0) {
        ESP_LOGE(TAG, "CSC Feature read failed; status=%d", error->status);
        ESP_LOGW(TAG, "Will determine device mode from actual measurement data");
        return 0;
    }

    if (attr == NULL) {
        ESP_LOGE(TAG, "CSC Feature attr is NULL");
        return 0;
    }

    // ??Feature??2???????? ?????3.3??
    uint8_t data[2];
    int len = os_mbuf_copydata(attr->om, 0, sizeof(data), data);
    if (len != 2) {
        ESP_LOGW(TAG, "CSC Feature data length error: %d (expected 2)", len);
        return 0;
    }

    uint16_t feature = data[0] | (data[1] << 8);
    bool supports_speed = (feature & 0x01) != 0;    // Bit 0: ????
    bool supports_cadence = (feature & 0x02) != 0;   // Bit 1: ????
    bool supports_multi_location = (feature & 0x04) != 0;  // Bit 2: ????????

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "CSC Feature read completed for conn_handle=%d", conn_handle);
    ESP_LOGI(TAG, "CSC Feature value: 0x%04X", feature);
    ESP_LOGI(TAG, "  Bit 0 (Speed/Wheel Rev): %s", supports_speed ? "Supported" : "Not supported");
    ESP_LOGI(TAG, "  Bit 1 (Cadence/Crank Rev): %s", supports_cadence ? "Supported" : "Not supported");
    ESP_LOGI(TAG, "  Bit 2 (Multiple Locations): %s", supports_multi_location ? "Supported" : "Not supported");
    ESP_LOGI(TAG, "  Bit 3-15: Reserved");

    device_connection_t *dev_conn = find_device_connection(conn_handle);
    if (dev_conn != NULL) {
        dev_conn->csc_feature_read = true;
        dev_conn->csc_feature_value = feature;

        // ??Feature????????- ?????6????????
        ESP_LOGI(TAG, "Determining device mode from CSC Feature...");
        if (supports_speed && supports_cadence) {
            dev_conn->cscs_mode = BSP_BLE_CSCS_MODE_BOTH;
            ESP_LOGI(TAG, "  -> Mode: BOTH (Speed + Cadence)");
            ESP_LOGI(TAG, "  -> Expected data: 11 bytes (flags + wheel_rev + wheel_time + crank_rev + crank_time)");
        } else if (supports_speed) {
            dev_conn->cscs_mode = BSP_BLE_CSCS_MODE_SPEED;
            ESP_LOGI(TAG, "  -> Mode: SPEED_ONLY");
            ESP_LOGI(TAG, "  -> Expected data: 7 bytes (flags + wheel_rev + wheel_time)");
        } else if (supports_cadence) {
            dev_conn->cscs_mode = BSP_BLE_CSCS_MODE_CADENCE;
            ESP_LOGI(TAG, "  -> Mode: CADENCE_ONLY");
            ESP_LOGI(TAG, "  -> Expected data: 5 bytes (flags + crank_rev + crank_time)");
        } else {
            dev_conn->cscs_mode = BSP_BLE_CSCS_MODE_UNKNOWN;
            ESP_LOGW(TAG, "  -> Mode: UNKNOWN (feature=0x%04X)", feature);
            ESP_LOGW(TAG, "  -> Will determine mode from actual measurement data");
        }
        ESP_LOGI(TAG, "========================================");
    }

    return 0;
}

/**
 * @brief CSCS????????
 * @note ??CSCS????????????Cycling Speed and Cadence Service (0x1816)
 *       ??????
 *       - CSC Measurement (0x2A5B): NOTIFY - ????????
 *       - CSC Feature (0x2A5C): READ - ????????
 *       - Sensor Location (0x2A5D): READ, WRITE - ??????????????
 *       - SC Control Point (0x2A55): WRITE, INDICATE - ??????????
 */
static void bsp_ble_on_cscs_disc_complete(const struct peer *peer, int status, void *arg)
{
    if (status != 0) {
        ESP_LOGE(TAG, "CSCS service discovery failed; status=%d conn_handle=%d",
                 status, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    // ???????CSCS
    device_connection_t *dev_conn = find_device_connection(peer->conn_handle);
    if (dev_conn != NULL) {
        dev_conn->device_type = BSP_BLE_DEVICE_TYPE_CSCS;
        dev_conn->cscs_mode = BSP_BLE_CSCS_MODE_UNKNOWN;  // ??????
        dev_conn->csc_feature_read = false;
    }

    ESP_LOGI(TAG, "Processing CSCS service for conn_handle=%d...", peer->conn_handle);

    // ??CSC Feature????0x2A5C?? ???????Feature????????
    const struct peer_chr *feature_chr = peer_chr_find_uuid(peer,
                                                             BLE_UUID16_DECLARE(BLE_UUID_CSCS_SERVICE),
                                                             BLE_UUID16_DECLARE(BLE_UUID_CSC_FEATURE_CHAR));
    if (feature_chr != NULL) {
        ESP_LOGI(TAG, "Found CSC Feature characteristic (0x2A5C), reading...");
        // ??CSC Feature????????
        int rc = ble_gattc_read(peer->conn_handle, feature_chr->chr.val_handle,
                                bsp_ble_csc_feature_read_cb, NULL);
        if (rc != 0) {
            ESP_LOGW(TAG, "Failed to read CSC Feature; rc=%d", rc);
            ESP_LOGW(TAG, "Will determine mode from actual measurement data");
            // ????????????????
        }
    } else {
        ESP_LOGW(TAG, "CSC Feature characteristic (0x2A5C) not found");
        ESP_LOGW(TAG, "Will determine mode from actual measurement data");
    }

    // ??Sensor Location????0x2A5D?? ????????????XingZhe????
    const struct peer_chr *sensor_loc_chr = peer_chr_find_uuid(peer,
                                                               BLE_UUID16_DECLARE(BLE_UUID_CSCS_SERVICE),
                                                               BLE_UUID16_DECLARE(BLE_UUID_CSC_SENSOR_LOCATION_CHAR));
    if (sensor_loc_chr != NULL) {
        ESP_LOGI(TAG, "Found Sensor Location characteristic (0x2A5D) - mode switching supported");
        if (dev_conn != NULL) {
            dev_conn->sensor_location_handle = sensor_loc_chr->chr.val_handle;
            ESP_LOGI(TAG, "  Sensor Location handle: %d", dev_conn->sensor_location_handle);
        }
    } else {
        ESP_LOGD(TAG, "Sensor Location characteristic (0x2A5D) not found - mode switching not supported");
        if (dev_conn != NULL) {
            dev_conn->sensor_location_handle = 0;
        }
    }

    // 查找 SC Control Point（0x2A55） — 用于"更新传感器位置"等控制命令
    const struct peer_chr *ctrl_point_chr = peer_chr_find_uuid(peer,
                                                               BLE_UUID16_DECLARE(BLE_UUID_CSCS_SERVICE),
                                                               BLE_UUID16_DECLARE(BLE_UUID_CSC_CONTROL_POINT_CHAR));
    if (ctrl_point_chr != NULL) {
        ESP_LOGI(TAG, "Found SC Control Point characteristic (0x2A55) - configuration supported");
        if (dev_conn != NULL) {
            dev_conn->control_point_handle = ctrl_point_chr->chr.val_handle;
            ESP_LOGI(TAG, "  SC Control Point value handle: %d", dev_conn->control_point_handle);
        }
        // 查找 SC Control Point 的 CCCD，订阅 Indicate（写 0x0002）
        // 注意：SC CP 的响应以 Indication 形式返回，必须先使能 Indicate，
        // 否则远端无法把响应送回我们这边，很多传感器会直接拒绝后续写命令。
        const struct peer_dsc *cp_cccd = peer_dsc_find_uuid(peer,
                                                            BLE_UUID16_DECLARE(BLE_UUID_CSCS_SERVICE),
                                                            BLE_UUID16_DECLARE(BLE_UUID_CSC_CONTROL_POINT_CHAR),
                                                            BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
        if (cp_cccd != NULL && dev_conn != NULL) {
            dev_conn->control_point_cccd_handle = cp_cccd->dsc.handle;
            ESP_LOGI(TAG, "  SC Control Point CCCD handle: %d", dev_conn->control_point_cccd_handle);
            uint8_t ind_val[2] = {0x02, 0x00};  // 0x0002 = Indications enabled
            // 使用 Write Without Response：XOSS ARENA S1610 对 CCCD 的 Write Request
            // 不返回 Write Response，会触发 ATT 30s 事务超时从而断链（和 CSC
            // Measurement CCCD 情况一致）。WWR 不等响应，绝大多数传感器也接受。
            int rc_cp = ble_gattc_write_no_rsp_flat(peer->conn_handle,
                                                    cp_cccd->dsc.handle,
                                                    ind_val, sizeof(ind_val));
            if (rc_cp != 0) {
                ESP_LOGW(TAG, "  Failed to enable SC CP Indicate (WWR); rc=%d", rc_cp);
            } else {
                dev_conn->sc_cp_indicate_enabled = true;
                ESP_LOGI(TAG, "  SC CP Indicate enabled (CCCD=0x0002, WWR)");
            }
        } else {
            if (dev_conn != NULL) {
                dev_conn->control_point_cccd_handle = 0;
            }
            ESP_LOGW(TAG, "  SC Control Point CCCD not found - mode switching responses won't be received");
        }
    } else {
        ESP_LOGD(TAG, "SC Control Point characteristic (0x2A55) not found - configuration not supported");
        if (dev_conn != NULL) {
            dev_conn->control_point_handle = 0;
            dev_conn->control_point_cccd_handle = 0;
        }
    }

    // ??CSC Measurement????0x2A5B?? ??
    const struct peer_chr *chr = peer_chr_find_uuid(peer,
                                                     BLE_UUID16_DECLARE(BLE_UUID_CSCS_SERVICE),
                                                     BLE_UUID16_DECLARE(BLE_UUID_CSC_MEASUREMENT_CHAR));
    if (chr == NULL) {
        ESP_LOGE(TAG, "Error: Peer doesn't support CSC Measurement characteristic (0x2A5B)");
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    ESP_LOGI(TAG, "Found CSC Measurement characteristic (0x2A5B)");

    // ??CCCD????
    const struct peer_dsc *dsc = peer_dsc_find_uuid(peer,
                                                     BLE_UUID16_DECLARE(BLE_UUID_CSCS_SERVICE),
                                                     BLE_UUID16_DECLARE(BLE_UUID_CSC_MEASUREMENT_CHAR),
                                                     BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
    if (dsc == NULL) {
        ESP_LOGE(TAG, "Error: Peer lacks a CCCD for CSC Measurement");
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    // ????????x0001?CCCD?? ?????3.2??
    // Enable NOTIFY on CSC Measurement CCCD via Write Without Response.
    // Rationale: XOSS ARENA S1610 firmware does not send a Write Response for
    // CCCD Write Requests, which triggers the BLE ATT 30s transaction timeout
    // (Core Spec Vol 3 Part F 3.3.3) and tears down the link. WWR has no
    // response expected, so no timeout, and most sensors still accept it.
    uint8_t value[2] = {0x01, 0x00};
    int rc = ble_gattc_write_no_rsp_flat(peer->conn_handle, dsc->dsc.handle,
                                         value, sizeof(value));
    if (rc != 0) {
        ESP_LOGE(TAG, "Error: Failed to subscribe to CSC Measurement (WWR); rc=%d", rc);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    } else {
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "Subscribed to CSC Measurement notifications (Write Without Response)");
        ESP_LOGI(TAG, "  CCCD handle: %d", dsc->dsc.handle);
        ESP_LOGI(TAG, "  CCCD value: 0x0001 (NOTIFY enabled)");
        s_cscs_subscribe_tick = xTaskGetTickCount();
        s_cscs_waiting_data   = true;
        s_cscs_conn_handle    = peer->conn_handle;
        s_cscs_notify_count   = 0;
        ESP_LOGI(TAG, "Waiting for measurement data...");
        ESP_LOGI(TAG, "========================================");
    }

    // 如果 SC CP 可用且 Indicate 已订阅，主动问一下"你支持哪些 sensor location"。
    // 响应会以 Indication 方式回来，由 bsp_ble_handle_sc_cp_indication 解析并
    // 填到 dev_conn->supported_locations[]，后续 set_cscs_mode 会优先采用。
    if (dev_conn != NULL &&
        dev_conn->control_point_handle != 0 &&
        dev_conn->sc_cp_indicate_enabled) {
        (void)bsp_ble_sc_cp_request_supported_locations(dev_conn);
    }

}

/**
 * @brief ??????????
 */
static void bsp_ble_on_hr_disc_complete(const struct peer *peer, int status, void *arg)
{
    if (status != 0) {
        ESP_LOGE(TAG, "HR service discovery failed; status=%d conn_handle=%d",
                 status, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    // ????????????
    device_connection_t *dev_conn = find_device_connection(peer->conn_handle);
    if (dev_conn != NULL) {
        dev_conn->device_type = BSP_BLE_DEVICE_TYPE_HR;
        ESP_LOGI(TAG, "Device type set to HR for conn_handle=%d", peer->conn_handle);
    }

    // ??????????
    const struct peer_chr *chr = peer_chr_find_uuid(peer,
                                                     BLE_UUID16_DECLARE(BLE_UUID_HR_SERVICE),
                                                     BLE_UUID16_DECLARE(BLE_UUID_HR_MEASUREMENT_CHAR));
    if (chr == NULL) {
        ESP_LOGE(TAG, "Error: Peer doesn't support Heart Rate Measurement characteristic");
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    // ??CCCD????
    const struct peer_dsc *dsc = peer_dsc_find_uuid(peer,
                                                    BLE_UUID16_DECLARE(BLE_UUID_HR_SERVICE),
                                                    BLE_UUID16_DECLARE(BLE_UUID_HR_MEASUREMENT_CHAR),
                                                    BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
    if (dsc == NULL) {
        ESP_LOGE(TAG, "Error: Peer lacks a CCCD for Heart Rate Measurement");
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    // ????????x0001?CCCD??
    // Enable NOTIFY on HR Measurement CCCD via Write Without Response
    // (see CSCS path for rationale on avoiding ATT 30s transaction timeout).
    uint8_t value[2] = {0x01, 0x00};
    int rc = ble_gattc_write_no_rsp_flat(peer->conn_handle, dsc->dsc.handle,
                                         value, sizeof(value));
    if (rc != 0) {
        ESP_LOGE(TAG, "Error: Failed to subscribe to Heart Rate Measurement (WWR); rc=%d", rc);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    } else {
        ESP_LOGI(TAG, "Subscribed to Heart Rate Measurement notifications (Write Without Response)");
    }
}

/**
 * @brief 从 MTU 回调中发起"发现全部服务"
 * @note  只在 bsp_ble_mtu_exchange_cb 里调用一次，或 MTU 请求发起失败时从
 *        CONNECT 事件兜底调用一次。内部幂等（peer_disc_all 本身会判断）。
 */
static void bsp_ble_start_service_discovery(uint16_t conn_handle)
{
    struct peer *peer = peer_find(conn_handle);
    if (peer == NULL) {
        ESP_LOGE(TAG, "start_service_discovery: peer not found; conn_handle=%d",
                 conn_handle);
        ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }
    ESP_LOGI(TAG, "Starting service discovery (all services)... conn_handle=%d",
             conn_handle);
    ESP_LOGI(TAG, "========================================");
    int rc = peer_disc_all(conn_handle, bsp_ble_on_disc_complete, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start service discovery; rc=%d", rc);
        peer_delete(conn_handle);
        ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
}

/**
 * @brief ??????????????????????????
 * @note ??CSCS????????????????
 */
static void bsp_ble_on_disc_complete(const struct peer *peer, int status, void *arg)
{
    if (status != 0) {
        ESP_LOGE(TAG, "Service discovery failed; status=%d (0x%02X) conn_handle=%d",
                 status, status, peer->conn_handle);
        // NimBLE error codes (host/ble_hs.h):
        //   BLE_HS_EAGAIN=1, BLE_HS_EALREADY=2, BLE_HS_EINVAL=3, BLE_HS_EMSGSIZE=4,
        //   BLE_HS_ENOENT=5, BLE_HS_ENOMEM=6, BLE_HS_ENOTCONN=7, BLE_HS_ENOTSUP=8,
        //   BLE_HS_EAPP=9, BLE_HS_EBADDATA=10, BLE_HS_EOS=11, BLE_HS_ETIMEOUT=14.
        ESP_LOGE(TAG, "Error code details: status=%d", status);
        if (status == BLE_HS_ETIMEOUT) {
            ESP_LOGE(TAG, "Service discovery timeout (BLE_HS_ETIMEOUT)");
            ESP_LOGE(TAG, "Possible causes:");
            ESP_LOGE(TAG, "  1. Device is slow to respond");
            ESP_LOGE(TAG, "  2. Connection parameters need adjustment");
            ESP_LOGE(TAG, "  3. Device firmware issue");
            ESP_LOGE(TAG, "  4. Peer memory pool may be insufficient");
            ESP_LOGE(TAG, "  5. GATT operation timeout (check sdkconfig BLE settings)");
        } else if (status == BLE_HS_ENOTCONN) {
            ESP_LOGE(TAG, "Not connected (BLE_HS_ENOTCONN) - connection may have been lost");
        } else if (status == BLE_HS_ENOMEM) {
            ESP_LOGE(TAG, "Out of memory (BLE_HS_ENOMEM=6) - peer shared pool overflowed");
            ESP_LOGE(TAG, "  Cause: the global svcs/chrs/dscs mempools (see peer_init) are full");
            ESP_LOGE(TAG, "  Current totals: svcs=%d chrs=%d dscs=%d (shared by up to %d peers)",
                     MAX_PEER_SERVICES, MAX_PEER_CHARS, MAX_PEER_DESCS, MAX_BLE_DEVICES);
            ESP_LOGE(TAG, "  Fix: increase MAX_PEER_SERVICES / MAX_PEER_CHARS / MAX_PEER_DESCS in bsp_ble.c");
        } else if (status == BLE_HS_EOS) {
            ESP_LOGE(TAG, "OS error (BLE_HS_EOS) - system error");
        } else if (status == BLE_HS_EUNKNOWN) {
            ESP_LOGE(TAG, "Unknown error (BLE_HS_EUNKNOWN)");
        } else {
            ESP_LOGE(TAG, "Other error: status=%d", status);
        }
        
        // ??peer?????
        peer_delete(peer->conn_handle);
        device_connection_t *dev_conn = find_device_connection(peer->conn_handle);
        if (dev_conn != NULL) {
            dev_conn->is_connected = false;
            dev_conn->conn_handle = 0;
        }
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    ESP_LOGI(TAG, "Service discovery completed successfully; conn_handle=%d", peer->conn_handle);

    device_connection_t *dev_conn = find_device_connection(peer->conn_handle);
    if (dev_conn == NULL) {
        ESP_LOGE(TAG, "Device connection not found for conn_handle=%d", peer->conn_handle);
        peer_delete(peer->conn_handle);
        return;
    }

    // ????????????????????
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Service discovery completed for conn_handle=%d", peer->conn_handle);
    
    // ??????
    int svc_count = 0;
    const struct peer_svc *svc_temp;
    SLIST_FOREACH(svc_temp, &peer->svcs, next) {
        svc_count++;
    }
    ESP_LOGI(TAG, "Discovered services (%d total):", svc_count);
    
    const struct peer_svc *svc;
    int svc_idx = 0;
    SLIST_FOREACH(svc, &peer->svcs, next) {
        svc_idx++;
        uint16_t uuid16 = ble_uuid_u16(&svc->svc.uuid.u);
        char svc_uuid_full[BLE_UUID_STR_LEN] = {0};
        ble_uuid_to_str_safe(&svc->svc.uuid.u, svc_uuid_full, sizeof(svc_uuid_full));
        const char *svc_name = "Unknown Service";
        bool is_custom_service = false;
        
        // ??????
        if (uuid16 == BLE_UUID_HR_SERVICE) {
            svc_name = "Heart Rate Service";
        } else if (uuid16 == BLE_UUID_CSCS_SERVICE) {
            svc_name = "Cycling Speed and Cadence Service";
        } else if (uuid16 == 0x180A) {
            svc_name = "Device Information Service";
        } else if (uuid16 == 0x180F) {
            svc_name = "Battery Service";
        } else if (uuid16 == 0x1800) {
            svc_name = "Generic Access Profile (GAP)";
        } else if (uuid16 == 0x1801) {
            svc_name = "Generic Attribute Profile (GATT)";
        } else if (uuid16 == 0x0000) {
            svc_name = "Custom Service (0x0000)";
            is_custom_service = true;
        } else {
            // ????????????128-bit UUID?ble_uuid_u16 ?? 0??
            svc_name = "Custom/Unknown Service";
            // ???handle ??????handle ??11-19 ????????????
            if (svc->svc.start_handle >= 11 && svc->svc.start_handle <= 19) {
                is_custom_service = true;
                ESP_LOGI(TAG, "  -> Potential custom service detected (handle range: %d-%d)", 
                         svc->svc.start_handle, svc->svc.end_handle);
            }
        }
        
        ESP_LOGI(TAG, "  [%d] Service: %s", svc_idx, svc_name);
        ESP_LOGI(TAG, "      UUID: 0x%04X  (full: %s)", uuid16, svc_uuid_full);
        ESP_LOGI(TAG, "      Handle range: %d - %d", svc->svc.start_handle, svc->svc.end_handle);
        
        // NOTE: Mode switching is now handled exclusively via the standard
        // CSCS SC Control Point (0x2A55) "Update Sensor Location" command,
        // not via vendor-specific custom services. We intentionally no longer
        // try to discover / write any custom writable characteristic here.
        (void)is_custom_service;
        
        // ????????????
        const struct peer_chr *chr;
        int chr_idx = 0;
        SLIST_FOREACH(chr, &svc->chrs, next) {
            chr_idx++;
            uint16_t chr_uuid16 = ble_uuid_u16(&chr->chr.uuid.u);
            char chr_uuid_full[BLE_UUID_STR_LEN] = {0};
            ble_uuid_to_str_safe(&chr->chr.uuid.u, chr_uuid_full, sizeof(chr_uuid_full));
            const char *chr_name = "Unknown Characteristic";
            
            // ????????
            if (chr_uuid16 == BLE_UUID_HR_MEASUREMENT_CHAR) {
                chr_name = "Heart Rate Measurement";
            } else if (chr_uuid16 == BLE_UUID_CSC_MEASUREMENT_CHAR) {
                chr_name = "CSC Measurement";
            } else if (chr_uuid16 == BLE_UUID_CSC_FEATURE_CHAR) {
                chr_name = "CSC Feature";
            } else if (chr_uuid16 == BLE_UUID_CSC_SENSOR_LOCATION_CHAR) {
                chr_name = "Sensor Location";
            } else if (chr_uuid16 == BLE_UUID_CSC_CONTROL_POINT_CHAR) {
                chr_name = "SC Control Point";
            }
            
            // ?????
            char props_buf[64] = {0};
            if (chr->chr.properties & BLE_GATT_CHR_PROP_READ) {
                strcat(props_buf, "READ ");
            }
            if (chr->chr.properties & BLE_GATT_CHR_PROP_WRITE) {
                strcat(props_buf, "WRITE ");
            }
            if (chr->chr.properties & BLE_GATT_CHR_PROP_NOTIFY) {
                strcat(props_buf, "NOTIFY ");
            }
            if (chr->chr.properties & BLE_GATT_CHR_PROP_INDICATE) {
                strcat(props_buf, "INDICATE ");
            }
            if (strlen(props_buf) == 0) {
                strcpy(props_buf, "NONE");
            }
            
            ESP_LOGI(TAG, "      [%d.%d] Characteristic: %s", svc_idx, chr_idx, chr_name);
            ESP_LOGI(TAG, "            UUID: 0x%04X  (full: %s)", chr_uuid16, chr_uuid_full);
            ESP_LOGI(TAG, "            Value handle: %d", chr->chr.val_handle);
            ESP_LOGI(TAG, "            Properties: 0x%02X (%s)", chr->chr.properties, props_buf);
            
            // ??????
            const struct peer_dsc *dsc;
            int dsc_idx = 0;
            SLIST_FOREACH(dsc, &chr->dscs, next) {
                dsc_idx++;
                uint16_t dsc_uuid16 = ble_uuid_u16(&dsc->dsc.uuid.u);
                const char *dsc_name = "Unknown Descriptor";
                if (dsc_uuid16 == BLE_GATT_DSC_CLT_CFG_UUID16) {
                    dsc_name = "Client Characteristic Configuration (CCCD)";
                }
                ESP_LOGI(TAG, "            [%d.%d.%d] Descriptor: %s", svc_idx, chr_idx, dsc_idx, dsc_name);
                ESP_LOGI(TAG, "                  UUID: 0x%04X", dsc_uuid16);
                ESP_LOGI(TAG, "                  Handle: %d", dsc->dsc.handle);
            }
        }
        ESP_LOGI(TAG, "      Total characteristics: %d", chr_idx);
    }
    ESP_LOGI(TAG, "========================================");

    // ??????????????
    if (dev_conn->device_type == BSP_BLE_DEVICE_TYPE_HR || 
        dev_conn->device_type == BSP_BLE_DEVICE_TYPE_UNKNOWN) {
        // ??????????x180D??
        const struct peer_svc *svc = peer_svc_find_uuid(peer, 
                                                         BLE_UUID16_DECLARE(BLE_UUID_HR_SERVICE));
        if (svc != NULL) {
            ESP_LOGI(TAG, "Found Heart Rate Service (0x180D)");
            bsp_ble_on_hr_disc_complete(peer, 0, NULL);
            return;
        }
    }

    if (dev_conn->device_type == BSP_BLE_DEVICE_TYPE_CSCS ||
        dev_conn->device_type == BSP_BLE_DEVICE_TYPE_UNKNOWN) {
        // ????CSCS????x1816??
        const struct peer_svc *svc = peer_svc_find_uuid(peer, 
                                                         BLE_UUID16_DECLARE(BLE_UUID_CSCS_SERVICE));
        if (svc != NULL) {
            ESP_LOGI(TAG, "Found CSCS Service (0x1816)");
            bsp_ble_on_cscs_disc_complete(peer, 0, NULL);
            return;
        }
    }

    ESP_LOGW(TAG, "No matching service found for device type=%d", dev_conn->device_type);
    ESP_LOGW(TAG, "Expected services: HR (0x180D) or CSCS (0x1816)");
}

/**
 * @brief GAP??????
 */
static int bsp_ble_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_hs_adv_fields fields;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        // ????
        rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
        if (rc != 0) {
            ESP_LOGW(TAG, "Failed to parse advertisement fields; rc=%d", rc);
            return 0;
        }

        // ????UUID
        if (check_target_service(&fields)) {
            // ??????
            save_scanned_device(&event->disc, &fields);
        } else {
            // Log non-target devices for debugging (at debug level)
            ESP_LOGD(TAG, "Device discovered but not a target (HR/CSCS): MAC=%02X:%02X:%02X:%02X:%02X:%02X, RSSI=%d",
                     event->disc.addr.val[5], event->disc.addr.val[4], event->disc.addr.val[3],
                     event->disc.addr.val[2], event->disc.addr.val[1], event->disc.addr.val[0],
                     event->disc.rssi);
        }
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        // ????
        ESP_LOGI(TAG, "Discovery complete; reason=%d", event->disc_complete.reason);
        s_scanning = false;
        return 0;

    case BLE_GAP_EVENT_CONNECT:
        // ????
        if (event->connect.status == 0) {
            // ????
            ESP_LOGI(TAG, "========================================");
            ESP_LOGI(TAG, "Connection established; conn_handle=%d", event->connect.conn_handle);
            
            // ????????????????
            struct ble_gap_conn_desc desc;
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            if (rc == 0) {
                ESP_LOGI(TAG, "Connection details:");
                ESP_LOGI(TAG, "  Peer MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                         desc.peer_id_addr.val[5], desc.peer_id_addr.val[4],
                         desc.peer_id_addr.val[3], desc.peer_id_addr.val[2],
                         desc.peer_id_addr.val[1], desc.peer_id_addr.val[0]);
                ESP_LOGI(TAG, "  Peer addr type: %d", desc.peer_id_addr.type);
                ESP_LOGI(TAG, "  Connection interval: %d (units: 1.25ms)", desc.conn_itvl);
                ESP_LOGI(TAG, "  Connection latency: %d", desc.conn_latency);
                ESP_LOGI(TAG, "  Supervision timeout: %d (units: 10ms)", desc.supervision_timeout);
                
                // ??????????RSSI??????
                if (xSemaphoreTake(s_scanned_devices.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    for (int i = 0; i < s_scanned_devices.count; i++) {
                        if (memcmp(s_scanned_devices.devices[i].addr, desc.peer_id_addr.val, 6) == 0) {
                            ESP_LOGI(TAG, "  RSSI: %d dBm (from scan)", s_scanned_devices.devices[i].rssi);
                            break;
                        }
                    }
                    xSemaphoreGive(s_scanned_devices.mutex);
                }
            }

            // ????????
            device_connection_t *dev_conn = find_device_connection(event->connect.conn_handle);
            bsp_ble_device_type_t scanned_type = BSP_BLE_DEVICE_TYPE_UNKNOWN;
            
            if (dev_conn == NULL) {
                // ??????????
                dev_conn = find_free_device_slot();
                if (dev_conn != NULL) {
                    if (rc == 0) {
                        dev_conn->conn_handle = event->connect.conn_handle;
                        memcpy(dev_conn->addr, desc.peer_id_addr.val, 6);
                        dev_conn->addr_type = desc.peer_id_addr.type;
                        dev_conn->is_connected = true;
                        dev_conn->device_type = BSP_BLE_DEVICE_TYPE_UNKNOWN;  // ??????????
                        dev_conn->cscs_mode = BSP_BLE_CSCS_MODE_UNKNOWN;  // ??????
                        dev_conn->csc_feature_read = false;
                        dev_conn->sensor_location_handle = 0;
                        dev_conn->control_point_handle = 0;
                        dev_conn->control_point_cccd_handle = 0;
                        dev_conn->sc_cp_indicate_enabled = false;
                        dev_conn->pending_sc_cp_op = 0;
                        dev_conn->supported_locations_count = 0;
                        dev_conn->supported_locations_known = false;
                        dev_conn->mode_manually_set = false;
                        
                        // ????????????????????????
                        if (xSemaphoreTake(s_scanned_devices.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                            for (int i = 0; i < s_scanned_devices.count; i++) {
                                if (memcmp(s_scanned_devices.devices[i].addr, desc.peer_id_addr.val, 6) == 0) {
                                    scanned_type = s_scanned_devices.devices[i].type;
                                    ESP_LOGI(TAG, "Found device in scan list: %s, Type: %d",
                                             s_scanned_devices.devices[i].name, scanned_type);
                                    break;
                                }
                            }
                            xSemaphoreGive(s_scanned_devices.mutex);
                        }
                    }
                }
            }

            // ??????????????????
            if (dev_conn != NULL) {
                if (scanned_type == BSP_BLE_DEVICE_TYPE_HR) {
                    dev_conn->device_type = BSP_BLE_DEVICE_TYPE_HR;
                    ESP_LOGI(TAG, "Device type: Heart Rate (HR)");
                } else if (scanned_type == BSP_BLE_DEVICE_TYPE_CSCS) {
                    dev_conn->device_type = BSP_BLE_DEVICE_TYPE_CSCS;
                    ESP_LOGI(TAG, "Device type: CSCS (Speed/Cadence)");
                } else {
                    ESP_LOGI(TAG, "Device type: Unknown (will be determined during service discovery)");
                }
            }

            // ???????????????????Peer
            // ????????????????????????
            vTaskDelay(pdMS_TO_TICKS(200));
            
            // ??Peer???????????? ??????
            rc = peer_add(event->connect.conn_handle);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to add peer; rc=%d", rc);
                if (rc == BLE_HS_ENOTCONN) {
                    ESP_LOGE(TAG, "Error: Not connected (connection may not be fully established)");
                    ESP_LOGE(TAG, "Waiting 300ms and retrying...");
                    vTaskDelay(pdMS_TO_TICKS(300));
                    rc = peer_add(event->connect.conn_handle);
                    if (rc != 0) {
                        ESP_LOGE(TAG, "Retry failed; rc=%d", rc);
                        ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                        return 0;
                    }
                } else {
                    ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                    return 0;
                }
            }
            ESP_LOGI(TAG, "Peer added successfully");
            bsp_buzzer_request(BSP_BUZZ_PATTERN_CONNECT);

            // 发起 MTU 交换。完成后（mtu_exchange_cb 回调里）再启动服务发现，
            // 这样对 XOSS ARENA S1610 这类传感器链路更稳。
            // 如果 MTU 请求本身都失败了，就退回原始路径，直接启动服务发现。
            {
                int _mtu_rc = ble_gattc_exchange_mtu(event->connect.conn_handle,
                                                    bsp_ble_mtu_exchange_cb, NULL);
                if (_mtu_rc != 0) {
                    ESP_LOGW(TAG, "Failed to start MTU exchange; rc=%d — falling back to immediate disc",
                             _mtu_rc);
                    bsp_ble_start_service_discovery(event->connect.conn_handle);
                } else {
                    ESP_LOGI(TAG, "MTU exchange initiated — discovery will start from its callback");
                }
            }
        } else {
            // ????
            ESP_LOGE(TAG, "========================================");
            ESP_LOGE(TAG, "Connection failed!");
            ESP_LOGE(TAG, "  Connection Handle: %d", event->connect.conn_handle);
            ESP_LOGE(TAG, "  Status Code: 0x%02X (%d)", event->connect.status, event->connect.status);
            
            // Decode connection failure reason
            const char *status_desc = "Unknown error";
            switch (event->connect.status) {
                case BLE_ERR_CONN_TERM_LOCAL:
                    status_desc = "Connection terminated by local host";
                    ESP_LOGE(TAG, "  Reason: Connection terminated by local host");
                    break;
                case BLE_ERR_REM_USER_CONN_TERM:
                    status_desc = "Connection terminated by remote host";
                    ESP_LOGE(TAG, "  Reason: Connection terminated by remote host (device rejected)");
                    break;
                case BLE_ERR_CONN_ESTABLISHMENT:
                    status_desc = "Connection establishment failed";
                    ESP_LOGE(TAG, "  Reason: Connection establishment failed");
                    ESP_LOGE(TAG, "    Possible causes:");
                    ESP_LOGE(TAG, "      - Device is out of range");
                    ESP_LOGE(TAG, "      - Device is not advertising");
                    ESP_LOGE(TAG, "      - Device rejected the connection");
                    ESP_LOGE(TAG, "      - Connection parameters incompatible");
                    break;
                case BLE_ERR_CONN_LIMIT:
                    status_desc = "Connection timeout or limit reached";
                    ESP_LOGE(TAG, "  Reason: Connection timeout or limit reached");
                    ESP_LOGE(TAG, "    Possible causes:");
                    ESP_LOGE(TAG, "      - Device did not respond in time");
                    ESP_LOGE(TAG, "      - Device is out of range");
                    ESP_LOGE(TAG, "      - Connection limit reached");
                    break;
                case BLE_ERR_UNK_CONN_ID:
                    status_desc = "Unknown connection ID";
                    ESP_LOGE(TAG, "  Reason: Unknown connection ID");
                    break;
                default:
                    // Check for out of memory using numeric value (0x07)
                    if (event->connect.status == 0x07) {
                        status_desc = "Out of memory";
                        ESP_LOGE(TAG, "  Reason: Out of memory - connection pool may be full");
                    } else {
                        ESP_LOGE(TAG, "  Reason: Unknown status code 0x%02X", event->connect.status);
                    }
                    break;
            }
            ESP_LOGE(TAG, "  Status Description: %s", status_desc);
            
            // Get device address if available
            struct ble_gap_conn_desc desc;
            if (ble_gap_conn_find(event->connect.conn_handle, &desc) == 0) {
                ESP_LOGE(TAG, "  Device MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                         desc.peer_id_addr.val[5], desc.peer_id_addr.val[4],
                         desc.peer_id_addr.val[3], desc.peer_id_addr.val[2],
                         desc.peer_id_addr.val[1], desc.peer_id_addr.val[0]);
            }
            ESP_LOGE(TAG, "========================================");
            
            // ???????
            if (!s_scanning) {
                bsp_ble_start_scan(0);
            }
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        // ????
        ESP_LOGI(TAG, "Disconnect; conn_handle=%d reason=%d",
                 event->disconnect.conn.conn_handle, event->disconnect.reason);

        // ??Peer
        peer_delete(event->disconnect.conn.conn_handle);
        
        // ????????
        device_connection_t *dev_conn = find_device_connection(event->disconnect.conn.conn_handle);
        if (dev_conn != NULL) {
            dev_conn->is_connected = false;
            dev_conn->conn_handle = 0;
            dev_conn->cscs_mode = BSP_BLE_CSCS_MODE_UNKNOWN;  // ????
            dev_conn->csc_feature_read = false;  // ??Feature????
            dev_conn->sensor_location_handle = 0;
            dev_conn->control_point_handle = 0;
            dev_conn->control_point_cccd_handle = 0;
            dev_conn->sc_cp_indicate_enabled = false;
            dev_conn->pending_sc_cp_op = 0;
            dev_conn->supported_locations_count = 0;
            dev_conn->supported_locations_known = false;
            dev_conn->mode_manually_set = false;
            
            // ????????
            if (dev_conn->device_type == BSP_BLE_DEVICE_TYPE_CSCS) {
                s_current_speed = 0.0f;
                s_current_cadence = 0.0f;
                s_last_wheel_revs = 0;
                s_last_wheel_time = 0;
                s_last_crank_revs = 0;
                s_last_crank_time = 0;
                s_last_wheel_motion_tick = 0;
                s_last_crank_motion_tick = 0;
                // Stop the "CSC watchdog still NO measurement data" warnings
                // that would otherwise keep firing forever after a CSCS link
                // drops before any data was received.
                s_cscs_waiting_data = false;
                s_cscs_conn_handle  = 0;
                s_cscs_notify_count = 0;
            }
        }

        // ???????
        if (!s_scanning) {
            bsp_ble_start_scan(0);
        }
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        // ??????
        {
            device_connection_t *dev_conn = find_device_connection(event->notify_rx.conn_handle);
            int nlen = os_mbuf_len(event->notify_rx.om);
            uint8_t nbuf[32] = {0};
            int ncopy = (nlen > (int)sizeof(nbuf)) ? (int)sizeof(nbuf) : nlen;
            os_mbuf_copydata(event->notify_rx.om, 0, ncopy, nbuf);
            int _dtype = (dev_conn != NULL) ? (int)dev_conn->device_type : -1;
            // Per-NOTIFY meta at LOGD to keep steady-state output clean;
            // promote to LOGI only while debugging subscription issues.
            ESP_LOGD(TAG, "NOTIFY_RX conn=%d attr_handle=%d indication=%d dev_type=%d len=%d",
                     event->notify_rx.conn_handle,
                     event->notify_rx.attr_handle,
                     event->notify_rx.indication,
                     _dtype, nlen);
            log_hex_dump("[NOTIFY bytes]", nbuf, ncopy);
            if (dev_conn != NULL) {
                // 优先识别 SC Control Point 的 Indication（切换模式的响应）
                if (event->notify_rx.indication &&
                    dev_conn->control_point_handle != 0 &&
                    event->notify_rx.attr_handle == dev_conn->control_point_handle) {
                    bsp_ble_handle_sc_cp_indication(dev_conn, nbuf, ncopy);
                } else if (dev_conn->device_type == BSP_BLE_DEVICE_TYPE_HR) {
                    parse_heart_rate_measurement(event->notify_rx.om);
                } else if (dev_conn->device_type == BSP_BLE_DEVICE_TYPE_CSCS) {
                    parse_csc_measurement(event->notify_rx.om, event->notify_rx.conn_handle);
                } else {
                    ESP_LOGW(TAG, "NOTIFY dropped: device_type=UNKNOWN (service discovery may have missed target service)");
                }
            } else {
                ESP_LOGW(TAG, "NOTIFY dropped: no device_connection for conn_handle=%d", event->notify_rx.conn_handle);
            }
        }
        return 0;

    default:
        return 0;
    }
}

/**
 * @brief BLE??????
 */
static void bsp_ble_on_sync(void)
{
    ESP_LOGI(TAG, "BLE host synchronized");
    
    // ????????????????
    // ?????????????bsp_ble_start_scan()
}

/**
 * @brief BLE??????
 */
static void bsp_ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE host reset; reason=%d", reason);
}

/**
 * @brief BLE????
 */
static void bsp_ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/**
 * @brief ????BLE??
 */
esp_err_t bsp_ble_start_scan(uint32_t duration_sec)
{
    if (!s_ble_initialized) {
        ESP_LOGE(TAG, "BLE not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_scanning) {
        ESP_LOGW(TAG, "Scan already in progress");
        return ESP_OK;
    }

    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params;
    int rc;

    // ??????
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "error determining address type; rc=%d", rc);
        return ESP_FAIL;
    }

    // ?????????????
    // filter_duplicates=1 ?????MAC??????????????????????
    // ??????????????+CSCS??????
    disc_params.filter_duplicates = 1;  // ?????????MAC??????
    disc_params.passive = 1;            // ?????????
    disc_params.itvl = BLE_GAP_SCAN_ITVL_MS(1000);  // ????1??
    disc_params.window = BLE_GAP_SCAN_WIN_MS(100);   // ????100ms
    disc_params.filter_policy = 0;      // ??????
    disc_params.limited = 0;            // ????????????
    
    // ?????????
    int connected_count = 0;
    for (int i = 0; i < MAX_BLE_DEVICES; i++) {
        if (s_device_connections[i].is_connected) {
            connected_count++;
        }
    }
    ESP_LOGI(TAG, "Starting scan (currently connected devices: %d/%d)", 
             connected_count, MAX_BLE_DEVICES);

    // ?????
    uint32_t duration_ms = duration_sec > 0 ? duration_sec * 1000 : BLE_HS_FOREVER;
    rc = ble_gap_disc(own_addr_type, duration_ms, &disc_params,
                      bsp_ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error initiating GAP discovery procedure; rc=%d", rc);
        return ESP_FAIL;
    }

    s_scanning = true;
    ESP_LOGI(TAG, "BLE scan started (duration: %lu seconds)", duration_sec);
    return ESP_OK;
}

/**
 * @brief ????
 */
esp_err_t bsp_ble_stop_scan(void)
{
    if (!s_scanning) {
        return ESP_OK;
    }

    int rc = ble_gap_disc_cancel();
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to cancel scan; rc=%d", rc);
        return ESP_FAIL;
    }

    s_scanning = false;
    ESP_LOGI(TAG, "BLE scan stopped");
    return ESP_OK;
}

/**
 * @brief ????
 */
esp_err_t bsp_ble_connect(const uint8_t *addr, uint8_t addr_type)
{
    if (!s_ble_initialized) {
        ESP_LOGE(TAG, "BLE not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // ?????????????
    for (int i = 0; i < MAX_BLE_DEVICES; i++) {
        if (s_device_connections[i].is_connected &&
            memcmp(s_device_connections[i].addr, addr, 6) == 0) {
            ESP_LOGW(TAG, "Device already connected");
            return ESP_OK;
        }
    }

    // ?????????????
    bool has_free_slot = false;
    for (int i = 0; i < MAX_BLE_DEVICES; i++) {
        if (!s_device_connections[i].is_connected) {
            has_free_slot = true;
            break;
        }
    }
    
    if (!has_free_slot) {
        ESP_LOGE(TAG, "No free connection slot available (max: %d)", MAX_BLE_DEVICES);
        return ESP_ERR_NO_MEM;
    }
    
    // ????????????BLE ????????????????
    if (s_scanning) {
        ESP_LOGI(TAG, "Stopping scan before connecting...");
        bsp_ble_stop_scan();
        // ???????????????
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    struct ble_gap_conn_params conn_params;
    ble_addr_t peer_addr;
    int rc;

    // ??????
    conn_params.scan_itvl = BLE_GAP_SCAN_ITVL_MS(16);
    conn_params.scan_window = BLE_GAP_SCAN_WIN_MS(16);
    conn_params.itvl_min = BLE_GAP_CONN_ITVL_MS(24);
    conn_params.itvl_max = BLE_GAP_CONN_ITVL_MS(40);
    conn_params.latency = 0;
    // 5000ms: fitness sensors (e.g. CSCS) may be radio-idle for up to several seconds between wheel revolutions; 500ms was too aggressive and risked spurious link drops.
    conn_params.supervision_timeout = BLE_GAP_SUPERVISION_TIMEOUT_MS(5000);
    conn_params.min_ce_len = 0;  // Connection event length (0 = use default)
    conn_params.max_ce_len = 0;  // Connection event length (0 = use default)

    // ??????
    memcpy(peer_addr.val, addr, 6);
    peer_addr.type = addr_type;

    // Log connection attempt details
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Attempting to connect to device:");
    ESP_LOGI(TAG, "  MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    ESP_LOGI(TAG, "  Address Type: %s (%d)",
             addr_type == BLE_ADDR_PUBLIC ? "Public" : 
             addr_type == BLE_ADDR_RANDOM ? "Random" : "Unknown",
             addr_type);
    ESP_LOGI(TAG, "  Connection Parameters:");
    ESP_LOGI(TAG, "    Scan Interval: %u ms", (unsigned)(conn_params.scan_itvl * 625 / 1000));
    ESP_LOGI(TAG, "    Scan Window: %u ms", (unsigned)(conn_params.scan_window * 625 / 1000));
    ESP_LOGI(TAG, "    Connection Interval: %u-%u ms",
             (unsigned)(conn_params.itvl_min * 5 / 4),
             (unsigned)(conn_params.itvl_max * 5 / 4));
    ESP_LOGI(TAG, "    Latency: %d", conn_params.latency);
    ESP_LOGI(TAG, "    Supervision Timeout: %u ms",
             (unsigned)(conn_params.supervision_timeout * 10));
    ESP_LOGI(TAG, "========================================");
    
    // ????
    rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &peer_addr,
                         BLE_HS_FOREVER, &conn_params,
                         bsp_ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "========================================");
        ESP_LOGE(TAG, "Connection initiation failed!");
        ESP_LOGE(TAG, "  MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
        ESP_LOGE(TAG, "  Error Code: rc=%d (0x%02X)", rc, rc);
        
        // Decode error code
        const char *error_desc = "Unknown error";
        switch (rc) {
            case BLE_HS_EALREADY:
                error_desc = "Already in progress or connected";
                ESP_LOGE(TAG, "  Error: BLE_HS_EALREADY - Connection already in progress or device already connected");
                break;
            case BLE_HS_EBUSY:
                error_desc = "BLE stack is busy";
                ESP_LOGE(TAG, "  Error: BLE_HS_EBUSY - BLE stack is busy, try again later");
                break;
            case BLE_HS_EINVAL:
                error_desc = "Invalid parameters";
                ESP_LOGE(TAG, "  Error: BLE_HS_EINVAL - Invalid connection parameters");
                break;
            case BLE_HS_ENOMEM:
                error_desc = "Out of memory";
                ESP_LOGE(TAG, "  Error: BLE_HS_ENOMEM - Out of memory, connection pool may be full");
                break;
            case BLE_HS_ETIMEOUT:
                error_desc = "Connection timeout";
                ESP_LOGE(TAG, "  Error: BLE_HS_ETIMEOUT - Connection attempt timed out");
                break;
            case BLE_HS_ECONTROLLER:
                error_desc = "Controller error";
                ESP_LOGE(TAG, "  Error: BLE_HS_ECONTROLLER - BLE controller error");
                break;
            case BLE_HS_EOS:
                error_desc = "OS error";
                ESP_LOGE(TAG, "  Error: BLE_HS_EOS - Operating system error");
                break;
            default:
                ESP_LOGE(TAG, "  Error: Unknown error code %d", rc);
                break;
        }
        ESP_LOGE(TAG, "  Error Description: %s", error_desc);
        ESP_LOGE(TAG, "========================================");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Connection request sent, waiting for connection result...");

    ESP_LOGI(TAG, "Connecting to device...");
    return ESP_OK;
}

/**
 * @brief ????
 */
esp_err_t bsp_ble_disconnect(uint16_t conn_handle)
{
    int rc = ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to disconnect; rc=%d", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief ??????????
 */
esp_err_t bsp_ble_get_scanned_devices(bsp_ble_device_info_t *devices,
                                       uint8_t *count,
                                       uint8_t max_count)
{
    if (devices == NULL || count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_scanned_devices.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    uint8_t copy_count = s_scanned_devices.count < max_count ? 
                         s_scanned_devices.count : max_count;
    memcpy(devices, s_scanned_devices.devices, 
           copy_count * sizeof(bsp_ble_device_info_t));
    
    // ?????????????????????
    for (int i = 0; i < copy_count; i++) {
        // ?????????????
        bool is_connected = false;
        for (int j = 0; j < MAX_BLE_DEVICES; j++) {
            if (s_device_connections[j].is_connected &&
                memcmp(s_device_connections[j].addr, devices[i].addr, 6) == 0) {
                is_connected = true;
                // ????????????????????
                if (s_device_connections[j].device_type != BSP_BLE_DEVICE_TYPE_UNKNOWN) {
                    devices[i].type = s_device_connections[j].device_type;
                }
                break;
            }
        }
        devices[i].is_connected = is_connected;
    }
    
    *count = copy_count;

    xSemaphoreGive(s_scanned_devices.mutex);
    return ESP_OK;
}

/**
 * @brief ??????
 */
uint16_t bsp_ble_get_heart_rate(void)
{
    return s_current_heart_rate;
}

/**
 * @brief ??????
 */
float bsp_ble_get_speed(void)
{
    return s_current_speed;
}

/**
 * @brief ??????
 */
float bsp_ble_get_cadence(void)
{
    return s_current_cadence;
}

/**
 * @brief ??????
 */
esp_err_t bsp_ble_set_wheel_circumference(float circumference_m)
{
    if (circumference_m <= 0.0f || circumference_m > 10.0f) {
        return ESP_ERR_INVALID_ARG;
    }
    s_wheel_circumference = circumference_m;
    ESP_LOGI(TAG, "Wheel circumference set to %.3f m", circumference_m);
    return ESP_OK;
}

float bsp_ble_get_wheel_circumference(void)
{
    return s_wheel_circumference;
}

/**
 * @brief ?????????
 */
bool bsp_ble_is_device_connected(bsp_ble_device_type_t type)
{
    device_connection_t *dev_conn = find_device_by_type(type);
    return (dev_conn != NULL && dev_conn->is_connected);
}

uint16_t bsp_ble_get_conn_handle(bsp_ble_device_type_t type)
{
    device_connection_t *dev_conn = find_device_by_type(type);
    if (dev_conn != NULL && dev_conn->is_connected) {
        return dev_conn->conn_handle;
    }
    return 0;
}

/**
 * @brief ??CSCS??????
 */
bsp_ble_cscs_mode_t bsp_ble_get_cscs_mode(void)
{
    device_connection_t *dev_conn = find_device_by_type(BSP_BLE_DEVICE_TYPE_CSCS);
    if (dev_conn != NULL && dev_conn->is_connected) {
        return dev_conn->cscs_mode;
    }
    return BSP_BLE_CSCS_MODE_UNKNOWN;
}

/**
 * @brief ??CSCS????????
 */
bool bsp_ble_cscs_supports_speed(void)
{
    device_connection_t *dev_conn = find_device_by_type(BSP_BLE_DEVICE_TYPE_CSCS);
    if (dev_conn != NULL && dev_conn->is_connected) {
        bsp_ble_cscs_mode_t mode = dev_conn->cscs_mode;
        return (mode == BSP_BLE_CSCS_MODE_SPEED || mode == BSP_BLE_CSCS_MODE_BOTH);
    }
    return false;
}

/**
 * @brief ??CSCS????????
 */
bool bsp_ble_cscs_supports_cadence(void)
{
    device_connection_t *dev_conn = find_device_by_type(BSP_BLE_DEVICE_TYPE_CSCS);
    if (dev_conn != NULL && dev_conn->is_connected) {
        bsp_ble_cscs_mode_t mode = dev_conn->cscs_mode;
        return (mode == BSP_BLE_CSCS_MODE_CADENCE || mode == BSP_BLE_CSCS_MODE_BOTH);
    }
    return false;
}

// ============================================================================
// CSCS 模式切换 —— 标准 SC Control Point "Update Sensor Location" 命令
// ----------------------------------------------------------------------------
// 参考 Bluetooth SIG "Cycling Speed and Cadence Profile" 1.0 §5.1：
//   写入 SC Control Point (0x2A55) 值，格式 = [OpCode][Parameter...]
//     OpCode 0x03 = Update Sensor Location，参数 = 1 字节 sensor location 值
//     OpCode 0x10 = Response Code (由远端通过 Indicate 回传)
//       响应格式 = [0x10][Request OpCode][Result Code][Response Parameter...]
//       Result Code：0x01=Success, 0x02=Op not supported, 0x03=Invalid parameter,
//                    0x04=Operation failed
//
// Sensor Location 取值（org.bluetooth.characteristic.sensor_location）：
//   0x06 = Right Crank（右曲柄）    —— 典型踏频安装位置
//   0x0C = Rear Wheel（后轮）       —— 典型速度安装位置
//   0x0D = Rear Hub（后轮花鼓）
//
// 对于 XOSS ARENA S1610 这类"单感"传感器：可通过切换 Sensor Location 在
// "测轮转(速度)"与"测曲柄转(踏频)"之间切换，无需厂家私有协议。
// ============================================================================

// SC CP Sensor Location 值（BT SIG 标准枚举）
// 参考 org.bluetooth.characteristic.sensor_location
#define BSP_BLE_LOC_OTHER           0x00
#define BSP_BLE_LOC_TOP_OF_SHOE     0x01
#define BSP_BLE_LOC_IN_SHOE         0x02
#define BSP_BLE_LOC_HIP             0x03
#define BSP_BLE_LOC_FRONT_WHEEL     0x04
#define BSP_BLE_LOC_LEFT_CRANK      0x05
#define BSP_BLE_LOC_RIGHT_CRANK     0x06
#define BSP_BLE_LOC_LEFT_PEDAL      0x07
#define BSP_BLE_LOC_RIGHT_PEDAL     0x08
#define BSP_BLE_LOC_FRONT_HUB       0x09
#define BSP_BLE_LOC_REAR_DROPOUT    0x0A
#define BSP_BLE_LOC_CHAINSTAY       0x0B
#define BSP_BLE_LOC_REAR_WHEEL      0x0C
#define BSP_BLE_LOC_REAR_HUB        0x0D
#define BSP_BLE_LOC_CHEST           0x0E

// SC CP Op Codes
#define BSP_BLE_SC_CP_OP_SET_CUMULATIVE  0x01
#define BSP_BLE_SC_CP_OP_START_CALIB     0x02
#define BSP_BLE_SC_CP_OP_UPDATE_LOCATION 0x03
#define BSP_BLE_SC_CP_OP_REQ_LOCATIONS   0x04
#define BSP_BLE_SC_CP_OP_RESPONSE        0x10

/**
 * @brief 判断某个 location 是 wheel 侧（速度）还是 crank 侧（踏频）
 * @return BSP_BLE_CSCS_MODE_SPEED / _CADENCE / _UNKNOWN（无法归类）
 */
static bsp_ble_cscs_mode_t bsp_ble_location_to_mode(uint8_t loc)
{
    switch (loc) {
        case BSP_BLE_LOC_FRONT_WHEEL:
        case BSP_BLE_LOC_FRONT_HUB:
        case BSP_BLE_LOC_REAR_DROPOUT:
        case BSP_BLE_LOC_REAR_WHEEL:
        case BSP_BLE_LOC_REAR_HUB:
            return BSP_BLE_CSCS_MODE_SPEED;
        case BSP_BLE_LOC_LEFT_CRANK:
        case BSP_BLE_LOC_RIGHT_CRANK:
        case BSP_BLE_LOC_LEFT_PEDAL:
        case BSP_BLE_LOC_RIGHT_PEDAL:
        case BSP_BLE_LOC_CHAINSTAY:
            return BSP_BLE_CSCS_MODE_CADENCE;
        default:
            return BSP_BLE_CSCS_MODE_UNKNOWN;
    }
}

static const char *bsp_ble_loc_name(uint8_t loc)
{
    switch (loc) {
        case BSP_BLE_LOC_OTHER:        return "Other";
        case BSP_BLE_LOC_TOP_OF_SHOE:  return "TopOfShoe";
        case BSP_BLE_LOC_IN_SHOE:      return "InShoe";
        case BSP_BLE_LOC_HIP:          return "Hip";
        case BSP_BLE_LOC_FRONT_WHEEL:  return "FrontWheel";
        case BSP_BLE_LOC_LEFT_CRANK:   return "LeftCrank";
        case BSP_BLE_LOC_RIGHT_CRANK:  return "RightCrank";
        case BSP_BLE_LOC_LEFT_PEDAL:   return "LeftPedal";
        case BSP_BLE_LOC_RIGHT_PEDAL:  return "RightPedal";
        case BSP_BLE_LOC_FRONT_HUB:    return "FrontHub";
        case BSP_BLE_LOC_REAR_DROPOUT: return "RearDropout";
        case BSP_BLE_LOC_CHAINSTAY:    return "Chainstay";
        case BSP_BLE_LOC_REAR_WHEEL:   return "RearWheel";
        case BSP_BLE_LOC_REAR_HUB:     return "RearHub";
        case BSP_BLE_LOC_CHEST:        return "Chest";
        default:                       return "?";
    }
}

/**
 * @brief SC Control Point 写入回调（Write With Response）
 * @note 仅表示"写请求被对端 ACK 了"；真正的命令执行结果要看 Indication。
 */
static int bsp_ble_sc_cp_write_cb(uint16_t conn_handle,
                                  const struct ble_gatt_error *error,
                                  struct ble_gatt_attr *attr,
                                  void *arg)
{
    (void)attr;
    (void)arg;
    if (error != NULL && error->status != 0) {
        const char *why = "Unknown";
        switch (error->status) {
            case 0x01: why = "Invalid Handle"; break;
            case 0x03: why = "Write Not Permitted"; break;
            case 0x07: why = "Invalid Attribute Length"; break;
            case 0x0D: why = "Invalid Attribute Value Length"; break;
            case 0x80: why = "CCCD Improperly Configured (Indicate not enabled?)"; break;
            default:   break;
        }
        ESP_LOGE(TAG, "SC CP write failed; conn=%d status=0x%02X (%s)",
                 conn_handle, error->status, why);
        return 0;
    }
    ESP_LOGI(TAG, "SC CP write ACK'd by peer; conn=%d — waiting for Indication response",
             conn_handle);
    return 0;
}

/**
 * @brief 主动发 Request Supported Sensor Locations (0x04)，让传感器告诉我们它支持哪些位置
 * @note 仅用于诊断日志——知道设备声称自己能落在哪些 location。
 *       切记：设备返回的 Success 对 Update Sensor Location 可能是假的（XOSS 实测），
 *       所以本驱动不再发 Update Sensor Location 命令，这里的查询只是拿信息看看。
 */
static int bsp_ble_sc_cp_request_supported_locations(device_connection_t *dev_conn)
{
    if (dev_conn == NULL || dev_conn->control_point_handle == 0) {
        return -1;
    }
    uint8_t cmd[1] = { BSP_BLE_SC_CP_OP_REQ_LOCATIONS };
    ESP_LOGI(TAG, "Requesting supported sensor locations (cmd=0x04)...");
    dev_conn->pending_sc_cp_op = BSP_BLE_SC_CP_OP_REQ_LOCATIONS;
    int rc = ble_gattc_write_flat(dev_conn->conn_handle,
                                  dev_conn->control_point_handle,
                                  cmd, sizeof(cmd),
                                  bsp_ble_sc_cp_write_cb, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "Failed to send 0x04 (Req Supported Locations); rc=%d", rc);
        dev_conn->pending_sc_cp_op = 0;
    }
    return rc;
}

/**
 * @brief 解析 SC Control Point Indication（由 notify_rx 派发进来）
 */
static void bsp_ble_handle_sc_cp_indication(device_connection_t *dev_conn,
                                            const uint8_t *data, int len)
{
    if (len < 3 || data[0] != BSP_BLE_SC_CP_OP_RESPONSE) {
        ESP_LOGW(TAG, "SC CP indication malformed (len=%d op=0x%02X)",
                 len, len > 0 ? data[0] : 0);
        return;
    }
    uint8_t req_op = data[1];
    uint8_t result = data[2];
    const char *result_str = "Unknown";
    switch (result) {
        case 0x01: result_str = "Success"; break;
        case 0x02: result_str = "Op Code Not Supported"; break;
        case 0x03: result_str = "Invalid Parameter"; break;
        case 0x04: result_str = "Operation Failed"; break;
        default:   break;
    }
    ESP_LOGI(TAG, "SC CP response: req_op=0x%02X result=0x%02X (%s), payload=%d bytes",
             req_op, result, result_str, len - 3);

    if (dev_conn == NULL) {
        return;
    }

    // ---- 支持位置列表查询的响应 ----
    if (req_op == BSP_BLE_SC_CP_OP_REQ_LOCATIONS) {
        dev_conn->supported_locations_count = 0;
        if (result == 0x01 && len > 3) {
            // 剩余字节 = 支持的位置值列表
            uint8_t n = (uint8_t)(len - 3);
            if (n > sizeof(dev_conn->supported_locations)) {
                n = (uint8_t)sizeof(dev_conn->supported_locations);
            }
            for (uint8_t i = 0; i < n; i++) {
                dev_conn->supported_locations[i] = data[3 + i];
            }
            dev_conn->supported_locations_count = n;
            ESP_LOGI(TAG, "Supported sensor locations (%u):", (unsigned)n);
            for (uint8_t i = 0; i < n; i++) {
                uint8_t loc = dev_conn->supported_locations[i];
                ESP_LOGI(TAG, "  - 0x%02X (%s)  [mode=%s]", loc, bsp_ble_loc_name(loc),
                         bsp_ble_location_to_mode(loc) == BSP_BLE_CSCS_MODE_SPEED   ? "SPEED"   :
                         bsp_ble_location_to_mode(loc) == BSP_BLE_CSCS_MODE_CADENCE ? "CADENCE" : "neutral");
            }
        } else if (result == 0x02) {
            ESP_LOGW(TAG, "Device does not support listing supported sensor locations");
        } else {
            ESP_LOGW(TAG, "Supported sensor locations query returned non-success (%s)",
                     result_str);
        }
        dev_conn->supported_locations_known = true; // 不管成功还是不支持，都算"已知"
        if (dev_conn->pending_sc_cp_op == BSP_BLE_SC_CP_OP_REQ_LOCATIONS) {
            dev_conn->pending_sc_cp_op = 0;
        }
        return;
    }

    // 其它 Op Code（含 Update Sensor Location 0x03 等）的响应只打印即可。
    // 本驱动不再主动发送 Update Sensor Location——XOSS ARENA 实测会撒谎回
    // Success 但不真切换；真正的硬件模式请用厂家手机 APP 设置。
    if (dev_conn->pending_sc_cp_op == req_op) {
        dev_conn->pending_sc_cp_op = 0;
    }
}

/**
 * @brief 选择 UI 的"显示偏好"：Speed 或 Cadence。
 * @note  XOSS ARENA 等单感传感器的硬件模式只能用厂家手机 APP 切换（通过私有
 *        NUS 通道）。标准 SC CP Update Sensor Location 在这类设备上会回假
 *        Success（实测），所以本接口**不**再做任何 BLE 侧的命令下发，仅更新
 *        本地 cscs_mode 让 UI 知道用哪个数据做大字体。真实的 speed / cadence
 *        两路数值始终由 CSC Measurement 的 flags 字节按位独立解析。
 */
esp_err_t bsp_ble_set_cscs_mode(bsp_ble_cscs_mode_t mode)
{
    device_connection_t *dev_conn = find_device_by_type(BSP_BLE_DEVICE_TYPE_CSCS);
    if (dev_conn == NULL || !dev_conn->is_connected) {
        ESP_LOGE(TAG, "CSCS device not connected");
        return ESP_ERR_INVALID_STATE;
    }
    if (mode != BSP_BLE_CSCS_MODE_SPEED && mode != BSP_BLE_CSCS_MODE_CADENCE) {
        ESP_LOGE(TAG, "Invalid mode: %d (only SPEED or CADENCE supported)", mode);
        return ESP_ERR_INVALID_ARG;
    }

    dev_conn->cscs_mode         = mode;
    dev_conn->mode_manually_set = true;

    s_last_wheel_revs = 0;
    s_last_wheel_time = 0;
    s_last_crank_revs = 0;
    s_last_crank_time = 0;
    s_current_speed   = 0.0f;
    s_current_cadence = 0.0f;

    ESP_LOGI(TAG, "CSCS display preference -> %s",
             (mode == BSP_BLE_CSCS_MODE_SPEED) ? "SPEED" : "CADENCE");
    ESP_LOGI(TAG, "  (hardware mode is NOT changed by this driver; use the vendor "
                  "phone app to physically switch XOSS between wheel/crank sensing)");
    return ESP_OK;
}

/**
 * @brief ??BLE??
 */
void bsp_ble_update(void)
{
    // Idle-zero: XOSS/CSCS sensors keep re-sending the last snapshot at ~1 Hz
    // even when the wheel/crank is still. Without this the UI would keep
    // showing the last non-zero reading forever. If no revolution delta has
    // been observed for BSP_BLE_IDLE_ZERO_MS, zero out the corresponding
    // channel so the dial/map falls back to 0 (or GPS speed via dp_sport).
    {
        uint32_t now_tick = xTaskGetTickCount();
        uint32_t now_ms   = now_tick * portTICK_PERIOD_MS;
        if (s_current_speed > 0.0f && s_last_wheel_motion_tick != 0) {
            uint32_t quiet_ms = (now_tick - s_last_wheel_motion_tick) * portTICK_PERIOD_MS;
            if (quiet_ms > BSP_BLE_IDLE_ZERO_MS) {
                s_current_speed = 0.0f;
            }
        }
        if (s_current_cadence > 0.0f && s_last_crank_motion_tick != 0) {
            uint32_t quiet_ms = (now_tick - s_last_crank_motion_tick) * portTICK_PERIOD_MS;
            if (quiet_ms > BSP_BLE_IDLE_ZERO_MS) {
                s_current_cadence = 0.0f;
            }
        }
        (void)now_ms;
    }

    if (s_cscs_waiting_data) {
        uint32_t elapsed_ms = (xTaskGetTickCount() - s_cscs_subscribe_tick) * portTICK_PERIOD_MS;
        static uint32_t last_warn_ms = 0;
        if (elapsed_ms > 10000 && (elapsed_ms - last_warn_ms) >= 10000) {
            ESP_LOGW(TAG, "[CSC watchdog] conn=%d subscribed %u ms ago, still NO measurement data "
                         "(notify_count=%u). Possible causes: "
                         "(1) sensor only sends data when the wheel/crank moves (rotate now); "
                         "(2) sensor uses a NON-standard service (check Discovered services list); "
                         "(3) CCCD write not accepted (some devices need SC Control Point 0x2A55 first); "
                         "(4) advertised 0x1816 but GATT does not expose 0x2A5B.",
                         s_cscs_conn_handle, (unsigned)elapsed_ms, (unsigned)s_cscs_notify_count);
            last_warn_ms = elapsed_ms;
        }
    }

    // ???????????????????????????????
}

/**
 * @brief ???BLE??
 */
esp_err_t bsp_ble_init(const bsp_ble_config_t *config)
{
    if (s_ble_initialized) {
        ESP_LOGW(TAG, "BLE already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing BLE driver...");

    // ???NimBLE??
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init nimble %d", ret);
        return ret;
    }

    // ??????
    ble_hs_cfg.reset_cb = bsp_ble_on_reset;
    ble_hs_cfg.sync_cb = bsp_ble_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // Init peer mempools. Note: the 2nd/3rd/4th args are GLOBAL pool sizes
    // (shared across all peers), NOT per-peer quotas. See peer.c:peer_init.
    ESP_LOGI(TAG, "Initializing peer management:");
    ESP_LOGI(TAG, "  Max peers: %d", MAX_BLE_DEVICES);
    ESP_LOGI(TAG, "  Total services pool:        %d (shared by all peers)", MAX_PEER_SERVICES);
    ESP_LOGI(TAG, "  Total characteristics pool: %d (shared by all peers)", MAX_PEER_CHARS);
    ESP_LOGI(TAG, "  Total descriptors pool:     %d (shared by all peers)", MAX_PEER_DESCS);

    int rc = peer_init(MAX_BLE_DEVICES, MAX_PEER_SERVICES, MAX_PEER_CHARS, MAX_PEER_DESCS);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to init peer; rc=%d", rc);
        if (rc == BLE_HS_ENOMEM) {
            ESP_LOGE(TAG, "Out of memory - try reducing memory pool sizes");
        }
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Peer management initialized successfully");

    // ????????????????
    ble_store_config_init();

    // ????????????
    s_scanned_devices.mutex = xSemaphoreCreateMutex();
    if (s_scanned_devices.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    s_scanned_devices.count = 0;

    // ??????????
    memset(s_device_connections, 0, sizeof(s_device_connections));
    for (int i = 0; i < MAX_BLE_DEVICES; i++) {
        s_device_connections[i].cscs_mode = BSP_BLE_CSCS_MODE_UNKNOWN;
        s_device_connections[i].csc_feature_read = false;
        s_device_connections[i].sensor_location_handle = 0;
        s_device_connections[i].control_point_handle = 0;
        s_device_connections[i].control_point_cccd_handle = 0;
        s_device_connections[i].sc_cp_indicate_enabled = false;
        s_device_connections[i].pending_sc_cp_op = 0;
    }

    // ??NimBLE????
    nimble_port_freertos_init(bsp_ble_host_task);

    s_ble_initialized = true;
    ESP_LOGI(TAG, "BLE driver initialized successfully");

    // ??LVGL??????????????
    if (config != NULL) {
        extern void bsp_ble_ui_set_lvgl_lock_functions(bsp_ble_lvgl_lock_fn_t lock_fn, bsp_ble_lvgl_unlock_fn_t unlock_fn);
        bsp_ble_ui_set_lvgl_lock_functions(config->lvgl_lock, config->lvgl_unlock);
    }

    // ????????????????
    if (config != NULL && config->enable_scan) {
        vTaskDelay(pdMS_TO_TICKS(1000));  // ??????
        bsp_ble_start_scan(0);
    }

    return ESP_OK;
}
