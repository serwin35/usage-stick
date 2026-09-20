#include "codex.h"
#include "config.h"
#include "certs.h"
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

static char     s_access[CODEX_ACCESS_MAX];
static uint32_t s_accessExp = 0;

static bool isOAuthRefresh(const char* cred) {
    return cred && cred[0] == 'r' && cred[1] == 't' && cred[2] == '.';
}

static bool isFiveHourWindow(int seconds) {
    const int five = 18000, seven = 604800;
    if (seconds <= 0) return false;
    return abs(seconds - five) < abs(seconds - seven);
}

static uint32_t jwtExpEpoch(const char* jwt) {
    if (!jwt) return 0;
    const char* p1 = strchr(jwt, '.');
    if (!p1) return 0;
    const char* p2 = strchr(p1 + 1, '.');
    if (!p2) return 0;
    // Payload is base64url; we only need the decimal exp claim, so a small
    // decode of the middle segment is enough.
    size_t n = (size_t)(p2 - (p1 + 1));
    if (n > 2000) return 0;
    char b64[2004];
    memcpy(b64, p1 + 1, n);
    b64[n] = '\0';
    for (size_t i = 0; i < n; i++) {
        if (b64[i] == '-') b64[i] = '+';
        else if (b64[i] == '_') b64[i] = '/';
    }
    while (n % 4) { b64[n++] = '='; b64[n] = '\0'; }

    // ArduinoJson can parse if we replace with a crude search after a
    // mbedTLS-less 3-in-4 decode.
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char payload[1536];
    size_t out = 0;
    int val = 0, valb = -8;
    for (size_t i = 0; b64[i] && out + 1 < sizeof(payload); i++) {
        if (b64[i] == '=') break;
        const char* t = strchr(tbl, b64[i]);
        if (!t) continue;
        val = (val << 6) + (int)(t - tbl);
        valb += 6;
        if (valb >= 0) {
            payload[out++] = (char)((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    payload[out] = '\0';
    const char* exp = strstr(payload, "\"exp\"");
    if (!exp) return 0;
    exp = strchr(exp, ':');
    if (!exp) return 0;
    return (uint32_t)strtoul(exp + 1, nullptr, 10);
}

static bool httpsBegin(WiFiClientSecure& client, HTTPClient& https, const char* url) {
    client.setCACert(CA_BUNDLE);
    return https.begin(client, url);
}

static bool refreshOAuth(char* credential, size_t credMax) {
    WiFiClientSecure client;
    HTTPClient https;
    if (!httpsBegin(client, https, CODEX_OAUTH_TOKEN_URL)) return false;
    https.addHeader("Content-Type", "application/json");
    https.setTimeout(API_TIMEOUT_MS);

    JsonDocument body;
    body["client_id"] = CODEX_OAUTH_CLIENT_ID;
    body["grant_type"] = "refresh_token";
    body["refresh_token"] = credential;
    String payload;
    serializeJson(body, payload);

    Serial.println("[CODEX] POST oauth/token");
    int code = https.POST(payload);
    Serial.printf("[CODEX] oauth HTTP %d\n", code);
    if (code != 200) {
        https.end();
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, https.getString());
    https.end();
    if (err) return false;

    const char* access = doc["access_token"] | "";
    if (!access[0] || strlen(access) >= sizeof(s_access)) return false;
    strlcpy(s_access, access, sizeof(s_access));
    uint32_t exp = jwtExpEpoch(s_access);
    s_accessExp = exp ? exp : (uint32_t)time(nullptr) + 1800;

    const char* nr = doc["refresh_token"] | "";
    if (nr[0] && strcmp(nr, credential) != 0 && strlen(nr) < credMax) {
        strlcpy(credential, nr, credMax);
        return true;   // rotated — caller still treats this as success
    }
    return true;
}

static bool fetchSessionAccess(const char* sessionToken) {
    WiFiClientSecure client;
    HTTPClient https;
    if (!httpsBegin(client, https, CODEX_SESSION_URL)) return false;
    https.addHeader("accept", "*/*");
    https.addHeader("origin", "https://chatgpt.com");
    https.addHeader("referer", "https://chatgpt.com/");
    https.addHeader("User-Agent",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36");
    https.addHeader("Cookie", String("__Secure-next-auth.session-token=") + sessionToken);
    https.setTimeout(API_TIMEOUT_MS);

    Serial.println("[CODEX] GET /api/auth/session");
    int code = https.GET();
    Serial.printf("[CODEX] session HTTP %d\n", code);
    if (code != 200) {
        https.end();
        return false;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, https.getString());
    https.end();
    if (err) return false;
    const char* access = doc["accessToken"] | "";
    if (!access[0] || strlen(access) >= sizeof(s_access)) return false;
    strlcpy(s_access, access, sizeof(s_access));
    uint32_t exp = jwtExpEpoch(s_access);
    s_accessExp = exp ? exp : (uint32_t)time(nullptr) + 1800;
    return true;
}

static bool ensureAccess(char* credential, size_t credMax, bool& rotated) {
    rotated = false;
    uint32_t now = (uint32_t)time(nullptr);
    if (s_access[0] && s_accessExp > now + 60) return true;

    if (isOAuthRefresh(credential)) {
        char before[CODEX_REFRESH_MAX];
        strlcpy(before, credential, sizeof(before));
        if (!refreshOAuth(credential, credMax)) {
            s_access[0] = '\0';
            return false;
        }
        rotated = strcmp(before, credential) != 0;
        return true;
    }
    return fetchSessionAccess(credential);
}

static void applyWindow(JsonObjectConst w, bool five, CodexUsage& out) {
    if (w.isNull()) return;
    float pct = w["used_percent"] | -1.0f;
    uint32_t resetAt = w["reset_at"] | 0;
    int resetAfter = w["reset_after_seconds"] | -1;
    if (pct < 0) return;
    if (pct == 0 && resetAt == 0 && resetAfter < 0) return;
    uint32_t epoch = resetAt;
    if (!epoch && resetAfter >= 0) epoch = (uint32_t)time(nullptr) + (uint32_t)resetAfter;
    if (five) {
        out.h5 = pct;
        out.h5ResetEpoch = epoch;
        out.hasH5 = true;
    } else {
        out.d7 = pct;
        out.d7ResetEpoch = epoch;
        out.hasD7 = true;
    }
}

static bool parseUsageBody(const String& body, CodexUsage& out) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        strlcpy(out.error, "json", sizeof(out.error));
        return false;
    }
    JsonObjectConst rl = doc["rate_limit"];
    applyWindow(rl["primary_window"].as<JsonObjectConst>(),
                isFiveHourWindow(rl["primary_window"]["limit_window_seconds"] | 0), out);
    applyWindow(rl["secondary_window"].as<JsonObjectConst>(),
                isFiveHourWindow(rl["secondary_window"]["limit_window_seconds"] | 0), out);

    JsonObjectConst credits = doc["credits"];
    if (!credits.isNull()) {
        out.hasCredits = credits["has_credits"] | false;
        out.unlimited  = credits["unlimited"] | false;
        bool overage   = credits["overage_limit_reached"] | false;
        bool spend     = false;
        JsonObjectConst sc = doc["spend_control"];
        if (!sc.isNull()) spend = sc["reached"] | false;
        JsonVariantConst bal = credits["balance"];
        if (!bal.isNull()) out.creditBalance = bal.as<float>();
        out.exhausted = !out.unlimited && (overage || spend ||
                         (out.hasCredits && out.creditBalance <= 0 && !bal.isNull()));
        if (out.hasCredits || out.unlimited || out.exhausted || out.creditBalance > 0)
            out.hasCredits = true;
    }
    return out.hasH5 || out.hasD7 || out.hasCredits;
}

bool fetchCodexUsage(char* credential, size_t credMax, CodexUsage& out) {
    memset(&out, 0, sizeof(out));
    if (!credential || !credential[0]) {
        strlcpy(out.error, "no_codex_token", sizeof(out.error));
        return false;
    }
    out.configured = true;

    bool rotated = false;
    if (!ensureAccess(credential, credMax, rotated)) {
        strlcpy(out.error, "codex_auth", sizeof(out.error));
        return false;
    }
    out.credRotated = rotated;

    WiFiClientSecure client;
    HTTPClient https;
    if (!httpsBegin(client, https, CODEX_USAGE_URL)) {
        strlcpy(out.error, "https_init", sizeof(out.error));
        return false;
    }
    https.addHeader("accept", "*/*");
    https.addHeader("content-type", "application/json");
    https.addHeader("origin", "https://chatgpt.com");
    https.addHeader("referer", "https://chatgpt.com/");
    https.addHeader("User-Agent",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36");
    https.addHeader("Authorization", String("Bearer ") + s_access);
    https.setTimeout(API_TIMEOUT_MS);

    Serial.println("[CODEX] GET /backend-api/wham/usage");
    int code = https.GET();
    Serial.printf("[CODEX] usage HTTP %d\n", code);

    if (code == 401) {
        https.end();
        s_access[0] = '\0';
        s_accessExp = 0;
        // One retry after dropping the cached access token.
        if (!ensureAccess(credential, credMax, rotated)) {
            strlcpy(out.error, "codex_auth", sizeof(out.error));
            return false;
        }
        out.credRotated = out.credRotated || rotated;
        if (!httpsBegin(client, https, CODEX_USAGE_URL)) {
            strlcpy(out.error, "https_init", sizeof(out.error));
            return false;
        }
        https.addHeader("Authorization", String("Bearer ") + s_access);
        https.addHeader("origin", "https://chatgpt.com");
        https.addHeader("referer", "https://chatgpt.com/");
        https.setTimeout(API_TIMEOUT_MS);
        code = https.GET();
        Serial.printf("[CODEX] usage retry HTTP %d\n", code);
    }

    if (code != 200) {
        snprintf(out.error, sizeof(out.error), "http_%d", code);
        https.end();
        return false;
    }

    String body = https.getString();
    https.end();
    bool ok = parseUsageBody(body, out);
    if (!ok) {
        if (!out.error[0]) strlcpy(out.error, "no_codex_usage", sizeof(out.error));
        return false;
    }
    out.ok = true;
    Serial.printf("[CODEX] 5h=%s%.0f  7d=%s%.0f  credits=%.1f\n",
                  out.hasH5 ? "" : "(", out.h5, out.hasD7 ? "" : "(", out.d7,
                  out.creditBalance);
    return true;
}
