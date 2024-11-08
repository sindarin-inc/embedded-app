#pragma once

#include <array>
#include <mbedtls/aes.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pk.h>
#include <memory>
#include <string>
#include <vector>

#include "Storage/PreferencesStore.hpp"

class SolDrm {
public:
    SolDrm(PreferencesStore &prefs) : prefs_(prefs){};
    ~SolDrm();

    auto publicKeyPem() -> std::string;
    auto decryptInPlaceIfNecessary(const std::shared_ptr<uint8_t[]> &data, size_t length)
        -> std::pair<std::shared_ptr<uint8_t[]>, size_t>;

private:
    struct FileHeader {
        size_t headerSize;
        uint8_t encryptionType;
        std::vector<uint8_t> encryptedAESKey;
        std::array<uint8_t, 16> aesNonce{};
        std::array<uint8_t, 16> aesTag{};
        uint8_t compressionType;
        size_t decompressedSize;

        static auto magicHeader() -> const char * { return "SOLDRMv01"; }
        static auto magicHeaderSize() -> size_t { return 9; }

        auto contentStart() const -> size_t { return magicHeaderSize() + headerSize; };
    };

    PreferencesStore &prefs_;
    bool initialized_ = false;
    mbedtls_ctr_drbg_context ctrDrbg_{};
    mbedtls_entropy_context entropy_{};

    void mbedtlsPrintError(const char *func, int ret);
    void initIfNecessary();
    auto generateKeypair(unsigned int keySize = 1024) -> int;
    auto parseFileHeader(const uint8_t *data, size_t length) -> std::optional<FileHeader>;
    auto decryptRSA(const uint8_t *data, size_t length)
        -> std::pair<std::shared_ptr<uint8_t[]>, size_t>;
    auto decryptAESInPlace(uint8_t *encryptedData, size_t encryptedLength, const uint8_t *key,
                           size_t keyLength, const std::array<uint8_t, 16> &iv,
                           const std::array<uint8_t, 16> &tag) -> bool;
};
