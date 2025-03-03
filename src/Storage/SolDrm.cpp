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

void SolDrm::mbedtlsPrintError(const char *func, int ret) {
    char errorBuf[100];
    mbedtls_strerror(ret, errorBuf, sizeof(errorBuf));
    log_e("%s failed: %s (error code: -0x%04x)", func, errorBuf, -ret);
}

void SolDrm::initIfNecessary() {
    if (initialized_) {
        return;
    }
    mbedtls_ctr_drbg_init(&ctrDrbg_);
    mbedtls_entropy_init(&entropy_);

    int ret = mbedtls_ctr_drbg_seed(&ctrDrbg_, mbedtls_entropy_func, &entropy_, nullptr, 0);
    if (ret != 0) {
        mbedtlsPrintError("mbedtls_ctr_drbg_seed", ret);
    }

    // If we don't have a private key, generate one
    if (!prefs_.privateKey.get().has_value()) {
        generateKeypair();
    }

    initialized_ = true;
}

auto SolDrm::generateKeypair(unsigned int keySize) -> int {
    log_d("Generating RSA keypair of size %d", keySize);

    PROFILE_START(generateKeypair);

    int ret;
    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    defer { mbedtls_pk_free(&key); };

    ret = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    if (ret != 0) {
        mbedtlsPrintError("mbedtls_pk_setup", ret);
        return ret;
    }

    // Takes ~4s for a 1024-bit key
    constexpr int exponent = 65537;
    ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(key), mbedtls_ctr_drbg_random, &ctrDrbg_, keySize,
                              exponent);
    if (ret != 0) {
        mbedtlsPrintError("mbedtls_rsa_gen_key", ret);
        return ret;
    }

    // Allocate enough space for DER format
    size_t outputBufferSize = (keySize / 8) * 8 + 64;
    char *outputBuffer = static_cast<char *>(SpiRamMalloc(outputBufferSize));
    defer { SpiRamFree(outputBuffer); };

    // Write the private key in DER format (~609 bytes)
    ret = mbedtls_pk_write_key_der(&key, reinterpret_cast<unsigned char *>(outputBuffer),
                                   outputBufferSize);
    if (ret <= 0) {
        mbedtlsPrintError("mbedtls_pk_write_key_der", ret);
        return ret;
    }
    size_t derLen = ret;
    log_i("Private key DER length: %zu bytes", derLen);

    // Important: DER data starts from the end of the buffer!
    auto *derStart = reinterpret_cast<unsigned char *>(outputBuffer + outputBufferSize - derLen);
    prefs_.privateKey.set({derStart, derStart + derLen});

    PROFILE_END_LOG_MS(generateKeypair);

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
        mbedtlsPrintError("mbedtls_pk_parse_key", ret);
        return "";
    }

    size_t estimatedPemSize = (privateKey.size() / 3) * 4 + 100;
    auto *outputBuffer = static_cast<unsigned char *>(SpiRamMalloc(estimatedPemSize));
    if (outputBuffer == nullptr) {
        log_e("Failed to allocate memory for output buffer");
        return "";
    }
    defer { SpiRamFree(outputBuffer); };

    ret = mbedtls_pk_write_pubkey_pem(&key, outputBuffer, estimatedPemSize);
    if (ret != 0) {
        mbedtlsPrintError("mbedtls_pk_write_pubkey_pem", ret);
        return "";
    }

    std::string publicKey = reinterpret_cast<char *>(outputBuffer);
    log_d("Public key:\n%s", publicKey.c_str());
    return publicKey;
}

auto SolDrm::decryptRSA(const uint8_t *data, size_t length) -> std::vector<uint8_t> {
    initIfNecessary();

    auto privateKeyOptional = prefs_.privateKey.get();
    if (!privateKeyOptional.has_value()) {
        log_e("Private key not found");
        return {};
    }
    auto privateKey = privateKeyOptional.value();

    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    defer { mbedtls_pk_free(&key); };

    int ret = mbedtls_pk_parse_key(&key, privateKey.data(), privateKey.size(), nullptr, 0,
                                   mbedtls_ctr_drbg_random, &ctrDrbg_);
    if (ret != 0) {
        mbedtlsPrintError("mbedtls_pk_parse_key", ret);
        return {};
    }

    if (mbedtls_pk_can_do(&key, MBEDTLS_PK_RSA)) {
        log_d("RSA key loaded successfully");
    } else {
        log_e("Failed to load RSA key");
        return {};
    }

    size_t keyLength = mbedtls_pk_get_len(&key);
    if (length != keyLength) {
        log_e("Encrypted data size (%zu) does not match key length (%zu)", length, keyLength);
        return {};
    }

    auto decryptedData = std::vector<uint8_t>(keyLength);
    size_t outputLength = 0;
    auto ctx = mbedtls_pk_rsa(key);

    ret = mbedtls_rsa_set_padding(ctx, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA1);
    if (ret != 0) {
        mbedtlsPrintError("mbedtls_rsa_set_padding", ret);
        return {};
    }

    // This takes ~160ms using a 1024-bit key
    ret = mbedtls_rsa_rsaes_oaep_decrypt(ctx, mbedtls_ctr_drbg_random, &ctrDrbg_, nullptr, 0,
                                         &outputLength, data, decryptedData.data(), keyLength);
    if (ret != 0) {
        mbedtlsPrintError("mbedtls_rsa_rsaes_oaep_decrypt", ret);
        return {};
    }
    decryptedData.resize(outputLength);

    return decryptedData;
}

auto SolDrm::decryptAESInPlace(uint8_t *encryptedData, size_t encryptedLength,
                               const std::vector<uint8_t> &aesKey,
                               const std::array<uint8_t, 16> &iv,
                               const std::array<uint8_t, 16> &tag) -> bool {

    mbedtls_gcm_context aes;
    mbedtls_gcm_init(&aes);
    defer { mbedtls_gcm_free(&aes); };

    if (aesKey.size() != 32) {
        log_e("Invalid AES key size. Expected 32 bytes for AES-256, got %zu bytes", aesKey.size());
        return false;
    }

    int ret = mbedtls_gcm_setkey(&aes, MBEDTLS_CIPHER_ID_AES, aesKey.data(), 256);
    if (ret != 0) {
        mbedtlsPrintError("mbedtls_gcm_setkey", ret);
        return false;
    }

    ret = mbedtls_gcm_auth_decrypt(&aes, encryptedLength, iv.data(), iv.size(), nullptr, 0,
                                   tag.data(), tag.size(), encryptedData, encryptedData);

    if (ret != 0) {
        mbedtlsPrintError("mbedtls_gcm_auth_decrypt", ret);
        log_d("AES Key: %s", BytesToHex(aesKey.data(), aesKey.size()).c_str());
        log_d("IV: %s", BytesToHex(iv.data(), iv.size()).c_str());
        log_d("Tag: %s", BytesToHex(tag.data(), tag.size()).c_str());
        log_d("First 16 bytes of encrypted data: %s",
              BytesToHex(encryptedData, std::min(size_t(16), encryptedLength)).c_str());
        return false;
    }

    return true;
}

auto SolDrm::decryptInPlaceIfNecessary(const std::vector<uint8_t> &aesKey,
                                       const std::shared_ptr<uint8_t[]> &data, size_t length)
    -> std::pair<std::shared_ptr<uint8_t[]>, size_t> {
    // Check if the file is encrypted
    auto fileHeader = parseContentFileHeader(data.get(), length);
    if (!fileHeader.has_value()) {
        // Not encrypted
        return {data, length};
    }

    size_t contentStart = ContentFileHeader::headerSize();

    // Decrypt the content with AES
    PROFILE_START(decryptAESInPlace);
    if (!decryptAESInPlace(data.get() + contentStart, length - contentStart - 1, aesKey,
                           fileHeader->aesNonce, fileHeader->aesTag)) {
        log_e("Failed to decrypt content with AES");
        return {nullptr, 0};
    }
    PROFILE_END_LOG_MS(decryptAESInPlace);

    // Now that we're decrypted, decompress the data
    PROFILE_START(decompress);
    defer { PROFILE_END_LOG_MS(decompress); };

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

    auto decompressedData = SpiRamArrayPtr<uint8_t>(fileHeader->decompressedSize);

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
        stream.avail_out = fileHeader->decompressedSize;

        log_d("Trying decompression with output buffer size %zu", fileHeader->decompressedSize);

        // Try to decompress
        status = mz_inflate(&stream, MZ_FINISH);
        log_d("Inflate status: %d, consumed %lu bytes, produced %lu bytes", status, stream.total_in,
              stream.total_out);

        if (status == MZ_STREAM_END) {
            // Success!
            break;
        } else {
            // Real error
            log_e("Decompression failed with status %d", status);
            return {nullptr, 0};
        }
    }

    return {decompressedData, stream.total_out};
}

auto SolDrm::parseKeyfile(const uint8_t *data, size_t length)
    -> std::optional<std::vector<uint8_t>> {
    // DRM'd files must start with SOLDRMv1
    if (!HasPrefix(reinterpret_cast<const char *>(data), FileHeader::magicHeader(),
                   std::min(length, FileHeader::magicHeaderSize()))) {
        return std::nullopt;
    }

    int dataPtr = FileHeader::magicHeaderSize();

    KeyFileHeader header;

    header.encryptionType = data[dataPtr];
    if (header.encryptionType != 1) {
        log_e("Unsupported encryption type %d", header.encryptionType);
        return std::nullopt;
    }

    dataPtr++;

    header.encryptedAESKeySize = (data[dataPtr] << 24) | (data[dataPtr + 1] << 16) |
                                 (data[dataPtr + 2] << 8) | data[dataPtr + 3];
    dataPtr += 4;

    if (length < (dataPtr + header.encryptedAESKeySize)) {
        log_w("Keyfile is too short for header %zu", length);
        return std::nullopt;
    }

    header.encryptedAESKey =
        std::vector<uint8_t>(&data[dataPtr], &data[dataPtr] + header.encryptedAESKeySize);

    log_d("Encrypted AES key of size %zu: %s", header.encryptedAESKey.size(),
          BytesToHex(header.encryptedAESKey.data(), header.encryptedAESKey.size()).c_str());

    // Decrypt the AES key using SolDrm's RSA decryption
    PROFILE_START(decryptRSA);
    auto aesKey = decryptRSA(header.encryptedAESKey.data(), header.encryptedAESKey.size());
    if (aesKey.empty()) {
        log_e("Failed to decrypt AES key");
        return std::nullopt;
    }
    PROFILE_END_LOG_MS(decryptRSA);

    return aesKey;
}

auto SolDrm::parseContentFileHeader(const uint8_t *data, size_t length)
    -> std::optional<ContentFileHeader> {
    // DRM'd files must start with SOLDRMv1
    if (!HasPrefix(reinterpret_cast<const char *>(data), FileHeader::magicHeader(),
                   std::min(length, FileHeader::magicHeaderSize()))) {
        return std::nullopt;
    }

    int dataPtr = FileHeader::magicHeaderSize();

    ContentFileHeader header;

    if (length < ContentFileHeader::headerSize()) {
        log_w("File is too short for header %zu %zu", length, ContentFileHeader::headerSize());
        return std::nullopt;
    }

    // Parse AES nonce and tag
    std::copy_n(&data[dataPtr], 16, header.aesNonce.begin());
    dataPtr += 16;
    std::copy_n(&data[dataPtr], 16, header.aesTag.begin());
    dataPtr += 16;

    header.compressionType = data[dataPtr];
    if (header.compressionType != 1) {
        log_e("Unsupported compression type %d", header.compressionType);
        return std::nullopt;
    }

    dataPtr++;

    header.decompressedSize = (data[dataPtr] << 24) | (data[dataPtr + 1] << 16) |
                              (data[dataPtr + 2] << 8) | data[dataPtr + 3];
    // dataPtr += 4;

    return header;
}
