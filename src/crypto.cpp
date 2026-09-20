#include "crypto.h"
#include "config.h"
#include "mbedtls/gcm.h"
#include "mbedtls/sha256.h"
#include "esp_system.h"
#include <string.h>

static uint8_t s_sessionKey[32];
static bool    s_sessionKeySet = false;

static bool gcmEncrypt(const uint8_t* key, const char* plaintext, EncryptedBlob& blob) {
    size_t ptLen = strlen(plaintext);
    if (ptLen > sizeof(blob.ciphertext)) return false;

    esp_efuse_mac_get_default(blob.salt);
    esp_fill_random(blob.iv, sizeof(blob.iv));

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    int ret = mbedtls_gcm_crypt_and_tag(
        &gcm, MBEDTLS_GCM_ENCRYPT, ptLen,
        blob.iv, sizeof(blob.iv),
        NULL, 0,
        (const uint8_t*)plaintext,
        blob.ciphertext,
        sizeof(blob.tag), blob.tag
    );
    mbedtls_gcm_free(&gcm);
    blob.len = (uint16_t)ptLen;
    return ret == 0;
}

void deriveKey(const char* pin, const uint8_t* salt, size_t saltLen, uint8_t* keyOut32) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);

    uint8_t hash[32];
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const uint8_t*)pin, strlen(pin));
    mbedtls_sha256_update(&ctx, salt, saltLen);
    mbedtls_sha256_finish(&ctx, hash);

    for (int i = 1; i < KDF_ROUNDS; i++) {
        mbedtls_sha256_starts(&ctx, 0);
        mbedtls_sha256_update(&ctx, hash, 32);
        mbedtls_sha256_finish(&ctx, hash);
    }

    mbedtls_sha256_free(&ctx);
    memcpy(keyOut32, hash, 32);
}

bool encryptToken(const char* plaintext, const char* pin, EncryptedBlob& blob) {
    uint8_t salt[6];
    esp_efuse_mac_get_default(salt);
    uint8_t key[32];
    deriveKey(pin, salt, sizeof(salt), key);
    bool ok = gcmEncrypt(key, plaintext, blob);
    if (ok) {
        memcpy(s_sessionKey, key, 32);
        s_sessionKeySet = true;
    }
    memset(key, 0, sizeof(key));
    return ok;
}

bool decryptToken(const EncryptedBlob& blob, const char* pin, char* plainOut, size_t plainMaxLen) {
    if (blob.len > plainMaxLen - 1) return false;

    uint8_t key[32];
    deriveKey(pin, blob.salt, sizeof(blob.salt), key);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);

    int ret = mbedtls_gcm_auth_decrypt(
        &gcm, blob.len,
        blob.iv, sizeof(blob.iv),
        NULL, 0,
        blob.tag, sizeof(blob.tag),
        blob.ciphertext,
        (uint8_t*)plainOut
    );

    mbedtls_gcm_free(&gcm);

    if (ret != 0) {
        memset(key, 0, sizeof(key));
        return false;
    }
    memcpy(s_sessionKey, key, 32);
    s_sessionKeySet = true;
    memset(key, 0, sizeof(key));
    plainOut[blob.len] = '\0';
    return true;
}

bool recryptToken(const char* plaintext, EncryptedBlob& blob) {
    if (!s_sessionKeySet) return false;
    return gcmEncrypt(s_sessionKey, plaintext, blob);
}
