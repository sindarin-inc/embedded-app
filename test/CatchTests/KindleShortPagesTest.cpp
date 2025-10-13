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
static std::atomic<int> mockLastReadChoiceCallCount{0};
static std::atomic<int> mockLastReadDialogShownCount{0};
static std::atomic<int> mockNavigateDialogTriggerCount{0};
static std::atomic<bool> mockEnableLastReadDialog{false};

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
    mockLastReadChoiceCallCount = 0;
    mockLastReadDialogShownCount = 0;
    mockNavigateDialogTriggerCount = 0;
    mockEnableLastReadDialog = false;
}

static void setupHttpServer(std::unique_ptr<httplib::Server> &httpServer,
                            std::shared_ptr<std::thread> &httpServerThread,
                            std::atomic<bool> &serverRunning, int port) {
    httpServer->Get("/kindle/open-book", [](const httplib::Request &req, httplib::Response &res) {
        mockOpenBookCallCount++;

        // Check if this is a short pages test book
        std::string bookTitle = req.has_param("title") ? req.get_param_value("title") : "";
        bool useShortPages = (bookTitle.find("SHORT_PAGES_TEST") != std::string::npos);

        log_i(
            "Mock server: open-book request #%d - title='%s', useShortPages=%s, lastReadChoices=%d",
            mockOpenBookCallCount.load(), bookTitle.c_str(), useShortPages ? "true" : "false",
            mockLastReadChoiceCallCount.load());

        // First open-book for short pages test returns a last-read dialog
        if (useShortPages && mockOpenBookCallCount.load() == 1 &&
            mockLastReadChoiceCallCount.load() == 0) {
            log_i("Mock server: Returning last-read dialog from /open-book");

            std::string dialogText = "You are currently on page 1. The most recent page read is "
                                     "page 42 (Chapter 42) from "
                                     "Samuel's Kindle at 11:45 PM yesterday.\n\nGo to that page?";

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
            // After acknowledging the first dialog, return chapter 42 content
            if (mockLastReadChoiceCallCount.load() > 0) {
                content = generateSyncedChapterContent(0);
            } else {
                // Generate very short content for initial page
                content = "=== PAGE 1 ===\n";
                content += "This is the beginning of the book.\n";
                content += "Starting from page 1.\n";
                content += "=== END PAGE 1 ===\n";
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
            R"(", "progress": {"current_page": 1, "total_pages": 100}, "book_session_key": "test_session_123", "authenticated": true})";
        res.set_content(jsonResponse, "application/json");
        res.status = 200;
    });

    httpServer->Get("/kindle/navigate", [](const httplib::Request &req, httplib::Response &res) {
        mockNavigateCallCount++;

        int navigateTo =
            req.has_param("navigate_to") ? std::stoi(req.get_param_value("navigate_to")) : 0;
        int previewTo =
            req.has_param("preview_to") ? std::stoi(req.get_param_value("preview_to")) : 0;
        std::string bookTitle = req.has_param("title") ? req.get_param_value("title") : "";

        bool useShortPages = (bookTitle.find("SHORT_PAGES_TEST") != std::string::npos);
        int pageToReturn = (previewTo > navigateTo) ? previewTo : navigateTo;

        log_i("Mock server: navigate request #%d - title='%s', navigate_to=%d, preview_to=%d, "
              "page=%d, useShortPages=%s, lastReadChoices=%d, dialogTriggers=%d",
              mockNavigateCallCount.load(), bookTitle.c_str(), navigateTo, previewTo, pageToReturn,
              useShortPages ? "true" : "false", mockLastReadChoiceCallCount.load(),
              mockNavigateDialogTriggerCount.load());

        // Trigger second dialog when navigating to page 4, only once
        // This should happen after the first dialog choice (from reopen /open-book)
        if (useShortPages && pageToReturn == 4 && mockLastReadChoiceCallCount.load() == 1 &&
            mockNavigateDialogTriggerCount.load() == 0) {
            mockNavigateDialogTriggerCount++;
            log_i("Mock server: Triggering second last-read dialog at page 4");

            std::string dialogText =
                "You are currently on page 4. The most recent page read is page 10 from "
                "Sam's iPad at 3:15 PM today.\n\nGo to that page?";

            // Escape newlines for JSON
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
            dialogResponse << "\"authenticated\": true,";
            dialogResponse << "\"ocr_text\": \"\",";
            dialogResponse << "\"book_session_key\": \"test_session_123\"";
            dialogResponse << "}";

            res.set_content(dialogResponse.str(), "application/json");
            res.status = 200;
            return;
        }

        std::string content;
        if (useShortPages) {
            // After first dialog choice (from /open-book), show chapter 42
            // After second dialog choice (from /navigate), show page 10+ content
            if (mockLastReadChoiceCallCount.load() == 1) {
                content = generateSyncedChapterContent(pageToReturn);
            } else if (mockLastReadChoiceCallCount.load() >= 2) {
                // After second dialog, we synced to page 10
                // Use the page number from pageToReturn which combines navigate_to and preview_to
                // But we need to map it to sequential pages starting from 10

                int actualPage;

                // For simplicity, just return sequential pages based on the requested position
                // pageToReturn is max(navigate_to, preview_to)
                actualPage = 10 + pageToReturn;

                log_i("Mock server: After second sync - returning PAGE %d content (navigate_to=%d, "
                      "preview_to=%d)",
                      actualPage, navigateTo, previewTo);
                content = "=== PAGE " + std::to_string(actualPage) + " ===\n";
                content += "This is page " + std::to_string(actualPage) + " after second sync.\n";
                // Make pages longer to ensure they're treated as separate pages
                for (int i = 0; i < 12; i++) {
                    content += "Page " + std::to_string(actualPage) + ", line " +
                               std::to_string(i + 1) + ": Content after sync to page 10.\n";
                }
                content += "Page " + std::to_string(actualPage) +
                           ", line 13: Content after sync to page 10.\n";
                content += "=== END PAGE " + std::to_string(actualPage) + " ===\n";
                content += "\n"; // Extra newline to separate pages
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
                                   R"(", "progress": {"current_page": )" +
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

static auto waitForLastReadPostCount(int targetCount,
                                     std::chrono::milliseconds timeout = std::chrono::seconds(5))
    -> bool {
    auto start = std::chrono::steady_clock::now();
    while (mockLastReadChoiceCallCount.load() < targetCount) {
        if (std::chrono::steady_clock::now() - start > timeout) {
            UNSCOPED_INFO("TEST: Timed out waiting for /kindle/last-read-page-dialog POSTs");
            UNSCOPED_INFO("(want=" << targetCount << ", have=" << mockLastReadChoiceCallCount.load()
                                   << ")");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
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
    defer { teardownHttpServer(httpServer, httpServerThread); };

    // Override SindarinAPI URL
    auto api = SindarinAPI::getInstance();
    api->overrideBaseUrl("http://127.0.0.1:18982"); // Match the port we're using
    api->setAccessToken("test-token");

    StartAppMain(FONT_CHOICES::PREFS_SOL_SMALL, 10, 10, -2, "KindleTestData");

    // Set cloud token and disable BLE after preferences are initialized
    prefs.cloudToken.set("test-token");
    prefs.bleDisabled.set(true);

    auto &display = GetRootDisplay();

    SECTION("Visual test of multi-page fetching with short pages and last-read dialogs") {
        // Take initial screenshot of main menu
        // First screen renders twice
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display, false);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest00-MainMenu");

        // Navigate to Kindle section
        SendRemoteCommand(RIGHT);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display, false);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest01-KindleSection");

        // Go to the last book in the list
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display, false);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest02-ShortPagesBookSelected");

        // Open the book - this should load then trigger the first last-read dialog from /open-book
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display, false);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest03-Connecting");

        auto screen = UIGetCurrentScreen();
        auto kindleScreen = std::dynamic_pointer_cast<KindleReadableScreen>(screen);
        REQUIRE(kindleScreen != nullptr);

        WAIT_FOR_DRAW(display, false);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest04-Opening");

        // Wait for the last-read dialog to be shown
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display, false);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest05-FirstLastReadDialog");

        // Accept the first dialog - should go to Chapter 42
        SendRemoteCommand(ENTER);

        // Wait for the "Sync to last read page" message
        WAIT_FOR_DRAW(display, false);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest06-SyncingToLastRead");
        // TODO: Unknown why message page renders twice
        WAIT_FOR_DRAW(display, false);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest06.5-SyncingToLastRead");

        // Wait for content to render - this will be Chapter 42
        WAIT_FOR_DRAW(display, false);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest07-Chapter42Initial");

        // Wait for POST to complete via mock server
        REQUIRE(waitForLastReadPostCount(1));

        // Page forward through Chapter 42
        log_i("TEST: Paging forward through Chapter 42");
        for (int i = 0; i < 3; i++) {
            SendRemoteCommand(RIGHT);
            WAIT_FOR_DRAW(display, false);
            ASSERT_VIEW_UNCHANGED(display,
                                  "ShortPagesTest09-Chapter42Forward" + std::to_string(i + 1));
        }

        // Navigate to page 4 - should trigger second last-read dialog
        SendRemoteCommand(RIGHT);

        // Three: one to render the page, then two to render the menu
        WAIT_FOR_DRAW(display, false);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest10-BeforeSecondLastReadDialog");
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display, false);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest12-SecondLastReadDialog");

        SendRemoteCommand(ENTER);

        // Wait for the "Sync to last read page" message
        WAIT_FOR_DRAW(display, false);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest13-SyncingToPage10");
        // TODO: Unknown why message page renders twice
        WAIT_FOR_DRAW(display, false);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest13.5-SyncingToPage10");

        // Wait for content to render - this will be Page 10
        WAIT_FOR_DRAW(display, false);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest14-Page10Initial");

        // Wait for POST to complete via mock server
        REQUIRE(waitForLastReadPostCount(2));

        // Page forward a few more times from page 10
        log_i("TEST: Paging forward from page 10");

        // After syncing to page 10, we need to navigate forward
        // The first RIGHT press might just move within the current page buffer

        // First RIGHT press - should move to PAGE 11
        SendRemoteCommand(RIGHT);
        WAIT_FOR_DRAW(display, false);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest16-ForwardFrom10-1");

        // Second RIGHT press - should move to PAGE 12
        SendRemoteCommand(RIGHT);
        WAIT_FOR_DRAW(display, false);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest16-ForwardFrom10-2");

        // Third RIGHT press - should move to PAGE 13
        SendRemoteCommand(RIGHT);
        WAIT_FOR_DRAW(display, false);
        ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest16-ForwardFrom10-3");

        // Page backward to verify backward navigation works
        log_i("TEST: Paging backward to verify backward navigation");
        for (int i = 0; i < 2; i++) {
            SendRemoteCommand(LEFT);
            WAIT_FOR_DRAW(display, false);
            ASSERT_VIEW_UNCHANGED(display, "ShortPagesTest19-Backward" + std::to_string(i + 1));
        }
    }

    api->overrideBaseUrl("");

    StopFreeRtos();
}
