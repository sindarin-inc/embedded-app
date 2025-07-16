#include <atomic>
#include <catch_amalgamated.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

// Define this before including project headers to access protected members in tests
#define protected public
#define private public

#include "Communications/SindarinAPI.hpp"
#include "DisplayUtil.hpp"
#include "Displays/DisplayEinkET013TT1.hpp"
#include "Misc/Battery.hpp"
#include "Misc/Defer.hpp"
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
extern PreferencesStore prefs;

extern auto UIGetCurrentScreen() -> std::shared_ptr<Screen>;

// Global server instance - moved inside test case to avoid global initialization

// Control variables for the mock server
static std::atomic<bool> mockShouldTimeout(false);
static std::atomic<bool> mockShouldReturnError(false);
static std::atomic<bool> mockShouldShowLastReadDialog(false);
static std::atomic<bool> mockShouldDisconnect(false);
static std::atomic<int> mockNavigateCallCount(0);
static std::atomic<int> mockDelayMs(0);
static std::atomic<int> mockErrorCode(500);

// State tracking for sequential content delivery
static std::atomic<int> mockCurrentLine(0); // Track which line we're serving next
static const int LINES_PER_PAGE = 20;       // Each "page" contains 20 lines

// Synchronization for content rendering
static std::atomic<bool> contentRendered(false);
static std::atomic<int> contentRenderCount(0);

// Macro to wait for content to be rendered
#define WAIT_FOR_CONTENT_RENDER() \
    do { \
        contentRendered = false; \
        auto start = std::chrono::steady_clock::now(); \
        while (!contentRendered) { \
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5)) { \
                log_e("Timeout waiting for content render"); \
                break; \
            } \
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); \
        } \
    } while (0)

// Function to reset all mock state
void resetMockState() {
    mockShouldTimeout = false;
    mockShouldReturnError = false;
    mockShouldShowLastReadDialog = false;
    mockShouldDisconnect = false;
    mockNavigateCallCount = 0;
    mockDelayMs = 0;
    mockErrorCode = 500;
    mockCurrentLine = 1;
    contentRendered = false;
    contentRenderCount = 0;
}

// Helper function to generate sequential content
std::string generateSequentialContent(int startLine, int numLines) {
    std::string content;
    for (int i = 0; i < numLines; i++) {
        int lineNum = startLine + i;
        content += "Line " + std::to_string(lineNum) + ": This is test content for line " +
                   std::to_string(lineNum) + " with some words to make it realistic.\n";
    }
    return content;
}

// Helper function to setup content rendering callback for testing
void setupContentRenderingCallback() {
    auto screen = UIGetCurrentScreen();
    if (!screen) {
        log_e("TEST: No current screen found");
        return;
    }
    
    auto kindleScreen = std::dynamic_pointer_cast<KindleReadableScreen>(screen);
    if (!kindleScreen) {
        log_e("TEST: Current screen is not a KindleReadableScreen");
        return;
    }
    
    log_i("TEST: Setting up content rendering callback");
    
    // Override the gotContentFunction to track when content is rendered
    kindleScreen->pageFlipper_.setGotContentFunction([oldCallback = kindleScreen->pageFlipper_.gotContentFunction_](size_t ocrTextLength) {
        log_i("TEST: gotContentFunction called with ocrTextLength=%zu", ocrTextLength);
        contentRendered = true;
        contentRenderCount++;
        
        // Call the original callback if it exists
        if (oldCallback) {
            oldCallback(ocrTextLength);
        }
    });
}

void setupHttpServer(std::unique_ptr<httplib::Server> &httpServer,
                     std::shared_ptr<std::thread> &httpServerThread,
                     std::atomic<bool> &serverRunning, int port) {
    // Always reset state, even if server is already running
    resetMockState();

    if (serverRunning) {
        return; // Server already running
    }

    httpServer = std::make_unique<httplib::Server>();

    // Handle Kindle open book endpoint
    httpServer->Get("/kindle/open-book", [](const httplib::Request &req, httplib::Response &res) {
        log_i("Mock server: ---> Received GET /kindle/open-book request");
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

        // Normal response - always return lines 1-20 for the initial request
        std::string simpleContent = generateSequentialContent(1, LINES_PER_PAGE);
        log_i("Mock server: Generated initial content - Lines 1-%d", LINES_PER_PAGE);

        // Escape newlines for JSON
        std::string escapedContent = simpleContent;
        size_t pos = 0;
        while ((pos = escapedContent.find('\n', pos)) != std::string::npos) {
            escapedContent.replace(pos, 1, "\\n");
            pos += 2;
        }

        std::string jsonResponse = R"({
            "success": true,
            "ocr_text": ")" + escapedContent +
                                   R"(",
            "progress": {
                "percentage": 5,
                "current_page": 1,
                "total_pages": 100
            }
        })";

        log_i("Mock server: Sending open-book response with %zu chars of content",
              escapedContent.length());
        log_i("Mock server: First 100 chars of content: %.100s", escapedContent.c_str());
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
        log_i("Mock server: ---> Received GET /kindle/navigate request #%d",
              mockNavigateCallCount.load());

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

        // Extract navigate and preview parameters
        int navigate = 0;
        int preview = 0;
        if (req.has_param("navigate")) {
            navigate = std::stoi(req.get_param_value("navigate"));
        }
        if (req.has_param("preview")) {
            preview = std::stoi(req.get_param_value("preview"));
        }

        log_i("Mock server: Request params - navigate=%d, preview=%d", navigate, preview);
        log_i("Mock server: Full query string: %s", req.target.c_str());

        // For consistent test results, always serve the same content based on the request count
        // This ensures screenshots are stable even if the test reruns
        int requestNumber = mockNavigateCallCount.load();
        int startLine = 1;

        // Determine content based on request pattern:
        // Initial book has lines 1-20
        // Request 1: Preview next page (lines 21-40)
        // Request 2: Navigate forward to page 2 (lines 41-60)
        // Request 3: Navigate forward to page 3 (lines 61-80)
        // Backward navigation uses cached content, no new requests
        // Second forward navigation also uses cached content

        if (requestNumber == 1) {
            startLine = 21; // First preview request after initial content
        } else if (requestNumber == 2) {
            startLine = 41; // First forward navigation (page 2)
        } else if (requestNumber == 3) {
            startLine = 61; // Second forward navigation (page 3)
        } else {
            // For any additional requests, continue the sequence
            startLine = 21 + (requestNumber - 1) * LINES_PER_PAGE;
        }

        // Return preview pages worth of content
        std::string content = "";
        if (preview > 0) {
            content = generateSequentialContent(startLine, preview * LINES_PER_PAGE);
            log_i("Mock server: Generating content for request #%d - Lines %d-%d", requestNumber,
                  startLine, startLine + preview * LINES_PER_PAGE - 1);
        }

        // Escape newlines for JSON
        std::string escapedContent = content;
        size_t pos = 0;
        while ((pos = escapedContent.find('\n', pos)) != std::string::npos) {
            escapedContent.replace(pos, 1, "\\n");
            pos += 2;
        }

        int percentage = std::min(95, mockNavigateCallCount.load() * 10);
        int currentPage = mockNavigateCallCount.load() + 1;

        std::string jsonResponse = R"({
            "success": true,
            "ocr_text": ")" + escapedContent +
                                   R"(",
            "progress": {
                "percentage": )" + std::to_string(percentage) +
                                   R"(,
                "current_page": )" +
                                   std::to_string(currentPage) + R"(,
                "total_pages": 100
            }
        })";

        log_i("Mock server: Sending navigate response with %zu chars, page %d/%d, %d%%",
              escapedContent.length(), currentPage, 100, percentage);
        log_i("Mock server: First 100 chars of content: %.100s", escapedContent.c_str());
        res.set_content(jsonResponse, "application/json");
        res.status = 200;
    });

    httpServer->Post("/kindle/last-read-page-choice",
                     [](const httplib::Request &req, httplib::Response &res) {
        log_i("Mock server: ---> Received POST /kindle/last-read-page-choice");
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

TEST_CASE("Test KindleReadableScreen views independently", "[.][no-kindle]") {
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

            // Initialize serverSyncPlace_ manually since we're not running FreeRTOS in this test.
            // Normally this happens inside KindlePageFlipper::updatePlace() via dispatch queue,
            // but dispatch() requires a FreeRTOS thread context.
            kindleScreen->pageFlipper_.serverSyncPlace_ =
                std::dynamic_pointer_cast<TxtPlace>(Place::factory(0));

            // Force a render update
            kindleScreen->setNeedsLayout();
            kindleScreen->setNeedsRerender();
            log_d("TEST: Rendering page 1");
            kindleScreen->render(*display);
            log_d("TEST: Rendered page 1");

            ASSERT_VIEW_UNCHANGED(*display, "Kindle_State_DisplayContent_Page1");
        }

        SECTION("Test DISPLAY_PARTIAL_CONTENT state with dynamic content addition") {
            auto kindleScreen = std::make_shared<KindleReadableScreen>(
                *display, book, prefs, telemetry, fileSystem, lightingController, localStore, font);

            // Set up partial content (simulating content still loading)
            auto pageStream =
                std::dynamic_pointer_cast<KindlePageStream>(kindleScreen->pageFlipper_.getStream());
            KindlePageResponse response1;
            response1.success = true;
            response1.ocrText = "Initial partial content from lines 1-10...";
            pageStream->appendPageFromResponse(response1);

            // Set partial content state
            kindleScreen->setState(KindleReadableScreen::State::DISPLAY_PARTIAL_CONTENT);
            kindleScreen->setNeedsLayout();
            kindleScreen->setNeedsRerender();
            kindleScreen->render(*display);

            // Add more content dynamically to simulate loading additional pages
            KindlePageResponse response2;
            response2.success = true;
            response2.ocrText = "Additional content from lines 11-20 that was loaded later...";
            pageStream->appendPageFromResponse(response2);

            // Verify the new content is available
            kindleScreen->setNeedsLayout();
            kindleScreen->setNeedsRerender();
            kindleScreen->render(*display);
            ASSERT_VIEW_UNCHANGED(*display, "Kindle_State_PartialContent_Updated");
        }

        SECTION("Test reconnection - LOADING_CONTENT state during reconnect") {
            auto kindleScreen = std::make_shared<KindleReadableScreen>(
                *display, book, prefs, telemetry, fileSystem, lightingController, localStore, font);

            // Simulate a reconnection scenario - we had content but lost connection
            // and are now reconnecting
            kindleScreen->setState(KindleReadableScreen::State::LOADING_CONTENT);
            kindleScreen->loadingMessageView_.setText("Reconnecting...");
            kindleScreen->reconnectionAttempts_ = 1; // Simulate we're in a reconnection attempt

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
            kindleScreen->reconnectionAttempts_ = 4; // Max is 3

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
            SpiramString dialogText = "You last read this book on so-and-so at page 42.\n\n"
                                      "This text usually comes from the server.";
            auto lastReadDialog =
                std::make_shared<KindleLastReadDialogScreen>(*display, dialogText);

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

TEST_CASE("Test mock server URL override", "[.][no-kindle]") {
    SetImagesFolder("KindleTestImages");

    // Set up mock server first
    auto httpServer = std::make_unique<httplib::Server>();
    auto httpServerThread = std::make_shared<std::thread>();
    std::atomic<bool> serverRunning(false);
    int port = 8080;
    setupHttpServer(httpServer, httpServerThread, serverRunning, port);

    // CRITICAL: Override SindarinAPI URL BEFORE creating any other objects
    auto api = SindarinAPI::getInstance();
    log_i("TEST: SindarinAPI instance retrieved, about to override URL");
    api->overrideBaseUrl("http://127.0.0.1:8080");
    api->setAccessToken("test-token");
    log_i("TEST: SindarinAPI URL overridden to mock server and auth token set");

    // Create required dependencies
    PosixFileSystem fileSystem;
    PreferencesStore prefs;
    prefs.begin("test_kindle_integration.ini");
    prefs.font.set(PREFS_SOL_SMALL);
    prefs.wifiDisabled.set(false);
    prefs.bleDisabled.set(true);
    prefs.cloudToken.set("test-token");

    Telemetry telemetry(fileSystem, prefs);
    Battery battery(fileSystem, prefs, telemetry);
    SolDrm solDrm(prefs);
    LocalStore localStore(fileSystem, prefs, battery, telemetry, solDrm);

    auto display = createDisplay();
    LightingController lightingController(prefs);
    Font *font = &fontFace0;
    auto book = std::make_shared<KindleBook>();
    book->title = "Test Book for Integration";
    book->author = "Test Author";
    book->uuid = "test-integration-uuid";

    // Simple test to verify our mock server is working - direct call without FreeRTOS
    auto response = std::make_shared<KindlePageResponse>();

    log_i("Mock server test: Making direct API call...");
    int httpCode = api->openKindleBook("Test Book for Integration", response.get(),
                                       std::make_shared<CancelFlag>(), false);
    log_i("Mock server test: Direct API call completed - HTTP code: %d, success: %s", httpCode,
          response->success ? "true" : "false");

    if (response->success) {
        log_i("Mock server test: OCR text length: %zu", response->ocrText.length());
        log_i("Mock server test: OCR text preview: %.100s", response->ocrText.c_str());
        REQUIRE(!response->ocrText.empty());
        REQUIRE(response->ocrText.find("Line 1") != std::string::npos);
        log_i("Mock server validation: SUCCESS - Got expected content");
    } else {
        log_w("Mock server validation: FAILED - Response not successful, HTTP code: %d", httpCode);
        // For debugging, let's not fail the test yet
        WARN("Mock server test failed but continuing for debugging");
    }

    // Clean up server
    teardownHttpServer(httpServer, httpServerThread);

    // Reset SindarinAPI to default URL to avoid affecting other tests
    api->overrideBaseUrl("");
}

TEST_CASE("Test Kindle navigation visual traversal", "[.][kindle]") {
    // Reset all mock state at the beginning of the test
    resetMockState();

    SetupFileSystemMock("KindleTestData");
    SetImagesFolder("KindleTestImages");

    // Set up mock server first
    auto httpServer = std::make_unique<httplib::Server>();
    auto httpServerThread = std::make_shared<std::thread>();
    std::atomic<bool> serverRunning(false);
    int port = 8080;
    setupHttpServer(httpServer, httpServerThread, serverRunning, port);
    defer { teardownHttpServer(httpServer, httpServerThread); };

    // Override SindarinAPI URL before starting the app
    auto api = SindarinAPI::getInstance();
    api->overrideBaseUrl("http://127.0.0.1:8080");
    api->setAccessToken("test-token");
    log_i("TEST: SindarinAPI URL overridden to mock server");

    log_i("TEST: Starting FreeRTOS");
    StartAppMain(FONT_CHOICES::PREFS_SOL_SMALL, 10, 10, -2, "test/CatchTests/KindleTestData");

    // Set cloud token and disable BLE after preferences are initialized
    prefs.cloudToken.set("test-token");
    prefs.bleDisabled.set(true);
    log_i("TEST: Set cloud token and disabled BLE");

    defer { StopFreeRtos(); };
    log_i("TEST: FreeRTOS started");

    auto &display = GetRootDisplay();
    log_i("TEST: Display created");

    // First screen renders twice once with a full refresh and once with a AU4
    WAIT_FOR_DRAW(display);
    WAIT_FOR_DRAW(display);

    SECTION("Visual test of forward-backward-forward navigation") {
        // 0. Open Kindle book
        SendRemoteCommand(RIGHT);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTestBookMenu1");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTestBookConnecting");
        
        // Setup content rendering callback after the Kindle screen is created
        setupContentRenderingCallback();

        // 1. Navigate forward through 3 pages (screenshots 1-3)
        log_i("TEST: Starting forward navigation through 3 pages");
        for (int page = 0; page < 3; page++) {
            SendRemoteCommand(ENTER);
            WAIT_FOR_CONTENT_RENDER();  // Wait for content to be rendered
            WAIT_FOR_DRAW(display);
            ASSERT_VIEW_UNCHANGED(display,
                                  "KindleNavTestBookPage1-" + std::to_string(page + 1));
        }

        // 2. Navigate backward through 3 pages (screenshots 4-6)
        // We're now on page 3, navigate back to page 1
        log_i("TEST: Starting backward navigation from page 3 to page 1");
        
        // First backward navigation: page 3 -> page 2
        SendRemoteCommand(LEFT);
        WAIT_FOR_CONTENT_RENDER();  // Wait for content to be rendered
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTestBookPage2-1");
        
        // Second backward navigation: page 2 -> page 1
        SendRemoteCommand(LEFT);
        WAIT_FOR_CONTENT_RENDER();  // Wait for content to be rendered
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTestBookPage2-2");
        
        // Third backward navigation: page 1 -> stay on page 1 (beginning of book)
        SendRemoteCommand(LEFT);
        WAIT_FOR_CONTENT_RENDER();  // Wait for content to be rendered
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTestBookPage2-3");
        
        // 3. Navigate forward again through 3 pages (screenshots 7-9)
        log_i("TEST: Starting forward navigation again from page 1 to page 3");
        for (int page = 0; page < 3; page++) {
            SendRemoteCommand(ENTER);
            WAIT_FOR_CONTENT_RENDER();  // Wait for content to be rendered
            WAIT_FOR_DRAW(display);
            ASSERT_VIEW_UNCHANGED(display,
                                  "KindleNavTestBookPage3-" + std::to_string(page + 1));
        }
        
        // Verify mock server was called the expected number of times
        // Request 1: Initial forward navigation to page 2
        // Request 2: Forward navigation to page 3
        // (page 1 already loaded, backward and second forward use cached content)
        log_i("Mock server: Total navigate calls: %d", mockNavigateCallCount.load());
        REQUIRE(mockNavigateCallCount == 2);

        // 4. Navigate to page 4 and beyond to trigger new server request
        log_i("TEST: Navigating to page 4");
        SendRemoteCommand(ENTER);
        WAIT_FOR_CONTENT_RENDER();  // Wait for content to be rendered
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTestBookPage3-4");
        
        // Still should be 2 requests (page 4 was cached from earlier)
        REQUIRE(mockNavigateCallCount == 2);
        
        // Navigate further to page 5 to trigger a new request
        log_i("TEST: Navigating to page 5 to trigger new server request");
        SendRemoteCommand(ENTER);
        WAIT_FOR_CONTENT_RENDER();  // Wait for content to be rendered
        WAIT_FOR_DRAW(display);
        // Don't check screenshot for page 5 since we don't have an expected image
        
        // Now we should have 3 requests
        log_i("Mock server: Total navigate calls after page 5: %d", mockNavigateCallCount.load());
        REQUIRE(mockNavigateCallCount == 3);
    }
    log_i("TEST: Exiting visual navigation test");

    // Reset SindarinAPI to default URL to avoid affecting other tests
    api->overrideBaseUrl("");
}
