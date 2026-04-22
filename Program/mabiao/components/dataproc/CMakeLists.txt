idf_component_register(
    SRCS "dp_storage.cpp" "dataproc.cpp" 
         "dp_gps.cpp" 
         "dp_imu.cpp" 
         "dp_sport.cpp" 
         "dp_power.cpp" 
         "dp_ble.cpp"
         "dp_statusbar.cpp"
         "dp_sysconfig.cpp"
         "dp_trackfilter.cpp"
         "dp_rgb.cpp"
         "utils/datacenter/Account.cpp"
         "utils/datacenter/DataCenter.cpp"
         "utils/datacenter/PingPongBuffer/PingPongBuffer.c"
    INCLUDE_DIRS "utils/datacenter"
                 "utils/datacenter/PingPongBuffer"
                 "include"
    REQUIRES hal_layer lvgl bsp_sd app bsp_buzzer
)

# 设置C++标准
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_11)

# 忽略LVGL枚举类型位运算的弃用警告（LVGL API使用枚举类型位运算，这是正常的）
# 只对C++文件应用此选项（C文件不支持此选项）
target_compile_options(${COMPONENT_LIB} PRIVATE 
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-enum-conversion>
)