idf_component_register(SRCS "main.c" "lvgl_port.cpp" "lv_port_fs.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash bsp_lcd bsp_touch bsp_sd bsp_imu bsp_ip5306 bsp_rgb_led bsp_buzzer hal_layer app heap esp_timer bt lvgl LovyanGFX)
