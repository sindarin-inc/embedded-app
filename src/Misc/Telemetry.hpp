#pragma once

#include <string>

#include "Misc/StringUtil.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"

class Telemetry {
public:
    Telemetry(FileSystem &fs, PreferencesStore &prefs) : fileSystem_(fs), prefs_(prefs) {}
    Telemetry(const Telemetry &) = delete;

    void logMetric(const std::string &name, float value);

    static auto metricLogPath(const std::string &name) -> const char * {
        return StringFormat("/logs/%s.txt", name.c_str()).c_str();
    }

protected:
    FileSystem &fileSystem_;
    PreferencesStore &prefs_;
    unsigned long lastLoggingTime_ = 0;
};
