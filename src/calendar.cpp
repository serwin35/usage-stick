#include "calendar.h"

#ifdef DUST_UI

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <string.h>
#include <time.h>
#include "certs.h"
#include "app_state.h"

CalState g_cal = {};

// ── time helpers ─────────────────────────────────────────────────────────
// The device runs a fixed-offset TZ (settingsApplyTZ installs no DST rules),
// so local time is always UTC + tzMin and a day is always exactly 86400s.
// That is what makes the repeat expansion below safe to do in plain epoch
// arithmetic instead of per-occurrence calendar math.
static int32_t tzOffsetSec() { return g_settings.tzMin * 60; }

// Days since 1970-01-01 for a civil date (Howard Hinnant's days_from_civil).
// Done by hand because this newlib exposes no timegm(), and mktime() would
// drag the process TZ into what has to be a pure UTC conversion.
static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
    y -= (m <= 2);
    const int64_t  era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static uint32_t ymdhmsToEpochUTC(int y, int mo, int d, int h, int mi, int s) {
    if (y < 1970 || mo < 1 || mo > 12 || d < 1 || d > 31) return 0;
    int64_t e = daysFromCivil(y, (unsigned)mo, (unsigned)d) * 86400LL + h * 3600 + mi * 60 + s;
    return (e < 0) ? 0 : (uint32_t)e;
}

// Local midnight of the day `now` falls in, as an epoch.
static uint32_t todayLocalMidnight(uint32_t now) {
    int32_t off = tzOffsetSec();
    int64_t local = (int64_t)now + off;
    return (uint32_t)((local - (local % 86400)) - off);
}

// ── iCal value parsing ───────────────────────────────────────────────────
// "20260918T140000Z" (UTC) · "20260918T140000" (calendar-local) · "20260918"
// (all-day). A TZID= parameter is treated as calendar-local: for a desk clock
// whose timezone matches the calendar's, that is correct, and it avoids
// shipping a tz database. Returns 0 if the value isn't a date at all.
static uint32_t parseICalDate(const char* v, bool* allDayOut) {
    int y, mo, d, h = 0, mi = 0, s = 0;
    size_t len = strlen(v);
    if (len < 8) return 0;
    for (int i = 0; i < 8; i++)
        if (!isdigit((unsigned char)v[i])) return 0;

    y  = (v[0] - '0') * 1000 + (v[1] - '0') * 100 + (v[2] - '0') * 10 + (v[3] - '0');
    mo = (v[4] - '0') * 10 + (v[5] - '0');
    d  = (v[6] - '0') * 10 + (v[7] - '0');

    bool dateOnly = (len < 15 || v[8] != 'T');
    if (!dateOnly) {
        h  = (v[9] - '0') * 10 + (v[10] - '0');
        mi = (v[11] - '0') * 10 + (v[12] - '0');
        s  = (v[13] - '0') * 10 + (v[14] - '0');
    }
    if (allDayOut) *allDayOut = dateOnly;

    uint32_t e = ymdhmsToEpochUTC(y, mo, d, h, mi, s);
    bool utc = (!dateOnly && len >= 16 && v[15] == 'Z');
    if (!utc) e -= tzOffsetSec();   // value was local wall-clock
    return e;
}

// TEXT values escape these; everything else is copied as printable ASCII.
static void unescapeText(const char* src, char* dst, size_t cap) {
    size_t n = 0;
    for (size_t i = 0; src[i] && n + 1 < cap; i++) {
        char c = src[i];
        if (c == '\\' && src[i + 1]) {
            char e = src[++i];
            if (e == 'n' || e == 'N') c = ' ';
            else c = e;                       // \, \; \\ → the literal char
        }
        if ((unsigned char)c < 0x20) c = ' ';
        if ((unsigned char)c > 0x7E) c = '?';  // the panel font is ASCII-only
        dst[n++] = c;
    }
    dst[n] = '\0';
}

// ── one event under construction ─────────────────────────────────────────
#define CAL_MAX_EXDATES 12

struct RawEvent {
    char     title[64];
    uint32_t start;
    bool     allDay;
    char     rrule[160];
    uint32_t exdates[CAL_MAX_EXDATES];
    uint8_t  exCount;

    void reset() {
        title[0] = '\0';
        start    = 0;
        allDay   = false;
        rrule[0] = '\0';
        exCount  = 0;
    }
};

// ── parser: assembles unfolded logical lines, then dispatches them ────────
// iCal folds long lines with CRLF + one space/tab, so a SUMMARY can arrive
// split across TCP reads in the middle of a word. Feeding byte by byte and
// only dispatching on a newline that ISN'T followed by whitespace handles
// that by construction.
struct ICalParser {
    char     line[512];
    uint16_t lineLen  = 0;
    bool     atEOL    = false;   // saw '\n', waiting to see if a fold follows
    bool     clipped  = false;   // line longer than the buffer — drop the rest
    bool     inEvent  = false;
    RawEvent cur;

    uint32_t now      = 0;
    uint32_t dayStart = 0;       // local midnight today
    uint32_t horizon  = 0;

    CalEvent out[CAL_MAX_ITEMS];
    uint8_t  count = 0;

    void begin(uint32_t nowEpoch) {
        now      = nowEpoch;
        dayStart = todayLocalMidnight(nowEpoch);
        horizon  = nowEpoch + (uint32_t)CAL_HORIZON_DAYS * 86400UL;
        cur.reset();
    }

    // Sorted insert into a fixed top-N, so the whole file can stream past
    // while only the soonest CAL_MAX_ITEMS events are ever held.
    void insert(uint32_t start, bool allDay, const char* title) {
        // An all-day event stays "upcoming" for the whole day; a timed one
        // gets an hour of grace so a meeting in progress doesn't vanish.
        uint32_t floorEpoch = allDay ? dayStart : (now > 3600 ? now - 3600 : 0);
        if (start < floorEpoch || start > horizon) return;
        if (count == CAL_MAX_ITEMS && start >= out[count - 1].startEpoch) return;

        int pos = 0;
        while (pos < count && out[pos].startEpoch <= start) pos++;
        int last = (count < CAL_MAX_ITEMS) ? count : CAL_MAX_ITEMS - 1;
        for (int i = last; i > pos; i--) out[i] = out[i - 1];
        strlcpy(out[pos].title, title[0] ? title : "(no title)", sizeof(out[pos].title));
        out[pos].startEpoch = start;
        out[pos].allDay     = allDay;
        if (count < CAL_MAX_ITEMS) count++;
    }

    bool excluded(uint32_t t) const {
        for (uint8_t i = 0; i < cur.exCount; i++)
            if (cur.exdates[i] == t) return true;
        return false;
    }

    // RRULE support is deliberately partial: FREQ=DAILY and FREQ=WEEKLY (with
    // INTERVAL, BYDAY, UNTIL and COUNT) cover day-to-day meetings, which is
    // what an upcoming-events panel is for. MONTHLY/YEARLY rules are skipped
    // rather than approximated — a wrong date is worse than a missing one.
    void expand() {
        if (!cur.start) return;
        if (!cur.rrule[0]) {
            if (!excluded(cur.start)) insert(cur.start, cur.allDay, cur.title);
            return;
        }

        char freq[12] = "";
        uint32_t interval = 1, count = 0, until = 0;
        bool byday[7] = {false, false, false, false, false, false, false};
        bool hasByday = false;

        char buf[sizeof(cur.rrule)];
        strlcpy(buf, cur.rrule, sizeof(buf));
        for (char* tok = strtok(buf, ";"); tok; tok = strtok(nullptr, ";")) {
            char* eq = strchr(tok, '=');
            if (!eq) continue;
            *eq = '\0';
            const char* key = tok;
            const char* val = eq + 1;
            if (!strcmp(key, "FREQ")) strlcpy(freq, val, sizeof(freq));
            else if (!strcmp(key, "INTERVAL")) interval = strtoul(val, nullptr, 10);
            else if (!strcmp(key, "COUNT")) count = strtoul(val, nullptr, 10);
            else if (!strcmp(key, "UNTIL")) until = parseICalDate(val, nullptr);
            else if (!strcmp(key, "BYDAY")) {
                static const char* NAMES[7] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
                for (const char* p = val; *p; p++) {
                    for (int d = 0; d < 7; d++) {
                        if (!strncmp(p, NAMES[d], 2)) { byday[d] = true; hasByday = true; }
                    }
                }
            }
        }
        if (interval < 1) interval = 1;

        bool daily  = !strcmp(freq, "DAILY");
        bool weekly = !strcmp(freq, "WEEKLY");
        if (!daily && !weekly) return;

        // Weekday of DTSTART in local time (0=Sunday), for BYDAY offsets.
        int startDow = (int)((((int64_t)cur.start + tzOffsetSec()) / 86400 + 4) % 7);

        uint32_t emitted = 0;
        const int MAX_STEPS = 400;   // bounds CPU for a rule with no UNTIL
        for (int step = 0; step < MAX_STEPS; step++) {
            uint32_t occ[7];
            int nOcc = 0;
            if (daily) {
                occ[nOcc++] = cur.start + (uint32_t)step * interval * 86400UL;
            } else if (hasByday) {
                uint32_t weekBase = cur.start + (uint32_t)step * interval * 7UL * 86400UL;
                for (int d = 0; d < 7; d++) {
                    if (!byday[d]) continue;
                    int32_t shift = (int32_t)(d - startDow) * 86400;
                    int64_t t = (int64_t)weekBase + shift;
                    if (t < (int64_t)cur.start) continue;   // before DTSTART
                    occ[nOcc++] = (uint32_t)t;
                }
            } else {
                occ[nOcc++] = cur.start + (uint32_t)step * interval * 7UL * 86400UL;
            }

            bool pastHorizon = true;
            for (int i = 0; i < nOcc; i++) {
                uint32_t t = occ[i];
                if (t <= horizon) pastHorizon = false;
                if (until && t > until) continue;
                if (count && emitted >= count) return;
                emitted++;
                if (!excluded(t)) insert(t, cur.allDay, cur.title);
            }
            if (pastHorizon) return;
        }
    }

    void handleLine() {
        line[lineLen] = '\0';
        if (!lineLen) return;

        // Property name runs to the first ':' or ';' (parameters follow ';').
        const char* colon = strchr(line, ':');
        if (!colon) return;
        size_t nameLen = 0;
        while (line[nameLen] && line[nameLen] != ':' && line[nameLen] != ';') nameLen++;
        const char* params = (line[nameLen] == ';') ? line + nameLen : nullptr;
        const char* value  = colon + 1;

        auto is = [&](const char* n) {
            return nameLen == strlen(n) && !strncmp(line, n, nameLen);
        };

        if (is("BEGIN") && !strcmp(value, "VEVENT")) {
            inEvent = true;
            cur.reset();
        } else if (is("END") && !strcmp(value, "VEVENT")) {
            if (inEvent) expand();
            inEvent = false;
        } else if (!inEvent) {
            return;
        } else if (is("SUMMARY")) {
            unescapeText(value, cur.title, sizeof(cur.title));
        } else if (is("DTSTART")) {
            bool allDay = false;
            uint32_t e = parseICalDate(value, &allDay);
            if (e) {
                cur.start = e;
                // VALUE=DATE is the authoritative all-day marker; the 8-digit
                // form is only a hint (a TZID'd value can be 8 digits too).
                cur.allDay = allDay || (params && strstr(params, "VALUE=DATE"));
            }
        } else if (is("RRULE")) {
            strlcpy(cur.rrule, value, sizeof(cur.rrule));
        } else if (is("EXDATE")) {
            // Comma-separated list of cancelled occurrences.
            const char* p = value;
            while (*p && cur.exCount < CAL_MAX_EXDATES) {
                char one[32];
                size_t n = 0;
                while (p[n] && p[n] != ',' && n < sizeof(one) - 1) { one[n] = p[n]; n++; }
                one[n] = '\0';
                uint32_t e = parseICalDate(one, nullptr);
                if (e) cur.exdates[cur.exCount++] = e;
                p += n;
                if (*p == ',') p++;
            }
        }
    }

    void feed(char c) {
        if (c == '\r') return;
        if (atEOL) {
            atEOL = false;
            if (c == ' ' || c == '\t') return;   // folded continuation
            handleLine();
            lineLen = 0;
            clipped = false;
        }
        if (c == '\n') { atEOL = true; return; }
        if (lineLen < sizeof(line) - 1) line[lineLen++] = c;
        else clipped = true;                      // overlong line: keep the head
    }

    void finish() {
        if (atEOL || lineLen) { handleLine(); lineLen = 0; }
    }
};

// ── fetch ────────────────────────────────────────────────────────────────
static bool calendarFetch() {
    if (!g_settings.icalUrl[0]) return false;
    uint32_t now = (uint32_t)time(nullptr);
    if (now < 1700000000) return false;   // no NTP yet — dates would be nonsense

    WiFiClientSecure client;
    client.setCACert(CA_BUNDLE);
    HTTPClient https;
    https.setTimeout(CAL_TIMEOUT_MS);
    https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!https.begin(client, g_settings.icalUrl)) return false;
    // As in news.cpp: getStreamPtr() bypasses chunked decoding, so ask for 1.0.
    https.useHTTP10(true);
    if (https.GET() != HTTP_CODE_OK) {
        https.end();
        return false;
    }

    // ~1.5KB of parser state — too big for the loop task's stack.
    static ICalParser p;
    p = ICalParser();
    p.begin(now);

    WiFiClient* stream = https.getStreamPtr();
    uint32_t deadline = millis() + CAL_TIMEOUT_MS + 15000;
    while ((int32_t)(deadline - millis()) > 0) {
        if (!stream->available()) {
            if (!stream->connected()) break;
            delay(2);
            continue;
        }
        uint8_t buf[512];
        int n = stream->read(buf, sizeof(buf));
        for (int i = 0; i < n; i++) p.feed((char)buf[i]);
    }
    p.finish();
    https.end();

    // An empty calendar is a legitimate result, so success is "we parsed the
    // feed", not "we found something".
    memset(g_cal.items, 0, sizeof(g_cal.items));
    for (uint8_t i = 0; i < p.count; i++) g_cal.items[i] = p.out[i];
    g_cal.count          = p.count;
    g_cal.fetchedAtEpoch = now;
    g_cal.ok             = true;
    return true;
}

static uint32_t s_nextAtMs = 0;

void calendarInvalidate() { s_nextAtMs = millis(); }

bool calendarTick() {
    g_cal.configured = g_settings.icalUrl[0] != '\0';
    if (!g_cal.configured) return false;
    if (g_lastFetchMs == 0) return false;   // let the first usage poll land first
    if (s_nextAtMs == 0) s_nextAtMs = millis() + 20000;
    if ((int32_t)(millis() - s_nextAtMs) < 0) return false;

    bool ok = calendarFetch();
    if (!ok) g_cal.ok = false;   // keep stale events, flag the failed attempt
    s_nextAtMs = millis() + (ok ? CAL_POLL_SEC : CAL_RETRY_SEC) * 1000UL;
    return ok;
}

#endif // DUST_UI
