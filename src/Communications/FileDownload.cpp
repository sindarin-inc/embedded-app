#include "FileDownload.hpp"

#include <esp_timer.h>
#include <utility>

#include "Communications/HttpClient.hpp"
#include "sindarin-debug.h"

auto DownloadFile(const char *url, int length,
                  const std::function<bool(int httpCode, int contentLength)> &openedConnection,
                  const std::function<size_t(HttpClient &client, int index)> &readWrite,
                  const std::function<void(int bytesWritten, float percentage)> &progressCallback)
    -> bool {
    HttpClient client(url);

    auto time = esp_timer_get_time();
    auto downloadStart = time;

    client.open();

    esp_err_t err;
    if ((err = client.open()) != ESP_OK) {
        log_e("Failed to open HTTP connection: %s", esp_err_to_name(err));
        return false;
    }

    auto contentLength = client.fetchHeaders();
    if (length == 0) {
        length = contentLength;
    }
    if (contentLength != length) {
        log_w("Content length was %d but expected %d", contentLength, length);
    }

    auto httpCode = client.getStatusCode();
    if (httpCode != 200) {
        log_w("Status code was %d", httpCode);
    }

    if (!openedConnection(httpCode, contentLength)) {
        return false;
    }

    int totalReadLen = 0;
    int lastPercentage = 0;

    while (totalReadLen < contentLength) {
        int readLen = readWrite(client, totalReadLen);
        if (readLen <= 0) {
            log_w("Error read data (readLen == 0)");
        }

        totalReadLen += readLen;

        float percentProgress = (float)totalReadLen / length * 100.0f;
        auto now = millis();
        // Report every 5-20s OR every 5% progress, whichever comes first
        if ((now - time > 5000) && (now - time > 20000 || (percentProgress - lastPercentage) > 5)) {
            log_d("Downloaded %.1f%% (%.1f KB/s)", percentProgress,
                  totalReadLen / 1024.0f / ((millis() - downloadStart) / 1000.0f));
            progressCallback(totalReadLen, percentProgress);
            time = now;
            lastPercentage = percentProgress;
        }
    }

    int64_t end = esp_timer_get_time();
    int64_t duration = end - downloadStart;
    float bps = totalReadLen / (duration / 1000000.0f);

    log_i("DownloadFile complete %d in %" PRId64 "us (%f Bps %f KB/s, %f kbps)", totalReadLen,
          duration, bps, bps / 1000, 8 * bps / 1000);

    float percentProgress = (float)totalReadLen / length * 100.0f;
    progressCallback(totalReadLen, percentProgress);

    client.close();

    if (length != totalReadLen) {
        log_e("Wrote fewer bytes than expected, expected %d bytes but got %d bytes", length,
              totalReadLen);
        return false;
    }
    return true;
}

auto DownloadFile(const char *url, int length,
                  const std::function<bool(int httpCode, int contentLength)> &openedConnection,
                  const std::function<size_t(const uint8_t *buf, size_t size)> &writeBytes,
                  const std::function<void(int bytesWritten, float percentage)> &progressCallback)
    -> bool {

    // W25Q512JV* chips have a page size of 256 bytes so we should only use a multiple of that
    constexpr int bufferSize = 4096;
    auto *buffer = (uint8_t *)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM);

    return DownloadFile(
        url, length, openedConnection,
        [buffer, writeBytes](HttpClient &client, int index) {
            int readLen = client.read((char *)buffer, bufferSize);

            auto written = writeBytes(buffer, (size_t)readLen);
            if (written != readLen) {
                log_w("Failed to write to file, wrote %zu but expected %d", written, readLen);
            }

            return readLen;
        },
        progressCallback);
}

auto DownloadFile(const char *url, FileSystem &fileSystem, const char *path, int length,
                  const std::function<void(float percentage)> &progressCallback) -> bool {
    // TODO: Support resuming?
    if (fileSystem.exists(path)) {
        log_w("File %s already exists, deleting", path);
        fileSystem.remove(path);
        log_d("Deleted %s", path);
    }

    SHOW_HEAP();

    auto file = fileSystem.open(path, FILE_WRITE, true);
    if (!file) {
        log_e("Failed to open file %s for writing", path);
        return false;
    }

    // Load the whole file into SPIRAM for fast network operations then write to disk
    // TODO: We could probably improve overall speed, while minimizing WiFi connected time by
    // writing to disk as we go from another core or writing to disk from a lower priority task. But
    // that will be harder to make
    uint8_t *fileBuffer;
    int fileLength;

    // Because file writes tend to be twice the time the http download takes right now, adjust the
    // progress accordingly

    auto fileProgress = [progressCallback](int bytesWritten, float percentage) {
        progressCallback(percentage / 3);
    };

    DownloadFile(
        url, length,
        [&fileBuffer, &fileLength](int httpCode, int contentLength) -> bool {
            fileLength = contentLength;
            fileBuffer = (uint8_t *)heap_caps_malloc(contentLength, MALLOC_CAP_SPIRAM);
            if (fileBuffer == nullptr) {
                log_e("Failed to allocate %d bytes for file download", contentLength);
                return false;
            }
            return true;
        },
        [&fileBuffer](HttpClient &client, int index) -> size_t {
            constexpr int bufferSize = 16384;
            return client.read((char *)(fileBuffer + index), bufferSize);
        },
        fileProgress);

    auto time = esp_timer_get_time();
    auto downloadStart = time;

    int written = 0;
    while (written < fileLength) {
        // Every ~ 6s at ~40-44KB/S
        auto bytesWrittenToFile = file.write(fileBuffer, std::min(fileLength - written, 262144));
        if (bytesWrittenToFile <= 0) {
            log_w("Failed to write to file");
            break;
        }
        written += bytesWrittenToFile;
        progressCallback(66.6 * written / fileLength + 33.4);
    }

    file.close();

    int64_t end = esp_timer_get_time();
    int64_t duration = end - downloadStart;
    float bps = fileLength / (duration / 1000000.0f);
    log_i("File write complete %d in %" PRId64 "us (%f Bps %f KB/s, %f kbps)", fileLength, duration,
          bps, bps / 1000, 8 * bps / 1000);

    heap_caps_free(fileBuffer);

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
