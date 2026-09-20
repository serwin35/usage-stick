#include "history.h"

#ifdef DUST_UI

#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>

// NTP sanity floor: epochs below this mean the clock isn't set and slot math
// would poison the ring.
static const uint32_t TIME_SANE_EPOCH = 1700000000UL;
static const char*    HIST_PATH        = "/history.bin";
static const char*    HIST_CODEX_PATH  = "/codex_hist.bin";

struct HistFile {
    uint32_t magic;         // 'CUH1'
    uint16_t version;
    uint16_t reserved;
    uint32_t lastAbsSlot;   // absolute slot (epoch/1800) of the newest sample
    HistSlot ring[HIST_SLOTS];
};
static const uint32_t HIST_MAGIC   = 0x31485543;   // "CUH1" little-endian
static const uint16_t HIST_VERSION = 1;

static HistFile s_hist;
static HistFile s_codex;
static bool     s_fsOk         = false;
static bool     s_slotAdvanced = false;
static bool     s_codexAdvanced = false;

static void clearRingOf(HistFile& h) {
    memset(h.ring, HIST_EMPTY, sizeof(h.ring));
    h.lastAbsSlot = 0;
}

static void persistTo(const char* path, const HistFile& h) {
    if (!s_fsOk) return;
    File f = LittleFS.open(path, "w");
    if (!f) return;
    f.write((const uint8_t*)&h, sizeof(h));
    f.close();
}

static void loadRing(const char* path, HistFile& h) {
    File f = LittleFS.open(path, "r");
    if (!f) return;
    HistFile onDisk;
    bool ok = f.read((uint8_t*)&onDisk, sizeof(onDisk)) == sizeof(onDisk) &&
              onDisk.magic == HIST_MAGIC && onDisk.version == HIST_VERSION;
    f.close();
    if (ok) h = onDisk;
}

void historyInit() {
    s_hist.magic   = HIST_MAGIC;
    s_hist.version = HIST_VERSION;
    s_codex.magic   = HIST_MAGIC;
    s_codex.version = HIST_VERSION;
    clearRingOf(s_hist);
    clearRingOf(s_codex);

    // The data partition is labeled "spiffs" in the table; LittleFS mounts it
    // fine (that label is arduino-esp32's default, passed explicitly to make
    // the dependency visible). formatOnFail covers the first boot.
    s_fsOk = LittleFS.begin(true, "/littlefs", 10, "spiffs");
    if (!s_fsOk) return;

    loadRing(HIST_PATH, s_hist);
    loadRing(HIST_CODEX_PATH, s_codex);
}

static void recordInto(HistFile& h, bool& advanced, const char* path, float h5, float d7) {
    uint32_t now = (uint32_t)time(nullptr);
    if (now < TIME_SANE_EPOCH) return;

    uint32_t absSlot = now / HIST_SLOT_SEC;
    if (h.lastAbsSlot == 0) {
        clearRingOf(h);
    } else if (absSlot > h.lastAbsSlot) {
        uint32_t gap = absSlot - h.lastAbsSlot;
        if (gap >= HIST_SLOTS) {
            clearRingOf(h);
        } else {
            for (uint32_t s = h.lastAbsSlot + 1; s <= absSlot; s++) {
                h.ring[s % HIST_SLOTS].h5 = HIST_EMPTY;
                h.ring[s % HIST_SLOTS].d7 = HIST_EMPTY;
            }
        }
    }

    HistSlot& slot = h.ring[absSlot % HIST_SLOTS];
    slot.h5 = (uint8_t)constrain((int)(h5 + 0.5f), 0, 100);
    slot.d7 = (uint8_t)constrain((int)(d7 + 0.5f), 0, 100);

    if (absSlot != h.lastAbsSlot) {
        h.lastAbsSlot = absSlot;
        advanced = true;
        persistTo(path, h);
    }
}

void historyRecord(const UsageData& u) {
    if (!u.ok) return;
    recordInto(s_hist, s_slotAdvanced, HIST_PATH, u.h5, u.d7);
}

void historyRecordCodex(float h5, float d7) {
    recordInto(s_codex, s_codexAdvanced, HIST_CODEX_PATH, h5, d7);
}

bool historySlotAdvancedTake() {
    bool r = s_slotAdvanced;
    s_slotAdvanced = false;
    return r;
}

bool historySlotAdvancedTakeCodex() {
    bool r = s_codexAdvanced;
    s_codexAdvanced = false;
    return r;
}

static void snapshotOf(const HistFile& h, HistSlot* out, uint32_t& newestEpoch) {
    uint32_t now = (uint32_t)time(nullptr);
    uint32_t nowAbs = (now >= TIME_SANE_EPOCH) ? now / HIST_SLOT_SEC : h.lastAbsSlot;
    if (nowAbs == 0) nowAbs = HIST_SLOTS;
    newestEpoch = (nowAbs + 1) * HIST_SLOT_SEC;

    for (uint16_t i = 0; i < HIST_SLOTS; i++) {
        uint32_t absSlot = nowAbs - (HIST_SLOTS - 1) + i;
        bool valid = h.lastAbsSlot != 0 &&
                     absSlot <= h.lastAbsSlot &&
                     absSlot + HIST_SLOTS > h.lastAbsSlot;
        out[i] = valid ? h.ring[absSlot % HIST_SLOTS]
                       : HistSlot{HIST_EMPTY, HIST_EMPTY};
    }
}

void historySnapshot(HistSlot* out, uint32_t& newestEpoch) {
    snapshotOf(s_hist, out, newestEpoch);
}

void historySnapshotCodex(HistSlot* out, uint32_t& newestEpoch) {
    snapshotOf(s_codex, out, newestEpoch);
}

void historyErase() {
    clearRingOf(s_hist);
    clearRingOf(s_codex);
    s_slotAdvanced = false;
    s_codexAdvanced = false;
    if (s_fsOk) {
        LittleFS.remove(HIST_PATH);
        LittleFS.remove(HIST_CODEX_PATH);
    }
}

#ifdef PANEL_DEBUG
void historySeedDemo(bool clear) {
    if (clear) {
        historyErase();
        s_slotAdvanced = true;
        s_codexAdvanced = true;
        return;
    }
    uint32_t now = (uint32_t)time(nullptr);
    if (now < TIME_SANE_EPOCH) return;
    uint32_t nowAbs = now / HIST_SLOT_SEC;

    // 7 days of plausible shape: the 5h line saws up and resets every ~5h,
    // the 7d line ramps slowly; two multi-hour gaps exercise the line breaks.
    for (uint16_t i = 0; i < HIST_SLOTS; i++) {
        uint32_t absSlot = nowAbs - (HIST_SLOTS - 1) + i;
        HistSlot& s = s_hist.ring[absSlot % HIST_SLOTS];
        if ((i >= 90 && i < 104) || (i >= 210 && i < 226)) {
            s.h5 = s.d7 = HIST_EMPTY;
        } else {
            s.h5 = (uint8_t)((i % 10) * 9);              // 0→81 over 5h, then reset
            s.d7 = (uint8_t)(15 + (i * 55) / HIST_SLOTS); // 15→70 slow ramp
        }
    }
    s_hist.lastAbsSlot = nowAbs;
    s_slotAdvanced = true;
    persistTo(HIST_PATH, s_hist);
}
#endif

#endif // DUST_UI
