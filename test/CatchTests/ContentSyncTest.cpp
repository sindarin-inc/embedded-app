#include <catch_amalgamated.hpp>
#include <filesystem>
#include <fstream>
#include <httplib.h>
#include <streambuf>

#include "Misc/Battery.hpp"
#include "Misc/Defer.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "ViewTestCommon.hpp"

std::string loadMockResponse(const std::string &filepath) {
    std::ifstream file(filepath);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

auto setupTestServer() -> std::shared_ptr<httplib::Server> {
    auto server = std::make_shared<httplib::Server>();

    server->Post("/telemetry/", [](const httplib::Request &, httplib::Response &res) {
        log_i("Mock server got request for telemetry");
        res.status = 201;
        res.set_content("{}", "application/json");
    });

    auto booksCount = std::make_shared<int>(0);
    server->Post("/sync/books/", [booksCount](const httplib::Request &, httplib::Response &res) {
        log_i("Mock server got request %d for /sync/books", *booksCount);
        res.set_content(loadMockResponse(StringFormat(
                            "./test/CatchTests/Fixtures/sync-books-%d.json", *booksCount)),
                        "application/json");
        (*booksCount)++;
    });

    // TODO: Mock out file downloading

    server->Post("/sync/articles/", [](const httplib::Request &, httplib::Response &res) {
        log_i("Mock server got request for /sync/articles");
        res.set_content(loadMockResponse("./test/CatchTests/Fixtures/sync-articles-empty.json"),
                        "application/json");
    });

    auto ret = server->bind_to_port("localhost", 55011);
    if (!ret) {
        throw std::runtime_error("Failed to bind to port");
    }

    // Raw std::thread doesn't play nicely with the FreeRTOS posix port so we have to use FreeRTOS
    // tasks
    DispatchQueue::dispatchOneOff([server]() {
        log_i("Mock server starting");
        server->listen_after_bind();
        log_i("Mock server shut down");
    });

    return server;
}

TEST_CASE("Run a basic content sync", "[.][content-sync]") {
    // Create a temporary directory for the content
    // TODO: This is a general temp dir, we should create a subdir we can delete
    std::filesystem::path tmpDir = std::filesystem::temp_directory_path();
    std::filesystem::create_directories(tmpDir);
    // defer { std::filesystem::remove_all(tmpDir); };

    // TODO: To test migrations, we should create a readables.json with a txt.gz readable
    // Then content sync
    // And make sure there's now an epub for the readable

    // for placemarks, we want to test three cases:
    // * If there is a txt file the txt place should be used
    // * If there is an epub file the epub place shoudl be used
    // * If the place is 0/null we shouldn't use it at all

    // Also probably good to write a local store migration test that tests the placemark format
    // migration

    PreferencesStore prefs;
    prefs.begin(StringFormat("%s/prefs.ini", tmpDir.string().c_str()).c_str());
    prefs.cloudToken.set("test-token");
    PosixFileSystem fs(tmpDir.string());
    Telemetry telemetry(fs, prefs);
    Battery battery(fs, prefs, telemetry);
    // Need to have at least one battery measurement
    // TODO: Maybe we could have Battery fail more gracefully when there are no battery measurements
    battery.loop();
    auto api = SindarinAPI::getInstance();
    api->overrideBaseUrl("http://localhost:55011");

    LocalStore localStore(fs, prefs, battery, telemetry);

    auto server = setupTestServer();
    StartFreeRtos();
    defer {
        server->stop();
        StopFreeRtos();
    };

    // TODO: Do we need to wait until FreeRTOS is running?

    ContentSyncState lastState;
    localStore.syncFromRemote(SyncReason::Manual, [&lastState](ContentSyncState state) {
        log_i("Test sync state: %d", state.state);
        lastState = state;
    });

    // DispatchQueue::dispatchOneOff([&lastState, &localStore]() {
    //     // This needs to run in a task to have things like Delay work
    //     localStore.syncFromRemote(SyncReason::Manual, [&lastState](ContentSyncState state) {
    //         log_i("Test sync state: %d", state.state);
    //         lastState = state;
    //     });
    // });

    REQUIRE(lastState.state == ContentSyncState::Synced);
    REQUIRE(prefs.readablePlace.get("66d82199") ==
            "[EPUB;offset:433,href:OEBPS/xhtml/Scot_9780375843174_epub3_adc_r1.xhtml]");
}
