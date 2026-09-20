// Standalone color-swatch picker for the WT32-SC01 Plus header color.
// Build/flash with: pio run -e wt32-sc01-plus-colorpick -t upload
// Not part of the real firmware — pick a swatch, then set C_HEAD in ui.cpp to
// its hex value and rebuild the normal wt32-sc01-plus env.
#include "../../src/lgfx_wt32_sc01_plus.h"

TFT_eSPI lcd;

#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

struct Swatch {
    const char* label;
    uint8_t r, g, b;
};

static const Swatch SWATCHES[] = {
    {"1 EB8700 (current)", 0xEB, 0x87, 0x00},
    {"2 DA7756 (Anthropic clay)", 0xDA, 0x77, 0x56},
    {"3 CC785C (Anthropic crail)", 0xCC, 0x78, 0x5C},
    {"4 E07B39", 0xE0, 0x7B, 0x39},
    {"5 FF8C00 dark orange", 0xFF, 0x8C, 0x00},
    {"6 FF7F50 coral", 0xFF, 0x7F, 0x50},
    {"7 FF6B35", 0xFF, 0x6B, 0x35},
    {"8 F2994A", 0xF2, 0x99, 0x4A},
    {"9 EA580C", 0xEA, 0x58, 0x0C},
    {"10 FB923C", 0xFB, 0x92, 0x3C},
    {"11 C2410C deep", 0xC2, 0x41, 0x0C},
    {"12 FFA500 classic", 0xFF, 0xA5, 0x00},
};
static const int N = sizeof(SWATCHES) / sizeof(SWATCHES[0]);

// Head-to-head: swatches 5 (FF8C00) and 12 (FFA500) only, full-height halves.
#define HEAD_TO_HEAD 1

void setup() {
    lcd.init();
    lcd.setRotation(3);
    lcd.setBrightness(200);

#if HEAD_TO_HEAD
    const int idx[2] = {4, 11};   // 0-based: swatches 5 and 12
    for (int half = 0; half < 2; half++) {
        const Swatch& s = SWATCHES[idx[half]];
        int x = half * 240;
        uint16_t color = RGB565(s.r, s.g, s.b);
        lcd.fillRect(x, 0, 240, 320, color);

        float lum = 0.299f * s.r + 0.587f * s.g + 0.114f * s.b;
        uint16_t textCol = (lum > 140) ? RGB565(0, 0, 0) : RGB565(255, 255, 255);
        lcd.setTextColor(textCol, color);
        lcd.setTextSize(2);
        lcd.setCursor(x + 10, 10);
        lcd.print(s.label);
    }
#else
    const int cols = 3, rows = 4;
    const int cellW = 480 / cols, cellH = 320 / rows;

    for (int i = 0; i < N; i++) {
        int col = i % cols, row = i / cols;
        int x = col * cellW, y = row * cellH;
        uint16_t color = RGB565(SWATCHES[i].r, SWATCHES[i].g, SWATCHES[i].b);
        lcd.fillRect(x, y, cellW, cellH, color);

        float lum = 0.299f * SWATCHES[i].r + 0.587f * SWATCHES[i].g + 0.114f * SWATCHES[i].b;
        uint16_t textCol = (lum > 140) ? RGB565(0, 0, 0) : RGB565(255, 255, 255);
        lcd.setTextColor(textCol, color);
        lcd.setTextSize(1);
        lcd.setCursor(x + 4, y + 4);
        lcd.print(SWATCHES[i].label);
    }
#endif
}

void loop() {}
