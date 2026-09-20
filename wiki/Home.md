# Usage Stick

**Your [Claude Code](https://docs.anthropic.com/en/docs/claude-code) rate limits, glanceable on a tiny ESP32 stick.**

A standalone desk gadget that polls the Anthropic API and shows your rate-limit usage in real time — no computer, no app, no cloud. Flash it, connect it to WiFi from your phone, and it just sits there telling you how much runway you have left.

<img src="https://github.com/user-attachments/assets/c51c5a9e-5a3e-4c3e-97a8-d5a4c5a44263" width="640" alt="Usage Stick — PIN unlock screen and usage dashboard on a LilyGo T-Display S3">

## Start here

| | |
| --- | --- |
| **New here?** | [Features](Features.md) — what the device actually shows |
| **Ready to build one?** | [Supported boards](Supported-Boards.md), then [Flashing](Flashing.md) |
| **Just flashed it?** | [Setup and daily use](Setup-and-Daily-Use.md) |
| **Something's wrong?** | [Troubleshooting](Troubleshooting.md) |

The fastest path to a working device: pick a board from the [supported list](Supported-Boards.md), open the **[web flasher](https://serwin35.github.io/usage-stick/)** in Chrome, Edge or Firefox, and plug the board in over USB-C. No toolchain, no drivers, no clone.

## All pages

**Using the device**
- [Features](Features.md) — usage bars, reset countdowns, model mascots, PIN lock, screen modes
- [The UI](The-UI.md) — firmware versions (Clarity, Mango, Dust) and how the layout adapts per screen size
- [Web panel](Web-Panel.md) — the in-browser control panel: settings, token rotation, WiFi (Dust boards)
- [Setup and daily use](Setup-and-Daily-Use.md) — captive-portal setup, PIN entry, button controls
- [Troubleshooting](Troubleshooting.md) — flashing, WiFi, token, and display problems

**Hardware**
- [Supported boards](Supported-Boards.md) — the full lineup, with a guide for each
- Board guides: [M5StickC Plus](M5StickC-Plus.md) · [M5StickC Plus2](M5StickC-Plus2.md) · [LilyGo T-Display S3](LilyGo-T-Display-S3.md) · [LilyGo T8 ESP32-S2](LilyGo-T8-ESP32-S2.md) · [CrowPanel Advance 3.5"](CrowPanel-Advance-35.md) · [WT32-SC01 Plus](WT32-SC01-Plus.md) · [T-Display S3 AMOLED](LilyGo-T-Display-S3-AMOLED.md) · [TTGO T-Display](TTGO-T-Display-ESP32.md) · [ESP32-C3-OLED](ESP32-C3-OLED.md)

**Under the hood**
- [Flashing](Flashing.md) — the web flasher and building from source
- [How it works](How-It-Works.md) — the API request and the headers it reads
- [Security](Security.md) — how your OAuth token is encrypted on-device
- [Project structure](Project-Structure.md) — repo layout and how to add a board

## Project links

- [Repository](https://github.com/serwin35/usage-stick) · [Web flasher](https://serwin35.github.io/usage-stick/) · [Issues](https://github.com/serwin35/usage-stick/issues)
- License: [MIT](https://github.com/serwin35/usage-stick/blob/main/LICENSE)
