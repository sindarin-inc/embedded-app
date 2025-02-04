#pragma once

#include <functional>
#include <string>

#include "Communications/HttpClient.hpp"
#include "Misc/CancelFlag.hpp"
#include "Misc/Telemetry.hpp"
#include "Storage/FileSystem.hpp"

auto DownloadFileCommon(
    const char *url, const std::function<bool(int httpCode, int contentLength)> &openedConnection,
    const std::function<size_t(std::shared_ptr<HttpClient> client, int index, int contentLength)>
        &readWrite,
    const std::shared_ptr<CancelFlag> &cancel, bool useBle,
    const std::function<void(int bytesWritten, float percentage)> &progressCallback, bool gzip,
    const std::string &requestBodyJson = "") -> int;

auto DownloadFileWithWriteBytes(
    const char *url, const std::function<bool(int httpCode, int contentLength)> &openedConnection,
    const std::function<size_t(const uint8_t *buf, size_t size)> &writeBytes,
    const std::shared_ptr<CancelFlag> &cancel, bool useBle,
    const std::function<void(int bytesWritten, float percentage)> &progressCallback, bool gzip)
    -> int;

auto DownloadFileToPath(const char *url, FileSystem &fileSystem, Telemetry &telemetry,
                        const char *path, int expectedSize,
                        const std::shared_ptr<CancelFlag> &cancel, bool useBle,
                        const std::function<void(float percentage)> &progressCallback, bool gzip,
                        const std::string &requestBodyJson = "") -> int;
