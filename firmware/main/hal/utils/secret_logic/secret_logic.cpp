/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "secret_logic.h"
#include <sdkconfig.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "system_info.h"

#include <esp_log.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mbedtls/base64.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>

namespace secret_logic {

namespace {

constexpr const char* kTag = "SecretLogic";

// Embedded via EMBED_TXTFILES in main/CMakeLists.txt, from
// main/certs/stackchan_server_public_key.pem (gitignored, local-only -- see
// main/certs/stackchan_server_public_key.pem.example for setup notes). EMBED_TXTFILES
// null-terminates the data, which mbedtls_pk_parse_public_key requires for PEM input.
extern "C" const uint8_t stackchan_server_public_key_pem_start[] asm(
    "_binary_stackchan_server_public_key_pem_start");
extern "C" const uint8_t stackchan_server_public_key_pem_end[] asm(
    "_binary_stackchan_server_public_key_pem_end");

// RAII wrapper so every early-return path below still frees the mbedtls contexts.
struct MbedtlsAuthCrypto {
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    MbedtlsAuthCrypto()
    {
        mbedtls_pk_init(&pk);
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&ctr_drbg);
    }

    ~MbedtlsAuthCrypto()
    {
        mbedtls_pk_free(&pk);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
    }
};

// Waits (briefly) for the system clock to be set by SNTP before trusting time(nullptr) for the
// auth token's timestamp field. Immediately after WiFi association, SNTP sync is still in flight
// (confirmed from boot logs: "SNTP init" ... several seconds later ... "SNTP time synchronized"),
// so time(nullptr) can return a tiny, unsynced value well outside the server's +/-10s validity
// window. The server's failure path in that case is silent (web_socket.go's GetMac() returns an
// error without logging when the timestamp check fails), so this needs to be handled defensively
// here rather than relying on the caller's network-ready callback implying clock-ready too.
bool wait_for_valid_clock(int timeout_ms = 8000, int poll_interval_ms = 200)
{
    // Threshold ~2023-11-14 (1700000000). Any real deployment's clock will be well past this;
    // an unsynced clock (e.g. seconds since an uninitialized epoch) will be nowhere close.
    constexpr time_t kMinPlausibleEpoch = 1700000000;
    int waited_ms = 0;
    while (time(nullptr) < kMinPlausibleEpoch && waited_ms < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(poll_interval_ms));
        waited_ms += poll_interval_ms;
    }
    return time(nullptr) >= kMinPlausibleEpoch;
}

// Encrypts `plaintext` with the embedded server RSA public key using RSA-OAEP-SHA256 with an
// empty label, matching server/internal/web_socket/web_socket.go's GetMac(), which decrypts via
// utility.RSADecrypt() -> rsa.DecryptOAEP(sha256.New(), rand.Reader, serverPrivateKey, cipherText, nil).
// Returns base64-encoded ciphertext, or an empty string on any failure (logged).
std::string rsa_oaep_encrypt_base64(const std::string& plaintext)
{
    MbedtlsAuthCrypto ctx;

    const char* pers = "stackchan_auth_token";
    int ret = mbedtls_ctr_drbg_seed(&ctx.ctr_drbg, mbedtls_entropy_func, &ctx.entropy,
                                     reinterpret_cast<const unsigned char*>(pers), strlen(pers));
    if (ret != 0) {
        ESP_LOGE(kTag, "mbedtls_ctr_drbg_seed failed: -0x%04x", -ret);
        return "";
    }

    ret = mbedtls_pk_parse_public_key(
        &ctx.pk, stackchan_server_public_key_pem_start,
        static_cast<size_t>(stackchan_server_public_key_pem_end - stackchan_server_public_key_pem_start));
    if (ret != 0) {
        ESP_LOGE(kTag, "mbedtls_pk_parse_public_key failed: -0x%04x (is main/certs/stackchan_server_public_key.pem set?)",
                 -ret);
        return "";
    }

    mbedtls_rsa_context* rsa = mbedtls_pk_rsa(ctx.pk);
    if (rsa == nullptr) {
        ESP_LOGE(kTag, "embedded key is not an RSA public key");
        return "";
    }

    ret = mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);
    if (ret != 0) {
        ESP_LOGE(kTag, "mbedtls_rsa_set_padding failed: -0x%04x", -ret);
        return "";
    }

    size_t key_len = mbedtls_rsa_get_len(rsa);
    std::string cipher(key_len, '\0');
    ret = mbedtls_rsa_rsaes_oaep_encrypt(rsa, mbedtls_ctr_drbg_random, &ctx.ctr_drbg, nullptr, 0, plaintext.size(),
                                         reinterpret_cast<const unsigned char*>(plaintext.data()),
                                         reinterpret_cast<unsigned char*>(cipher.data()));
    if (ret != 0) {
        ESP_LOGE(kTag, "mbedtls_rsa_rsaes_oaep_encrypt failed: -0x%04x (plaintext too long for this key size?)", -ret);
        return "";
    }

    size_t b64_len = 0;
    // Size-probe call: dst=NULL is documented as valid, just to learn the required length in b64_len.
    mbedtls_base64_encode(nullptr, 0, &b64_len, reinterpret_cast<const unsigned char*>(cipher.data()), cipher.size());
    std::string b64(b64_len, '\0');
    size_t written = 0;
    ret = mbedtls_base64_encode(reinterpret_cast<unsigned char*>(b64.data()), b64.size(), &written,
                                reinterpret_cast<const unsigned char*>(cipher.data()), cipher.size());
    if (ret != 0) {
        ESP_LOGE(kTag, "mbedtls_base64_encode failed: -0x%04x", -ret);
        return "";
    }
    b64.resize(written);
    return b64;
}

}  // namespace

__attribute__((weak)) std::string get_server_url()
{
#ifdef CONFIG_STACKCHAN_SERVER_URL
    return CONFIG_STACKCHAN_SERVER_URL;
#else
    return "http://localhost:3000";
#endif
}

__attribute__((weak)) std::string generate_auth_token()
{
    // Plaintext "<mac>|<nonce>|<unix-timestamp>", matching
    // server/internal/web_socket/web_socket.go's GetMac(): strings.Split(tokenStr, "|"),
    // parts[0] = mac, parts[2] = timestamp (timestamp must be within +/-10s of server time).
    // parts[1] (the nonce) is never validated server-side, but must be present since parts[2]
    // is accessed unconditionally -- it's included to vary the plaintext per request.
    if (!wait_for_valid_clock()) {
        ESP_LOGE(kTag, "system clock not yet synced (SNTP); refusing to generate a token with an invalid timestamp");
        return "";
    }

    std::string mac = SystemInfo::GetMacAddress();

    unsigned char nonce_bytes[8];
    esp_fill_random(nonce_bytes, sizeof(nonce_bytes));
    char nonce_hex[sizeof(nonce_bytes) * 2 + 1];
    for (size_t i = 0; i < sizeof(nonce_bytes); i++) {
        snprintf(nonce_hex + i * 2, 3, "%02x", nonce_bytes[i]);
    }

    time_t now = time(nullptr);
    // NOTE: this build has CONFIG_NEWLIB_NANO_FORMAT=y, whose printf/snprintf does NOT support
    // 64-bit format specifiers (%lld) -- confirmed against Espressif's own docs, and confirmed by
    // observation (this previously produced a garbled 1-2 digit "timestamp" that silently failed
    // the server's timestamp-freshness check). time_t comfortably fits in a 32-bit long through
    // the year 2038, and %ld is already used elsewhere in this codebase (stackchan_camera.cc)
    // under this same nano-format build, so it's the safe choice here.
    char plaintext[128];
    int n = snprintf(plaintext, sizeof(plaintext), "%s|%s|%ld", mac.c_str(), nonce_hex, static_cast<long>(now));
    if (n < 0 || static_cast<size_t>(n) >= sizeof(plaintext)) {
        ESP_LOGE(kTag, "auth token plaintext truncated/failed to format");
        return "";
    }

    std::string token = rsa_oaep_encrypt_base64(std::string(plaintext, static_cast<size_t>(n)));
    if (token.empty()) {
        ESP_LOGE(kTag, "failed to generate auth token; server calls needing it will be rejected (401)");
    }
    return token;
}

__attribute__((weak)) std::string generate_handshake_token(std::string_view data)
{
    return "hi-stack-chan";
}

}  // namespace secret_logic
