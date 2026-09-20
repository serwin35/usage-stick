#pragma once

// ── Firmware version ─────────────────────────────────────
#define FW_VERSION              "3.1.0"  // Dust — Claude + Codex usage screens

// ── Polling ──────────────────────────────────────────────
#define DEFAULT_POLL_SEC        120
#define MIN_POLL_SEC            30
#define MAX_POLL_SEC            300

// ── Security ─────────────────────────────────────────────
#define MAX_PIN_ATTEMPTS        10
#define LOCKOUT_BASE_SEC        60       // doubles each failure
#define KDF_ROUNDS              10000

// ── Display ──────────────────────────────────────────────
#ifdef BOARD_ESP32C3_OLED
  #define SCREEN_W              72
  #define SCREEN_H              40
  // No SCREEN_ROT — U8g2 uses U8G2_R0
#elif defined(BOARD_TDISPLAY_S3)
  #define SCREEN_W              320
  #define SCREEN_H              170
  #define SCREEN_ROT            1
#elif defined(BOARD_TDISPLAY_S3_AMOLED)
  #define SCREEN_W              536
  #define SCREEN_H              240
  #define SCREEN_ROT            0
#elif defined(BOARD_TDISPLAY_ESP32)
  #define SCREEN_W              240
  #define SCREEN_H              135
  #define SCREEN_ROT            3
#elif defined(BOARD_T8_S2)
  #define SCREEN_W              240
  #define SCREEN_H              135
  #define SCREEN_ROT            3
#elif defined(BOARD_CROWPANEL_ADV_35)
  #define SCREEN_W              480
  #define SCREEN_H              320
  #define SCREEN_ROT            3
#elif defined(BOARD_WT32_SC01_PLUS)
  #define SCREEN_W              480
  #define SCREEN_H              320
  #define SCREEN_ROT            3   // TODO(hardware): verify — flip to 1 if the boot screen is upside down
#else
  #define SCREEN_W              240
  #define SCREEN_H              135
  #define SCREEN_ROT            3
#endif
#define DEFAULT_BRIGHTNESS      2        // 0=off 1=dim 2=normal 3=bright

// ── Network ──────────────────────────────────────────────
#define WIFI_CONNECT_TIMEOUT_S  20
#define API_TIMEOUT_MS          15000
#define MESSAGES_ENDPOINT       "https://api.anthropic.com/v1/messages"
#define CODEX_USAGE_URL         "https://chatgpt.com/backend-api/wham/usage"
#define CODEX_SESSION_URL       "https://chatgpt.com/api/auth/session"
#define CODEX_OAUTH_TOKEN_URL   "https://auth.openai.com/oauth/token"
#define CODEX_OAUTH_CLIENT_ID   "app_EMoamEEZ73f0CkXaXp7hrann"
#define CODEX_REFRESH_MAX       512
#define CODEX_ACCESS_MAX        3072
#define ANTHROPIC_VERSION       "2023-06-01"
#define PROBE_MODEL             "claude-haiku-4-5-20251001"
// status.anthropic.com redirects here — query the canonical host directly
#define STATUS_ENDPOINT         "https://status.claude.com/api/v2/incidents/unresolved.json"

// ── Anthropic news feed (T-Display S3 news screen) ───────
// Streamed and abandoned after the first NEWS_MAX_ITEMS items — the full feed
// is ~200KB but each item is tiny.
#define NEWS_FEED_URL           "https://raw.githubusercontent.com/Olshansk/rss-feeds/main/feeds/feed_anthropic_news.xml"
#define NEWS_POLL_SEC           21600    // refetch every 6h
#define NEWS_RETRY_SEC          1800     // failed fetch retries in 30min
#define NEWS_MAX_ITEMS          5
#define NEWS_TIMEOUT_MS         10000

// ── Google Calendar (secret iCal URL, calendar screen) ───
// The whole .ics has to be read (events aren't in date order), so this runs on
// a slow cadence — each fetch blocks the loop for as long as it takes.
#define CAL_MAX_ITEMS           6
#define CAL_POLL_SEC            900      // refetch every 15min
#define CAL_RETRY_SEC           300      // failed fetch retries in 5min
#define CAL_TIMEOUT_MS          20000
#define CAL_HORIZON_DAYS        30       // how far ahead repeats are expanded

// ── NVS ──────────────────────────────────────────────────
#define NVS_NAMESPACE           "claude"

// ── Feature flags (set via build_flags in platformio.ini) ────
// MANGO_UI — the "Mango" dashboard: model-status mascots from status.claude.com,
//   battery + WiFi-signal icons in the header, dashboard-styled PIN screen, and
//   Button A = flip screen / Button B = brightness. Enabled on the boards whose
//   panels have the vertical room for it: BOARD_TDISPLAY_S3 (320x170) and
//   BOARD_M5STICK_C_PLUS (240x135). Mango-specific geometry branches on the board.
