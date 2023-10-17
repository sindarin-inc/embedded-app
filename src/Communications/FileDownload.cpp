#include "FileDownload.hpp"

#include <esp_timer.h>
#include <utility>

#include "Communications/HttpClient.hpp"
#include "Communications/SindarinAPI.hpp"
#include "Misc/Defer.hpp"
#include "Misc/GUnzipStream.hpp"
#include "sindarin-debug.h"

auto DownloadFile(
    const char *url, const std::function<bool(int httpCode, int contentLength)> &openedConnection,
    const std::function<size_t(HttpClient &client, int index, int contentLength)> &readWrite,
    const std::function<void(int bytesWritten, float percentage)> &progressCallback, bool gzip)
    -> int {

    HttpClient client(url);

    if (SindarinAPI::isSindarinUrl(url)) {
        SindarinAPI::getInstance()->setAuth(client);
    }

    auto time = esp_timer_get_time();
    auto downloadStart = time;

    if (gzip) {
        auto err = client.setHeader("Accept-Encoding", "gzip");
        if (err != ESP_OK) {
            return ERR_SET_HEADER;
        }
    }

    esp_err_t err = client.open();
    if (err != ESP_OK) {
        return ERR_CLIENT_OPEN;
    }
    defer { client.close(); };

    auto contentLength = client.fetchHeaders();
    if (contentLength <= 0) {
        // if we somehow don't have a content length header, consider it an error for now
        log_e("Server did not return a content length for %s", url);
        return ERR_CONTENT_LENGTH;
    }

    auto httpCode = client.getStatusCode();
    if (httpCode != 200) {
        log_w("Status code was %d", httpCode);
        return httpCode;
    }

    if (gzip) {
        char *contentEncoding = nullptr;
        err = client.getResponseHeader("Content-Encoding", &contentEncoding);
        if (err == ESP_ERR_NOT_FOUND) {
            log_e("Requested GZIP but did not get any Content-Encoding header!");
        } else if (err != ESP_OK) {
            log_e("Unexpected error getting Content-Encoding header %d %s", err,
                  esp_err_to_name(err));
        } else if (contentEncoding == nullptr) {
            log_e("contentEncoding is null but didn't throw an error");
        } else if (strcmp(contentEncoding, "gzip") != 0) {
            log_e("Requested GZIP but instead got Content-Encoding: %s!", contentEncoding);
        } else {
            log_d("Got %s content encoding", contentEncoding);
        }
    }

    if (!openedConnection(httpCode, contentLength)) {
        // Get the body anyways, for debugging, since it might contain an error message
        char buffer[256];
        auto read = client.read(buffer, sizeof(buffer));
        if (read > 0) {
            log_w("Error body: %s", buffer);
        }
        return ERR_OPEN_BLOCK;
    }

    int totalReadLen = 0;
    int lastPercentage = 0;

    while (totalReadLen < contentLength) {
        int readLen = readWrite(client, totalReadLen, contentLength);
        if (readLen <= 0) {
            log_w("readWrite failed (readLen == %d)", readLen);
            if (!client.connected) {
                log_w("Connection closed, bailing out");
                return ERR_CLIENT_DISCONNECTED;
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
            if (progressCallback != nullptr) {
                progressCallback(totalReadLen, percentProgress);
            }
            time = now;
            lastPercentage = percentProgress;
        }
    }

    int64_t end = esp_timer_get_time();
    int64_t duration = end - downloadStart;
    float bps = totalReadLen / (duration / 1000000.0f);

    log_i("Downloadeded %d bytes in %.2fs (%.1f KB/s)", totalReadLen, duration / 1000000.0f,
          bps / 1000);

    float percentProgress = (float)totalReadLen / contentLength * 100.0f;
    if (progressCallback != nullptr) {
        progressCallback(totalReadLen, percentProgress);
    }

    // verify we received enough bytes according to the content length header
    // rather than the length passed in, this works for compressed or uncompressed files
    if (contentLength != totalReadLen) {
        log_e("Wrote fewer bytes than expected, expected %d bytes but got %d bytes", contentLength,
              totalReadLen);
        return ERR_SIZE_MISMATCH;
    }

    return httpCode;
}

auto DownloadFile(const char *url,
                  const std::function<bool(int httpCode, int contentLength)> &openedConnection,
                  const std::function<size_t(const uint8_t *buf, size_t size)> &writeBytes,
                  const std::function<void(int bytesWritten, float percentage)> &progressCallback,
                  bool gzip) -> int {
    constexpr int bufferSize = 4096;
    auto buffer = SpiRamArrayPtr<uint8_t>(bufferSize);

    return DownloadFile(
        url, openedConnection,
        [buffer, bufferSize, writeBytes](HttpClient &client, int index, int contentLength) {
        int readLen =
            client.read((char *)buffer.get(), std::min(bufferSize, contentLength - index));

        auto written = writeBytes(buffer.get(), (size_t)readLen);
        if (written != readLen) {
            log_w("Failed to write to file, wrote %zu but expected %d", written, readLen);
        }

        return readLen;
        },
        progressCallback, gzip);
}

struct DataBlock {
    uint8_t *data;
    int size;
};

auto DownloadFile(const char *url, FileSystem &fileSystem, Telemetry &telemetry, const char *path,
                  int length, const std::function<void(float percentage)> &progressCallback,
                  bool gzip) -> int {
    std::string fullPath(path);

    SHOW_HEAP();
    // TODO: Support resuming?
    if (fileSystem.exists(fullPath.c_str())) {
        log_w("File %s already exists, deleting", path);
        fileSystem.remove(path);
        log_d("Deleted %s", path);
    }

    // Disk writes are relatively slow (~40KB/s) while network downloads may be 10x faster. To
    // minimize the chances of a network hiccup breaking the download, we want to download as fast
    // as possible to SPIRAM but we still want to be able to download large files that _may_ not
    // fully fit in SPIRAM. We accomplish this by running the disk writing in a separate task
    // (thread) and passing data between the downloader and the file writer using a FreeRTOS queue.

    // 70 queue slots * 65536 bytes = 4587520 bytes. Any files bigger than this _might_ get their
    // download delayed by disk write performance.
    constexpr int bufferSize = 65536;
    QueueHandle_t bufferQueue = xQueueCreate(70, sizeof(DataBlock));
    if (bufferQueue == NULL) {
        log_e("xQueueCreate failed");
        return ERR_ALLOCATION;
    }

    int fileLength = 0;
    auto openedConnection = [&fileLength](int httpCode, int contentLength) -> bool {
        fileLength = contentLength;
        // TODO: Bail out early if we don't have enough disk space to do this download
        return true;
    };

    auto reader = [bufferQueue](HttpClient &client, int index, int contentLength) -> size_t {
        uint8_t *data = (uint8_t *)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM);
        if (data == nullptr) {
            log_e("heap_caps_malloc(%d) failed", bufferSize);
            return 0;
        }
        int bytesRead = client.read((char *)data, bufferSize);
        if (bytesRead <= 0) {
            log_w("client.read returned %d bytes", bytesRead);
            heap_caps_free(data);
            return 0;
        }
        DataBlock block = {data, bytesRead};
        if (xQueueSend(bufferQueue, &block, portMAX_DELAY) != pdPASS) {
            log_e("xQueueSend failed");
            heap_caps_free(data);
            return 0;
        }
        return bytesRead;
    };

    auto downloadStart = esp_timer_get_time();

    // Spin up the low-priority disk writer task
    DispatchQueue::dispatchOneOff(
        [&fileSystem, bufferQueue, fullPath, &fileLength, progressCallback, downloadStart] {
        auto file = fileSystem.open(fullPath.c_str(), FILE_WRITE, true);
        if (!file) {
            log_e("Failed to open file %s for writing", fullPath.c_str());
            return;
        }
        defer { file.close(); };
        int written = 0;
        auto time = esp_timer_get_time();

        while (true) {
            DataBlock block;
            if (xQueueReceive(bufferQueue, &block, portMAX_DELAY) != pdPASS) {
                log_e("xQueueReceive failed");
            }

            // TODO: Remove after testing
            log_d("Writing %d bytes %p", block.size, block.data);

            // This is the signal for EOF
            if (block.data == nullptr) {
                break;
            }

            auto bytesWritten = file.write(block.data, block.size); // Adjust size as needed
            if (bytesWritten != block.size) {
                log_e("Failed to write %d bytes, wrote %zu", block.size, bytesWritten);
                // TODO: Fail the download
            }

            written += bytesWritten;

            // Report every 5s
            auto now = esp_timer_get_time();
            if (now - time > 5000000) {
                float percentProgress = (float)written / fileLength * 100.0f;
                log_d("Wrote %.1f%% (%.1f KB/s)", percentProgress,
                      fileLength / 1024.0f / ((esp_timer_get_time() - downloadStart) / 1000000.0f));
                progressCallback(percentProgress);
                time = now;
            }

            heap_caps_free(block.data);
        }
        UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
        log_d("Disk writer task stack high watermark: %d", watermark);
        },
        4000, 0, 1);

    auto httpCode = DownloadFile(url, openedConnection, reader, nullptr, gzip);

    // Once we get here the download is done, so we can signal the writer task to exit
    DataBlock block = {nullptr, 0};
    if (xQueueSend(bufferQueue, &block, portMAX_DELAY) != pdPASS) {
        log_e("xQueueSend error");
    }

    // Wait for the disk write to complete
    while (uxQueueMessagesWaiting(bufferQueue) > 0) {
        log_d("Waiting for file write...");
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    auto diskWriteEnd = esp_timer_get_time();

    if (httpCode != 200) {
        log_e("Failed to download file");
        return httpCode;
    }

    int64_t diskWriteDuration = diskWriteEnd - downloadStart;
    float diskWriteBps = fileLength / (diskWriteDuration / 1000000.0f);
    log_i("Downloaded+wrote %d bytes in %.2fs (%.1f KB/s)", fileLength,
          diskWriteDuration / 1000000.0f, diskWriteBps / 1000);

    // Open the file to make sure it's the right size
    size_t fileSize = 0;
    auto file = fileSystem.open(fullPath.c_str());
    if (gzip) {
        GUnzipStream gzStream(file);
        fileSize = gzStream.size();
    } else {
        fileSize = file.size();
    }
    file.close();

    if (length != fileSize) {
        // TODO: What's the best API to get errors from the stream?
        log_e("Downloaded file is shorter than expected, expected %d bytes but got %zu bytes",
              length, fileSize);
        telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "wrong-size",
                            {{"expected", length}, {"actual", static_cast<int>(fileSize)}});
        return ERR_SIZE_MISMATCH;
    } else {
        log_d("Downloaded file is correct size: %zu bytes", fileSize);
    }

    return httpCode;
}
