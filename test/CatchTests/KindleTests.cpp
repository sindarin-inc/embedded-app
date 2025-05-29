// Define this before including headers to access protected members in tests
#define protected public
#define private public

#include <atomic>
#include <catch_amalgamated.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

#include "Communications/SindarinAPI.hpp"
#include "DisplayUtil.hpp"
#include "Displays/DisplayEinkET013TT1.hpp"
#include "Misc/Battery.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/Telemetry.hpp"
#include "Models/KindleBook.hpp"
#include "Models/KindlePageFlipper.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "Storage/PreferencesStore.hpp"
#include "Storage/SolDrm.hpp"
#include "UI/KindleLastReadDialogScreen.hpp"
#include "UI/KindleReadableScreen.hpp"
#include "ViewTestCommon.hpp"
#include "httplib.h"
#include "sindarin-debug.h"

#undef protected
#undef private

extern Font fontFace0;
extern Font fontFace1;
extern Font fontFace2;

// Global server instance - moved inside test case to avoid global initialization

// Control variables for the mock server
static std::atomic<bool> mockShouldTimeout(false);
static std::atomic<bool> mockShouldReturnError(false);
static std::atomic<bool> mockShouldShowLastReadDialog(false);
static std::atomic<bool> mockShouldDisconnect(false);
static std::atomic<int> mockNavigateCallCount(0);
static std::atomic<int> mockDelayMs(0);
static std::atomic<int> mockErrorCode(500);

void setupHttpServer(std::unique_ptr<httplib::Server> &httpServer,
                     std::shared_ptr<std::thread> &httpServerThread,
                     std::atomic<bool> &serverRunning, int port) {
    // Reset state
    mockNavigateCallCount = 0;
    mockShouldTimeout = false;
    mockShouldReturnError = false;
    mockShouldShowLastReadDialog = false;
    mockShouldDisconnect = false;
    mockDelayMs = 0;

    if (serverRunning) {
        return; // Server already running
    }

    httpServer = std::make_unique<httplib::Server>();

    // Handle Kindle open book endpoint
    httpServer->Get("/kindle/open-book", [](const httplib::Request &req, httplib::Response &res) {
        // Simulate delay if requested
        if (mockDelayMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mockDelayMs.load()));
        }

        // Simulate timeout/disconnect
        if (mockShouldTimeout) {
            // Don't send response, just hang
            std::this_thread::sleep_for(std::chrono::seconds(30));
            return;
        }

        if (mockShouldDisconnect) {
            // Close connection abruptly - httplib will report as error
            return;
        }

        // Simulate server errors
        if (mockShouldReturnError) {
            res.set_content("{\"success\": false, \"error\": \"Server error\"}",
                            "application/json");
            res.status = mockErrorCode.load();
            return;
        }

        // Normal response - create JSON response
        std::string jsonResponse = R"({
            "success": true,
            "ocrText": "Page 1 of 100\n\nChapter 1: Introduction\n\nLorem ipsum dolor sit amet, consectetur adipiscing elit.",
            "progress": {
                "percentage": 5,
                "currentPage": 1,
                "totalPages": 100
            }
        })";

        res.set_content(jsonResponse, "application/json");
        res.status = 200;

        // Add last read dialog header if enabled
        if (mockShouldShowLastReadDialog) {
            res.set_header("x-last-read-dialog",
                           "You last read this book on January 15th at page 42.");
            mockShouldShowLastReadDialog = false;
        }
    });

    // Handle Kindle navigate endpoint
    httpServer->Get("/kindle/navigate", [](const httplib::Request &req, httplib::Response &res) {
        mockNavigateCallCount++;

        // Simulate delay if requested
        if (mockDelayMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mockDelayMs.load()));
        }

        // Simulate timeout/disconnect
        if (mockShouldTimeout) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            return;
        }

        if (mockShouldDisconnect) {
            return;
        }

        // Simulate server errors
        if (mockShouldReturnError) {
            res.set_content("{\"success\": false, \"error\": \"Server error\"}",
                            "application/json");
            res.status = mockErrorCode.load();
            return;
        }

        // Normal response
        std::string content = "Page " + std::to_string(mockNavigateCallCount.load() + 1) +
                              " of 100\n\n"
                              "Chapter " +
                              std::to_string(mockNavigateCallCount.load() + 1) +
                              ": Content\n\n"
                              "Lorem ipsum dolor sit amet, consectetur adipiscing elit.";

        std::string jsonResponse = R"({
            "success": true,
            "ocrText": ")" + content +
                                   R"(",
            "progress": {
                "percentage": )" + std::to_string((mockNavigateCallCount.load() + 1) * 5) +
                                   R"(,
                "currentPage": )" + std::to_string(mockNavigateCallCount.load() + 1) +
                                   R"(,
                "totalPages": 100
            }
        })";

        res.set_content(jsonResponse, "application/json");
        res.status = 200;
    });

    httpServer->Post("/kindle/last-read-page-choice",
                     [](const httplib::Request &req, httplib::Response &res) {
        res.set_content("{\"success\": true}", "application/json");
        res.status = 200;
    });

    // Start the server in a separate thread
    httpServerThread = std::make_shared<std::thread>([port, &httpServer, &serverRunning]() {
        log_i("Starting mock HTTP server on port %d", port);
        serverRunning = true;
        httpServer->listen("127.0.0.1", port);
        serverRunning = false;
        log_i("Mock HTTP server stopped");
    });

    // Give the server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

void teardownHttpServer(std::unique_ptr<httplib::Server> &httpServer,
                        std::shared_ptr<std::thread> &httpServerThread) {
    if (httpServer) {
        log_i("Stopping mock HTTP server");
        httpServer->stop();
    }
    if (httpServerThread && httpServerThread->joinable()) {
        httpServerThread->join();
    }
    httpServerThread = nullptr;
    httpServer = nullptr;
}

TEST_CASE("Test KindleReadableScreen views independently", "[kindle]") {
    SetImagesFolder("KindleTestImages");

    auto display = createDisplay();

    SECTION("Test KindleReadableScreen states") {
        // Create required dependencies
        PosixFileSystem fileSystem;
        PreferencesStore prefs;
        prefs.begin("test_kindle_prefs.ini");
        prefs.font.set(PREFS_SOL_SMALL);
        prefs.wifiDisabled.set(true); // Disable network to avoid dispatch queue usage
        prefs.bleDisabled.set(true);

        Telemetry telemetry(fileSystem, prefs);
        Battery battery(fileSystem, prefs, telemetry);
        SolDrm solDrm(prefs);
        LocalStore localStore(fileSystem, prefs, battery, telemetry, solDrm);
        LightingController lightingController(prefs);

        Font *font = &fontFace0;
        auto book = std::make_shared<KindleBook>();
        book->title = "Test Book";
        book->author = "Test Author";
        book->uuid = "test-uuid";

        SECTION("Test initial CONNECTING state") {
            auto kindleScreen = std::make_shared<KindleReadableScreen>(
                *display, book, prefs, telemetry, fileSystem, lightingController, localStore, font);

            // The screen starts in CONNECTING state
            kindleScreen->render(*display);
            ASSERT_VIEW_UNCHANGED(*display, "Kindle_State_Connecting");
        }

        SECTION("Test ERROR state") {
            auto kindleScreen = std::make_shared<KindleReadableScreen>(
                *display, book, prefs, telemetry, fileSystem, lightingController, localStore, font);

            // Directly set error state and message
            kindleScreen->setState(KindleReadableScreen::State::ERROR);
            kindleScreen->errorMessageView_.setText("Failed to load book content.");
            kindleScreen->errorMessageView_.setHidden(false);

            kindleScreen->render(*display);
            ASSERT_VIEW_UNCHANGED(*display, "Kindle_State_Error");
        }

        SECTION("Test LOADING_CONTENT state") {
            auto kindleScreen = std::make_shared<KindleReadableScreen>(
                *display, book, prefs, telemetry, fileSystem, lightingController, localStore, font);

            // Set loading content state
            kindleScreen->setState(KindleReadableScreen::State::LOADING_CONTENT);
            kindleScreen->loadingMessageView_.setText("Opening your Kindle book...");

            kindleScreen->render(*display);
            ASSERT_VIEW_UNCHANGED(*display, "Kindle_State_LoadingContent");
        }

        SECTION("Test LOADING_CONTENT with delayed message") {
            auto kindleScreen = std::make_shared<KindleReadableScreen>(
                *display, book, prefs, telemetry, fileSystem, lightingController, localStore, font);

            // Set loading content state with delayed message
            kindleScreen->setState(KindleReadableScreen::State::LOADING_CONTENT);
            kindleScreen->loadingMessageView_.setText("Opening your Kindle book...");
            kindleScreen->delayedMessageView_.setText("This can take up to 60 seconds.");
            kindleScreen->delayedMessageView_.setHidden(false);

            kindleScreen->render(*display);
            ASSERT_VIEW_UNCHANGED(*display, "Kindle_State_LoadingDelayed");
        }

        SECTION("Test DISPLAY_CONTENT state - reading book") {
            auto kindleScreen = std::make_shared<KindleReadableScreen>(
                *display, book, prefs, telemetry, fileSystem, lightingController, localStore, font);

            // Set up the page flipper with formatted numbered content
            auto pageStream =
                std::dynamic_pointer_cast<KindlePageStream>(kindleScreen->pageFlipper_.getStream());
            KindlePageResponse response;
            response.success = true;
            response.ocrText = "Chapter 1: Short Lines (2-10 words, spelled numbers)\n\n"
                               "1 one two three four five six 1\n"
                               "2 one two three 2\n"
                               "3 one two three four five six seven eight 3\n"
                               "4 one two three four five six 4\n"
                               "5 one two three four 5\n"
                               "6 one two three four five six seven eight nine 6\n"
                               "7 one two three four five 7\n"
                               "8 one two three four five six seven eight 8\n"
                               "9 one two three four 9\n"
                               "10 one two three four 10";
            pageStream->appendPageFromResponse(response);

            // Set display content state
            kindleScreen->setState(KindleReadableScreen::State::DISPLAY_CONTENT);

            // Force a render update
            kindleScreen->setNeedsLayout();
            kindleScreen->setNeedsRerender();

            kindleScreen->render(*display);
            ASSERT_VIEW_UNCHANGED(*display, "Kindle_State_DisplayContent_Page1");
        }


        SECTION("Test DISPLAY_PARTIAL_CONTENT state") {
            auto kindleScreen = std::make_shared<KindleReadableScreen>(
                *display, book, prefs, telemetry, fileSystem, lightingController, localStore, font);

            // Set up partial content (simulating content still loading)
            auto pageStream =
                std::dynamic_pointer_cast<KindlePageStream>(kindleScreen->pageFlipper_.getStream());
            KindlePageResponse response;
            response.success = true;
            response.ocrText =
                "Page 1 of 100\n\nChapter 1: Introduction\n\nPartial content loaded...";
            pageStream->appendPageFromResponse(response);

            // Set partial content state
            kindleScreen->setState(KindleReadableScreen::State::DISPLAY_PARTIAL_CONTENT);

            // Force a render update
            kindleScreen->setNeedsLayout();
            kindleScreen->setNeedsRerender();

            kindleScreen->render(*display);
            ASSERT_VIEW_UNCHANGED(*display, "Kindle_State_DisplayPartialContent");
        }

        SECTION("Test reconnection - LOADING_CONTENT state during reconnect") {
            auto kindleScreen = std::make_shared<KindleReadableScreen>(
                *display, book, prefs, telemetry, fileSystem, lightingController, localStore, font);

            // Simulate a reconnection scenario - we had content but lost connection
            // and are now reconnecting
            kindleScreen->setState(KindleReadableScreen::State::LOADING_CONTENT);
            kindleScreen->loadingMessageView_.setText("Reconnecting...");
            kindleScreen->reconnectionAttempts_ = 1;  // Simulate we're in a reconnection attempt

            kindleScreen->render(*display);
            ASSERT_VIEW_UNCHANGED(*display, "Kindle_State_Reconnecting");
        }

        SECTION("Test reconnection - Max attempts exceeded") {
            auto kindleScreen = std::make_shared<KindleReadableScreen>(
                *display, book, prefs, telemetry, fileSystem, lightingController, localStore, font);

            // Simulate max reconnection attempts exceeded
            kindleScreen->setState(KindleReadableScreen::State::ERROR);
            kindleScreen->errorMessageView_.setText(
                "Failed to load content. Please check your network connection.");
            kindleScreen->errorMessageView_.setHidden(false);
            kindleScreen->reconnectionAttempts_ = 4;  // Max is 3

            kindleScreen->render(*display);
            ASSERT_VIEW_UNCHANGED(*display, "Kindle_State_ReconnectionFailed");
        }

        SECTION("Test multiple pages - Page 2") {
            auto kindleScreen = std::make_shared<KindleReadableScreen>(
                *display, book, prefs, telemetry, fileSystem, lightingController, localStore, font);

            // Add all the content as one continuous stream
            auto pageStream =
                std::dynamic_pointer_cast<KindlePageStream>(kindleScreen->pageFlipper_.getStream());
            
            // Add all 60 lines in one response
            KindlePageResponse response;
            response.success = true;
            response.ocrText = "Chapter 1: Short Lines (2-10 words, spelled numbers)\n\n"
                               "1 one two three four five six 1\n"
                               "2 one two three 2\n"
                               "3 one two three four five six seven eight 3\n"
                               "4 one two three four five six 4\n"
                               "5 one two three four 5\n"
                               "6 one two three four five six seven eight nine 6\n"
                               "7 one two three four five 7\n"
                               "8 one two three four five six seven eight 8\n"
                               "9 one two three four 9\n"
                               "10 one two three four 10\n"
                               "11 one two three four five six seven 11\n"
                               "12 one two 12\n"
                               "13 one two three four 13\n"
                               "14 one two three four five six seven 14\n"
                               "15 one two three four five six seven eight nine 15\n"
                               "16 one two three four five six seven 16\n"
                               "17 one two three four five six 17\n"
                               "18 one two three four five six seven eight nine ten 18\n"
                               "19 one two three four five six seven eight 19\n"
                               "20 one two three four five six seven eight 20\n"
                               "21 one two three four five six seven 21\n"
                               "22 one two three 22\n"
                               "23 one two three four five six seven eight nine ten 23\n"
                               "24 one two three 24\n"
                               "25 one two 25\n"
                               "26 one two three four five 26\n"
                               "27 one two three four five six seven 27\n"
                               "28 one two three four 28\n"
                               "29 one two three four five 29\n"
                               "30 one two three four five six seven eight nine 30\n"
                               "31 one two three four five 31\n"
                               "32 one two three four five six 32\n"
                               "33 one two three four five six seven 33\n"
                               "34 one two three four five six 34\n"
                               "35 one two three four five six seven eight 35\n"
                               "36 one two three four five six seven eight nine 36\n"
                               "37 one two three four five six seven eight nine 37\n"
                               "38 one two three four five 38\n"
                               "39 one two three four five six 39\n"
                               "40 one two three four five six seven eight 40\n"
                               "41 one two three four 41\n"
                               "42 one two three four 42\n"
                               "43 one two three four five six seven eight nine 43\n"
                               "44 one two three four five six 44\n"
                               "45 one two three four five six seven eight 45\n"
                               "46 one two three 46\n"
                               "47 one two 47\n"
                               "48 one two three 48\n"
                               "49 one two three four five six seven eight nine 49\n"
                               "50 one two 50\n"
                               "51 one two three four five six seven eight nine ten 51\n"
                               "52 one two three four five six seven eight nine ten 52\n"
                               "53 one two three 53\n"
                               "54 one two three four five six seven eight 54\n"
                               "55 one two three four five six seven eight 55\n"
                               "56 one two 56\n"
                               "57 one two three four five six 57\n"
                               "58 one two three four 58\n"
                               "59 one two three four five six seven 59\n"
                               "60 one two three four five six seven eight nine ten 60";
            pageStream->appendPageFromResponse(response);

            // Set display content state
            kindleScreen->setState(KindleReadableScreen::State::DISPLAY_CONTENT);
            
            // Render page 1 first (lines 1-10)
            kindleScreen->currentPlace_ = std::dynamic_pointer_cast<TxtPlace>(Place::factory(0));
            kindleScreen->setNeedsLayout();
            kindleScreen->setNeedsRerender();
            kindleScreen->render(*display);
            
            // Get the next place from the renderer after page 1
            // This should be the start of line 11
            auto nextPlace = kindleScreen->newPlace_;
            
            // Navigate to page 2 using the actual next place
            kindleScreen->currentPlace_ = nextPlace;
            kindleScreen->setNeedsLayout();
            kindleScreen->setNeedsRerender();
            
            kindleScreen->render(*display);
            ASSERT_VIEW_UNCHANGED(*display, "Kindle_State_DisplayContent_Page2");
        }

        SECTION("Test last read dialog") {
            auto kindleScreen = std::make_shared<KindleReadableScreen>(
                *display, book, prefs, telemetry, fileSystem, lightingController, localStore, font);

            // Create a dialog screen directly
            SpiramString dialogText = "You last read this book on January 15th at page 42.\n\n"
                                     "Would you like to continue from where you left off?";
            auto lastReadDialog = std::make_shared<KindleLastReadDialogScreen>(*display, dialogText);
            
            lastReadDialog->render(*display);
            ASSERT_VIEW_UNCHANGED(*display, "Kindle_LastReadDialog");
        }

        SECTION("Test navigation at end of content") {
            auto kindleScreen = std::make_shared<KindleReadableScreen>(
                *display, book, prefs, telemetry, fileSystem, lightingController, localStore, font);

            auto pageStream =
                std::dynamic_pointer_cast<KindlePageStream>(kindleScreen->pageFlipper_.getStream());
            
            // Add the last lines of the formatted content
            KindlePageResponse response;
            response.success = true;
            response.ocrText = "Chapter 1: Short Lines (continued)\n\n"
                              "91 one two three four five 91\n"
                              "92 one two three four five six seven eight 92\n"
                              "93 one two three 93\n"
                              "94 one two three 94\n"
                              "95 one two three four five six seven eight nine 95\n"
                              "96 one two three four five 96\n"
                              "97 one two three 97\n"
                              "98 one two 98\n"
                              "99 one two three four 99\n"
                              "100 one two three four five six 100\n\n"
                              "THE END";
            pageStream->appendPageFromResponse(response);

            // Set at end of content
            kindleScreen->setState(KindleReadableScreen::State::DISPLAY_PARTIAL_CONTENT);
            kindleScreen->loadingMessageView_.setText(""); // Clear any loading message
            kindleScreen->setNeedsLayout();
            kindleScreen->setNeedsRerender();

            kindleScreen->render(*display);
            ASSERT_VIEW_UNCHANGED(*display, "Kindle_State_EndOfBook");
        }
    }
}

// TODO: Add FreeRTOS test once we resolve the dispatch queue initialization issue
// The KindleReadableScreen constructor sets up callbacks that use DispatchGetUiQueue()
// which causes assertion failures when FreeRTOS is not fully initialized
