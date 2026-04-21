idf_component_register(
    SRCS "bsp_power.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos bsp_lcd esp_adc
)
