#pragma once

#include "Storage/FileSystem.hpp"

class CrashReportWriter {
public:
    CrashReportWriter(FileSystem &fileSystem, int bootNum)
        : fileSystem_(fileSystem), bootNum_(bootNum) {}
    ~CrashReportWriter() {}

    bool writeCrashReportIfExists();

private:
    FileSystem &fileSystem_;
    int bootNum_;
};