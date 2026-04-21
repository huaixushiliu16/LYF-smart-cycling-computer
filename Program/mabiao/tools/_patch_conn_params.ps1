$ErrorActionPreference = 'Stop'
$path = 'components\bsp_ble\bsp_ble.c'

$enc = [System.Text.Encoding]::GetEncoding(936)  # GBK
$content = [System.IO.File]::ReadAllText($path, $enc)
$orig = $content

function Apply-Replace([string]$old, [string]$new, [string]$marker) {
    $script:content = $script:content
    if ($script:content.Contains($new)) {
        Write-Host "[SKIP already-applied] $marker"
        return
    }
    $idx = $script:content.IndexOf($old)
    if ($idx -lt 0) { throw "Anchor NOT FOUND: $marker" }
    $idx2 = $script:content.IndexOf($old, $idx + 1)
    if ($idx2 -ge 0) { throw "Anchor NOT UNIQUE for: $marker" }
    $script:content = $script:content.Substring(0, $idx) + $new + $script:content.Substring($idx + $old.Length)
    Write-Host "[OK] $marker"
}

# ---- 1) supervision_timeout: 500 ms -> 5000 ms ----
$old1 = '    conn_params.supervision_timeout = BLE_GAP_SUPERVISION_TIMEOUT_MS(500);'
$new1 = '    // 5000ms: fitness sensors (e.g. CSCS) may be radio-idle for up to several seconds between wheel revolutions; 500ms was too aggressive and risked spurious link drops.' + "`r`n" +
        '    conn_params.supervision_timeout = BLE_GAP_SUPERVISION_TIMEOUT_MS(5000);'
Apply-Replace $old1 $new1 'supervision_timeout 500->5000'

# ---- 2) fix display bug: scan interval ----
$old2 = '    ESP_LOGI(TAG, "    Scan Interval: %d ms", BLE_GAP_SCAN_ITVL_MS(conn_params.scan_itvl));'
$new2 = '    ESP_LOGI(TAG, "    Scan Interval: %u ms", (unsigned)(conn_params.scan_itvl * 625 / 1000));'
Apply-Replace $old2 $new2 'print scan_itvl'

# ---- 3) fix display bug: scan window ----
$old3 = '    ESP_LOGI(TAG, "    Scan Window: %d ms", BLE_GAP_SCAN_WIN_MS(conn_params.scan_window));'
$new3 = '    ESP_LOGI(TAG, "    Scan Window: %u ms", (unsigned)(conn_params.scan_window * 625 / 1000));'
Apply-Replace $old3 $new3 'print scan_window'

# ---- 4) fix display bug: connection interval ----
$old4 = @"
    ESP_LOGI(TAG, "    Connection Interval: %d-%d ms",
             BLE_GAP_CONN_ITVL_MS(conn_params.itvl_min),
             BLE_GAP_CONN_ITVL_MS(conn_params.itvl_max));
"@
$new4 = @"
    ESP_LOGI(TAG, "    Connection Interval: %u-%u ms",
             (unsigned)(conn_params.itvl_min * 5 / 4),
             (unsigned)(conn_params.itvl_max * 5 / 4));
"@
Apply-Replace $old4 $new4 'print conn itvl'

# ---- 5) fix display bug: supervision timeout ----
$old5 = @"
    ESP_LOGI(TAG, "    Supervision Timeout: %d ms",
             BLE_GAP_SUPERVISION_TIMEOUT_MS(conn_params.supervision_timeout));
"@
$new5 = @"
    ESP_LOGI(TAG, "    Supervision Timeout: %u ms",
             (unsigned)(conn_params.supervision_timeout * 10));
"@
Apply-Replace $old5 $new5 'print supervision_timeout'

if ($content -eq $orig) {
    Write-Host "`nNo changes (all already applied)."
} else {
    [System.IO.File]::WriteAllText($path, $content, $enc)
    Write-Host "`nWrote $path ($($content.Length) chars)"
}
