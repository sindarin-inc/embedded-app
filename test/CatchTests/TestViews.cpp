#include <catch_amalgamated.hpp>

// Allow access to protected methods from tests
#define protected public
#define private public
#include "Communications/USBSerial.hpp"
#include "Misc/BMPImage.hpp"
#include "Misc/Defer.hpp"
#include "Misc/Telemetry.hpp"
#include "Renderers/RendererStream.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "UI/AboutScreen.hpp"
#include "UI/BitmapScreen.hpp"
#include "UI/ContentSyncScreen.hpp"
#include "UI/DemoScreen.hpp"
#include "UI/LayoutTestScreen.hpp"
#include "UI/LegalScreen.hpp"
#include "UI/LineSpacingScreen.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/PowerDepletedScreen.hpp"
#include "UI/ReadableScreen.hpp"
#include "UI/ReadableTableOfContentsScreen.hpp"
#include "UI/ReleaseNotesScreen.hpp"
#include "UI/ResizeBoxScreen.hpp"
#include "ViewTestCommon.hpp"
#include "sindarin-debug.h"
#undef protected
#undef private

extern Font fontFace0;
extern Font fontFace1;
extern Font fontFace2;

TEST_CASE("First few screens render correctly and handle remote commands") {

    SetupFileSystemMock("TestData");
    SetImagesFolder("Images");

    StartAppMain();
    defer { StopFreeRtos(); };

    auto &display = GetRootDisplay();

    // First screen renders twice once with a full refresh and once with a AU4
    WAIT_FOR_DRAW(display);
    WAIT_FOR_DRAW(display);

    ASSERT_VIEW_UNCHANGED(display, "FirstScreen");

    // TODO: Currently there's a problem with starting and stopping the scheduler that makes the
    // tests lock up with multiple SECTIONs, otherwise we should split up individual test flows into
    // multiple sections so that all the state resets between tests.
    SECTION("Render a bunch of views") {
        // Send a remote command for left (for Settings)
        SendRemoteCommand(LEFT);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);

        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(display);

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);

        ASSERT_VIEW_UNCHANGED(display, "ContentSettings");

        SendRemoteCommand(LEFT);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);

        // Go back to the main screen
        SendRemoteCommand(RIGHT);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);

        // Render a bunch of readable pages
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);

        ASSERT_VIEW_UNCHANGED(display, "ReadableIntro");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "LastQuestion1");

        // Check to make sure going forward from page 1 and then back
        // results in the same page.
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(LEFT);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "LastQuestion1");

        // This is the new default way to get a bookmark on a page
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(RIGHT);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "LastQuestion1Bookmarked");

        // Render the popup menu to confirm page 1
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "PopupMenu1");

        // Return to book on page 1
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);

        // Start paging through the book
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "LastQuestion2");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "LastQuestion3");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "LastQuestion4");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "LastQuestion5");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "LastQuestion6");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "LastQuestion7");

        // Navigate to seek in the popup menu
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(LEFT);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "PopupMenu2");

        // Seek to the end of the book, then confirm page number
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(DOWN); // 1% granularity
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(DOWN); // 0.1% granularity
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(DOWN); // 10% granularity
        WAIT_FOR_DRAW(display);
        for (int i = 0; i < 10; i++) {
            SendRemoteCommand(RIGHT);
            WAIT_FOR_DRAW(display);
        }
        ASSERT_VIEW_UNCHANGED(display, "SeekingAround");
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(UP); // Show popup menu and we are on seek item still
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "PopupMenu3");

        // Go back to the main menu
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);

        // Go to the second readable (which has a TOC)
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(display);

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);

        // Render the popup menu
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);

        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(display);

        // Open the TOC
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);

        for (int i = 0; i < 6; i++) {
            SendRemoteCommand(DOWN);
            WAIT_FOR_DRAW(display);
        }

        ASSERT_VIEW_UNCHANGED(display, "PopupTOC");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);

        ASSERT_VIEW_UNCHANGED(display, "Dawn");

        // Render the popup menu, navigate to the dictionary
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(RIGHT);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(display);
        // Should show the updated seeked spot from the TOC
        ASSERT_VIEW_UNCHANGED(display, "PopupMenu4");

        // Open the dictionary
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "Dictionary1");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "Dictionary2");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "Dawn1");

        // Render the popup menu, navigate to Contents
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(LEFT);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "PopupMenu5");

        // Open the next chapter to ensure the dictionary shows something different
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "DawnToC");
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "DawnToC2");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);

        ASSERT_VIEW_UNCHANGED(display, "Dawn2");

        // Render the popup menu
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(RIGHT);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);

        // Open the dictionary
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "Dictionary3");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "Dictionary4");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "Dictionary5");
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "Dawn3");

        // Render the popup menu and go to the summary
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "PopupMenu6");

        // Open the summary
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "Summary1");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "Summary2");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "Summary3");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "Summary4");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "Summary5");
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "Summary6");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "Summary7");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "Summary8");
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "Dawn4");

        // Render the popup menu and go to placemark history
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "PopupMenu7");

        // Open the placemark history
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "PlacemarkHistory1");
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(display);
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        ASSERT_VIEW_UNCHANGED(display, "Dawn5");
    }
}

TEST_CASE("Test some views independently") {
    auto display = createDisplay();

    SECTION("Test PowerDepletedScreen") {
        auto screen = std::make_shared<PowerDepletedScreen>();
        screen->render(*display);
        ASSERT_VIEW_UNCHANGED(display, "PowerDepletedScreen");
    }

    SECTION("Test LayoutTestScreen") {
        auto screen = std::make_shared<LayoutTestScreen>(*display);
        screen->render(*display);
        ASSERT_VIEW_UNCHANGED(display, "LayoutTestScreen");
    }

    SECTION("Test things that need prefs") {
        PreferencesStore prefs;
        prefs.begin("./test/CatchTests/TestData/simprefs.ini");
        prefs.invertSetting.remove();
        prefs.font.remove();
        prefs.lineSpacing.remove();
        prefs.textInsetLeft.remove();
        prefs.textInsetRight.remove();
        prefs.textInsetTop.remove();
        prefs.textInsetBottom.remove();
        PosixFileSystem fileSystem("./test/CatchTests/TestData");
        Telemetry telemetry = Telemetry(fileSystem, prefs);
        USBSerial usbSerial;

        SECTION("Test things that need power") {
            LightingController lightingController(prefs);
            PowerManager power(fileSystem, prefs, lightingController, *display, telemetry,
                               usbSerial);

            SECTION("Test AboutScreen") {
                auto screen = std::make_shared<AboutScreen>(*display, prefs, power);
                screen->render(*display);
                ASSERT_VIEW_UNCHANGED(display, "AboutScreen");
            }

            SECTION("Test LegalScreen") {
                auto screen = std::make_shared<LegalScreen>(*display, prefs, power);
                screen->render(*display);
                ASSERT_VIEW_UNCHANGED(display, "LegalScreen1Before");
                screen->processCommand(LEFT);
                screen->render(*display);
                ASSERT_VIEW_UNCHANGED(display, "LegalScreen2Before");
                screen->processCommand(LEFT);
                screen->render(*display);
                ASSERT_VIEW_UNCHANGED(display, "LegalScreen1After");
                screen->processCommand(RIGHT);
                screen->render(*display);
                ASSERT_VIEW_UNCHANGED(display, "LegalScreen2After");
            }

            SECTION("Test ReleaseNotesScreen") {
                PosixFileSystem fs("./test/CatchTests/TestData/ReleaseNotesScreen/");
                Telemetry telemetry(fs, prefs);
                Battery battery(fs, prefs, telemetry);
                LocalStore localStore(fs, prefs, battery, telemetry);
                Firmware firmware;
                auto upgradeAvailable = localStore.getPendingFirmwareUpdate(firmware);
                REQUIRE(upgradeAvailable);
                auto screen = std::make_shared<ReleaseNotesScreen>(*display, prefs, power,
                                                                   localStore, firmware);
                screen->render(*display);
                ASSERT_VIEW_UNCHANGED(display, "ReleaseNotesScreen");
            }

            SECTION("Test ContentSyncScreen") {
                Telemetry telemetry(fileSystem, prefs);
                Battery battery(fileSystem, prefs, telemetry);
                LocalStore localStore(fileSystem, prefs, battery, telemetry);
                auto screen = std::make_shared<ContentSyncScreen>(*display, localStore, prefs,
                                                                  telemetry, power);
                screen->render(*display);
                ASSERT_VIEW_UNCHANGED(display, "ContentSyncScreen1");

                screen->state_ = ContentSyncState{.state = ContentSyncState::State::SyncingArticles,
                                                  .percentage = 63};
                screen->render(*display);
                ASSERT_VIEW_UNCHANGED(display, "ContentSyncScreen2");

                screen->state_ = ContentSyncState{.state = ContentSyncState::State::Synced,
                                                  .booksDownloaded = 3,
                                                  .articlesDownloaded = 10};
                screen->render(*display);
                ASSERT_VIEW_UNCHANGED(display, "ContentSyncScreen3");
            }
        }

        SECTION("Test OffsetAdjustScreen") {
            auto screen = std::make_shared<OffsetAdjustScreen>(*display, prefs, telemetry);
            screen->render(*display);
            ASSERT_VIEW_UNCHANGED(display, "OffsetAdjustScreen");
        }

        SECTION("Test ResizeBoxScreen") {
            auto screen = std::make_shared<ResizeBoxScreen>(*display, prefs, telemetry);
            screen->render(*display);
            ASSERT_VIEW_UNCHANGED(display, "ResizeBoxScreen");
        }

        SECTION("Test ResizeBoxScreen with pinned left") {
            auto offsetScreen = std::make_shared<OffsetAdjustScreen>(*display, prefs, telemetry);
            offsetScreen->render(*display);
            offsetScreen->processCommand(LEFT);
            offsetScreen->processCommand(LEFT);
            offsetScreen->processCommand(LEFT);
            offsetScreen->processCommand(LEFT);
            offsetScreen->processCommand(LEFT);
            offsetScreen->processCommand(LEFT);
            offsetScreen->processCommand(LEFT);
            // These are the extra lefts to make sure nothing changes on display
            offsetScreen->processCommand(LEFT);
            offsetScreen->processCommand(LEFT);
            // Save setting
            offsetScreen->processCommand(ENTER);
            auto resizeScreen = std::make_shared<ResizeBoxScreen>(*display, prefs, telemetry);
            resizeScreen->render(*display);
            resizeScreen->processCommand(LEFT);
            resizeScreen->processCommand(LEFT);
            resizeScreen->render(*display);
            ASSERT_VIEW_UNCHANGED(display, "ResizeBoxScreenLeftPinned");
        }

        SECTION("Test LineSpacingScreen") {
            auto screen = std::make_shared<LineSpacingScreen>(*display, prefs, telemetry);
            screen->render(*display);
            ASSERT_VIEW_UNCHANGED(display, "LineSpacingScreen");
            screen->processCommand(UP);
            screen->processCommand(UP);
            screen->processCommand(UP);
            screen->render(*display);
            ASSERT_VIEW_UNCHANGED(display, "LineSpacingScreenWide");
            screen->processCommand(DOWN);
            screen->processCommand(DOWN);
            screen->processCommand(DOWN);
            screen->render(*display);
            ASSERT_VIEW_UNCHANGED(display, "LineSpacingScreen");
        }

        SECTION("Test ReadableTableOfContentsScreen") {
            auto readable = std::make_shared<Readable>();
            auto readablesJsonDoc = LoadJsonDoc(fileSystem, "/readables.json");
            auto prefs = PreferencesStore();
            readable->fromJson((*readablesJsonDoc)["readables"][1]);

            Renderer renderer(*display, display->defaultTextRect(),
                              static_cast<Font *>(&fontFace0));
            auto [path, size] = LocalStore::readableFilePathForReading(fileSystem, readable);
            auto stream = fileSystem.newEPubRendererStream(path.c_str(), readable);
            renderer.setStream(stream);
            auto screen = std::make_shared<ReadableTableOfContentsScreen>(prefs, readable, renderer,
                                                                          *display, telemetry);
            screen->loadMenuItems();

            screen->render(*display);
            ASSERT_VIEW_UNCHANGED(display, "ReadableTableOfContentsScreen");
        }

        LightingController lightingController(prefs);

        SECTION("Test Last Question in CES mode") {
            Battery battery(fileSystem, prefs, telemetry);
            LocalStore localStore(fileSystem, prefs, battery, telemetry);
            prefs.cesMode.set(true);

            auto readable = std::make_shared<Readable>();
            auto readablesJsonDoc = LoadJsonDoc(fileSystem, "/readables.json");
            readable->fromJson((*readablesJsonDoc)["readables"][0]);

            Renderer renderer(*display, display->defaultTextRect(),
                              static_cast<Font *>(&fontFace0));
            auto [path, size] = LocalStore::readableFilePathForReading(fileSystem, readable);
            auto stream = fileSystem.newGzipRendererStream(path.c_str());
            renderer.setStream(stream);

            auto readableScreen = std::make_shared<ReadableScreen>(
                stream, *display, readable, prefs, telemetry, fileSystem, lightingController,
                localStore, static_cast<Font *>(&fontFace0));
            readableScreen->render(*display);
            ASSERT_VIEW_UNCHANGED(display, "CesMode");
        }

        SECTION("Test EPUB image rendering with alternate bounding box") {
            Battery battery(fileSystem, prefs, telemetry);
            LocalStore localStore(fileSystem, prefs, battery, telemetry);
            auto readable = std::make_shared<Readable>();
            auto readablesJsonDoc = LoadJsonDoc(fileSystem, "/readables-lq-epub.json");
            readable->fromJson((*readablesJsonDoc)["readables"][0]);

            Renderer renderer(*display, {10, 20, 236, 236}, static_cast<Font *>(&fontFace0));
            auto [path, size] = LocalStore::readableFilePathForReading(fileSystem, readable);
            auto stream = fileSystem.newEPubRendererStream(path.c_str(), readable);
            renderer.setStream(stream);

            auto readableScreen = std::make_shared<ReadableScreen>(
                stream, *display, readable, prefs, telemetry, fileSystem, lightingController,
                localStore, static_cast<Font *>(&fontFace0));
            readableScreen->render(*display);
            ASSERT_VIEW_UNCHANGED(display, "EpubImage");
        }

        SECTION("Test DemoScreen") {
            auto screen = std::make_shared<DemoScreen>(*display, prefs);
            screen->render(*display);
            ASSERT_VIEW_UNCHANGED(display, "DemoScreen");
        }

        SECTION("Test ContentSyncScreen") {
            PosixFileSystem fs("./test/CatchTests/TestData/ReleaseNotesScreen/");
            Telemetry telemetry(fs, prefs);
            Battery battery(fs, prefs, telemetry);
            LightingController lightingController(prefs);
            PowerManager power(fs, prefs, lightingController, *display, telemetry, usbSerial);
            LocalStore localStore(fs, prefs, battery, telemetry);

            ContentSyncScreen screen(*display, localStore, prefs, telemetry, power);
            screen.render(*display);
            ASSERT_VIEW_UNCHANGED(display, "ContentSyncScreen1");

            screen.state_ = ContentSyncState{.state = ContentSyncState::State::SyncingArticles,
                                             .percentage = 63};
            screen.render(*display);
            ASSERT_VIEW_UNCHANGED(display, "ContentSyncScreen2");

            screen.state_ = ContentSyncState{.state = ContentSyncState::State::Synced,
                                             .booksDownloaded = 3,
                                             .articlesDownloaded = 10};
            screen.render(*display);
            ASSERT_VIEW_UNCHANGED(display, "ContentSyncScreen3");
        }
    }
}

TEST_CASE("Test bmp file rendering") {
    PosixFileSystem fs("./test/CatchTests/TestData");
    fs.begin();
    // create and render a full screen bitmap view
    auto display = createDisplay();

    SECTION("Test bitmap view Books (grayscale)") {
        SFile file = fs.open("/books.bmp");
        ContainerStream<SFile> stream(file);
        BMPImage image;
        image.readFromStream(stream);

        auto screen =
            std::make_shared<BitmapScreen>(image.getBuffer(), image.getWidth(), image.getHeight());
        screen->render(*display);
        ASSERT_VIEW_UNCHANGED(display, "BitmapViewBooks");
    }

    SECTION("Test bitmap view Books (pre-dithered)") {
        SFile file = fs.open("/books_dithered.bmp");
        ContainerStream<SFile> stream(file);
        BMPImage image;
        image.readFromStream(stream);

        auto screen =
            std::make_shared<BitmapScreen>(image.getBuffer(), image.getWidth(), image.getHeight());
        screen->render(*display);
        ASSERT_VIEW_UNCHANGED(display, "BitmapViewBooks");
    }
}
