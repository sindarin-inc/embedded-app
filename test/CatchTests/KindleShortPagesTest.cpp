#include <atomic>
#include <catch_amalgamated.hpp>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <httplib.h>
#include <memory>
#include <mutex>
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
        content += "Page " + std::to_string(pageNumber) + ": line " + std::to_string(baseLine + i) +
                   " is the most recent synced page.\n";
    }

    content += SYNCED_CHAPTER_FOOTER;
    return content;
}

class TestEvent {
public:
    void signal() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            triggered_ = true;
        }
        cv_.notify_all();
    }

    template <typename Rep, typename Period>
    auto waitFor(const std::chrono::duration<Rep, Period> &timeout) -> bool {
        std::unique_lock<std::mutex> lock(mutex_);
        bool result = cv_.wait_for(lock, timeout, [&]() { return triggered_; });
        if (result) {
            triggered_ = false;
        }
        return result;
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool triggered_ = false;
};

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
    mockNavigateDialogTriggerCount = 0;
    mockEnableLastReadDialog = false;
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

                // Wrap the existing onContentReady callback
                auto originalCallback = kindleScreen->pageFlipper_.getOnContentReady();
                kindleScreen->pageFlipper_.setOnContentReady(
                    [originalCallback](size_t ocrTextLength) {
                    log_i("TEST: short-pages content ready, length=%zu", ocrTextLength);
                    if (originalCallback) {
                        originalCallback(ocrTextLength);
                    }
                });

                kindleScreen->setOnTestLastReadDialogShown(
                    []() { log_i("TEST: last-read dialog shown callback"); });
                kindleScreen->setOnTestLastReadDialogPostComplete([](bool success) {
                    log_i("TEST: last-read dialog POST complete callback, success=%d", success);
                });
                return kindleScreen;
            }
        }

        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(1)) {
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

        // Trigger second dialog when navigating to page 10, only once
        // This should happen after the first dialog choice (from reopen /open-book)
        if (useShortPages && previewTo == 9 && mockLastReadChoiceCallCount.load() == 1 &&
            mockNavigateDialogTriggerCount.load() == 0) {
            mockNavigateDialogTriggerCount++;
            log_i("Mock server: Triggering second last-read dialog at page 10");

            std::string dialogText =
                "You are currently on page 10. The most recent page read is page 20 from "
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
            // After second dialog choice (from /navigate), show page 20+ content
            if (mockLastReadChoiceCallCount.load() == 1) {
                content = generateSyncedChapterContent(pageToReturn);
            } else if (mockLastReadChoiceCallCount.load() >= 2) {
                // After second dialog, we synced to page 20
                // Use the page number from pageToReturn which combines navigate_to and preview_to
                // But we need to map it to sequential pages starting from 20

                int actualPage;

                // For simplicity, just return sequential pages based on the requested position
                // pageToReturn is max(navigate_to, preview_to)
                actualPage = 20 + pageToReturn;

                log_i("Mock server: After second sync - returning PAGE %d content (navigate_to=%d, "
                      "preview_to=%d)",
                      actualPage, navigateTo, previewTo);
                content = "=== PAGE " + std::to_string(actualPage) + " ===\n";
                content += "This is page " + std::to_string(actualPage) + " after second sync.\n";
                // Make pages longer to ensure they're treated as separate pages
                for (int i = 0; i < 12; i++) {
                    content += "Page " + std::to_string(actualPage) + ", line " +
                               std::to_string(i + 1) + ": Content after sync to page 20.\n";
                }
                content += "Page " + std::to_string(actualPage) +
                           ", line 13: Content after sync to page 20.\n";
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
    log_i("TEST: SindarinAPI URL overridden to mock server");

    log_i("TEST: Starting FreeRTOS for short pages test");
    StartAppMain(FONT_CHOICES::PREFS_SOL_SMALL, 10, 10, -2, "KindleTestData");
    log_i("TEST: FreeRTOS started successfully");

    // Set cloud token and disable BLE after preferences are initialized
    prefs.cloudToken.set("test-token");
    prefs.bleDisabled.set(true);
    log_i("TEST: Set cloud token and disabled BLE");

    auto &display = GetRootDisplay();
    log_i("TEST: Display created");

    // First screen renders twice
    WAIT_FOR_DRAW(display);
    WAIT_FOR_DRAW(display);

    log_i("TEST: About to enter SECTION");
    SECTION("Visual test of multi-page fetching with short pages and last-read dialogs") {
        log_i("TEST: Inside SECTION");

        int screenshotNum = 0;
        auto nextScreenshot = [&screenshotNum](const std::string &name) {
            char buffer[128];
            snprintf(buffer, sizeof(buffer), "ShortPagesTest%02d-%s", screenshotNum++,
                     name.c_str());
            return std::string(buffer);
        };

        // Take initial screenshot of main menu
        ASSERT_VIEW_UNCHANGED(display, nextScreenshot("MainMenu"));

        // Navigate to Kindle section
        SendRemoteCommand(RIGHT);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, nextScreenshot("KindleSection"));

        // Navigate up to the last book (SHORT_PAGES_TEST)
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ASSERT_VIEW_UNCHANGED(display, nextScreenshot("ShortPagesBookSelected"));

        // Open the book - this should trigger the first last-read dialog from /open-book
        log_i("TEST: Opening book with ENTER command, expecting last-read dialog");
        SendRemoteCommand(ENTER);

        // Wait for connection and initial load
        log_i("TEST: Waiting for connection and initial load");
        WAIT_FOR_DRAW(display); // Draw for CONNECTING state
        // Skip ShortPagesTest03-Connecting screenshot - transitional state may show overlapping UI
        // elements

        TestEvent firstDialogStateEvent;
        TestEvent secondDialogStateEvent;
        TestEvent firstDialogChoiceEvent;
        TestEvent secondDialogChoiceEvent;
        TestEvent initialContentRendered;
        bool firstDialogObserved = false;
        bool firstDialogChoiceValue = false;
        bool secondDialogChoiceValue = false;
        int dialogChoiceCount = 0;

        auto kindleScreen = setupShortPagesContentCallback();
        REQUIRE(kindleScreen != nullptr);

        kindleScreen->setOnTestStateChanged(
            [&firstDialogStateEvent, &secondDialogStateEvent,
             &firstDialogObserved](KindleReadableScreen::State state) {
            log_d("TEST: Kindle screen state changed to %d", static_cast<int>(state));
            if (state == KindleReadableScreen::State::SHOWING_DIALOG) {
                if (!firstDialogObserved) {
                    firstDialogObserved = true;
                    log_d("TEST: First last-read dialog observed");
                    firstDialogStateEvent.signal();
                } else {
                    log_d("TEST: Second last-read dialog observed");
                    secondDialogStateEvent.signal();
                }
            }
        });

        kindleScreen->setOnTestLastReadDialogChoice([&](bool gotoLastReadPage) {
            log_i("TEST: Last-read dialog choice #%d, goto=%d", dialogChoiceCount + 1,
                  gotoLastReadPage);
            if (dialogChoiceCount == 0) {
                firstDialogChoiceValue = gotoLastReadPage;
                firstDialogChoiceEvent.signal();
            } else {
                secondDialogChoiceValue = gotoLastReadPage;
                secondDialogChoiceEvent.signal();
            }
            dialogChoiceCount++;
        });

        WAIT_FOR_DRAW(display); // Draw for LOADING_INITIAL_CONTENT state
        ASSERT_VIEW_UNCHANGED(display, nextScreenshot("LoadingInitialContent"));

        // Wait for the first last-read dialog from /open-book
        log_i("Waiting for first last-read dialog state transition");
        REQUIRE(firstDialogStateEvent.waitFor(std::chrono::seconds(1)));
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ASSERT_VIEW_UNCHANGED(display, nextScreenshot("FirstLastReadDialog"));

        // Accept the first dialog - should go to Chapter 42
        log_i("TEST: Accepting first last-read dialog to go to Chapter 42");
        SendRemoteCommand(ENTER);

        // Wait for the "Sync to last read page" message
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);

        // Wait for content to render - this will be Chapter 42
        REQUIRE(firstDialogChoiceEvent.waitFor(std::chrono::seconds(1)));
        CHECK(firstDialogChoiceValue);
        ASSERT_VIEW_UNCHANGED(display, nextScreenshot("SyncingToLastRead"));

        WAIT_FOR_DRAW(display);
        // REQUIRE(initialContentRendered.waitFor(std::chrono::seconds(1)));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ASSERT_VIEW_UNCHANGED(display, nextScreenshot("Chapter42Initial"));

        // Page forward through Chapter 42
        log_i("TEST: Paging forward through Chapter 42");
        for (int i = 0; i < 8; i++) {
            SendRemoteCommand(RIGHT);
            WAIT_FOR_DRAW(display);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            ASSERT_VIEW_UNCHANGED(display,
                                  nextScreenshot("Chapter42Forward" + std::to_string(i + 1)));
            if (i == 5) {
                // Navigate to page 10 - should trigger second last-read dialog
                log_i("TEST: Navigating to page 10 to trigger second last-read dialog");
            }
        }

        // Wait for second dialog
        log_i("Waiting for second last-read dialog state transition");

        REQUIRE(secondDialogStateEvent.waitFor(std::chrono::seconds(1)));
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, nextScreenshot("SecondLastReadDialog"));

        // Accept the second dialog - should go to page 20
        log_i("TEST: Accepting second last-read dialog to go to page 20");
        SendRemoteCommand(ENTER);

        // Wait for the "Sync to last read page" message
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);

        // Wait for content to render - this will be Page 20
        REQUIRE(secondDialogChoiceEvent.waitFor(std::chrono::seconds(1)));
        CHECK(secondDialogChoiceValue);
        ASSERT_VIEW_UNCHANGED(display, nextScreenshot("SyncingToPage20"));

        WAIT_FOR_DRAW(display);
        // REQUIRE(initialContentRendered.waitFor(std::chrono::seconds(1)));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        ASSERT_VIEW_UNCHANGED(display, nextScreenshot("Page20Initial"));

        // The reader may have already buffered ahead, so capture the state after buffering
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, nextScreenshot("Page20AfterBuffer"));

        // Page forward a few more times from page 20
        log_i("TEST: Paging forward from page 20");

        // After syncing to page 20, we need to navigate forward
        // The first RIGHT press might just move within the current page buffer

        // First RIGHT press - should move to PAGE 21
        SendRemoteCommand(RIGHT);
        WAIT_FOR_DRAW(display);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ASSERT_VIEW_UNCHANGED(display, nextScreenshot("ForwardFrom20-1"));

        // Second RIGHT press - should move to PAGE 22
        SendRemoteCommand(RIGHT);
        WAIT_FOR_DRAW(display);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ASSERT_VIEW_UNCHANGED(display, nextScreenshot("ForwardFrom20-2"));

        // Third RIGHT press - should move to PAGE 23
        SendRemoteCommand(RIGHT);
        WAIT_FOR_DRAW(display);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ASSERT_VIEW_UNCHANGED(display, nextScreenshot("ForwardFrom20-3"));

        // Page backward to verify backward navigation works
        log_i("TEST: Paging backward to verify backward navigation");
        for (int i = 0; i < 2; i++) {
            SendRemoteCommand(LEFT);
            WAIT_FOR_DRAW(display);
            ASSERT_VIEW_UNCHANGED(display, nextScreenshot("Backward" + std::to_string(i + 1)));
        }

        log_i("TEST: Final screenshot after backward navigation");
        ASSERT_VIEW_UNCHANGED(display, nextScreenshot("FinalState"));

        // Clear any test callbacks to avoid dangling references
        if (kindleScreen) {
            kindleScreen->setOnTestLastReadDialogShown(nullptr);
            kindleScreen->setOnTestLastReadDialogChoice(nullptr);
            kindleScreen->setOnTestLastReadDialogPostComplete(nullptr);
            kindleScreen->setOnTestStateChanged(nullptr);
        }

        log_i("TEST: Kindle short pages test with last-read dialogs complete");
    }

    log_i("TEST: Exiting short pages visual test");
    api->overrideBaseUrl("");

    // Properly stop FreeRTOS to avoid thread cleanup issues in CI
    StopFreeRtos();

    // HTTP server will be cleaned up by defer
}
