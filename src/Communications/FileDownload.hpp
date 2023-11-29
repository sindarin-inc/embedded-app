#pragma once

#include <functional>

#include "Communications/HttpClient.hpp"
#include "Misc/Telemetry.hpp"
#include "Storage/FileSystem.hpp"

auto DownloadFile(
    const char *url, const std::function<bool(int httpCode, int contentLength)> &openedConnection,
    const std::function<size_t(HttpClient &client, int index, int contentLength)> &readWrite,
    std::shared_ptr<bool> cancel,
    const std::function<void(int bytesWritten, float percentage)> &progressCallback, bool gzip)
    -> int;

auto DownloadFile(const char *url,
                  const std::function<bool(int httpCode, int contentLength)> &openedConnection,
                  const std::function<size_t(const uint8_t *buf, size_t size)> &writeBytes,
                  std::shared_ptr<bool> cancel,
                  const std::function<void(int bytesWritten, float percentage)> &progressCallback,
                  bool gzip) -> int;

auto DownloadFile(const char *url, FileSystem &fileSystem, Telemetry &telemetry, const char *path,
                  int length, std::shared_ptr<bool> cancel,
                  const std::function<void(float percentage)> &progressCallback, bool gzip) -> int;
