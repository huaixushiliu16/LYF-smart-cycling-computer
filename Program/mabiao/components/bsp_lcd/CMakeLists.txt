idf_component_register(
    SRCS "bsp_lcd.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd LovyanGFX
)
