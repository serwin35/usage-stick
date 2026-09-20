#pragma once
#include <stdint.h>
#include "config.h"

#ifdef DUST_UI

// Upcoming events from a Google Calendar "secret address in iCal format" URL
// (Calendar settings → Integrate calendar → Secret address in iCal format).
// No OAuth: the URL itself is the credential, stored in NVS like the WiFi
// password. The .ics is streamed and parsed on the fly — it is never buffered
// whole, but unlike the news feed it cannot be abandoned early, because iCal
// events are not in chronological order.
struct CalEvent {
    char     title[64];
    uint32_t startEpoch;
    bool     allDay;
};

struct CalState {
    CalEvent items[CAL_MAX_ITEMS];
    uint8_t  count;
    uint32_t fetchedAtEpoch;   // 0 = never fetched
    bool     ok;               // last attempt succeeded
    bool     configured;       // a URL is set
};

extern CalState g_cal;

// Call every loop() pass. Owns its own schedule: first fetch shortly after the
// first usage poll, then every CAL_POLL_SEC (CAL_RETRY_SEC after a failure).
// Returns true when a fetch just succeeded, so the caller can redraw.
// Blocks for the length of the fetch — a large calendar can take several
// seconds, which is why it runs on a slow cadence.
bool calendarTick();

// Forces the next calendarTick() to fetch (the panel calls this when the URL
// changes, so a new calendar shows up without waiting out the poll interval).
void calendarInvalidate();

#endif // DUST_UI
