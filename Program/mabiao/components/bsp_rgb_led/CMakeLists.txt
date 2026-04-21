idf_component_register(
    SRCS "bsp_rgb_led.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos led_strip
)
