#include "CrashReportWriter.hpp"

#include <cstdio>
#include <esp_core_dump.h>
#include <esp_flash.h>

#include "Communications/MultiPartData.hpp"
#include "Misc/Platform.hpp"
#include "Version.hpp"

auto CrashReportWriter::writeCrashReportIfExists() -> bool {
    // first check if a core dump is present
    size_t outAddr, outSize;
    if (esp_core_dump_image_get(&outAddr, &outSize) != ESP_OK) {
        log_i("no core dump found.");
        return true;
    }

    if (outSize == 0) {
        // may never hit this, but seems nice to have
        log_i("invalid core dump size");
        return false;
    }

    // at this point we know there is a core dump available.
    // write it and metadata into a file for later upload.
    if (!fileSystem_.createDir("/coredumps")) {
        log_e("failed to create /coredumps directory");
        return false;
    }

    log_i("core dump found for boot %d", bootNum_);
    uint8_t tmp[1024];
    if (!SafeSnprintf((char *)tmp, sizeof(tmp), "/coredumps/%d.coredump", bootNum_)) {
        return false;
    }

    log_i("saving core dump to file %s", tmp);
    MultiPartData mpData(fileSystem_, (char *)tmp);
    if (!mpData.startFile("file")) {
        log_e("failed to open core dump file %s", tmp);
        return false;
    }

    int bytesRemaining = outSize;
    auto currAddr = (uint32_t)outAddr;
    while (bytesRemaining > 0) {
        // read chunk
        int bytesToRead = bytesRemaining >= sizeof(tmp) ? sizeof(tmp) : bytesRemaining;
        if (esp_flash_read(nullptr, tmp, currAddr, bytesToRead) != ESP_OK) {
            log_e("could not read crash report from flash");
            return false;
        }
        if (!mpData.writeFileChunk(tmp, bytesToRead)) {
            log_e("failed to write address %" PRIu32 " to core dump file", currAddr);
            return false;
        }
        // updates
        bytesRemaining -= bytesToRead;
        currAddr += bytesToRead;
    }
    if (!mpData.endFile()) {
        return false;
    }

    if (!mpData.addKeyValue("git_version", GitVersionString())) {
        return false;
    }

    if (!mpData.addKeyValue("serial_number", GetSerialNumber())) {
        return false;
    }

    if (!mpData.addKeyValue("fw_version", SemanticVersionString())) {
        return false;
    }

    if (!mpData.endMultiPartData()) {
        return false;
    }

    // at this point file has been successfully written, we can remove the core dump
    // from flash.
    if (esp_core_dump_image_erase() != ESP_OK) {
        log_e("failed to erase core dump from flash.");
        return false;
    }

    return true;
}
