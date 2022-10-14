#include "crypto.h"
#include <furi.h>
#include <furi_hal.h>
#include "../config/config.h"
#include "../../types/common.h"

#define CRYPTO_KEY_SLOT 2
#define CRYPTO_VERIFY_KEY "FFF_Crypto_pass"
#define CRYPTO_VERIFY_KEY_LENGTH 16
#define CRYPTO_ALIGNMENT_FACTOR 16

uint8_t* totp_crypto_encrypt(const uint8_t* plain_data, const uint8_t plain_data_length, const uint8_t* iv, uint8_t* encrypted_data_length) {
    uint8_t* encrypted_data;
    size_t remain = plain_data_length % CRYPTO_ALIGNMENT_FACTOR;
    if(remain) {
        uint8_t plain_data_aligned_length = plain_data_length - remain + CRYPTO_ALIGNMENT_FACTOR;
        uint8_t* plain_data_aligned = malloc(plain_data_aligned_length);
        memset(plain_data_aligned, 0, plain_data_aligned_length);
        memcpy(plain_data_aligned, plain_data, plain_data_length);
        
        encrypted_data = malloc(plain_data_aligned_length);
        *encrypted_data_length = plain_data_aligned_length;
    
        furi_hal_crypto_store_load_key(CRYPTO_KEY_SLOT, iv);
        furi_hal_crypto_encrypt(plain_data_aligned, encrypted_data, plain_data_aligned_length);
        furi_hal_crypto_store_unload_key(CRYPTO_KEY_SLOT);

        memset(plain_data_aligned, 0, plain_data_aligned_length);
        free(plain_data_aligned);
    } else {
        encrypted_data = malloc(plain_data_length);
        *encrypted_data_length = plain_data_length;

        furi_hal_crypto_store_load_key(CRYPTO_KEY_SLOT, iv);
        furi_hal_crypto_encrypt(plain_data, encrypted_data, plain_data_length);
        furi_hal_crypto_store_unload_key(CRYPTO_KEY_SLOT);
    }

    return encrypted_data;
}

uint8_t* totp_crypto_decrypt(const uint8_t* encrypted_data, const uint8_t encrypted_data_length, const uint8_t* iv, uint8_t* decrypted_data_length) {
    *decrypted_data_length = encrypted_data_length;
    uint8_t* decrypted_data = malloc(*decrypted_data_length);
    furi_hal_crypto_store_load_key(CRYPTO_KEY_SLOT, iv);
    furi_hal_crypto_decrypt(encrypted_data, decrypted_data, encrypted_data_length);
    furi_hal_crypto_store_unload_key(CRYPTO_KEY_SLOT);
    return decrypted_data;
}

void totp_crypto_seed_iv(PluginState* plugin_state, uint8_t* pin, uint8_t pin_length) {
    if (plugin_state->crypto_verify_data == NULL) {
        FURI_LOG_D(LOGGING_TAG, "Generating new IV");
        furi_hal_random_fill_buf(&plugin_state->base_iv[0], TOTP_IV_SIZE);
    }

    memcpy(&plugin_state->iv[0], &plugin_state->base_iv[0], TOTP_IV_SIZE);
    if (pin != NULL && pin_length > 0) {
        for (uint8_t i = 0; i < pin_length; i++) {
            plugin_state->iv[i] = plugin_state->iv[i] ^ (uint8_t)(pin[i] * (i + 1));
        }
    }

    if (plugin_state->crypto_verify_data == NULL) {
        FURI_LOG_D(LOGGING_TAG, "Generating crypto verify data");
        plugin_state->crypto_verify_data = malloc(CRYPTO_VERIFY_KEY_LENGTH);
        plugin_state->crypto_verify_data_length = CRYPTO_VERIFY_KEY_LENGTH;
        Storage* storage = totp_open_storage();
        FlipperFormat* config_file = totp_open_config_file(storage);

        plugin_state->crypto_verify_data = totp_crypto_encrypt((uint8_t* )CRYPTO_VERIFY_KEY, CRYPTO_VERIFY_KEY_LENGTH, &plugin_state->iv[0], &plugin_state->crypto_verify_data_length);

        flipper_format_insert_or_update_hex(config_file, TOTP_CONFIG_KEY_BASE_IV, plugin_state->base_iv, TOTP_IV_SIZE);
        flipper_format_insert_or_update_hex(config_file, TOTP_CONFIG_KEY_CRYPTO_VERIFY, plugin_state->crypto_verify_data, CRYPTO_VERIFY_KEY_LENGTH);
        plugin_state->pin_set = pin != NULL && pin_length > 0;
        flipper_format_insert_or_update_bool(config_file, TOTP_CONFIG_KEY_PINSET, &plugin_state->pin_set, 1);
        totp_close_config_file(config_file);
        totp_close_storage();
    }
}

bool totp_crypto_verify_key(const PluginState* plugin_state) {
    uint8_t decrypted_key_length;
    uint8_t* decrypted_key = totp_crypto_decrypt(plugin_state->crypto_verify_data, plugin_state->crypto_verify_data_length, &plugin_state->iv[0], &decrypted_key_length);

    bool key_valid = true;
    for (uint8_t i = 0; i < CRYPTO_VERIFY_KEY_LENGTH && key_valid; i++) {
        if (decrypted_key[i] != CRYPTO_VERIFY_KEY[i]) key_valid = false;
    }

    return key_valid;
}