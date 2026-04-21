idf_component_register(
    SRCS "bsp_ble.c" "bsp_ble_ui.c" "peer.c"
    INCLUDE_DIRS "include"
    REQUIRES driver bt nvs_flash esp_timer freertos lvgl bsp_gps bsp_imu hal_layer bsp_buzzer
)
