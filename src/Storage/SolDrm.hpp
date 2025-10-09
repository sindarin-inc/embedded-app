#pragma once

#include <EPubFile/EPubDrm.hpp>
#include <array>
#include <mbedtls/aes.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pk.h>
#include <memory>
#include <string>
#include <vector>

#include "Storage/PreferencesStore.hpp"

class SolDrm : public EPubDrm {
public:
    SolDrm(PreferencesStore &prefs) : prefs_(prefs){};
    ~SolDrm() override;

    auto publicKeyPem() -> std::string;
    auto parseKeyfile(const uint8_t *data, size_t length)
        -> std::optional<std::vector<uint8_t>> override;
    auto decryptInPlaceIfNecessary(const std::vector<uint8_t> &aesKey,
                                   const std::shared_ptr<uint8_t[]> &data, size_t length)
        -> std::pair<std::shared_ptr<uint8_t[]>, size_t> override;

private:
    class FileHeader {
    public:
        static auto magicHeader() -> const char * { return "SOLDRMv01"; }
        static auto magicHeaderSize() -> size_t { return 9; }
    };

    class KeyFileHeader : FileHeader {
    public:
        uint8_t encryptionType{};
        size_t encryptedAESKeySize{};
        std::vector<uint8_t> encryptedAESKey;
    };

    class ContentFileHeader : FileHeader {
    public:
        std::array<uint8_t, 16> aesNonce{};
        std::array<uint8_t, 16> aesTag{};
        uint8_t compressionType{};
        size_t decompressedSize{};

        static auto headerSize() -> size_t { return magicHeaderSize() + 16 + 16 + 1 + 4; };
    };

    PreferencesStore &prefs_;
    bool initialized_ = false;
    mbedtls_ctr_drbg_context ctrDrbg_{};
    mbedtls_entropy_context entropy_{};

    void mbedtlsPrintError(const char *func, int ret);
    void initIfNecessary();
    auto generateKeypair(unsigned int keySize = 1024) -> int;
    auto parseContentFileHeader(const uint8_t *data, size_t length)
        -> std::optional<ContentFileHeader>;
    auto decryptRSA(const uint8_t *data, size_t length) -> std::vector<uint8_t>;
    auto decryptAESInPlace(uint8_t *encryptedData, size_t encryptedLength,
                           const std::vector<uint8_t> &key, const std::array<uint8_t, 16> &iv,
                           const std::array<uint8_t, 16> &tag) -> bool;
};
