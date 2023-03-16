#include "FileDownload.hpp"

#include <esp_timer.h>
#include <utility>

#include "Communications/HttpClient.hpp"
#include "Communications/SindarinAPI.hpp"
#include "Misc/GUnzipStream.hpp"
#include "sindarin-debug.h"

auto DownloadFile(
    const char *url, const std::function<bool(int httpCode, int contentLength)> &openedConnection,
    const std::function<size_t(HttpClient &client, int index, int contentLength)> &readWrite,
    const std::function<void(int bytesWritten, float percentage)> &progressCallback, bool gzip)
    -> bool {

    HttpClient client(url);

    if (SindarinAPI::isSindarinUrl(url)) {
        std::shared_ptr<SindarinAPI> api = SindarinAPI::getInstance();
        if (api) {
            log_d("Attaching Sindarin auth to HTTP request to %s", url);
            api->setAuth(client);
        }
    }

    auto time = esp_timer_get_time();
    auto downloadStart = time;

    if (gzip) {
        client.setHeader("Accept-Encoding", "gzip");
        // TODO: check server headers to verify we got gzip
    }

    esp_err_t err = client.open();
    if (err != ESP_OK) {
        log_e("Failed to open HTTP connection: %s", esp_err_to_name(err));
        return false;
    }

    auto contentLength = client.fetchHeaders();

    auto httpCode = client.getStatusCode();
    if (httpCode != 200) {
        log_w("Status code was %d", httpCode);
        return false;
    }

    if (gzip) {
        char *contentEncoding = nullptr;
        err = client.getHeader("Content-Encoding", &contentEncoding);
        if (err == ESP_ERR_NOT_FOUND) {
            log_e("Requested GZIP but did not get any Content-Encoding header!");
        } else if (err != ESP_OK) {
            log_e("Unexpected error getting Content-Encoding header %d %s", err,
                  esp_err_to_name(err));
        } else if (contentEncoding == nullptr) {
            log_e("contentEncoding is null but didn't throw an error");
        } else if (strcmp(contentEncoding, "gzip") != 0) {
            log_e("Requested GZIP but instead got Content-Encoding: %s!", contentEncoding);
        }
    }

    if (contentLength <= 0) {
        // if we somehow don't have a content length header, consider it an error for now
        log_e("Server did not return a content length for %s", url);
        return false;
    }

    if (!openedConnection(httpCode, contentLength)) {
        // Get the body anyways, for debugging, since it might contain an error message
        char buffer[256];
        auto read = client.read(buffer, sizeof(buffer));
        if (read > 0) {
            log_w("Error body: %s", buffer);
        }
        return false;
    }

    int totalReadLen = 0;
    int lastPercentage = 0;

    while (totalReadLen < contentLength) {
        int readLen = readWrite(client, totalReadLen, contentLength);
        if (readLen <= 0) {
            log_w("Error read data (readLen == 0)");
            if (!client.connected) {
                log_w("Connection closed, bailing out");
                client.close();
                return false;
            }
        }

        totalReadLen += readLen;

        float percentProgress = (float)totalReadLen / contentLength * 100.0f;
        auto now = esp_timer_get_time();
        // Report every 5-20s OR every 5% progress, whichever comes first
        if ((now - time > 5000000) &&
            (now - time > 20000000 || (percentProgress - lastPercentage) > 5)) {
            log_d("Downloaded %.1f%% (%.1f KB/s)", percentProgress,
                  totalReadLen / 1024.0f / ((esp_timer_get_time() - downloadStart) / 1000000.0f));
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

    float percentProgress = (float)totalReadLen / contentLength * 100.0f;
    progressCallback(totalReadLen, percentProgress);

    client.close();

    // verify we received enough bytes according to the content length header
    // rather than the length passed in, this works for compressed or uncompressed files
    if (contentLength != totalReadLen) {
        log_e("Wrote fewer bytes than expected, expected %d bytes but got %d bytes", contentLength,
              totalReadLen);
        return false;
    }
    return true;
}

auto DownloadFile(const char *url,
                  const std::function<bool(int httpCode, int contentLength)> &openedConnection,
                  const std::function<size_t(const uint8_t *buf, size_t size)> &writeBytes,
                  const std::function<void(int bytesWritten, float percentage)> &progressCallback,
                  bool gzip) -> bool {

    // W25Q512JV* chips have a page size of 256 bytes so we should only use a multiple of that
    constexpr int bufferSize = 4096;
    auto *buffer = (uint8_t *)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM);

    return DownloadFile(
        url, openedConnection,
        [buffer, bufferSize, writeBytes](HttpClient &client, int index, int contentLength) {
            int readLen = client.read((char *)buffer, std::min(bufferSize, contentLength - index));

            auto written = writeBytes(buffer, (size_t)readLen);
            if (written != readLen) {
                log_w("Failed to write to file, wrote %zu but expected %d", written, readLen);
            }

            return readLen;
        },
        progressCallback, gzip);
}

auto DownloadFile(const char *url, FileSystem &fileSystem, const char *path, int length,
                  const std::function<void(float percentage)> &progressCallback, bool gzip)
    -> bool {

    std::string fullPath(path);

    SHOW_HEAP();
    // TODO: Support resuming?
    if (fileSystem.exists(fullPath.c_str())) {
        log_w("File %s already exists, deleting", path);
        fileSystem.remove(path);
        log_d("Deleted %s", path);
    }

    // Load the whole file into SPIRAM for fast network operations then write to disk
    // TODO: We could probably improve overall speed, while minimizing WiFi connected time by
    // writing to disk as we go from another core or writing to disk from a lower priority task. But
    // that will be harder to make
    uint8_t *fileBuffer = nullptr;
    int fileLength;

    // Because file writes tend to be twice the time the http download takes right now, adjust the
    // progress accordingly

    auto fileProgress = [progressCallback](int bytesWritten, float percentage) {
        progressCallback(percentage / 3);
    };

    auto success = DownloadFile(
        url,
        [&fileBuffer, &fileLength](int httpCode, int contentLength) -> bool {
            fileLength = contentLength;
            fileBuffer = (uint8_t *)heap_caps_malloc(contentLength, MALLOC_CAP_SPIRAM);
            if (fileBuffer == nullptr) {
                log_e("Failed to allocate %d bytes for file download", contentLength);
                return false;
            }
            return true;
        },
        [&fileBuffer](HttpClient &client, int index, int contentLength) -> size_t {
            constexpr int bufferSize = 16384;
            return client.read((char *)(fileBuffer + index),
                               std::min(bufferSize, contentLength - index));
        },
        fileProgress, gzip);

    auto time = esp_timer_get_time();
    auto downloadStart = time;

    if (!success) {
        log_e("Failed to download file");
        return false;
    }

    auto file = fileSystem.open(fullPath.c_str(), FILE_WRITE, true);
    if (!file) {
        log_e("Failed to open file %s for writing", fullPath.c_str());
        return false;
    }

    int written = 0;
    while (written < fileLength) {
        // Every ~ 6s at ~40-44KB/S
        auto bytesWrittenToFile =
            file.write(fileBuffer + written, std::min(fileLength - written, 262144));
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
    size_t fileSize = 0;
    if (gzip) {
        file = fileSystem.open(fullPath.c_str());
        GUnzipStream gzStream(file);
        fileSize = gzStream.size();
    } else {
        file = fileSystem.open(fullPath.c_str());
        fileSize = file.size();
        file.close();
    }

    if (length != fileSize) {
        // TODO: What's the best API to get errors from the stream?
        log_e("Downloaded file is shorter than expected, expected %d bytes but got %zu bytes",
              length, fileSize);
        return false;
    }

    return true;
}
