#include <catch_amalgamated.hpp>

// Allow access to protected methods from tests
#define protected public
#define private public
#include "Misc/Defer.hpp"
#include "Misc/Telemetry.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "Storage/PreferencesStore.hpp"
#include "ViewTestCommon.hpp"
#undef protected
#undef private

#include <chrono>
#include <iostream>
#include <semaphore>
#include <signal.h>
#include <unistd.h>

TEST_CASE("Test Telemetry Logging", "[.][telemetry]") {
    SetupFileSystemMock("TelemetryTestData");
    MockClock();
    defer { UnMockClock(); };

    PosixFileSystem fs("./test/CatchTests");
    fs.begin();
    PreferencesStore prefs;
    Telemetry telemetry(fs, prefs);

    std::binary_semaphore semaphore(0);

    // FreeRTOS stuff needs to run in a task
    DispatchQueue::dispatchOneOff(
        [&telemetry, &semaphore]() {
        // Log some test metrics
        telemetry.logMetric(METRIC_BATTERY_VOLTAGE, 3.7f);
        telemetry.logMetric(METRIC_EVENT, "test_event");
        std::string testString = "TestWiFi";
        telemetry.logMetricData(METRIC_WIFI,
                                {{"ssid", testString}, {"status", std::string("connected")}});
        testString = "CHANGED";

        Telemetry::dispatchQueue()->waitEmpty(pdMS_TO_TICKS(100));
        semaphore.release();
        },
        2048, 1, 1, "TestTelem");

    StartFreeRtos();
    defer { StopFreeRtos(); };

    semaphore.try_acquire_for(std::chrono::seconds(2));

    // Read the telemetry file
    auto file = fs.open(Telemetry::metricLogPath(), FILE_READ);
    REQUIRE(file);
    defer {
        file.close();
        fs.remove(Telemetry::metricLogPath());
    };

    std::string fileContents;
    auto line = file.readStringUntil('\n');
    REQUIRE(line == R"({"ts":1610000000,"k":"ba","v":3.70})");
    line = file.readStringUntil('\n');
    REQUIRE(line == R"({"ts":1610000000,"k":"ev","v":"test_event"})");
    line = file.readStringUntil('\n');
    REQUIRE(line == R"({"ts":1610000000,"k":"wf","status":"connected","ssid":"TestWiFi"})");
}
