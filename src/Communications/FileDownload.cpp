#include "FileDownload.hpp"

#include <utility>

#include "sindarin-debug.h"

auto DownloadFile(HTTPClient &httpClient, int length,
                  const std::function<size_t(const uint8_t *buf, size_t size)> &writeBytes,
                  const std::function<void(int bytesWritten, float percentage)> &progressCallback)
    -> bool {
    // W25Q512JV* chips have a page size of 256 bytes so that seems like a good value to use
    // here.
    constexpr int bufferSize = 256;
    uint8_t buff[bufferSize];

    auto *stream = httpClient.getStreamPtr();
    auto time = millis();
    auto downloadStart = time;

    // Read all data from server
    length = length > 0 ? length : httpClient.getSize();
    int len = length;
    int lastPercentage = 0;
    int bytesWritten = 0;
    while (stream->connected() && (len > 0 || len == -1)) {
        auto available = stream->available();
        if (available == 0) {
            // TODO: In the future it will be good to use a HTTP library that allows us to do
            // some kind of select or block exactly until data is available
            // TODO: We're still sometimes getting a watchdog timer issue on Core 0
            log_d("Waiting for data...");
            delay(1000);
            continue;
        }

        // TODO: Does readBytes ever sleep? I think maybe it doesn't
        // https://github.com/espressif/arduino-esp32/blob/d8f98450fc1f1e1646077260a22287df6332580a/cores/esp32/Stream.cpp#L285
        // That's weird for a http client, at least on a high priority task.
        int c = stream->readBytes(buff, std::min(available, bufferSize));
        // log_d("Reading %d bytes (%d available)", std::min(available, bufferSize), available);

        auto written = writeBytes(buff, c);
        if (written != c) {
            log_w("Failed to write to file, wrote %zu but expected %d", written, c);
            break;
        }
        bytesWritten += written;
        if (len > 0) {
            len -= c;
        }

        float percentProgress = (float)bytesWritten / length * 100.0f;
        auto now = millis();
        // Report every 5-20s OR every 5% progress, whichever comes first
        if ((now - time > 5000) && (now - time > 20000 || (percentProgress - lastPercentage) > 5)) {
            log_d("Downloaded %.1f%% (%.1f KB/s)", percentProgress,
                  bytesWritten / 1024.0f / ((millis() - downloadStart) / 1000.0f));
            progressCallback(bytesWritten, percentProgress);
            time = now;
            lastPercentage = percentProgress;
        }

        vPortYield();
    }

    float percentProgress = (float)bytesWritten / length * 100.0f;
    progressCallback(bytesWritten, percentProgress);

    if (length != bytesWritten) {
        // TODO: What's the best API to get errors from the stream?
        log_e("Wrote fewer bytes than expected, expected %d bytes but got %d bytes", length,
              bytesWritten);
        return false;
    } else {
        return true;
    }
}

auto DownloadFile(HTTPClient &httpClient, FileSystem &fileSystem, const char *path, int length,
                  const std::function<void(int bytesWritten, float percentage)> &progressCallback)
    -> bool {
    // TODO: Support resuming?
    if (fileSystem.exists(path)) {
        log_w("File %s already exists, deleting", path);
        fileSystem.remove(path);
        log_d("Deleted %s", path);
    }

    auto file = fileSystem.open(path, FILE_WRITE, true);
    if (!file) {
        log_e("Failed to open file %s for writing", path);
        return false;
    }

    DownloadFile(
        httpClient, length, [&](const uint8_t *buf, size_t size) { return file.write(buf, size); },
        progressCallback);

    file.close();

    // The file must be re-opened to check its size
    file = fileSystem.open(path);
    auto fileSize = file.size();
    file.close();

    if (length != fileSize) {
        // TODO: What's the best API to get errors from the stream?
        log_e("Downloaded file is shorter than expected, expected %d bytes but got %zu bytes",
              length, fileSize);
        return false;
    } else {
        return true;
    }
}
