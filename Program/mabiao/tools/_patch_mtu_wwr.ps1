$ErrorActionPreference = 'Stop'
$path = 'components\bsp_ble\bsp_ble.c'

$enc = [System.Text.Encoding]::GetEncoding(936)  # GBK
$content = [System.IO.File]::ReadAllText($path, $enc)
$orig = $content

function Apply-Replace([string]$old, [string]$new, [string]$marker) {
    if ($script:content.Contains($new)) {
        Write-Host "[SKIP already-applied] $marker"
        return
    }
    $idx = $script:content.IndexOf($old)
    if ($idx -lt 0) { throw "Anchor NOT FOUND: $marker" }
    $idx2 = $script:content.IndexOf($old, $idx + 1)
    if ($idx2 -ge 0) { throw "Anchor NOT UNIQUE for: $marker (2+ occurrences)" }
    $script:content = $script:content.Substring(0, $idx) + $new + $script:content.Substring($idx + $old.Length)
    Write-Host "[OK] $marker"
}

# ---------- Patch 1: add MTU exchange callback (after log_hex_dump) ----------
$old1 = @"
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
    ESP_LOGI(TAG, "%s len=%d | %s%s",
             prefix, len, buf, (len > max) ? " ...(truncated)" : "");
}
"@

$new1 = @"
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
    ESP_LOGI(TAG, "%s len=%d | %s%s",
             prefix, len, buf, (len > max) ? " ...(truncated)" : "");
}

/**
 * MTU exchange callback. Some sensors require an MTU exchange handshake before
 * they will respond to subsequent GATT operations; we do this early after connect.
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
        ESP_LOGW(TAG, "MTU exchange failed: conn_handle=%u status=%d",
                 (unsigned)conn_handle, error->status);
    }
    return 0;
}
"@
Apply-Replace $old1 $new1 'add bsp_ble_mtu_exchange_cb'

# ---------- Patch 2: initiate MTU exchange after peer_add ----------
# Anchor is ASCII-only single line (unique: only 1 occurrence in the file).
# We append the MTU exchange block *after* the "Peer added successfully" log line.
$old2 = '            ESP_LOGI(TAG, "Peer added successfully");'
$new2 = @'
            ESP_LOGI(TAG, "Peer added successfully");

            // Initiate MTU exchange early. Some sensors (e.g. XOSS ARENA S1610)
            // will not properly respond to subsequent GATT writes until the client
            // has negotiated MTU; skipping this can leave Write Requests unanswered
            // and ultimately trigger the ATT 30s transaction timeout (link teardown).
            {
                int _mtu_rc = ble_gattc_exchange_mtu(event->connect.conn_handle,
                                                    bsp_ble_mtu_exchange_cb, NULL);
                if (_mtu_rc != 0) {
                    ESP_LOGW(TAG, "Failed to start MTU exchange; rc=%d (continuing)", _mtu_rc);
                } else {
                    ESP_LOGI(TAG, "MTU exchange initiated");
                }
            }
'@
Apply-Replace $old2 $new2 'add MTU exchange after peer_add'

# ---------- Patch 3: CSC Measurement CCCD -> Write Without Response ----------
# Anchor skips the preceding Chinese comment line (encoding-sensitive); uniqueness
# is guaranteed by the trailing ESP_LOGE text "CSC Measurement" combined with the
# rest of the block.
$old3 = @'
    uint8_t value[2] = {0x01, 0x00};
    int rc = ble_gattc_write_flat(peer->conn_handle, dsc->dsc.handle,
                                   value, sizeof(value),
                                   NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error: Failed to subscribe to CSC Measurement; rc=%d", rc);
'@

$new3 = @'
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
'@
Apply-Replace $old3 $new3 'CSCS CCCD -> WriteWithoutResponse'

# Patch 3b: also update the success-side log message to reflect WWR
$old3b = '        ESP_LOGI(TAG, "Subscribed to CSC Measurement notifications");'
$new3b = '        ESP_LOGI(TAG, "Subscribed to CSC Measurement notifications (Write Without Response)");'
Apply-Replace $old3b $new3b 'CSCS subscribe success log'

# ---------- Patch 4: HR Measurement CCCD -> Write Without Response ----------
$old4 = @'
    uint8_t value[2] = {0x01, 0x00};
    int rc = ble_gattc_write_flat(peer->conn_handle, dsc->dsc.handle,
                                   value, sizeof(value),
                                   NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error: Failed to subscribe to Heart Rate Measurement; rc=%d", rc);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    } else {
        ESP_LOGI(TAG, "Subscribed to Heart Rate Measurement notifications");
    }
'@

$new4 = @'
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
'@
Apply-Replace $old4 $new4 'HR CCCD -> WriteWithoutResponse'

if ($content -eq $orig) {
    Write-Host "`nNo changes (all already applied)."
} else {
    [System.IO.File]::WriteAllText($path, $content, $enc)
    Write-Host "`nWrote $path ($($content.Length) chars)"
}
