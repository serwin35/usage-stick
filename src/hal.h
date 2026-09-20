#pragma once
#include <stdint.h>

#ifdef BOARD_ESP32C3_OLED
  #include <U8g2lib.h>
  #include <Wire.h>
  // SSD1306 128×64 controller; 72×40 pixels physically wired. Use NONAME init
  // sequence — the ER variant has a different command set that silently fails.
  extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
#elif defined(BOARD_TDISPLAY_S3)
  #include <TFT_eSPI.h>
  extern TFT_eSPI lcd;
#elif defined(BOARD_TDISPLAY_ESP32)
  #include <TFT_eSPI.h>
  extern TFT_eSPI lcd;
#elif defined(BOARD_T8_S2)
  #include <TFT_eSPI.h>
  extern TFT_eSPI lcd;
#elif defined(BOARD_CROWPANEL_ADV_35)
  #include <TFT_eSPI.h>
  #include <Wire.h>
  #include <TAMC_GT911.h>
  extern TFT_eSPI lcd;
#elif defined(BOARD_WT32_SC01_PLUS)
  // Not TFT_eSPI — see src/lgfx_wt32_sc01_plus.h for why. That header pulls in
  // LovyanGFX and aliases TFT_eSPI/TFT_eSprite to it via LGFX_TFT_eSPI.hpp, so the
  // rest of this codebase (ui.cpp included) doesn't need to know the difference.
  #include "lgfx_wt32_sc01_plus.h"
  extern TFT_eSPI lcd;
#elif defined(BOARD_TDISPLAY_S3_AMOLED)
  #include <LilyGo_AMOLED.h>
  #include <TFT_eSPI.h>
  extern LilyGo_Class amoled;
  extern TFT_eSprite  spr;
  #define lcd spr
#elif defined(BOARD_M5STICK_C_PLUS2)
  #include <M5Unified.h>
  #define lcd M5.Display
#else
  #include <M5StickCPlus.h>
  #ifdef lcd
    #undef lcd
  #endif
  #define lcd M5.Lcd
#endif

void halInit();
void halUpdate();
bool halBtnAWasPressed();
bool halBtnBWasPressed();
bool halBtnAIsPressed();
bool halBtnBIsPressed();
int  halBatPercent();
void halSetBrightness(uint8_t level);
void halFlush();
void halClear(uint16_t color);

#ifdef BOARD_WT32_SC01_PLUS
// Short alert tone through the onboard I2S amp — the only board in this repo
// with a speaker. Blocks for the duration of the tone (~150ms).
void halBeep();
#endif
