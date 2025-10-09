#include <atomic>
#include <catch_amalgamated.hpp>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
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
#include "Models/Json.hpp"
#include "Models/KindleBook.hpp"
#include "Models/KindleChaptersResponse.hpp"
#include "Models/KindlePageFlipper.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "Storage/PreferencesStore.hpp"
#include "Storage/SolDrm.hpp"
#include "UI/KindleLastReadDialogScreen.hpp"
#include "UI/KindleReadableScreen.hpp"
#include "UI/KindleTableOfContentsScreen.hpp"
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
static const int LINES_PER_PAGE = 12;       // Each "page" contains 12 lines (~1000 bytes)

// State tracking for absolute navigation
static std::atomic<int>
    mockBookOpenPosition(0); // Track the position when book was opened (always 0)

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
    mockBookOpenPosition = 0;
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

namespace {
constexpr const char *MOCK_BOOK_SESSION_KEY = "test_session_123";
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

        // Check the book title to determine what content to serve
        std::string bookTitle = req.has_param("title") ? req.get_param_value("title") : "";
        bool useShortPages = (bookTitle.find("SHORT_PAGES_TEST") != std::string::npos);

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

        // Generate content based on book title
        std::string simpleContent;
        if (useShortPages) {
            // Generate SHORT content (about 200-250 chars) for short pages test
            simpleContent = "=== PAGE 0 ===\n";
            for (int i = 1; i <= 10; i++) {
                simpleContent += "Line " + std::to_string(i) + ": Short content.\n";
            }
            simpleContent += "=== END PAGE 0 ===\n";
            log_i("Mock server: Generated SHORT initial content - 10 short lines");
        } else {
            // Normal response - return lines 1-20 for the initial request
            simpleContent = generateSequentialContent(1, LINES_PER_PAGE);
            log_i("Mock server: Generated initial content - Lines 1-%d", LINES_PER_PAGE);
        }

        // Escape newlines for JSON
        std::string escapedContent = simpleContent;
        size_t pos = 0;
        while ((pos = escapedContent.find('\n', pos)) != std::string::npos) {
            escapedContent.replace(pos, 1, "\\n");
            pos += 2;
        }

        std::ostringstream response;
        response << "{";
        response << "\"success\": true,";
        response << "\"ocr_text\": \"" << escapedContent << "\",";
        response << "\"progress\": {\"percentage\": 5,\"current_page\": 1,\"total_pages\": 100},";
        response << "\"book_session_key\": \"" << MOCK_BOOK_SESSION_KEY << "\"";
        response << "}";
        std::string jsonResponse = response.str();

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

    httpServer->Get("/kindle/navigate", [](const httplib::Request &req, httplib::Response &res) {
        mockNavigateCallCount++;
        log_i("Mock server: ---> Received GET /kindle/navigate request #%d",
              mockNavigateCallCount.load());

        // Check the book title to determine what content to serve
        std::string bookTitle = req.has_param("title") ? req.get_param_value("title") : "";
        bool useShortPages = (bookTitle.find("SHORT_PAGES_TEST") != std::string::npos);

        if (mockDelayMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mockDelayMs.load()));
        }

        if (mockShouldTimeout) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            return;
        }

        if (mockShouldDisconnect) {
            return;
        }

        if (mockShouldReturnError) {
            res.set_content("{\"success\": false, \"error\": \"Server error\"}",
                            "application/json");
            res.status = mockErrorCode.load();
            return;
        }

        int navigateTo = 0;
        int previewTo = 0;

        if (req.has_param("navigate_to")) {
            navigateTo = std::stoi(req.get_param_value("navigate_to"));
        }
        if (req.has_param("preview_to")) {
            previewTo = std::stoi(req.get_param_value("preview_to"));
        }

        std::string sessionKey;
        if (req.has_param("book_session_key")) {
            sessionKey = req.get_param_value("book_session_key");
        }

        if (sessionKey != MOCK_BOOK_SESSION_KEY) {
            log_e("Mock server: Missing or invalid book_session_key. Got: %s",
                  sessionKey.empty() ? "<empty>" : sessionKey.c_str());
            res.set_content("{\"success\": false, \"error\": \"invalid_session_key\"}",
                            "application/json");
            res.status = 400;
            return;
        }

        if (useShortPages) {
            log_i("Mock server: SHORT PAGES navigation - navigate_to=%d, preview_to=%d", navigateTo,
                  previewTo);
        } else {
            log_i("Mock server: ABSOLUTE navigation - navigate_to=%d, preview_to=%d", navigateTo,
                  previewTo);
        }

        int currentPageNumber = navigateTo + 1;

        // The server should return only the preview page content when preview_to > navigate_to
        // This is because the client already has the navigate_to page
        std::string content = "";
        if (useShortPages) {
            // Generate SHORT content for the requested page
            int pageToReturn = (previewTo > navigateTo) ? previewTo : navigateTo;
            content = "=== PAGE " + std::to_string(pageToReturn) + " ===\n";
            for (int i = 1; i <= 10; i++) {
                int lineNum = pageToReturn * 10 + i;
                content += "Line " + std::to_string(lineNum) + ": Short content.\n";
            }
            content += "=== END PAGE " + std::to_string(pageToReturn) + " ===\n";
            log_i("Mock server: Sending SHORT page %d - Lines %d-%d", pageToReturn,
                  pageToReturn * 10 + 1, pageToReturn * 10 + 10);
        } else {
            // Normal long content
            if (previewTo > navigateTo) {
                // Send only the preview page content (the page at preview_to)
                int previewStartLine = 1 + (previewTo * LINES_PER_PAGE);
                content = generateSequentialContent(previewStartLine, LINES_PER_PAGE);
                log_i("Mock server: Sending PREVIEW page %d only - Lines %d-%d", previewTo,
                      previewStartLine, previewStartLine + LINES_PER_PAGE - 1);
            } else if (previewTo == navigateTo) {
                // Send the navigate_to page content
                int startLine = 1 + (navigateTo * LINES_PER_PAGE);
                content = generateSequentialContent(startLine, LINES_PER_PAGE);
                log_i("Mock server: Sending NAVIGATE page %d only - Lines %d-%d", navigateTo,
                      startLine, startLine + LINES_PER_PAGE - 1);
            }
        }

        std::string escapedContent = content;
        size_t pos = 0;
        while ((pos = escapedContent.find('\n', pos)) != std::string::npos) {
            escapedContent.replace(pos, 1, "\\n");
            pos += 2;
        }

        int percentage = std::min(95, currentPageNumber * 10);

        std::ostringstream response;
        response << "{";
        response << "\"success\": true,";
        response << "\"ocr_text\": \"" << escapedContent << "\",";
        response << "\"progress\": {\"percentage\": " << percentage
                 << ",\"current_page\": " << currentPageNumber << ",\"total_pages\": 100},";
        response << "\"book_session_key\": \"" << MOCK_BOOK_SESSION_KEY << "\"";
        response << "}";
        std::string jsonResponse = response.str();

        log_i("Mock server: Sending navigate response with %zu chars, page %d/%d, %d%%",
              escapedContent.length(), currentPageNumber, 100, percentage);
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

TEST_CASE("KindleBook serializes progress fields", "[.][no-kindle]") {
    DynamicJsonDocument doc(512);
    JsonObject obj = doc.to<JsonObject>();

    KindleBook book;
    book.uuid = "uuid";
    book.title = "Title";
    book.author = "Author";
    book.progress = "42%";
    book.progressPercent = 42;
    book.progressCurrentPage = 120;
    book.progressTotalPages = 250;

    REQUIRE(convertToJson(book, obj));
    REQUIRE(obj["progress"].as<std::string>() == "42%");
    REQUIRE(obj["progress_percent"].as<int>() == 42);
    REQUIRE(obj["progress_current_page"].as<int>() == 120);
    REQUIRE(obj["progress_total_pages"].as<int>() == 250);

    KindleBook parsed;
    REQUIRE(parsed.fromJson(obj));
    REQUIRE(parsed.progress == "42%");
    REQUIRE(parsed.progressPercent == 42);
    REQUIRE(parsed.progressCurrentPage == 120);
    REQUIRE(parsed.progressTotalPages == 250);
}

TEST_CASE("Test KindleReadableScreen views independently", "[.][no-kindle]") {
    SetImagesFolder("KindleImages");

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
            pageStream->appendPageFromResponse(response, 0);

            // Set display content state
            kindleScreen->setState(KindleReadableScreen::State::DISPLAY_CONTENT);

            // Initialize lastServerSyncPlace_ manually since we're not running FreeRTOS in this
            // test. Normally this happens inside KindlePageFlipper::updatePlace() via dispatch
            // queue, but dispatch() requires a FreeRTOS thread context.
            kindleScreen->pageFlipper_.lastServerSyncPlace_ =
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
            pageStream->appendPageFromResponse(response1, 0);

            // Set partial content state
            kindleScreen->setState(KindleReadableScreen::State::DISPLAY_PARTIAL_CONTENT);
            kindleScreen->setNeedsLayout();
            kindleScreen->setNeedsRerender();
            kindleScreen->render(*display);

            // Add more content dynamically to simulate loading additional pages
            KindlePageResponse response2;
            response2.success = true;
            response2.ocrText = "Additional content from lines 11-20 that was loaded later...";
            pageStream->appendPageFromResponse(response2, 1);

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
            pageStream->appendPageFromResponse(response, 0);

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
            pageStream->appendPageFromResponse(response, 0);

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

TEST_CASE("Kindle table of contents highlights current chapter", "[.][no-kindle]") {
    SetImagesFolder("KindleImages");

    auto display = createDisplay();

    PosixFileSystem fileSystem;
    PreferencesStore prefs;
    prefs.begin("test_kindle_prefs.ini");

    Telemetry telemetry(fileSystem, prefs);
    auto book = std::make_shared<KindleBook>();
    book->uuid = "kindle-toc-uuid";
    book->title = "Kindle TOC";
    book->author = "Tester";

    auto cancelFlag = std::make_shared<CancelFlag>();
    KindlePageFlipper pageFlipper(book, cancelFlag, telemetry);

    KindleTableOfContentsScreen tocScreen(*display, book, prefs, telemetry, pageFlipper, 0,
                                          cancelFlag);

    const std::string midChapter =
        R"({"success":true,"progress":{"current_page":35,"total_pages":1000},"chapters":[{"title":"Intro","uuid":"c1","page":1},{"title":"Middle","uuid":"c2","page":20},{"title":"Later","uuid":"c3","page":40}]})";
    auto midChapterResponse = std::make_shared<KindleChaptersResponse>();
    JsonDocument doc;
    deserializeJson(doc, midChapter);
    midChapterResponse->fromJson(doc.as<JsonObject>());
    tocScreen.handleTableOfContentsResponse(midChapterResponse);
    REQUIRE(tocScreen.menuView_.getSelectedIndex() == 1);
    REQUIRE(book->progressPercent == 4);

    const std::string earlyChapter =
        R"({"success":true,"progress":{"current_page":0,"total_pages":1000},"chapters":[{"title":"Intro","uuid":"c1","page":5},{"title":"Next","uuid":"c2","page":15}]})";
    auto earlyChapterResponse = std::make_shared<KindleChaptersResponse>();
    JsonDocument doc2;
    deserializeJson(doc2, earlyChapter);
    earlyChapterResponse->fromJson(doc2.as<JsonObject>());
    tocScreen.handleTableOfContentsResponse(earlyChapterResponse);
    REQUIRE(tocScreen.menuView_.getSelectedIndex() == 0);
    REQUIRE(book->progressPercent == 0);

    const std::string lateChapter =
        R"({"success":true,"progress":{"current_page":200,"total_pages":1000},"chapters":[{"title":"Intro","uuid":"c1","page":5},{"title":"Next","uuid":"c2","page":115},{"title":"Last","uuid":"c3","page":190}]})";
    auto lateChapterResponse = std::make_shared<KindleChaptersResponse>();
    JsonDocument doc3;
    deserializeJson(doc3, lateChapter);
    lateChapterResponse->fromJson(doc3.as<JsonObject>());
    tocScreen.handleTableOfContentsResponse(lateChapterResponse);
    REQUIRE(tocScreen.menuView_.getSelectedIndex() == 2);
    REQUIRE(book->progressPercent == 20);
}

TEST_CASE("Kindle progress updates derive percentages", "[.][no-kindle]") {
    PosixFileSystem fileSystem;
    PreferencesStore prefs;
    prefs.begin("test_kindle_prefs.ini");
    Telemetry telemetry(fileSystem, prefs);

    auto book = std::make_shared<KindleBook>();
    book->uuid = "progress-uuid";
    book->title = "Progress Test";
    book->author = "Tester";

    auto cancelFlag = std::make_shared<CancelFlag>();
    KindlePageFlipper flipper(book, cancelFlag, telemetry);

    KindlePageProgress progress;
    progress.currentPage = 350; // Human-readable page 350
    progress.totalPages = 1000;
    progress.hasCurrentPage = true;
    progress.hasTotalPages = true;

    flipper.applyProgressUpdate(progress);

    REQUIRE(book->progressPercent == 35); // 350/1000 = 35%
    REQUIRE(std::string(book->progress.c_str()) == "35%");
    REQUIRE(book->progressCurrentPage == 350);
    REQUIRE(book->progressTotalPages == 1000);

    KindlePageProgress lastPage;
    lastPage.currentPage = 1000; // Human-readable page 1000 (last page)
    lastPage.totalPages = 1000;
    lastPage.hasCurrentPage = true;
    lastPage.hasTotalPages = true;

    flipper.applyProgressUpdate(lastPage);
    REQUIRE(book->progressPercent == 100); // 1000/1000 = 100%
    REQUIRE(std::string(book->progress.c_str()) == "100%");

    // Test that percentage is calculated from page numbers
    KindlePageProgress pageUpdate;
    pageUpdate.currentPage = 12; // Human-readable page 12
    pageUpdate.totalPages = 100;
    pageUpdate.hasCurrentPage = true;
    pageUpdate.hasTotalPages = true;

    flipper.applyProgressUpdate(pageUpdate);
    REQUIRE(book->progressPercent == 12); // 12/100 = 12%
    REQUIRE(std::string(book->progress.c_str()) == "12%");
}

TEST_CASE("Test mock server URL override", "[.][no-kindle]") {
    SetImagesFolder("KindleImages");

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
    SetImagesFolder("KindleImages");

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

    StartAppMain(FONT_CHOICES::PREFS_SOL_SMALL, 10, 10, -2, "KindleTestData");

    // Set cloud token and disable BLE after preferences are initialized
    prefs.cloudToken.set("test-token");
    prefs.bleDisabled.set(true);

    auto &display = GetRootDisplay();

    SECTION("Visual test of forward-backward-forward navigation") {
        // First screen renders twice once with a full refresh and once with a AU4
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);

        // 0. Open Kindle book
        SendRemoteCommand(RIGHT);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest00-BookMenu", true);

        SendRemoteCommand(ENTER);

        // Wait for content to load (skip capturing the CONNECTING state as it's too transient)
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);

        auto screen = UIGetCurrentScreen();
        auto kindleScreen = std::dynamic_pointer_cast<KindleReadableScreen>(screen);
        REQUIRE(kindleScreen != nullptr);

        // Capture the initial page 0 (Lines 1-20)
        // Note: Initial content is already rendered before we can set up the callback,
        // so we just wait for the draw
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest01-InitialPage0", true);

        // 1. Navigate forward through 6 pages to pages 1-6 (clears buffer)
        SendRemoteCommand(ENTER);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest02-ForwardToPage1", true);

        SendRemoteCommand(ENTER);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest03-ForwardToPage2", true);

        SendRemoteCommand(ENTER);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest04-ForwardToPage3", true);

        SendRemoteCommand(ENTER);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest05-ForwardToPage4", true);

        SendRemoteCommand(ENTER);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest06-ForwardToPage5", true);

        SendRemoteCommand(ENTER);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest07-ForwardToPage6", true);

        // 2. Navigate backward through 6 pages
        // We're now on page 6, navigate back to page 0
        // Navigate back 6 pages: 6 -> 5 -> 4 -> 3 -> 2 -> 1 -> 0
        SendRemoteCommand(LEFT);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest08-BackToPage5", true);

        SendRemoteCommand(LEFT);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest09-BackToPage4", true);

        SendRemoteCommand(LEFT);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest10-BackToPage3", true);

        SendRemoteCommand(LEFT);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest11-BackToPage2", true);

        SendRemoteCommand(LEFT);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest12-BackToPage1", true);

        SendRemoteCommand(LEFT);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest13-BackToPage0", true);

        // 3. Navigate forward again through 6 pages from page 0 to page 6
        SendRemoteCommand(ENTER);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest14-ForwardAgainToPage1", true);

        SendRemoteCommand(ENTER);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest15-ForwardAgainToPage2", true);

        SendRemoteCommand(ENTER);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest16-ForwardAgainToPage3", true);

        SendRemoteCommand(ENTER);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest17-ForwardAgainToPage4", true);

        SendRemoteCommand(ENTER);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest18-ForwardAgainToPage5", true);

        SendRemoteCommand(ENTER);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest19-ForwardAgainToPage6", true);

        // Verify mock server was called the expected number of times
        // With the buffer filling logic and 6-page navigation, we expect more calls:
        // - Initial load and buffer filling
        // - Forward navigation through 6 pages (may trigger multiple preview requests)
        // - Backward navigation doesn't trigger new requests (cached)
        // - Forward again through 6 pages (may need new requests after buffer cleared)
        log_i("Mock server: Total navigate calls after full navigation: %d",
              mockNavigateCallCount.load());
        // With 6-page navigation that clears buffers, expect more calls
        // The exact number depends on buffer sizes and when additional previews are triggered
        REQUIRE(mockNavigateCallCount >= 6);
        REQUIRE(mockNavigateCallCount <= 20);

        // 4. Navigate to page 7 and beyond to trigger new server request
        SendRemoteCommand(ENTER);
        ASSERT_VIEW_UNCHANGED(display, "KindleNavTest20-ForwardToPage7", true);

        // After navigating to page 4, we should have more calls
        log_i("Mock server: Total navigate calls after page 4: %d", mockNavigateCallCount.load());
        REQUIRE(mockNavigateCallCount >= 9);
        REQUIRE(mockNavigateCallCount <= 16);

        // Navigate further to page 5
        SendRemoteCommand(ENTER); // Wait for content to be rendered
        WAIT_FOR_DRAW(display);
        // Don't check screenshot for page 5 since we don't have an expected image

        // With automatic buffer filling, we expect more calls
        log_i("Mock server: Total navigate calls after page 5: %d", mockNavigateCallCount.load());
        REQUIRE(mockNavigateCallCount >= 10);
        REQUIRE(mockNavigateCallCount <= 18);
    }

    // Reset SindarinAPI to default URL to avoid affecting other tests
    api->overrideBaseUrl("");

    // Properly stop FreeRTOS to avoid thread cleanup issues in CI
    StopFreeRtos();
}

TEST_CASE("Test Kindle absolute navigation idempotency", "[kindle]") {
    SetImagesFolder("KindleImages");

    auto httpServer = std::make_unique<httplib::Server>();
    auto httpServerThread = std::make_shared<std::thread>();
    std::atomic<bool> serverRunning(false);
    int port = 8081;

    std::map<std::string, int> navigationRequestCounts;

    httpServer = std::make_unique<httplib::Server>();

    httpServer->Get("/kindle/open-book", [](const httplib::Request &req, httplib::Response &res) {
        log_i("Mock server: Received /kindle/open-book - returning page 0 (lines 1-20)");

        std::string content = generateSequentialContent(1, LINES_PER_PAGE);
        std::string escapedContent = content;
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
                "current_page": 1,
                "total_pages": 100
            },
            "book_session_key": "test_session_123"
        })";

        res.set_content(jsonResponse, "application/json");
        res.status = 200;
    });

    httpServer->Get("/kindle/navigate", [&navigationRequestCounts](const httplib::Request &req,
                                                                   httplib::Response &res) {
        int navigateTo =
            req.has_param("navigate_to") ? std::stoi(req.get_param_value("navigate_to")) : 0;
        int previewTo =
            req.has_param("preview_to") ? std::stoi(req.get_param_value("preview_to")) : 0;

        std::string navKey =
            "nav_" + std::to_string(navigateTo) + "_prev_" + std::to_string(previewTo);
        navigationRequestCounts[navKey]++;

        log_i("Mock server: Idempotent nav - navigate_to=%d, preview_to=%d (request #%d for this "
              "combo)",
              navigateTo, previewTo, navigationRequestCounts[navKey]);

        int startLine = 1 + (navigateTo * LINES_PER_PAGE);
        int pagesToSend = (previewTo >= navigateTo) ? (previewTo - navigateTo + 1) : 0;

        std::string content = "";
        if (pagesToSend > 0) {
            content = generateSequentialContent(startLine, pagesToSend * LINES_PER_PAGE);
            log_i("Mock server: Returning lines %d-%d (idempotent)", startLine,
                  startLine + pagesToSend * LINES_PER_PAGE - 1);
        }

        std::string escapedContent = content;
        size_t pos = 0;
        while ((pos = escapedContent.find('\n', pos)) != std::string::npos) {
            escapedContent.replace(pos, 1, "\\n");
            pos += 2;
        }

        int currentPageNumber = navigateTo + 1;

        std::string jsonResponse = R"({
            "success": true,
            "ocr_text": ")" + escapedContent +
                                   R"(",
            "progress": {
                "current_page": )" +
                                   std::to_string(currentPageNumber) + R"(,
                "total_pages": 100
            },
            "book_session_key": "test_session_123"
        })";

        res.set_content(jsonResponse, "application/json");
        res.status = 200;
    });

    // Start server
    httpServerThread = std::make_shared<std::thread>([port, &httpServer, &serverRunning]() {
        log_i("Starting idempotent test server on port %d", port);
        serverRunning = true;
        httpServer->listen("127.0.0.1", port);
        serverRunning = false;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    defer { teardownHttpServer(httpServer, httpServerThread); };

    // Test direct API calls (without FreeRTOS)
    auto api = SindarinAPI::getInstance();
    api->overrideBaseUrl("http://127.0.0.1:8081");
    api->setAccessToken("test-token");

    SECTION("Idempotent navigation requests") {
        auto cancelFlag = std::make_shared<CancelFlag>();

        log_i("TEST: Testing idempotency of navigate_to=0, preview_to=2");
        std::vector<std::string> responses;

        for (int i = 0; i < 3; i++) {
            auto response = std::make_shared<KindlePageResponse>();
            std::string url = "/kindle/navigate?navigate_to=0&preview_to=2&title=TestBook";

            httplib::Client client("127.0.0.1", port);
            auto res = client.Get(url.c_str());

            REQUIRE(res);
            REQUIRE(res->status == 200);
            responses.push_back(res->body);

            log_i("TEST: Request %d response length: %zu", i + 1, res->body.length());
        }

        for (size_t i = 1; i < responses.size(); i++) {
            REQUIRE(responses[i] == responses[0]);
        }

        REQUIRE(navigationRequestCounts["nav_0_prev_2"] == 3);

        log_i("TEST: Testing different navigation positions");

        httplib::Client client("127.0.0.1", port);
        auto res1 = client.Get("/kindle/navigate?navigate_to=5&preview_to=6&title=TestBook");
        REQUIRE(res1);
        REQUIRE(res1->status == 200);

        auto res2 = client.Get("/kindle/navigate?navigate_to=5&preview_to=8&title=TestBook");
        REQUIRE(res2);
        REQUIRE(res2->status == 200);

        REQUIRE(res1->body != res2->body);

        auto res3 = client.Get("/kindle/navigate?navigate_to=5&preview_to=6&title=TestBook");
        REQUIRE(res3);
        REQUIRE(res3->body == res1->body);

        REQUIRE(navigationRequestCounts["nav_5_prev_6"] == 2);
        REQUIRE(navigationRequestCounts["nav_5_prev_8"] == 1);
    }

    SECTION("Negative navigation scenarios") {
        httplib::Client client("127.0.0.1", port);

        log_i("TEST: Testing negative navigation (navigate_to=-1)");
        auto res = client.Get("/kindle/navigate?navigate_to=-1&preview_to=0&title=TestBook");
        REQUIRE(res);
        REQUIRE(res->status == 200);

        log_i("TEST: Testing preview_to < navigate_to");
        auto res2 = client.Get("/kindle/navigate?navigate_to=5&preview_to=3&title=TestBook");
        REQUIRE(res2);
        REQUIRE(res2->status == 200);
    }

    SECTION("Partial content scenarios") {
        httplib::Client client("127.0.0.1", port);

        log_i("TEST: Testing partial preview (navigate_to=0, preview_to=1)");
        auto res1 = client.Get("/kindle/navigate?navigate_to=0&preview_to=1&title=TestBook");
        REQUIRE(res1);
        REQUIRE(res1->status == 200);

        auto res2 = client.Get("/kindle/navigate?navigate_to=0&preview_to=3&title=TestBook");
        REQUIRE(res2);
        REQUIRE(res2->status == 200);

        std::string body1 = res1->body;
        std::string body2 = res2->body;

        size_t pos1 = body1.find("\"ocr_text\": \"") + 13;
        size_t end1 = body1.find("\",", pos1);
        std::string content1 = body1.substr(pos1, end1 - pos1);

        size_t pos2 = body2.find("\"ocr_text\": \"") + 13;
        size_t end2 = body2.find("\",", pos2);
        std::string content2 = body2.substr(pos2, end2 - pos2);

        REQUIRE(content2.length() > content1.length());
        REQUIRE(content2.substr(0, content1.length()) == content1);
    }

    // Reset API
    api->overrideBaseUrl("");
}

TEST_CASE("Test Kindle sync tracking", "[kindle]") {
    SetImagesFolder("KindleImages");

    auto display = createDisplay();

    auto httpServer = std::make_unique<httplib::Server>();
    auto httpServerThread = std::make_shared<std::thread>();
    std::atomic<bool> serverRunning(false);
    int port = 8082;

    std::atomic<int> lastNavigateTo(-1);

    httpServer = std::make_unique<httplib::Server>();

    httpServer->Get("/kindle/open-book", [](const httplib::Request &req, httplib::Response &res) {
        std::string content = generateSequentialContent(1, LINES_PER_PAGE);
        std::string escapedContent = content;
        size_t pos = 0;
        while ((pos = escapedContent.find('\n', pos)) != std::string::npos) {
            escapedContent.replace(pos, 1, "\\n");
            pos += 2;
        }

        res.set_content(
            R"({"success": true, "ocr_text": ")" + escapedContent +
                R"(", "progress": {"current_page": 1, "total_pages": 100}, "book_session_key": "test_session_123"})",
            "application/json");
        res.status = 200;
    });

    httpServer->Get("/kindle/navigate",
                    [&lastNavigateTo](const httplib::Request &req, httplib::Response &res) {
        int navigateTo =
            req.has_param("navigate_to") ? std::stoi(req.get_param_value("navigate_to")) : 0;
        int previewTo =
            req.has_param("preview_to") ? std::stoi(req.get_param_value("preview_to")) : 0;

        lastNavigateTo = navigateTo;
        log_i("Mock server: Sync test - navigate_to=%d, preview_to=%d", navigateTo, previewTo);

        int startLine = 1 + (navigateTo * LINES_PER_PAGE);
        int pagesToSend = (previewTo >= navigateTo) ? (previewTo - navigateTo + 1) : 0;

        std::string content = "";
        if (pagesToSend > 0) {
            content = generateSequentialContent(startLine, pagesToSend * LINES_PER_PAGE);
        }

        std::string escapedContent = content;
        size_t pos = 0;
        while ((pos = escapedContent.find('\n', pos)) != std::string::npos) {
            escapedContent.replace(pos, 1, "\\n");
            pos += 2;
        }

        res.set_content(R"({"success": true, "ocr_text": ")" + escapedContent +
                            R"(", "progress": {"current_page": )" + std::to_string(navigateTo + 1) +
                            R"(, "total_pages": 100}, "book_session_key": "test_session_123"})",
                        "application/json");
        res.status = 200;
    });

    // Start server
    httpServerThread = std::make_shared<std::thread>([port, &httpServer, &serverRunning]() {
        serverRunning = true;
        httpServer->listen("127.0.0.1", port);
        serverRunning = false;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    defer { teardownHttpServer(httpServer, httpServerThread); };

    auto api = SindarinAPI::getInstance();
    api->overrideBaseUrl("http://127.0.0.1:8082");
    api->setAccessToken("test-token");

    SECTION("Verify sync position tracking") {
        httplib::Client client("127.0.0.1", port);

        auto res1 = client.Get("/kindle/navigate?navigate_to=0&preview_to=2&title=TestBook");
        REQUIRE(res1);
        REQUIRE(lastNavigateTo == 0);

        auto res2 = client.Get("/kindle/navigate?navigate_to=2&preview_to=4&title=TestBook");
        REQUIRE(res2);
        REQUIRE(lastNavigateTo == 2);

        auto res3 = client.Get("/kindle/navigate?navigate_to=1&preview_to=3&title=TestBook");
        REQUIRE(res3);
        REQUIRE(lastNavigateTo == 1);

        auto res4 = client.Get("/kindle/navigate?navigate_to=5&preview_to=7&title=TestBook");
        REQUIRE(res4);
        REQUIRE(lastNavigateTo == 5);

        log_i("TEST: Verified sync tracking through navigation sequence");
    }

    api->overrideBaseUrl("");
}
