idf_component_register(
    SRCS "hal.cpp" "hal_lcd.cpp" "hal_touch.cpp" "hal_gps.cpp" "hal_imu.cpp" "hal_power.cpp" "hal_ble.cpp" "hal_rgb.cpp"
    INCLUDE_DIRS "include"
    REQUIRES bsp_lcd bsp_touch bsp_gps bsp_imu bsp_power bsp_ble bsp_rgb_led bt
)
