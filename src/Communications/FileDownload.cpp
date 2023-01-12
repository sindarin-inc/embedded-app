#include "FileDownload.hpp"

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <utility>

#include "sindarin-debug.h"
static const char *tag = "FileDownload";

// TODO: Headers
auto DownloadFile(const char *url, int length,
                  const std::function<size_t(const uint8_t *buf, size_t size)> &writeBytes,
                  const std::function<void(int bytesWritten, float percentage)> &progressCallback)
    -> bool {
    // W25Q512JV* chips have a page size of 256 bytes so that seems like a good value to use
    // here.
    constexpr int bufferSize = 4096;
    char buff[bufferSize];

    esp_http_client_config_t config = {
        // .url = "http://httpbin.org/get",
        // .url =
        // "http://api.sindarin.com/proxy/?url=https%3A%2F%2Fnyc3.digitaloceanspaces.com%2Fsol-cloud%2Fuploads%2Fold-testament.txt%3FX-Amz-Algorithm%3DAWS4-HMAC-SHA256%26X-Amz-Credential%3DDO00Q9MT7TLXHYDWUFQW%2F20230110%2Fnyc3%2Fs3%2Faws4_request%26X-Amz-Date%3D20230110T002724Z%26X-Amz-Expires%3D3600%26X-Amz-SignedHeaders%3Dhost%26X-Amz-Signature%3D6c1b45842537fcb95a539f13faeec7dd582400cadd5b4cd14030668d60a1d5a7"
        // .url = "https://nyc3.digitaloceanspaces.com/sol-cloud/uploads/"
        //        "old-testament.txt?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential="
        //        "DO00Q9MT7TLXHYDWUFQW%2F20230110%2Fnyc3%2Fs3%2Faws4_request&X-Amz-Date="
        //        "20230110T002724Z&X-Amz-Expires=3600&X-Amz-SignedHeaders=host&X-Amz-Signature="
        //        "6c1b45842537fcb95a539f13faeec7dd582400cadd5b4cd14030668d60a1d5a7",
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach

    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    auto time = esp_timer_get_time();
    auto downloadStart = time;

    esp_err_t err;
    if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
        ESP_LOGE(tag, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        return false;
    }

    int contentLength = esp_http_client_fetch_headers(client);

    auto statusCode = esp_http_client_get_status_code(client);
    if (statusCode != 200) {
        log_w("Status code was %d", statusCode);
    }

    int totalReadLen = 0, readLen;
    int lastPercentage = 0;

    while (totalReadLen < contentLength) {
        readLen = esp_http_client_read(client, buff, bufferSize);
        if (readLen <= 0) {
            ESP_LOGE(tag, "Error read data");
        }

        auto written = writeBytes((uint8_t *)buff, (size_t)readLen);
        if (written != readLen) {
            log_w("Failed to write to file, wrote %zu but expected %d", written, readLen);
            break;
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

    ESP_LOGI(tag, "DownloadFile complete %d in %" PRId64 "us (%f Bps %f KB/s, %f kbps)",
             totalReadLen, duration, bps, bps / 1000, 8 * bps / 1000);

    float percentProgress = (float)totalReadLen / length * 100.0f;
    progressCallback(totalReadLen, percentProgress);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (length != totalReadLen) {
        // TODO: What's the best API to get errors from the stream?
        log_e("Wrote fewer bytes than expected, expected %d bytes but got %d bytes", length,
              totalReadLen);
        return false;
    } else {
        return true;
    }
    // // Read all data from server
    // length = length > 0 ? length : httpClient.getSize();
    // int len = length;
    // int lastPercentage = 0;
    // int bytesWritten = 0;
    // while (stream->connected() && (len > 0 || len == -1)) {
    //     auto available = stream->available();
    //     if (available == 0) {
    //         // TODO: In the future it will be good to use a HTTP library that allows us to do
    //         // some kind of select or block exactly until data is available
    //         // TODO: We're still sometimes getting a watchdog timer issue on Core 0
    //         log_d("Waiting for data...");
    //         delay(1000);
    //         continue;
    //     }

    //     // TODO: Does readBytes ever sleep? I think maybe it doesn't
    //     //
    //     https://github.com/espressif/arduino-esp32/blob/d8f98450fc1f1e1646077260a22287df6332580a/cores/esp32/Stream.cpp#L285
    //     // That's weird for a http client, at least on a high priority task.
    //     int c = stream->readBytes(buff, std::min(available, bufferSize));
    //     // log_d("Reading %d bytes (%d available)", std::min(available, bufferSize), available);

    //     auto written = writeBytes(buff, c);
    //     if (written != c) {
    //         log_w("Failed to write to file, wrote %zu but expected %d", written, c);
    //         break;
    //     }
    //     bytesWritten += written;
    //     if (len > 0) {
    //         len -= c;
    //     }

    //     float percentProgress = (float)bytesWritten / length * 100.0f;
    //     auto now = millis();
    //     // Report every 5-20s OR every 5% progress, whichever comes first
    //     if ((now - time > 5000) && (now - time > 20000 || (percentProgress - lastPercentage) >
    //     5)) {
    //         log_d("Downloaded %.1f%% (%.1f KB/s)", percentProgress,
    //               bytesWritten / 1024.0f / ((millis() - downloadStart) / 1000.0f));
    //         progressCallback(bytesWritten, percentProgress);
    //         time = now;
    //         lastPercentage = percentProgress;
    //     }

    //     vPortYield();
    // }

    // float percentProgress = (float)bytesWritten / length * 100.0f;
    // progressCallback(bytesWritten, percentProgress);
}

bool allocdBuffer = false;
uint8_t *bigBuffer = nullptr;

auto DownloadFile(const char *url, FileSystem &fileSystem, const char *path, int length,
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

    if (!allocdBuffer) {
        bigBuffer = (uint8_t *)malloc(4000000);
        allocdBuffer = true;
    }

    int bigSize = 0;

    DownloadFile(
        url, length,
        [&bigSize](const uint8_t *buf, size_t size) {
            strncpy((char *)(bigBuffer + bigSize), (char *)buf, size);
            bigSize += size;
            return size;
        },
        progressCallback);

    file.write(bigBuffer, bigSize);
    //    free(big_buffer);
    //    big_buffer = NULL;
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
