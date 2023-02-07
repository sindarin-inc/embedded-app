#pragma once

#include "Communications/HttpClient.hpp"
#include "Storage/FileSystem.hpp"

auto DownloadFile(const char *url,
                  const std::function<bool(int httpCode, int contentLength)> &openedConnection,
                  const std::function<size_t(HttpClient &client)> &readWrite,
                  const std::function<void(int bytesWritten, float percentage)> &progressCallback,
                  bool gzip) -> bool;

auto DownloadFile(const char *url,
                  const std::function<bool(int httpCode, int contentLength)> &openedConnection,
                  const std::function<size_t(const uint8_t *buf, size_t size)> &writeBytes,
                  const std::function<void(int bytesWritten, float percentage)> &progressCallback,
                  bool gzip) -> bool;

auto DownloadFile(const char *url, FileSystem &fileSystem, const char *path, int length,
                  const std::function<void(float percentage)> &progressCallback, bool gzip) -> bool;
