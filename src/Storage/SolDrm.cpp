#include "SolDrm.hpp"

#include <cstdio>
#include <mbedtls/error.h>
#include <mbedtls/gcm.h>
#include <mbedtls/rsa.h>
#include <miniz/miniz.h>

#include "Misc/Defer.hpp"
#include "Misc/SpiramAllocator.hpp"
#include "sindarin-debug.h"

SolDrm::~SolDrm() {
    if (initialized_) {
        mbedtls_entropy_free(&entropy_);
        mbedtls_ctr_drbg_free(&ctrDrbg_);
    }
}

void SolDrm::initIfNecessary() {
    if (initialized_) {
        return;
    }
    mbedtls_ctr_drbg_init(&ctrDrbg_);
    mbedtls_entropy_init(&entropy_);

    int ret = mbedtls_ctr_drbg_seed(&ctrDrbg_, mbedtls_entropy_func, &entropy_, nullptr, 0);
    if (ret != 0) {
        log_e("mbedtls_ctr_drbg_seed 0x%x", ret);
    }

    // If we don't have a private key, generate one
    if (!prefs_.privateKey.get().has_value()) {
        generateKeypair();
    }

    initialized_ = true;
}

auto SolDrm::generateKeypair(unsigned int keySize) -> int {
    log_i("Generating RSA keypair of size %d", keySize);

    PROFILE_START(generateKeypair);

    int ret;
    mbedtls_pk_context key;
    mbedtls_pk_init(&key);

    PROFILE_START(generateKeypairSetup);
    ret = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    if (ret != 0) {
        log_e("mbedtls_pk_setup 0x%x", ret);
        return ret;
    }
    PROFILE_END(generateKeypairSetup);

    PROFILE_START(generateKeypairGen);
    constexpr int exponent = 65537;
    ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(key), mbedtls_ctr_drbg_random, &ctrDrbg_, keySize,
                              exponent);
    if (ret != 0) {
        log_e("mbedtls_rsa_gen_key 0x%x", ret);
        return ret;
    }
    PROFILE_END(generateKeypairGen);

    // Save or use the key here; for example, print it in PEM format
    size_t outputBufferSize = 10000;
    char *outputBuffer = static_cast<char *>(SpiRamMalloc(outputBufferSize));

    // Write the private key in DER format
    // auto privateKeyLength =
    //     mbedtls_pk_write_key_der(&key, (unsigned char *)outputBuffer, outputBufferSize);
    // if (privateKeyLength < 0) {
    //     log_e("Failed to write private key in DER format - 0x%x", privateKeyLength);
    //     SpiRamFree(outputBuffer);
    //     mbedtls_pk_free(&key);
    //     return privateKeyLength;
    // }

    // log_i("Private key DER length: %d bytes", privateKeyLength);

    ret = mbedtls_pk_write_key_pem(&key, reinterpret_cast<unsigned char *>(outputBuffer),
                                   outputBufferSize);
    if (ret != 0) {
        log_e("mbedtls_pk_write_key_pem 0x%x\n", ret);
        SpiRamFree(outputBuffer);
        mbedtls_pk_free(&key);
        return ret;
    }
    log_i("Private key:\n%s\n", outputBuffer);

    std::vector<uint8_t> privateKey(outputBuffer, outputBuffer + strlen(outputBuffer) + 1);
    prefs_.privateKey.set(privateKey);

    // // Write the public key in DER format
    // auto public_key_len = mbedtls_pk_write_pubkey_der(&key, (unsigned char *)output_buf, 64000);
    // if (public_key_len < 0) {
    //     log_e("Failed to write public key in DER format - 0x%x", public_key_len);
    //     SpiRamFree(output_buf);
    //     mbedtls_pk_free(&key);
    //     return public_key_len;
    // }
    // log_i("Public key DER length: %d bytes", public_key_len);

    ret = mbedtls_pk_write_pubkey_pem(&key, reinterpret_cast<unsigned char *>(outputBuffer),
                                      outputBufferSize);
    if (ret != 0) {
        log_e("mbedtls_pk_write_key_pem 0x%x\n", ret);
        SpiRamFree(outputBuffer);
        mbedtls_pk_free(&key);
        return ret;
    }
    log_i("Public key:\n%s\n", outputBuffer);

    SpiRamFree(outputBuffer);
    mbedtls_pk_free(&key);

    PROFILE_END(generateKeypair);
    PROFILE_LOG_MS(generateKeypair);
    PROFILE_LOG_MS(generateKeypairSetup);
    PROFILE_LOG_MS(generateKeypairGen);

    return 0;
}

auto SolDrm::publicKeyPem() -> std::string {
    initIfNecessary();

    auto privateKeyOptional = prefs_.privateKey.get();
    if (!privateKeyOptional.has_value()) {
        log_e("Private key not found");
        return "";
    }
    auto privateKey = privateKeyOptional.value();

    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    defer { mbedtls_pk_free(&key); };

    // TODO: Not actually working for DER keys for some reason which would save us a couple hundred
    // bytes
    int ret = mbedtls_pk_parse_key(&key, privateKey.data(), privateKey.size(), nullptr, 0,
                                   mbedtls_ctr_drbg_random, &ctrDrbg_);
    if (ret != 0) {
        log_e("mbedtls_pk_parse_key: -0x%x", -ret);
        return "";
    }

    size_t estimatedPemSize = (privateKey.size() * 4 / 3) + 100;
    auto *output_buf = static_cast<unsigned char *>(SpiRamMalloc(estimatedPemSize));
    defer { SpiRamFree(output_buf); };
    if (output_buf == nullptr) {
        log_e("Failed to allocate memory for output buffer");
        return "";
    }

    ret = mbedtls_pk_write_pubkey_pem(&key, output_buf, estimatedPemSize);
    if (ret != 0) {
        log_e("Failed to write public key in PEM format: -0x%x", -ret);
        return "";
    }

    std::string publicKey = reinterpret_cast<char *>(output_buf);
    log_i("Public key:\n%s\n", publicKey.c_str());
    return publicKey;
}

auto SolDrm::decryptRSA(const uint8_t *data, size_t length)
    -> std::pair<std::shared_ptr<uint8_t[]>, size_t> {
    initIfNecessary();

    auto privateKeyOptional = prefs_.privateKey.get();
    if (!privateKeyOptional.has_value()) {
        log_e("Private key not found");
        return {nullptr, 0};
    }
    auto privateKey = privateKeyOptional.value();

    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    defer { mbedtls_pk_free(&key); };

    int ret = mbedtls_pk_parse_key(&key, privateKey.data(), privateKey.size(), nullptr, 0,
                                   mbedtls_ctr_drbg_random, &ctrDrbg_);
    if (ret != 0) {
        char errorBuf[100];
        mbedtls_strerror(ret, errorBuf, sizeof(errorBuf));
        log_e("mbedtls_pk_parse_key failed: %s (error code: -0x%04x)", errorBuf, -ret);
        return {nullptr, 0};
    }

    if (mbedtls_pk_can_do(&key, MBEDTLS_PK_RSA)) {
        log_d("RSA key loaded successfully");
    } else {
        log_e("Failed to load RSA key");
        return {nullptr, 0};
    }

    size_t keyLength = mbedtls_pk_get_len(&key);
    if (length != keyLength) {
        log_e("Encrypted data size (%zu) does not match key length (%zu)", length, keyLength);
        return {nullptr, 0};
    }

    auto decryptedData = std::shared_ptr<uint8_t[]>(new uint8_t[keyLength]);
    size_t outputLength = 0;
    auto ctx = mbedtls_pk_rsa(key);

    mbedtls_rsa_set_padding(ctx, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA1);

    ret = mbedtls_rsa_rsaes_oaep_decrypt(ctx, mbedtls_ctr_drbg_random, &ctrDrbg_, nullptr, 0,
                                         &outputLength, data, decryptedData.get(), keyLength);

    if (ret != 0) {
        char errorBuf[100];
        mbedtls_strerror(ret, errorBuf, sizeof(errorBuf));
        log_e("mbedtls_rsa_rsaes_oaep_decrypt failed: %s (error code: -0x%04x)", errorBuf, -ret);
        return {nullptr, 0};
    }

    return {decryptedData, outputLength};
}

auto SolDrm::decryptAESInPlace(uint8_t *encryptedData, size_t encryptedLength, const uint8_t *key,
                               size_t keyLength, const std::array<uint8_t, 16> &iv,
                               const std::array<uint8_t, 16> &tag) -> bool {

    mbedtls_gcm_context aes;
    mbedtls_gcm_init(&aes);
    defer { mbedtls_gcm_free(&aes); };

    if (keyLength != 32) {
        log_e("Invalid AES key size. Expected 32 bytes for AES-256, got %zu bytes", keyLength);
        return false;
    }

    int ret = mbedtls_gcm_setkey(&aes, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (ret != 0) {
        char errorBuf[100];
        mbedtls_strerror(ret, errorBuf, sizeof(errorBuf));
        log_e("mbedtls_gcm_setkey failed: %s", errorBuf);
        return false;
    }

    ret = mbedtls_gcm_auth_decrypt(&aes, encryptedLength, iv.data(), iv.size(), nullptr, 0,
                                   tag.data(), tag.size(), encryptedData, encryptedData);

    if (ret != 0) {
        char errorBuf[100];
        mbedtls_strerror(ret, errorBuf, sizeof(errorBuf));
        log_e("mbedtls_gcm_auth_decrypt failed: %s (error code: -0x%04x)", errorBuf, -ret);
        log_d("AES Key: %s", BytesToHex(key, keyLength).c_str());
        log_d("IV: %s", BytesToHex(iv.data(), iv.size()).c_str());
        log_d("Tag: %s", BytesToHex(tag.data(), tag.size()).c_str());
        log_d("First 16 bytes of encrypted data: %s",
              BytesToHex(encryptedData, std::min(size_t(16), encryptedLength)).c_str());
        // return {};
        return false;
    }

    return true;
}

auto SolDrm::decryptInPlaceIfNecessary(const std::shared_ptr<uint8_t[]> &data, size_t length)
    -> std::pair<std::shared_ptr<uint8_t[]>, size_t> {
    // Check if the file is encrypted
    auto encryptionInfo = parseFileHeader(data.get(), length);
    if (!encryptionInfo.has_value()) {
        // Not encrypted
        return {data, length};
    }

    size_t contentStart =
        8 + encryptionInfo->encryptionType.size() + encryptionInfo->encryptedAESKey.size() + 32;

    // Decrypt the AES key using SolDrm's RSA decryption
    PROFILE_START(decryptRSA);
    auto [aesKey, aesKeyLength] =
        decryptRSA(encryptionInfo->encryptedAESKey.data(), encryptionInfo->encryptedAESKey.size());
    if (!aesKey) {
        log_e("Failed to decrypt AES key");
        return {nullptr, 0};
    }
    PROFILE_END_LOG_US(decryptRSA);

    // Decrypt the content with AES
    PROFILE_START(decryptAESInPlace);
    if (!decryptAESInPlace(data.get() + contentStart, length - contentStart - 1, aesKey.get(),
                           aesKeyLength, encryptionInfo->aesNonce, encryptionInfo->aesTag)) {
        log_e("Failed to decrypt content with AES");
        return {nullptr, 0};
    }
    PROFILE_END_LOG_US(decryptAESInPlace);

    // Now that we're decrypted, decompress the data
    PROFILE_START(decompress);
    defer { PROFILE_END_LOG_US(decompress); };

    // log_d("First 16 bytes of compressed data: %s",
    //       BytesToHex(data.get() + contentStart, std::min(size_t(16), length - contentStart -
    //       1))
    //           .c_str());

    mz_stream stream;
    memset(&stream, 0, sizeof(stream));
    stream.zalloc = SpiZAlloc;
    stream.zfree = SpiZFree;
    stream.opaque = nullptr;
    auto status = mz_inflateInit(&stream);
    if (status != MZ_OK) {
        log_e("Failed to initialize decompression with error %d", status);
        return {nullptr, 0};
    }
    defer { mz_inflateEnd(&stream); };

    // Set up source buffer and verify it
    stream.next_in = data.get() + contentStart;
    stream.avail_in = length - contentStart - 1;
    log_d("Input buffer: next_in=%p, avail_in=%u", (void *)stream.next_in, stream.avail_in);
    log_d("First two bytes at next_in: 0x%02x 0x%02x", stream.next_in[0], stream.next_in[1]);

    // TODO: It would be nice to have a way to know the decompressed size so we wouldnt have to
    // handle MZ_BUF_ERROR and potentially duplicate work
    size_t decompressedSize = (length - contentStart - 1) * 3;
    auto decompressedData = SpiRamArrayPtr<uint8_t>(decompressedSize);

    while (true) {
        // Reset stream for each attempt
        mz_inflateEnd(&stream);
        status = mz_inflateInit(&stream);
        if (status != MZ_OK) {
            log_e("Failed to reinitialize decompression with error %d", status);
            return {nullptr, 0};
        }

        // Reset input position
        stream.next_in = data.get() + contentStart;
        stream.avail_in = length - contentStart - 1;

        // Set up output buffer
        stream.next_out = decompressedData.get();
        stream.avail_out = decompressedSize;

        log_d("Trying decompression with output buffer size %zu", decompressedSize);

        // Try to decompress
        status = mz_inflate(&stream, MZ_FINISH);
        log_d("Inflate status: %d, consumed %lu bytes, produced %lu bytes", status, stream.total_in,
              stream.total_out);

        if (status == MZ_STREAM_END) {
            // Success!
            break;
        } else if (status == MZ_BUF_ERROR || (status == MZ_OK && stream.avail_out == 0)) {
            // Need more output space
            decompressedSize *= 2;
            log_w("Increasing buffer size to %zu", decompressedSize);
            decompressedData = std::shared_ptr<uint8_t[]>(new uint8_t[decompressedSize]);
            continue;
        } else {
            // Real error
            log_e("Decompression failed with status %d", status);
            return {nullptr, 0};
        }
    }

    return {decompressedData, stream.total_out};
}

auto SolDrm::parseFileHeader(const uint8_t *data, size_t length) -> std::optional<FileHeader> {
    // Minimum size for encryption header
    if (length < 40) {
        return std::nullopt;
    }

    FileHeader header;

    // Parse encryption type length (big-endian)
    uint32_t encTypeLength = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    log_d("Encryption type length: %" PRIu32, encTypeLength);
    if (length < 4 + encTypeLength + 36) {
        log_e("Encryption info is too short");
        return std::nullopt;
    }
    header.encryptionType = std::string(data + 4, data + 4 + encTypeLength);

    // Parse encrypted AES key length (big-endian)
    uint32_t aesKeyLength = (data[4 + encTypeLength] << 24) | (data[5 + encTypeLength] << 16) |
                            (data[6 + encTypeLength] << 8) | data[7 + encTypeLength];
    if (length < 8 + encTypeLength + aesKeyLength + 32) {
        log_e("Encrypted AES key is too short");
        return std::nullopt;
    }
    header.encryptedAESKey =
        std::vector<uint8_t>(data + 8 + encTypeLength, data + 8 + encTypeLength + aesKeyLength);

    log_d("Encrypted AES key of size %zu: %s", header.encryptedAESKey.size(),
          BytesToHex(header.encryptedAESKey.data(), header.encryptedAESKey.size()).c_str());

    // Parse AES nonce and tag
    std::copy_n(data + 8 + encTypeLength + aesKeyLength, 16, header.aesNonce.begin());
    std::copy_n(data + 24 + encTypeLength + aesKeyLength, 16, header.aesTag.begin());

    log_d("Encryption info: %s", header.encryptionType.c_str());

    return header;
}
