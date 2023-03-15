#pragma once

#include "Storage/FileSystem.hpp"

class CrashReportWriter {
public:
    CrashReportWriter(FileSystem &fileSystem, int bootNum)
        : fileSystem_(fileSystem), bootNum_(bootNum) {}
    ~CrashReportWriter() = default;

    auto writeCrashReportIfExists() -> bool;

private:
    FileSystem &fileSystem_;
    int bootNum_;
};