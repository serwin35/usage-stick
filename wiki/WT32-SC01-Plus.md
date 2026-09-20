# WT32-SC01 Plus

Part of [Usage Stick](Home.md). This is the 3.5" ESP32-S3 touch board used to develop and verify the Claude + Codex Dust firmware in this fork.

## Specs

| | |
| --- | --- |
| MCU | ESP32-S3-WROVER N16R2 (dual-core, WiFi, 16 MB flash, 2 MB QSPI PSRAM) |
| Display | 3.5" ST7796UI IPS, 480×320, 8-bit parallel |
| Touch | FT6336U capacitive, I²C (SDA 6, SCL 5) |
| Battery | Not available |
| Buttons | Touch zones; enclosure reset button |
| USB | Native ESP32-S3 USB |
| Firmware | ✨ **Dust (v3.1.0)** · tier XL · Claude + Codex |
| PlatformIO env | `wt32-sc01-plus` |

## Flash

Easiest: open the **[web flasher](https://serwin35.github.io/usage-stick/)** in Chrome, Edge or Firefox, select **WT32-SC01 Plus**, and connect the board over USB-C.

From source, with the [PlatformIO CLI](https://platformio.org/install/cli):

```bash
pio run -e wt32-sc01-plus -t upload
```

## Controls

The two-button interface maps to the left and right halves of the touch panel:

| Touch | Acts as | PIN entry | Dashboard |
| ----- | ------- | --------- | --------- |
| Tap **LEFT** half | Button A | Cycle the current digit | Step through screens |
| Tap **RIGHT** half | Button B | Confirm digit | Force refresh |

## Notes

- The firmware uses LovyanGFX because the parallel data bus spans GPIOs that TFT_eSPI cannot address with its register-bitmask driver.
- The LCD and FT6336U touch controller share GPIO 4 as reset.
- There is no two-button factory-reset gesture. Reflash the board to wipe stored settings.
- The default control panel address is `http://usage-stick.local`.

---

**WT32-SC01 Plus** · env `wt32-sc01-plus` · [All boards](Supported-Boards.md) · [Flashing](Flashing.md) · [Setup and daily use](Setup-and-Daily-Use.md) · [Troubleshooting](Troubleshooting.md)
