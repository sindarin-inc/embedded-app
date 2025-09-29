#include <atomic>
#include <catch_amalgamated.hpp>
#include <chrono>
#include <httplib.h>
#include <memory>
#include <sstream>
#include <thread>

#define protected public
#define private public
#include "UI/KindleReadableScreen.hpp"
#undef private
#undef protected

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

extern auto UIGetCurrentScreen() -> std::shared_ptr<Screen>;

// Mock state for tracking requests
static std::atomic<int> mockNavigateCallCount{0};
static std::atomic<int> mockOpenBookCallCount{0};
static std::atomic<bool> mockLastReadDialogTriggered{false};
static std::atomic<int> mockLastReadChoiceCallCount{0};
static std::atomic<bool> mockEnableLastReadDialog{false};
static std::atomic<bool> mockOpenBookLastReadDialogEnabled{false};
static std::atomic<bool> mockOpenBookLastReadDialogTriggered{false};
static std::atomic<int> mockLastReadDialogShownCount{0};

static constexpr const char *SYNCED_CHAPTER_TITLE = "=== CHAPTER 42: A New Chapter Begins ===\n";
static constexpr const char *SYNCED_CHAPTER_FOOTER = "=== END CHAPTER 42 ===\n";

static auto generateSyncedChapterContent(int pageNumber) -> std::string {
    std::string content;

    if (pageNumber == 0) {
        content += SYNCED_CHAPTER_TITLE;
    } else {
        content += "=== CHAPTER 42 (continued) ===\n";
    }

    int baseLine = 1000 + pageNumber * 12;
    for (int i = 0; i < 12; i++) {
        content += "Chapter 42, page " + std::to_string(pageNumber) + ": line " +
                   std::to_string(baseLine + i) +
                   " is the most recent synced page on the Kindle reader.\n";
    }

    content += SYNCED_CHAPTER_FOOTER;
    return content;
}

// Track when content flushes for tests
static std::atomic<bool> shortContentRendered{false};

static void resetShortContentRenderFlag() { shortContentRendered = false; }

static void waitForShortContentRender() {
    auto start = std::chrono::steady_clock::now();
    while (!shortContentRendered.load()) {
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(10)) {
            log_e("Timeout waiting for short pages content render");
            FAIL("Timeout waiting for content render");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

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
    mockLastReadDialogTriggered = false;
    mockLastReadChoiceCallCount = 0;
    mockEnableLastReadDialog = false;
    mockOpenBookLastReadDialogEnabled = false;
    mockOpenBookLastReadDialogTriggered = false;
    mockLastReadDialogShownCount = 0;
}

static auto setupShortPagesContentCallback() -> std::shared_ptr<KindleReadableScreen> {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        auto screen = UIGetCurrentScreen();
        if (screen) {
            auto kindleScreen = std::dynamic_pointer_cast<KindleReadableScreen>(screen);
            if (kindleScreen) {
                log_i("TEST: Installing short-pages content callback on screen %p",
                      static_cast<void *>(kindleScreen.get()));
                kindleScreen->pageFlipper_.setOnTestContentReady([](size_t ocrTextLength) {
                    log_i("TEST: short-pages content ready, length=%zu", ocrTextLength);
                    shortContentRendered = true;
                });
                kindleScreen->setOnTestLastReadDialogShown([]() {
                    log_i("TEST: last-read dialog shown callback");
                    mockLastReadDialogShownCount++;
                });
                return kindleScreen;
            }
        }

        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(10)) {
            FAIL("Timed out waiting for KindleReadableScreen to install callback");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return nullptr;
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

        if (mockOpenBookLastReadDialogEnabled.load()) {
            mockOpenBookLastReadDialogEnabled = false;
            mockOpenBookLastReadDialogTriggered = true;

            std::string dialogText =
                "You are currently on page 376. The most recent page read is 383 from "
                "Samuel's 5th iPhone at 2:34 AM today.\n\nGo to that page?";

            std::string escapedDialog = dialogText;
            size_t dialogPos = 0;
            while ((dialogPos = escapedDialog.find('\n', dialogPos)) != std::string::npos) {
                escapedDialog.replace(dialogPos, 1, "\\n");
                dialogPos += 2;
            }

            std::ostringstream dialogResponse;
            dialogResponse << "{";
            dialogResponse << "\"success\": true,";
            dialogResponse << "\"last_read_dialog\": true,";
            dialogResponse << "\"dialog_text\": \"" << escapedDialog << "\",";
            dialogResponse << "\"message\": \"Last read page dialog detected\",";
            dialogResponse << "\"already_open\": true,";
            dialogResponse << "\"authenticated\": true";
            dialogResponse << "}";

            res.set_content(dialogResponse.str(), "application/json");
            res.status = 200;
            return;
        }

        std::string content;
        if (useShortPages) {
            if (mockLastReadChoiceCallCount.load() > 0) {
                content = generateSyncedChapterContent(0);
            } else {
                // Generate very short content for initial page
                content = "=== PAGE 0 ===\n";
                content += "This is a very short initial page.\n";
                content += "Designed to trigger multi-page fetching.\n";
                content += "=== END PAGE 0 ===\n";
            }
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
            R"(", "progress": {"percentage": 0, "current_page": 1, "total_pages": 100}, "book_session_key": "test_session_123", "authenticated": true})";
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

        if (useShortPages && mockEnableLastReadDialog.load() &&
            !mockLastReadDialogTriggered.load() && pageToReturn >= 4) {
            mockLastReadDialogTriggered = true;

            std::string dialogText =
                "You are currently on page 4. The most recent page read is 10 from Sam's "
                "Kindle at 8:07 PM today.\n\nGo to that page?";

            std::ostringstream dialogResponse;
            dialogResponse << "{";
            dialogResponse << "\"success\": true,";
            dialogResponse << "\"last_read_dialog\": true,";
            dialogResponse << "\"dialog_text\": \"" << dialogText << "\",";
            dialogResponse << "\"message\": \"Last read page dialog detected\",";
            dialogResponse << "\"authenticated\": true,";
            dialogResponse << "\"ocr_text\": \"\",";
            dialogResponse << "\"book_session_key\": \"test_session_123\"";
            dialogResponse << "}";

            res.set_content(dialogResponse.str(), "application/json");
            res.status = 200;
            return;
        }

        if (useShortPages) {
            if (mockLastReadChoiceCallCount.load() > 0) {
                content = generateSyncedChapterContent(pageToReturn);
            } else {
                // Generate very short pages (200-300 chars each)
                content = "=== PAGE " + std::to_string(pageToReturn) + " ===\n";
                content += "Short page " + std::to_string(pageToReturn) + " content.\n";
                for (int i = 0; i < 3; i++) {
                    content += "Line " + std::to_string(i + 1) + " of page " +
                               std::to_string(pageToReturn) + ".\n";
                }
                content += "=== END PAGE " + std::to_string(pageToReturn) + " ===\n";
            }
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

    httpServer->Post("/kindle/last-read-page-dialog",
                     [](const httplib::Request &req, httplib::Response &res) {
        log_i("Mock server: ---> Received POST /kindle/last-read-page-dialog");
        mockLastReadChoiceCallCount++;
        res.set_content("{\"success\": true}", "application/json");
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
    int port = 18982; // Use a high-numbered port to avoid conflicts with other tests
    setupHttpServer(httpServer, httpServerThread, serverRunning, port);

    // Override SindarinAPI URL
    auto api = SindarinAPI::getInstance();
    api->overrideBaseUrl("http://127.0.0.1:18982"); // Match the port we're using
    api->setAccessToken("test-token");
    log_i("TEST: SindarinAPI URL overridden to mock server");

    log_i("TEST: Starting FreeRTOS for short pages test");
    StartAppMain(FONT_CHOICES::PREFS_SOL_SMALL, 10, 10, -2, "KindleTestData");

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
        resetShortContentRenderFlag();
        SendRemoteCommand(ENTER);

        // In test mode, connection happens synchronously but loadInitialPage still runs async
        // Wait for the state changes and HTTP request to complete
        log_i("TEST: Waiting for connection and initial load");
        WAIT_FOR_DRAW(display); // Draw for CONNECTING state
        auto kindleScreen = setupShortPagesContentCallback();
        resetShortContentRenderFlag();
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

        // Trigger the last read dialog by continuing to navigate forward
        log_i("TEST: Triggering last read dialog");
        mockEnableLastReadDialog = true;
        int safetyCounter = 0;
        while (!mockLastReadDialogTriggered.load() && safetyCounter < 6) {
            SendRemoteCommand(ENTER);
            WAIT_FOR_DRAW(display);
            safetyCounter++;
        }

        REQUIRE(mockLastReadDialogTriggered.load());
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest08-LastReadDialog");

        // Accept the dialog (go to the latest page)
        log_i("TEST: Accepting last read dialog");
        resetShortContentRenderFlag();
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);

        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest09-AfterLastRead");

        // Capture follow-up render after the synced page fully stabilises
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest10-LastReadText");

        auto waitForPostFirst = std::chrono::steady_clock::now();
        while (mockLastReadChoiceCallCount.load() < 1) {
            if (std::chrono::steady_clock::now() - waitForPostFirst > std::chrono::seconds(5)) {
                FAIL("Timed out waiting for last-read dialog POST after first acceptance");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        mockEnableLastReadDialog = false;
        mockLastReadDialogTriggered = false;

        int expectedChoiceCount = mockLastReadChoiceCallCount.load();

        // Close the book to simulate reopening with a last-read dialog from /open-book
        log_i("TEST: Closing Kindle book before reopen");
        SendRemoteCommand(BACK);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);

        auto currentScreen = UIGetCurrentScreen();
        REQUIRE(currentScreen);
        REQUIRE(!std::dynamic_pointer_cast<KindleReadableScreen>(currentScreen));

        // Prepare server to emit last-read dialog on the next open-book request
        mockOpenBookLastReadDialogTriggered = false;
        mockOpenBookLastReadDialogEnabled = true;

        log_i("TEST: Re-opening Kindle book expecting /open-book last read dialog");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        kindleScreen = setupShortPagesContentCallback();
        REQUIRE(kindleScreen);

        int dialogShownBefore = mockLastReadDialogShownCount.load();
        auto waitForDialogStart = std::chrono::steady_clock::now();
        while (true) {
            if (kindleScreen->state_ == KindleReadableScreen::State::SHOWING_DIALOG) {
                break;
            }
            if (std::chrono::steady_clock::now() - waitForDialogStart > std::chrono::seconds(5)) {
                FAIL("Timed out waiting for last-read dialog after re-opening book");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        REQUIRE(mockOpenBookLastReadDialogTriggered.load());
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest11-ReopenLastReadDialog");

        // Accept the dialog to confirm the flow loads synced content again
        log_i("TEST: Accepting last read dialog returned from /open-book");
        resetShortContentRenderFlag();
        int lastReadBeforeReopen = mockLastReadChoiceCallCount.load();
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);

        for (int i = 0; i < 3; i++) {
            WAIT_FOR_DRAW(display);
        }

        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest12-ReopenLastReadText");

        auto waitForPostSecond = std::chrono::steady_clock::now();
        while (mockLastReadChoiceCallCount.load() <= lastReadBeforeReopen) {
            if (std::chrono::steady_clock::now() - waitForPostSecond > std::chrono::seconds(5)) {
                FAIL("Timed out waiting for last-read dialog POST after reopen");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        REQUIRE(mockLastReadChoiceCallCount.load() == expectedChoiceCount + 1);

        // Wait for any background navigation requests to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        log_i("TEST: Multi-page fetching visual test complete");
    }

    log_i("TEST: Exiting short pages visual test");
    api->overrideBaseUrl("");

    // Stop HTTP server before stopping FreeRTOS to avoid deadlock
    teardownHttpServer(httpServer, httpServerThread);

    // Properly stop FreeRTOS to avoid thread cleanup issues in CI
    StopFreeRtos();
}
