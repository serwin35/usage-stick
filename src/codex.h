#pragma once
#include <stdint.h>
#include <stddef.h>

// Codex (ChatGPT) rate-limit windows + Extra Usage credits. Optional: the
// device works as before with only a Claude token. Mirrors Usage4Claude's
// parse of GET chatgpt.com/backend-api/wham/usage.
struct CodexUsage {
    float    h5;
    float    d7;
    uint32_t h5ResetEpoch;
    uint32_t d7ResetEpoch;
    bool     hasH5;
    bool     hasD7;
    bool     hasCredits;
    bool     unlimited;
    bool     exhausted;
    float    creditBalance;
    bool     ok;
    bool     configured;
    bool     credRotated;     // credential[] now holds a new refresh_token
    char     error[64];
};

// `credential` is the Codex CLI refresh_token (rt.… ) or a ChatGPT
// session-token cookie. On OAuth refresh-token rotation it is overwritten
// in place so the caller can re-encrypt it.
bool fetchCodexUsage(char* credential, size_t credMax, CodexUsage& out);
