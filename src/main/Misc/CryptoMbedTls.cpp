#include <mbedtls/md.h>

#include "Crypto.hpp"
#include "Storage/FileSystem.hpp"
#include "StringUtil.hpp"
#include "sindarin-debug.h"

auto CalculateFileHash(const FileSystem &fileSystem, const char *path) -> std::string {
    SFile file = fileSystem.open(path);
    if (!file) {
        log_e("Failed to open file: %s is the filename", path);
        return "";
    }

    PROFILE_START(mbedtls_md);

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_type_t mdType = MBEDTLS_MD_SHA256;
    if (mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(mdType), 0)) {
        log_e("Failed to mbedtls_md_setup");
    }
    if (mbedtls_md_starts(&ctx)) {
        log_e("Failed to mbedtls_md_starts");
    }
    uint8_t buffer[1024];
    size_t bytesRead;
    while ((bytesRead = file.read(buffer, sizeof(buffer))) > 0) {
        mbedtls_md_update(&ctx, buffer, bytesRead);
    }
    file.close();

    unsigned char output[32];
    if (mbedtls_md_finish(&ctx, output)) {
        log_e("Failed to mbedtls_md_finish");
    }
    auto hashString = BytesToHex(output, sizeof(output));
    log_d("File hash: %s", hashString.c_str());

    mbedtls_md_free(&ctx);
    PROFILE_END_LOG_MS(mbedtls_md);

    return hashString;
}

auto CalculateStringHash(const std::string &input) -> std::string {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_type_t mdType = MBEDTLS_MD_SHA256;
    if (mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(mdType), 0)) {
        log_e("Failed to mbedtls_md_setup");
    }
    if (mbedtls_md_starts(&ctx)) {
        log_e("Failed to mbedtls_md_starts");
    }

    mbedtls_md_update(&ctx, (const unsigned char *)input.c_str(), input.size());

    unsigned char output[32];
    if (mbedtls_md_finish(&ctx, output)) {
        log_e("Failed to mbedtls_md_finish");
    }
    auto hashString = BytesToHex(output, sizeof(output));
    log_d("File hash: %s", hashString.c_str());

    mbedtls_md_free(&ctx);

    return hashString;
}