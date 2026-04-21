/**
 * @file lgfx_sss.hpp
 * @brief LovyanGFX配置文件 - SSS项目专用
 * @note 参考version2项目的LGFX_ChappieCore.hpp，针对ILI9341 2.8寸SPI屏幕
 *       引脚和参数与version2完全一致，已验证显示正常
 */
#pragma once

#define LGFX_USE_V1

#include <LovyanGFX.hpp>

class LGFX_SSS : public lgfx::LGFX_Device
{
    // ILI9341面板实例
    lgfx::Panel_ILI9341 _panel_instance;

    // SPI总线实例
    lgfx::Bus_SPI _bus_instance;

    // PWM背光实例
    lgfx::Light_PWM _light_instance;

public:
    LGFX_SSS(void)
    {
        // ============ SPI总线配置 ============
        {
            auto cfg = _bus_instance.config();

            cfg.spi_host = SPI3_HOST;          // 使用SPI3_HOST（与SD卡的SPI2_HOST独立）
            cfg.spi_mode = 0;                  // SPI模式0
            cfg.freq_write = 40000000;         // 写入频率40MHz（与version2一致）
            cfg.freq_read  = 16000000;         // 读取频率16MHz
            cfg.spi_3wire = false;             // 4线SPI
            cfg.use_lock   = true;             // 使用事务锁
            cfg.dma_channel = SPI_DMA_CH_AUTO; // DMA自动选择
            cfg.pin_sclk = 18;                 // SPI时钟
            cfg.pin_mosi = 17;                 // SPI数据输出
            cfg.pin_miso = 3;                  // SPI数据输入
            cfg.pin_dc   = 16;                 // 数据/命令

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // ============ 面板配置 ============
        {
            auto cfg = _panel_instance.config();

            cfg.pin_cs           =    7;       // LCD_CS -> IO7
            cfg.pin_rst          =   15;       // LCD_RST -> IO15
            cfg.pin_busy         =   -1;       // 无busy引脚

            cfg.panel_width      =  240;       // 屏幕宽度
            cfg.panel_height     =  320;       // 屏幕高度
            cfg.memory_width     =  240;       // 显存宽度（必须与panel_width一致）
            cfg.memory_height    =  320;       // 显存高度（必须与panel_height一致）
            cfg.offset_x         =    0;       // X偏移
            cfg.offset_y         =    0;       // Y偏移
            cfg.offset_rotation  =    0;       // 旋转偏移
            cfg.dummy_read_pixel =    8;       // 像素读取前的dummy位数
            cfg.dummy_read_bits  =    1;       // 数据读取前的dummy位数
            cfg.readable         = true;       // 支持数据读取
            cfg.invert           = false;      // 颜色不反转
            cfg.rgb_order        = true;       // BGR顺序（厂商0x36=0x08）
            cfg.dlen_16bit       = false;      // 非16位数据长度模式
            cfg.bus_shared       = false;       // SD卡使用独立SPI2_HOST，不共享

            _panel_instance.config(cfg);
        }

        // ============ 旋转设置 ============
        {
            _panel_instance.setRotation(0);    // 竖屏模式 (240x320)
        }

        // ============ 背光配置 ============
        {
            auto cfg = _light_instance.config();

            cfg.pin_bl = 8;                    // 背光引脚 IO8
            cfg.invert = false;                // 不反转
            cfg.freq   = 44100;                // PWM频率
            cfg.pwm_channel = 7;               // PWM通道

            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};
