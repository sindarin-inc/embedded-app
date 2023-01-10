#pragma once

#include <HTTPClient.h>

#include "Storage/FileSystem.hpp"

auto DownloadFile(const char *url, int length,
                  const std::function<size_t(const uint8_t *buf, size_t size)> &writeBytes,
                  const std::function<void(int bytesWritten, float percentage)> &progressCallback)
    -> bool;

auto DownloadFile(const char *url, FileSystem &fileSystem, const char *path, int length,
                  const std::function<void(int bytesWritten, float percentage)> &progressCallback)
    -> bool;
