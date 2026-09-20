<div align="center">

# usage-stick

**Your [Claude](https://docs.anthropic.com/en/docs/claude-code) and [Codex](https://github.com/openai/codex) rate limits, glanceable on a tiny ESP32 stick.**

A fork of [claude-usage-stick](https://github.com/oauramos/claude-usage-stick) with Codex 5-hour / 7-day / Extra Usage screens (same windows [Usage4Claude](https://github.com/f-is-h/Usage4Claude) shows in the macOS menu bar). Tokens stay on the device, PIN-encrypted — nothing in this repo is a credential.

[![Web Flasher](https://img.shields.io/badge/⚡_web_flasher-flash_from_your_browser-D97757?style=flat-square)](https://serwin35.github.io/usage-stick/)
[![Wiki](https://img.shields.io/badge/docs-wiki-4c8eda?style=flat-square)](wiki/)
[![Boards](https://img.shields.io/badge/boards-8_supported-44cc11?style=flat-square)](wiki/Supported-Boards.md)
[![License](https://img.shields.io/badge/license-MIT-blue?style=flat-square)](LICENSE)

<a href="https://serwin35.github.io/usage-stick/"><img src="assets/hero.gif" width="820" alt="Claude Usage Stick — a LilyGo T-Display S3 turning slowly, its screen showing the 5-hour and 7-day usage bars, reset countdowns and the four model mascots"/></a>

## [⚡ Flash your ESP32 — right from your browser](https://serwin35.github.io/usage-stick/)

*No toolchain, no drivers, no clone — just Chrome, Edge or Firefox and a USB-C cable.*

*Claude + Codex 5-hour & 7-day windows · Codex Extra/credits · reset countdowns · model health mascots · PIN-encrypted tokens · web control panel · screen carousel with 7-day charts, Anthropic news & clock (✨ Dust, v3.1)*

</div>

---

A standalone desk gadget that polls Anthropic and (optionally) ChatGPT/Codex and shows your subscription rate-limit usage in real time — no computer, no app, no cloud. Flash it, connect it to WiFi from your phone, and it just sits there telling you how much runway you have left.

Codex is optional: paste a Codex CLI `refresh_token` (`rt.…` from `~/.codex/auth.json`) in the web panel. Claude-only setups stay unchanged.

## What it does

**On every board**

- **Live usage bars** — Claude 5-hour and 7-day windows from the `anthropic-ratelimit-unified-*` headers, refreshed every 30 s – 5 min
- **Codex screens (Dust)** — matching 5-hour / 7-day bars plus Extra Usage credits, a separate 7-day chart, teal header so it is obvious which provider you are looking at
- **Reset countdowns** — exactly how long until each window frees up
- **Model health mascots** — Haiku / Sonnet / Opus / Fable from status.claude.com as blinking Clawds (Mango v2+)
- **PIN-encrypted token** — AES-256-GCM on the device's own flash; the PIN is never stored, and 10 wrong tries wipes it
- **Captive-portal setup** — no hardcoded credentials, no serial console; configure it from your phone

**✨ New in Dust (v3 — [T-Display S3](wiki/LilyGo-T-Display-S3.md) and [M5StickC Plus](wiki/M5StickC-Plus.md))**

- **[Web control panel](wiki/Web-Panel.md)** — the stick serves its own settings page at `http://claude-usage-stick.local`: every setting, WiFi changes and factory reset from any browser on your LAN. Logging in with the PIN also unlocks the screen
- **Token rotation without a reset** — paste a fresh `claude setup-token`, confirmed with your PIN and verified live against the API; the stored token is write-only
- **Screen modes** — static, carousel (dwell 5–30 s) or desk clock; Button A steps through every screen
- **7-day usage chart** — one sample every 30 minutes, persisted on-device; time spent off shows as honest gaps
- **Anthropic news** — the latest headlines, streamed from the official feed every 6 h
- **Clock & timezone** — local time on the clock screen and on the chart's day markers
- **WiFi recovery** — an unreachable network opens a reconfigure portal (token, PIN and settings survive) instead of a reboot loop
- **Quality of life** — brightness and screen flip persist across reboots, the header alternates device name ↔ address, mDNS, and the board's full 16 MB flash + 8 MB PSRAM are finally in use

## Get one running

### ⚡ Flash it from your browser

**[serwin35.github.io/usage-stick](https://serwin35.github.io/usage-stick/)** — pick your board, plug it in over USB-C, hit Flash. Nothing to install; any desktop browser with Web Serial — Chrome, Edge, Opera, or Firefox 151+.

### Or build from source

```bash
git clone https://github.com/serwin35/usage-stick.git
cd usage-stick

pio run -e <env> -t upload      # firmware
```

Needs the [PlatformIO CLI](https://platformio.org/install/cli). Board envs are listed in [Supported boards](wiki/Supported-Boards.md).

Either way, the device then opens its own WiFi network so you can configure it from your phone — see [Setup and daily use](wiki/Setup-and-Daily-Use.md).

## Documentation

Everything lives in [`wiki/`](wiki/) in this repo:

| | |
| --- | --- |
| [Features](wiki/Features.md) | What the device shows |
| [The UI](wiki/The-UI.md) | Firmware versions and display tiers |
| [Web panel](wiki/Web-Panel.md) | The in-browser control panel (✨ Dust boards) |
| [Supported boards](wiki/Supported-Boards.md) | The lineup, with a guide for each board |
| [Flashing](wiki/Flashing.md) | Web flasher and building from source |
| [Setup and daily use](wiki/Setup-and-Daily-Use.md) | Captive-portal setup, PIN, controls |
| [How it works](wiki/How-It-Works.md) | The API request and the headers it reads |
| [Security](wiki/Security.md) | How your OAuth token is encrypted on-device |
| [Troubleshooting](wiki/Troubleshooting.md) | When something doesn't work |

Pages are plain Markdown in [`wiki/`](wiki/), so docs get reviewed in PRs like any other change. A workflow mirrors them to the GitHub wiki once that wiki has its first page.

## Contributing

PRs welcome — especially new board support and photos of real builds. See [Project structure](wiki/Project-Structure.md) for the repo layout and what adding a board involves.

## License

[MIT](LICENSE)
