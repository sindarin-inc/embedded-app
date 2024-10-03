#include "FileDownload.hpp"

#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/ringbuf.h>
#include <utility>

#if CONFIG_USE_BLE_SERVER_PROXY
#include "Communications/BLEHttpClient.hpp"
#endif
#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFiHttpClient.hpp"
#include "Misc/Defer.hpp"
#include "Misc/GUnzipStream.hpp"
#include "Misc/RateLimiter.hpp"
#include "Misc/SpiramAllocator.hpp"
#include "sindarin-debug.h"

// Saves us a little RAM at the expense of some download performance
// NOTE: Running the disk write on a separate core from BLE seems to potentially trigger the
// btdm_controller_task WDT issues. Test this thoroughly with BLE sync if enabling.
#define CONFIG_FILEDOWNLOAD_DISK_WRITE_NO_TASK 0
#define CONFIG_FILEDOWNLOAD_DOWNLOAD_NO_TASK 1

#if CONFIG_FILEDOWNLOAD_DOWNLOAD_NO_TASK && CONFIG_FILEDOWNLOAD_DISK_WRITE_NO_TASK
#error "Both download and disk write tasks are disabled, this is not supported"
#endif

auto CreateClient(const std::string &url, bool useBle, const std::shared_ptr<CancelFlag> &cancel)
    -> std::shared_ptr<HttpClient> {
    return SindarinAPI::createClient(url, "", HTTP_METHOD_GET, useBle, cancel);
}

auto DownloadFileCommon(
    const char *url, const std::function<bool(int httpCode, int contentLength)> &openedConnection,
    const std::function<size_t(std::shared_ptr<HttpClient> client, int index, int contentLength)>
        &readWrite,
    const std::shared_ptr<CancelFlag> &cancel, bool useBle, int expectedSize,
    const std::function<void(int bytesWritten, float percentage)> &progressCallback, bool gzip)
    -> int {
    if (CancelFlag::isCancelled(cancel)) {
        return ERR_CANCELLED;
    }
    if (useBle) {
        // TODO: Support gzip over BLE
        gzip = false;
    }
    auto client = CreateClient(url, useBle, cancel);

    auto sindarinApi = SindarinAPI::getInstance();
    if (sindarinApi->isSindarinUrl(url)) {
        sindarinApi->setAuth(client);
    }

    auto time = esp_timer_get_time();
    auto downloadStart = time;

    if (gzip) {
        auto err = client->setHeader("Accept-Encoding", "gzip");
        if (err != ESP_OK) {
            return ERR_SET_HEADER;
        }
    }

    esp_err_t err = client->open();
    if (err != ESP_OK) {
        return ERR_CLIENT_OPEN;
    }
    defer { client->close(); };

    auto contentLength = client->fetchHeaders(expectedSize, cancel);

    auto httpCode = client->getStatusCode();
    if (httpCode != 200) {
        if (httpCode != 0) {
            log_w("Status code was %d", httpCode);
            return httpCode;
        } else {
            log_w("Status code was 0, assuming connection error");
            return ERR_CLIENT_DISCONNECTED;
        }
    }

    if (gzip) {
        char *contentEncoding = nullptr;
        err = client->getResponseHeader("Content-Encoding", &contentEncoding);
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
        if (client->isChunkedResponse() &&
            client->getResponseHeader("File-Size", &value) == ESP_OK) {
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
        auto read = client->read(buffer, sizeof(buffer));
        if (read > 0) {
            log_w("Error body: %s", buffer);
        } else {
            log_d("No error body: %s", buffer);
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
            log_w("readWrite failed (readLen == %d, totalReadLen: %d, contentLength: %d, connected "
                  "%d)",
                  readLen, totalReadLen, contentLength, client->connected);
            if (!client->connected) {
                log_w("Connection closed, bailing out");
                return ERR_CLIENT_DISCONNECTED;
            } else {
                log_w("readWrite failed, bailing out");
                return ERR_READ_EMPTY;
            }
        }

        totalReadLen += readLen;

        float percentage = static_cast<float>(totalReadLen) / contentLength * 100.0f;
        rl.checkAndExecute(percentage, [totalReadLen, downloadStart,
                                        progressCallback](int64_t time, float percentage) {
            log_d("Downloaded %.1f%% (%.1f KB/s, %d bytes)", percentage,
                  totalReadLen / 1024.0f / ((time - downloadStart) / 1000000.0f), totalReadLen);
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

auto DownloadFileWithWriteBytes(
    const char *url, const std::function<bool(int httpCode, int contentLength)> &openedConnection,
    const std::function<size_t(const uint8_t *buf, size_t size)> &writeBytes,
    const std::shared_ptr<CancelFlag> &cancel, bool useBle, int expectedSize,
    const std::function<void(int bytesWritten, float percentage)> &progressCallback, bool gzip)
    -> int {

    constexpr int bufferSize = 4096;
    auto buffer = SpiRamArrayPtr<uint8_t>(bufferSize);

    return DownloadFileCommon(
        url, openedConnection,
        [buffer, bufferSize, writeBytes](const std::shared_ptr<HttpClient> &client, int index,
                                         int contentLength) {
        int readLen = client->read(reinterpret_cast<char *>(buffer.get()),
                                   std::min(bufferSize, contentLength - index));

        auto written = writeBytes(buffer.get(), static_cast<size_t>(readLen));
        if (written != readLen) {
            log_w("Failed to write to file, wrote %zu but expected %d", written, readLen);
        }

        return readLen;
        },
        cancel, useBle, expectedSize, progressCallback, gzip);
}

auto DownloadFileToPath(const char *url, FileSystem &fileSystem, Telemetry &telemetry,
                        const char *path, int length, const std::shared_ptr<CancelFlag> &cancel,
                        bool useBle, const std::function<void(float percentage)> &progressCallback,
                        bool gzip) -> int {

    std::string fullPath(path);

    if (useBle) {
        // TODO: Support gzip over BLE
        gzip = false;
    }

    SHOW_HEAP();
    // TODO: Support resuming?
    if (fileSystem.exists(fullPath)) {
        log_w("File %s already exists, deleting", path);
        fileSystem.remove(path);
        log_d("Deleted %s", path);
    }

    // Disk writes are relatively slow (~40KB/s) while network downloads may be up to 10x faster. To
    // minimize the chances of a network hiccup breaking the download, we want to download as fast
    // as possible to SPIRAM but we still want to be able to download large files that _may_ not
    // fully fit in SPIRAM. We accomplish this by running the disk writing in a separate task
    // (thread) and passing data between the downloader and the file writer using a ring buffer.

    struct DataBlock {
        uint8_t data[4096];
        size_t size;
    };

    // Create a ~4MB ring buffer for data transfer between download and write tasks. Round to the
    // nearest sizeof(DataBlock)
    constexpr size_t maxRingbufSize = 4000000 / sizeof(DataBlock) * sizeof(DataBlock);
    auto ringbufSize =
        std::min(maxRingbufSize, heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) - 10000);

    log_d("Creating ring buffer of size %zu", ringbufSize);

    RingbufHandle_t ringbuf = xRingbufferCreate(ringbufSize, RINGBUF_TYPE_NOSPLIT);
    if (ringbuf == nullptr) {
        log_e("xRingbufferCreate failed");
        return ERR_ALLOCATION;
    }
    defer { vRingbufferDelete(ringbuf); };

    // Open the file for writing
    auto file = fileSystem.open(fullPath, FILE_WRITE, true);
    if (!file) {
        log_e("Failed to open file %s for writing", fullPath.c_str());
        return ERR_FILE_OPEN;
    }
    defer { file.close(); };

    int fileLength = 0;
    auto openedConnection = [&fileLength](int httpCode, int contentLength) -> bool {
        fileLength = contentLength;
        // TODO: Bail out early if we don't have enough disk space to do this download
        return true;
    };

    auto reader = [ringbuf](const std::shared_ptr<HttpClient> &client, int index,
                            int contentLength) -> size_t {
        DataBlock *block = nullptr;
        auto err = xRingbufferSendAcquire(ringbuf, reinterpret_cast<void **>(&block),
                                          sizeof(DataBlock), portMAX_DELAY);
        if (err != pdTRUE || block == nullptr) {
            log_e("Failed to acquire ring buffer slot");
            return 0;
        }

        int bytesRead = client->read(reinterpret_cast<char *>(block->data), sizeof(block->data));
        if (bytesRead <= 0) {
            log_w("client.read returned %d bytes", bytesRead);
            return 0;
        }

        block->size = bytesRead;

        err = xRingbufferSendComplete(ringbuf, block);
        if (err != pdTRUE) {
            log_e("Failed to complete ring buffer send");
            return 0;
        }
        return bytesRead;
    };

    auto downloadStart = esp_timer_get_time();

    RateLimiter rl(5, 20, 10);
    auto writer = [&file, ringbuf, progressCallback, downloadStart, &fileLength, &rl]() -> void {
        int written = 0;

        while (true) {
            size_t blockSize = 0;
            auto *block = reinterpret_cast<DataBlock *>(
                xRingbufferReceive(ringbuf, &blockSize, portMAX_DELAY));

            if (block == nullptr || blockSize == 0) {
                log_e("xRingbufferReceive failed");
                break;
            }

            // This is the signal for EOF
            if (block->size == 0) {
                progressCallback(100);
                vRingbufferReturnItem(ringbuf, block);
                break;
            }

            auto bytesWritten = file.write(block->data, block->size);

            if (bytesWritten != block->size) {
                log_e("Failed to write %zu bytes, wrote %zu", block->size, bytesWritten);
                vRingbufferReturnItem(ringbuf, block);
                break;
            }

            vRingbufferReturnItem(ringbuf, block);

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

    // Spin up the downloader task
    int httpCode;
    auto downloader =
        [url, openedConnection, reader, gzip, &httpCode, ringbuf, cancel, useBle, length]() {
        httpCode = DownloadFileCommon(url, openedConnection, reader, cancel, useBle, length,
                                      nullptr, gzip);
        DataBlock *block;
        auto err = xRingbufferSendAcquire(ringbuf, reinterpret_cast<void **>(&block),
                                          sizeof(DataBlock), portMAX_DELAY);
        if (err != pdTRUE) {
            log_e("xRingbufferSendAcquire error");
            return;
        }
        block->size = 0;
        err = xRingbufferSendComplete(ringbuf, block);
        if (err != pdTRUE) {
            log_e("xRingbufferSendComplete error");
            return;
        }
    };

#if !CONFIG_FILEDOWNLOAD_DISK_WRITE_NO_TASK
    // Having both the HTTP and the disk write tasks on core 1 seems to be the fastest
    // configuration, presumably since it allows the WiFi driver to have free reign over core 0
    SemaphoreHandle_t completeSemaphore = xSemaphoreCreateBinary();
    defer { vSemaphoreDelete(completeSemaphore); };
    auto writeDispatched = DispatchQueue::dispatchOneOff(
        [completeSemaphore, &writer]() {
        writer();
        xSemaphoreGive(completeSemaphore);
        },
        4000, 2, 1, "FWrite");
    if (!writeDispatched) {
        return ERR_ALLOCATION;
    }
#endif

#if CONFIG_FILEDOWNLOAD_DOWNLOAD_NO_TASK
    auto priority = uxTaskPriorityGet(nullptr);
    vTaskPrioritySet(nullptr, 3);
    downloader();
    vTaskPrioritySet(nullptr, priority);
    SHOW_WATERMARK();
#else
    // Note: 8192 is the size that all the esp-idf examples use for esp_http_client. It's probably
    // overkill but after getting stack overflows on 1.0.0-1.0.3 it's probably better to be safe
    auto downloadDispatched = DispatchQueue::dispatchOneOff(downloader, 8192, 3, 1, "FDownload");
    if (!downloadDispatched) {
        return ERR_ALLOCATION;
    }
#endif

#if CONFIG_FILEDOWNLOAD_DISK_WRITE_NO_TASK
    // This is ~1-5KB/s slower but saves us the RAM overhead of creating a new task
    auto priority = uxTaskPriorityGet(nullptr);
    vTaskPrioritySet(nullptr, 2);
    writer();
    vTaskPrioritySet(nullptr, priority);
    SHOW_WATERMARK();
#else
    // Wait for the writer task to finish
    xSemaphoreTake(completeSemaphore, portMAX_DELAY);
#endif

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
        log_e("Downloaded file is shorter than expected, expected %d bytes but got %zu bytes (%s)",
              length, fileSize, gzip ? "uncompressed" : "compressed");
        telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "wrong-size",
                            {{"expected", length}, {"actual", static_cast<int>(fileSize)}});
        return ERR_SIZE_MISMATCH;
    }

    return httpCode;
}
