#pragma once

// Wireless-Tag / Smart Panlee WT32-SC01 Plus (ZX3D50CE08S-USRC-4832).
//
// TFT_eSPI cannot drive this panel: its ESP32-S3 parallel driver bit-bangs the 8 data
// lines through a single 32-bit GPIO register, which requires all 8 pins to sit in the
// same half of the GPIO range (0-31 or 32-48). This board's data bus mixes both — D1 is
// GPIO46 while the rest are <32 — so TFT_eSPI silently drives the wrong register bit for
// D1 (confirmed at compile time: the bit-mask macro overflows a 32-bit int for GPIO46).
// LovyanGFX's Bus_Parallel8 instead drives the bus through the GPIO matrix, so pins can
// be any mix of ranges. Its LGFX_TFT_eSPI.hpp compatibility header aliases `TFT_eSPI` and
// `TFT_eSprite` to LovyanGFX types, so hal.cpp/ui.cpp don't need board-specific code paths
// beyond this file and the touch handling in hal.cpp.
//
// Pin mapping is from Wireless-Tag's own datasheet, cross-checked against openHASP's
// verified TFT_eSPI/LovyanGFX config for this exact board. Not yet confirmed on this
// specific unit — verify colors (invert/rgb_order), touch axes and rotation on hardware.

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7796   _panel_instance;
    lgfx::Bus_Parallel8  _bus_instance;
    lgfx::Light_PWM      _light_instance;
    lgfx::Touch_FT5x06   _touch_instance;  // covers FT6206/FT6236/FT6336/FT6436 too

public:
    LGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.freq_write = 20000000;
            cfg.pin_wr     = 47;
            cfg.pin_rd     = -1;  // not broken out on this board — write-only bus
            cfg.pin_rs     = 0;   // LCD_RS / D-C (shared with the GPIO0 BOOT strap)
            cfg.pin_d0     = 9;
            cfg.pin_d1     = 46;
            cfg.pin_d2     = 3;
            cfg.pin_d3     = 8;
            cfg.pin_d4     = 18;
            cfg.pin_d5     = 17;
            cfg.pin_d6     = 16;
            cfg.pin_d7     = 15;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs    = -1;  // no CS on this board — single fixed panel
            cfg.pin_rst   = 4;   // shared with the touch controller's reset line
            cfg.pin_busy  = -1;
            cfg.panel_width  = 320;
            cfg.panel_height = 480;
            cfg.offset_rotation = 0;
            cfg.readable  = false;  // no RD line wired
            cfg.invert    = false;  // confirmed on hardware: true turns the header blue again
            cfg.rgb_order = true;   // confirmed on hardware: red/blue were swapped at false
            cfg.bus_shared = false;
            _panel_instance.config(cfg);
        }
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = 45;
            cfg.invert = false;
            cfg.freq   = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        {
            auto cfg = _touch_instance.config();
            cfg.x_min = 0;   cfg.x_max = 319;   // native panel_width
            cfg.y_min = 0;   cfg.y_max = 479;   // native panel_height
            cfg.pin_int    = 7;
            cfg.bus_shared = false;
            cfg.offset_rotation = 0;  // TODO(hardware): adjust if touch/display axes disagree
            cfg.i2c_port = 1;
            cfg.i2c_addr = 0x38;
            cfg.pin_sda  = 6;
            cfg.pin_scl  = 5;
            cfg.freq     = 400000;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }
        setPanel(&_panel_instance);
    }
};

#include <LGFX_TFT_eSPI.hpp>
