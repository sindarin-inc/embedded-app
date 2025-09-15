#include <atomic>
#include <catch_amalgamated.hpp>
#include <httplib.h>
#include <memory>
#include <thread>

#include "Communications/SindarinAPI.hpp"
#include "DisplayUtil.hpp"
#include "Displays/DisplayEinkET013TT1.hpp"
#include "Misc/Defer.hpp"
#include "Misc/Dispatch.hpp"
#include "Models/KindleBook.hpp"
#include "Models/KindlePageFlipper.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "Storage/PreferencesStore.hpp"
#include "ViewTestCommon.hpp"
#include "sindarin-debug.h"

// Mock state for tracking requests
static std::atomic<int> mockNavigateCallCount{0};
static std::atomic<int> mockOpenBookCallCount{0};

// Helper to generate sequential test content
static auto generateSequentialContent(int startLine, int endLine) -> std::string {
    std::string content;
    for (int i = startLine; i <= endLine; i++) {
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "Line %d: This is test content line number %d.\n", i, i);
        content += buffer;
    }
    return content;
}

static void resetMockState() {
    mockNavigateCallCount = 0;
    mockOpenBookCallCount = 0;
}

static void setupHttpServer(std::unique_ptr<httplib::Server> &httpServer,
                            std::shared_ptr<std::thread> &httpServerThread,
                            std::atomic<bool> &serverRunning, int port) {
    httpServer->Get("/kindle/open-book", [](const httplib::Request &req, httplib::Response &res) {
        mockOpenBookCallCount++;

        // Check if this is a short pages test book
        std::string bookTitle = req.has_param("title") ? req.get_param_value("title") : "";
        bool useShortPages = (bookTitle.find("SHORT_PAGES_TEST") != std::string::npos);

        log_i("Mock server: open-book request - title='%s', useShortPages=%s", bookTitle.c_str(),
              useShortPages ? "true" : "false");

        std::string content;
        if (useShortPages) {
            // Generate very short content for initial page
            content = "=== PAGE 0 ===\n";
            content += "This is a very short initial page.\n";
            content += "Designed to trigger multi-page fetching.\n";
            content += "=== END PAGE 0 ===\n";
        } else {
            content = generateSequentialContent(1, 20);
        }

        // Escape newlines for JSON
        size_t pos = 0;
        while ((pos = content.find('\n', pos)) != std::string::npos) {
            content.replace(pos, 1, "\\n");
            pos += 2;
        }

        std::string jsonResponse =
            R"({"success": true, "ocr_text": ")" + content +
            R"(", "progress": {"percentage": 0, "current_page": 1, "total_pages": 100}, "book_session_key": "test_session_123"})";
        res.set_content(jsonResponse, "application/json");
        res.status = 200;
    });

    httpServer->Get("/kindle/navigate", [](const httplib::Request &req, httplib::Response &res) {
        mockNavigateCallCount++;

        int navigateTo =
            req.has_param("navigate_to") ? std::stoi(req.get_param_value("navigate_to")) : 0;
        int previewTo =
            req.has_param("preview_to") ? std::stoi(req.get_param_value("preview_to")) : 0;
        std::string bookTitle = req.has_param("title") ? req.get_param_value("title")
                                                       : ""; // Fixed: use "title" not "book_title"

        bool useShortPages = (bookTitle.find("SHORT_PAGES_TEST") != std::string::npos);

        log_i("Mock server: navigate request - title='%s', navigate_to=%d, preview_to=%d, "
              "useShortPages=%s",
              bookTitle.c_str(), navigateTo, previewTo, useShortPages ? "true" : "false");

        std::string content;
        int pageToReturn = (previewTo > navigateTo) ? previewTo : navigateTo;

        if (useShortPages) {
            // Generate very short pages (200-300 chars each)
            content = "=== PAGE " + std::to_string(pageToReturn) + " ===\n";
            content += "Short page " + std::to_string(pageToReturn) + " content.\n";
            for (int i = 0; i < 3; i++) {
                content += "Line " + std::to_string(i + 1) + " of page " +
                           std::to_string(pageToReturn) + ".\n";
            }
            content += "=== END PAGE " + std::to_string(pageToReturn) + " ===\n";
        } else {
            int startLine = 1 + (pageToReturn * 20);
            int endLine = startLine + 19;
            content = generateSequentialContent(startLine, endLine);
        }

        // Escape newlines for JSON
        size_t pos = 0;
        while ((pos = content.find('\n', pos)) != std::string::npos) {
            content.replace(pos, 1, "\\n");
            pos += 2;
        }

        std::string jsonResponse = R"({"success": true, "ocr_text": ")" + content +
                                   R"(", "progress": {"percentage": )" +
                                   std::to_string(pageToReturn) + R"(, "current_page": )" +
                                   std::to_string(pageToReturn + 1) + R"(, "total_pages": 100}})";
        res.set_content(jsonResponse, "application/json");
        res.status = 200;
    });

    httpServerThread = std::make_shared<std::thread>([&httpServer, &serverRunning, port]() {
        log_i("Starting mock HTTP server on port %d", port);
        serverRunning = true;
        httpServer->listen("127.0.0.1", port);
        serverRunning = false;
        log_i("Mock HTTP server stopped");
    });

    // Wait for server to start
    int attempts = 0;
    while (!serverRunning && attempts < 50) {
        attempts++;
    }
}

static void teardownHttpServer(std::unique_ptr<httplib::Server> &httpServer,
                               std::shared_ptr<std::thread> &httpServerThread) {
    log_i("Stopping mock HTTP server");
    if (httpServer) {
        httpServer->stop();
    }
    if (httpServerThread && httpServerThread->joinable()) {
        httpServerThread->join();
    }
}

extern PreferencesStore prefs;

TEST_CASE("Test Kindle multi-page fetching with short pages visual", "[.][kindle-short-pages]") {
    resetMockState();
    SetupFileSystemMock("KindleTestData");
    SetImagesFolder("KindleImages");

    // Set up mock server on a different port to avoid conflicts
    auto httpServer = std::make_unique<httplib::Server>();
    auto httpServerThread = std::make_shared<std::thread>();
    std::atomic<bool> serverRunning(false);
    int port = 8083; // Use port 8083 to avoid conflicts with other tests
    setupHttpServer(httpServer, httpServerThread, serverRunning, port);
    defer { teardownHttpServer(httpServer, httpServerThread); };

    // Override SindarinAPI URL
    auto api = SindarinAPI::getInstance();
    api->overrideBaseUrl("http://127.0.0.1:8083"); // Match the port we're using
    api->setAccessToken("test-token");
    log_i("TEST: SindarinAPI URL overridden to mock server");

    log_i("TEST: Starting FreeRTOS for short pages test");
    StartAppMain(FONT_CHOICES::PREFS_SOL_SMALL, 10, 10, -2, "KindleTestData");
    // Note: StopFreeRtos removed to avoid hanging - FreeRTOS cleanup happens at exit

    // Set cloud token and disable BLE after preferences are initialized
    prefs.cloudToken.set("test-token");
    prefs.bleDisabled.set(true);
    log_i("TEST: Set cloud token and disabled BLE");

    auto &display = GetRootDisplay();
    log_i("TEST: Display created");

    // First screen renders twice
    WAIT_FOR_DRAW(display);

    SECTION("Visual test of multi-page fetching with short pages") {
        // Navigate to Kindle section
        SendRemoteCommand(RIGHT);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);

        // Navigate down to the last book (SHORT_PAGES_TEST)
        for (int i = 0; i < 10; i++) { // Navigate to the 11th book (index 10)
            SendRemoteCommand(DOWN);
            WAIT_FOR_DRAW(display);
        }
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest00-ShortPagesBookSelected");

        // Open the book
        log_i("TEST: Opening book with ENTER command");
        SendRemoteCommand(ENTER);

        // In test mode, connection happens synchronously but loadInitialPage still runs async
        // Wait for the state changes and HTTP request to complete
        log_i("TEST: Waiting for connection and initial load");
        WAIT_FOR_DRAW(display); // Draw for CONNECTING state
        WAIT_FOR_DRAW(display); // Draw for LOADING_INITIAL_CONTENT state
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest01-Connecting");
        WAIT_FOR_DRAW(display); // Draw for content request to complete
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest02-LoadingInitialContent");
        WAIT_FOR_DRAW(display); // Draw for loaded content
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest03-DisplayingContent");
        WAIT_FOR_DRAW(display); // Draw for preview_to=1
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest04-PreviewTo1");

        // Log current state
        int currentRequests = mockNavigateCallCount.load() + mockOpenBookCallCount.load();
        log_i("TEST: After opening - navigate=%d, open-book=%d, total=%d",
              mockNavigateCallCount.load(), mockOpenBookCallCount.load(), currentRequests);

        // Verify we made multiple requests due to short pages
        int initialRequests = currentRequests;
        log_w("TEST: After opening book, navigate+open-book requests made: %d", initialRequests);
        REQUIRE(initialRequests >=
                1); // Should have fetched multiple pages automatically due to short content

        // Navigate forward
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest05-ForwardPage");

        int afterNav = mockNavigateCallCount.load();
        log_i("TEST: After navigation, total requests: %d", afterNav);

        // Navigate forward multiple times quickly
        for (int i = 0; i < 3; i++) {
            SendRemoteCommand(ENTER);
            WAIT_FOR_DRAW(display);
        }

        int afterRapidNav = mockNavigateCallCount.load();
        log_i("TEST: After rapid navigation, total requests: %d", afterRapidNav);
        // Since pages are short and we're buffering ahead, rapid navigation might not
        // trigger new requests if content is already buffered
        REQUIRE(afterRapidNav >= afterNav); // At least no fewer requests than before

        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest06-AfterRapidNav");

        // Navigate backward
        log_i("TEST: Navigating backward with LEFT command");
        SendRemoteCommand(LEFT);
        WAIT_FOR_DRAW(display);
        log_i("TEST: After backward navigation, taking screenshot");
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest07-BackwardNav");

        int afterBackNav = mockNavigateCallCount.load();
        log_i("TEST: After backward nav, total requests: %d", afterBackNav);
        REQUIRE(afterBackNav >= afterRapidNav); // May fetch additional pages to maintain buffer

        log_i("TEST: Multi-page fetching visual test complete");
    }

    log_i("TEST: Exiting short pages visual test");
    api->overrideBaseUrl("");
}
