#include "FileDownload.hpp"

#include <esp_timer.h>
#include <utility>

#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFiHttpClient.hpp"
#include "Misc/Defer.hpp"
#include "Misc/GUnzipStream.hpp"
#include "Misc/RateLimiter.hpp"
#include "Misc/SpiramAllocator.hpp"
#include "sindarin-debug.h"

auto DownloadFile(
    const char *url, const std::function<bool(int httpCode, int contentLength)> &openedConnection,
    const std::function<size_t(WiFiHttpClient &client, int index, int contentLength)> &readWrite,
    const std::shared_ptr<CancelFlag> &cancel,
    const std::function<void(int bytesWritten, float percentage)> &progressCallback, bool gzip)
    -> int {
    if (CancelFlag::isCancelled(cancel)) {
        return ERR_CANCELLED;
    }

    WiFiHttpClient client(url);

    auto sindarinApi = SindarinAPI::getInstance();
    if (sindarinApi->isSindarinUrl(url)) {
        sindarinApi->setAuth(client);
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

    // Because Cloudflare sometimes likes to chunk our responses for us, we need to check for the
    // File-Size header
    if (contentLength <= 0) {
        char *value;
        if (client.isChunkedResponse() && client.getResponseHeader("File-Size", &value) == ESP_OK) {
            contentLength = strtol(value, nullptr, 10);
            log_d("Chunked transmission of File-Size %d", contentLength);
        }
    }

    if (contentLength <= 0) {
        // if we somehow don't have a content length header, consider it an error for now
        log_e("Server did not return a content length for %s", url);
        return ERR_CONTENT_LENGTH;
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
    RateLimiter rl(5, 20, 10);

    while (totalReadLen < contentLength) {
        if (CancelFlag::isCancelled(cancel)) {
            return ERR_CANCELLED;
        }

        int readLen = readWrite(client, totalReadLen, contentLength);
        if (readLen <= 0) {
            log_w("readWrite failed (readLen == %d)", readLen);
            if (!client.connected) {
                log_w("Connection closed, bailing out");
                return ERR_CLIENT_DISCONNECTED;
            }
        }

        totalReadLen += readLen;

        float percentage = static_cast<float>(totalReadLen) / contentLength * 100.0f;
        rl.checkAndExecute(percentage, [totalReadLen, downloadStart,
                                        progressCallback](int64_t time, float percentage) {
            log_d("Downloaded %.1f%% (%.1f KB/s)", percentage,
                  totalReadLen / 1024.0f / ((time - downloadStart) / 1000000.0f));
            if (progressCallback != nullptr) {
                progressCallback(totalReadLen, percentage);
            }
        });
    }

    int64_t end = esp_timer_get_time();
    int64_t duration = end - downloadStart;
    float bps = totalReadLen / (duration / 1000000.0f);

    log_i("Downloaded %d bytes in %.2fs (%.1f KB/s)", totalReadLen, duration / 1000000.0f,
          bps / 1000);

    float percentProgress = static_cast<float>(totalReadLen) / contentLength * 100.0f;
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
                  const std::shared_ptr<CancelFlag> &cancel,
                  const std::function<void(int bytesWritten, float percentage)> &progressCallback,
                  bool gzip) -> int {

    constexpr int bufferSize = 4096;
    auto buffer = SpiRamArrayPtr<uint8_t>(bufferSize);

    return DownloadFile(
        url, openedConnection,
        [buffer, bufferSize, writeBytes](WiFiHttpClient &client, int index, int contentLength) {
        int readLen = client.read(reinterpret_cast<char *>(buffer.get()),
                                  std::min(bufferSize, contentLength - index));

        auto written = writeBytes(buffer.get(), static_cast<size_t>(readLen));
        if (written != readLen) {
            log_w("Failed to write to file, wrote %zu but expected %d", written, readLen);
        }

        return readLen;
        },
        cancel, progressCallback, gzip);
}

auto DownloadFile(const char *url, FileSystem &fileSystem, Telemetry &telemetry, const char *path,
                  int length, const std::shared_ptr<CancelFlag> &cancel,
                  const std::function<void(float percentage)> &progressCallback, bool gzip) -> int {

    std::string fullPath(path);

    SHOW_HEAP();
    // TODO: Support resuming?
    if (fileSystem.exists(fullPath)) {
        log_w("File %s already exists, deleting", path);
        fileSystem.remove(path);
        log_d("Deleted %s", path);
    }

    // Disk writes are relatively slow (~40KB/s) while network downloads may be 10x faster. To
    // minimize the chances of a network hiccup breaking the download, we want to download as fast
    // as possible to SPIRAM but we still want to be able to download large files that _may_ not
    // fully fit in SPIRAM. We accomplish this by running the disk writing in a separate task
    // (thread) and passing data between the downloader and the file writer using a FreeRTOS queue.

    // 1000 queue slots * 4096 bytes = 4096000 bytes. Any files bigger than this _might_ get their
    // download delayed by disk write performance.
    struct DataBlock {
        uint8_t *data;
        int size;
    };
    constexpr int bufferSize = 4096;
    HEAP_PROFILE_START(FileDownloadBuffer);
    QueueHandle_t bufferQueue = xQueueCreate(1000, sizeof(DataBlock));
    HEAP_PROFILE_END(FileDownloadBuffer);
    if (bufferQueue == nullptr) {
        log_e("xQueueCreate failed");
        return ERR_ALLOCATION;
    }
    defer { vQueueDelete(bufferQueue); };
    SemaphoreHandle_t writeCompleteSemaphore = xSemaphoreCreateBinary();
    defer { vSemaphoreDelete(writeCompleteSemaphore); };

    // Open the file for writing
    auto file = fileSystem.open(fullPath, FILE_WRITE, true);
    if (!file) {
        log_e("Failed to open file %s for writing", fullPath.c_str());
        return ERR_FILE_OPEN;
    }

    int fileLength = 0;
    auto openedConnection = [&fileLength](int httpCode, int contentLength) -> bool {
        fileLength = contentLength;
        // TODO: Bail out early if we don't have enough disk space to do this download
        return true;
    };

    auto reader = [bufferQueue](WiFiHttpClient &client, int index, int contentLength) -> size_t {
        // NOTE: This gets free'd by the queue consumer. It only needs to be free'd here if we fail
        // to write it to the queue
        auto *data = static_cast<uint8_t *>(SpiRamMalloc(bufferSize));
        if (data == nullptr) {
            log_e("SpiRamMalloc(%d) failed", bufferSize);
            return 0;
        }
        int bytesRead = client.read(reinterpret_cast<char *>(data), bufferSize);
        if (bytesRead <= 0) {
            log_w("client.read returned %d bytes", bytesRead);
            SpiRamFree(data);
            return 0;
        }
        DataBlock block = {data, bytesRead};
        if (xQueueSend(bufferQueue, &block, portMAX_DELAY) != pdPASS) {
            log_e("xQueueSend failed");
            SpiRamFree(data);
            return 0;
        }
        return bytesRead;
    };

    auto downloadStart = esp_timer_get_time();

    RateLimiter rl(5, 20, 10);
    auto writer = [&file, bufferQueue, progressCallback, downloadStart, &fileLength,
                   &rl]() -> void {
        int written = 0;

        while (true) {
            DataBlock block{};
            if (xQueueReceive(bufferQueue, &block, portMAX_DELAY) != pdPASS) {
                log_e("xQueueReceive failed");
            }

            // log_d("Writing %d bytes %p", block.size, block.data);

            // This is the signal for EOF
            if (block.data == nullptr) {
                progressCallback(100);
                break;
            }

            auto bytesWritten = file.write(block.data, block.size); // Adjust size as needed
            SpiRamFree(block.data);
            if (bytesWritten != block.size) {
                log_e("Failed to write %d bytes, wrote %zu", block.size, bytesWritten);
                // We'll catch the failure at the filesize check at the end
                break;
            }

            written += bytesWritten;

            float percentage = static_cast<float>(written) / fileLength * 100.0f;
            rl.checkAndExecute(percentage, [written, downloadStart,
                                            progressCallback](int64_t time, float percentage) {
                log_d("Wrote %.1f%% (%.1f KB/s)", percentage,
                      written / 1024.0f / ((time - downloadStart) / 1000000.0f));
                progressCallback(percentage);
            });
        }
    };

    // Spin up the disk writer task and wait for bytes in the queue
    DispatchQueue::dispatchOneOff(
        [writer, writeCompleteSemaphore]() {
        writer();
        xSemaphoreGive(writeCompleteSemaphore);
        },
        4000, 2, 1, "FWrite");

    // Spin up the downloader task
    int httpCode;
    DispatchQueue::dispatchOneOff(
        [url, openedConnection, reader, gzip, &httpCode, bufferQueue, cancel]() {
        httpCode = DownloadFile(url, openedConnection, reader, cancel, nullptr, gzip);
        // Once we get here the download is done, so we can signal the writer task to exit
        DataBlock block = {nullptr, 0};
        if (xQueueSend(bufferQueue, &block, portMAX_DELAY) != pdPASS) {
            log_e("xQueueSend error");
        }
        },
        5000, 3, 1, "FDownload");

    // Wait for disk write to complete
    xSemaphoreTake(writeCompleteSemaphore, portMAX_DELAY);

    file.close();

    auto diskWriteEnd = esp_timer_get_time();

    if (httpCode != 200) {
        log_e("Failed to download file (http: %d)", httpCode);
        return httpCode;
    }

    int64_t diskWriteDuration = diskWriteEnd - downloadStart;
    float diskWriteBps = fileLength / (diskWriteDuration / 1000000.0f);
    log_i("Downloaded+wrote %d bytes in %.2fs (%.1f KB/s)", fileLength,
          diskWriteDuration / 1000000.0f, diskWriteBps / 1000);

    if (CancelFlag::isCancelled(cancel)) {
        return ERR_CANCELLED;
    }

    // Make sure the file is the right size
    size_t fileSize = 0;
    if (gzip) {
        auto file = fileSystem.open(fullPath);
        GUnzipStream gzStream(file);
        fileSize = gzStream.size();
        file.close();
    } else {
        fileSize = fileSystem.size(fullPath);
    }

    if (length != fileSize) {
        // TODO: What's the best API to get errors from the stream?
        log_e("Downloaded file is shorter than expected, expected %d bytes but got %zu bytes",
              length, fileSize);
        telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "wrong-size",
                            {{"expected", length}, {"actual", static_cast<int>(fileSize)}});
        return ERR_SIZE_MISMATCH;
    }

    return httpCode;
}
