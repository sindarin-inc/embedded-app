#pragma once

#include <HTTPClient.h>

#include "Storage/FileSystem.hpp"

auto DownloadFile(HTTPClient &httpClient, FileSystem &fileSystem, const char *path, int length,
                  std::function<size_t(const uint8_t *buf, size_t size)> writeBytes,
                  std::function<void(int bytesWritten, float percentage)> progressCallback) -> bool;

auto DownloadFile(HTTPClient &httpClient, FileSystem &fileSystem, const char *path, int length,
                  std::function<void(int bytesWritten, float percentage)> progressCallback) -> bool;
