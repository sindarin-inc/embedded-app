#include <Defer/Defer.hpp>
#include <catch_amalgamated.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <httplib.h>
#include <streambuf>
#include <thread>

#include "Communications/WiFi.hpp"
#include "Misc/Battery.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "Storage/SolDrm.hpp"
#include "ViewTestCommon.hpp"

std::string loadMockResponse(const std::string &filepath) {
    std::ifstream file(filepath);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

auto setupTestServer()
    -> std::pair<std::shared_ptr<httplib::Server>, std::shared_ptr<std::thread>> {
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

    server->Get("/epub/alchemyst.epub", [](const httplib::Request &, httplib::Response &res) {
        log_i("Mock server got request for /epub/alchemyst.epub");
        auto content = loadMockResponse("./test/CatchTests/Fixtures/epub/alchemyst.epub");
        res.set_header("Content-Length", std::to_string(content.length()));
        res.set_content(content, "application/epub+zip");
    });

    server->Post("/sync/articles/", [](const httplib::Request &, httplib::Response &res) {
        log_i("Mock server got request for /sync/articles");
        res.set_content(loadMockResponse("./test/CatchTests/Fixtures/sync-articles-empty.json"),
                        "application/json");
    });

    auto ret = server->bind_to_port("localhost", 55011);
    if (!ret) {
        throw std::runtime_error("Failed to bind to port");
    }

    auto serverThread = std::make_shared<std::thread>([server]() {
        log_i("Mock server starting");
        server->listen_after_bind();
        log_i("Mock server shut down");
    });

    return {server, serverThread};
}

TEST_CASE("Run a basic content sync", "[.][content-sync]") {
    // Create a temporary directory for the content
    std::filesystem::path tmpPath = std::filesystem::temp_directory_path();
    std::string tmpTemplate = tmpPath.string() + "/tempdirXXXXXX";
    // Ensure the string is modifiable
    std::vector<char> tmpTemplateCStr(tmpTemplate.begin(), tmpTemplate.end());
    tmpTemplateCStr.push_back('\0');
    char *tmpDir = mkdtemp(tmpTemplateCStr.data());
    REQUIRE(tmpDir != nullptr);
    log_d("Created tmpdir: %s", tmpDir);

    PreferencesStore prefs;
    prefs.begin(StringFormat("%s/prefs.ini", tmpDir).c_str());
    prefs.cloudToken.set("test-token");
    PosixFileSystem fs(tmpDir);
    Telemetry telemetry(fs, prefs);
    Battery battery(fs, prefs, telemetry);
    // Need to have at least one battery measurement
    // TODO: Maybe we could have Battery fail more gracefully when there are no battery measurements
    battery.loop();
    auto api = SindarinAPI::getInstance();
    api->overrideBaseUrl("http://localhost:55011");
    SolDrm solDrm(prefs);

    LocalStore localStore(fs, prefs, battery, telemetry, solDrm);

    ContentSyncState lastState;
    auto cancel = std::make_shared<CancelFlag>();

    DispatchQueue::dispatchOneOff(
        [&lastState, &localStore, cancel]() {
        // This needs to run in a task to have things like Delay work
        WiFi::getInstance().connectWiFiFromSettings();
        localStore.syncFromRemote(true, false, SyncReason::Manual, nullptr,
                                  [&lastState](ContentSyncState state) {
            log_i("Test sync state: %d %s", state.state, state.usedBle ? "(used BLE)" : "");
            lastState = state;
        });
        },
        8192, 1, 1, "contentsync");

    StartFreeRtos();
    std::shared_ptr<httplib::Server> server;
    std::shared_ptr<std::thread> serverThread;
    std::tie(server, serverThread) = setupTestServer();
    defer {
        StopFreeRtos();
        server->stop();
        log_d("wait for server to exit");
        serverThread->join();
    };

    // Wait until content sync is done but with a 30s timeout
    auto start = std::chrono::steady_clock::now();
    bool done = false;
    while (!done && (std::chrono::steady_clock::now() - start < std::chrono::seconds(30))) {
        switch (lastState.state) {
        case ContentSyncState::Idle:
        case ContentSyncState::ConnectingWifi:
        case ContentSyncState::UploadTelemetry:
        case ContentSyncState::CheckingSync:
        case ContentSyncState::SyncingBooks:
        case ContentSyncState::SyncingArticles:
            break;
        case ContentSyncState::Synced:
        case ContentSyncState::AlreadySynced:
        case ContentSyncState::Failure:
            done = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    REQUIRE(lastState.state == ContentSyncState::Synced);
    REQUIRE(prefs.readablePlace.get("66d82199") ==
            "[EPUB;offset:433,href:OEBPS/xhtml/Scot_9780375843174_epub3_adc_r1.xhtml]");
}
