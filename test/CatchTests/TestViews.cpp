#include <catch_amalgamated.hpp>

// Allow access to protected methods from tests
#define protected public
#define private public
#include "Misc/BMPImage.hpp"
#include "Misc/Defer.hpp"
#include "Misc/Telemetry.hpp"
#include "Renderers/RendererStream.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "UI/BitmapScreen.hpp"
#include "UI/DemoScreen.hpp"
#include "UI/LayoutTestScreen.hpp"
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

TEST_CASE("First few screens render correctly and handle remote commands") {

    SetupFileSystemMock("TestData");
    SetImagesFolder("Images");

    StartAppMain();
    defer { StopFreeRtos(); };

    // First screen renders twice once with a full refresh and once with a AU4
    WAIT_FOR_DRAW();
    WAIT_FOR_DRAW();

    ASSERT_VIEW_UNCHANGED("FirstScreen");

    // TODO: Currently there's a problem with starting and stopping the scheduler that makes the
    // tests lock up with multiple SECTIONs, otherwise we should split up individual test flows into
    // multiple sections so that all the state resets between tests.
    SECTION("Render a bunch of views") {
        // Send a remote command for left (for Settings)
        SendRemoteCommand(LEFT);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();

        ASSERT_VIEW_UNCHANGED("ContentSettings");

        SendRemoteCommand(LEFT);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();

        // Go back to the main screen
        SendRemoteCommand(RIGHT);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();

        // Render a bunch of readable pages
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();

        ASSERT_VIEW_UNCHANGED("ReadableIntro");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("LastQuestion1");

        // Check to make sure going forward from page 1 and then back
        // results in the same page.
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        SendRemoteCommand(LEFT);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("LastQuestion1");

        // This is the new default way to get a bookmark on a page
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW();
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW();
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("LastQuestion1Bookmarked");

        // Render the popup menu to confirm page 1
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("PopupMenu1");

        // Return to book on page 1, then start paging through the book
        SendRemoteCommand(LEFT);
        WAIT_FOR_DRAW();
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("LastQuestion2");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("LastQuestion3");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("LastQuestion4");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("LastQuestion5");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("LastQuestion6");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("LastQuestion7");

        // Render the popup menu
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("PopupMenu2");

        // Seek to the end of the book, then confirm page number
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        SendRemoteCommand(DOWN); // 1% granularity
        WAIT_FOR_DRAW();
        SendRemoteCommand(DOWN); // 0.1% granularity
        WAIT_FOR_DRAW();
        SendRemoteCommand(DOWN); // 10% granularity
        WAIT_FOR_DRAW();
        for (int i = 0; i < 10; i++) {
            SendRemoteCommand(RIGHT);
            WAIT_FOR_DRAW();
        }
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW();
        SendRemoteCommand(UP); // Show popup menu
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("PopupMenu3");

        // Go back to the main menu
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();

        // Go to the second readable (which has a TOC)
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW();

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();

        // Render the popup menu
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();

        for (int i = 0; i < 6; i++) {
            SendRemoteCommand(DOWN);
            WAIT_FOR_DRAW();
        }

        ASSERT_VIEW_UNCHANGED("PopupTOC");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();

        ASSERT_VIEW_UNCHANGED("Dawn");

        // Render the popup menu
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("PopupMenu4"); // Should show the updated seeked spot from the TOC

        // Open the dictionary
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW();
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW();
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW();
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Dictionary1");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Dictionary2");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Dictionary3");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Dawn");

        // Render the popup menu
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("PopupMenu4");

        // Open the next chapter to ensure the dictionary shows something different
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("DawnToC");
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("DawnToC2");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();

        ASSERT_VIEW_UNCHANGED("Dawn2");

        // Render the popup menu
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("PopupMenu5");

        // Open the dictionary
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW();
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW();
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW();
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Dictionary4");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Dictionary5");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Dictionary6");
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Dawn3");

        // Render the popup menu
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("PopupMenu5");

        // Open the summary
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW();
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW();
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW();
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW();
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Summary1");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Summary2");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Summary3");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Summary4");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Summary5");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Summary6");
        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Summary7");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Summary8");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Summary9");
        SendRemoteCommand(UP);
        WAIT_FOR_DRAW();
        ASSERT_VIEW_UNCHANGED("Dawn4");
    }
}

TEST_CASE("Test some views independently") {
    // Just create and render a PowerDepletedScreen
    auto display = createDisplay();

    FakeEinkET013TT1SkipSignalling = true;
    FakeEinkET013TT1SkipWaveform = true;
    defer {
        FakeEinkET013TT1SkipWaveform = false;
        FakeEinkET013TT1SkipSignalling = false;
    };

    SECTION("Test PowerDepletedScreen") {
        PowerDepletedScreen screen;
        screen.render(*display);
        ASSERT_VIEW_UNCHANGED("PowerDepletedScreen");
    }

    /* With the main menu refactor, this test hangs and does not complete. */
    SECTION("Test LayoutTestScreen") {
        LayoutTestScreen screen(*display);
        screen.render(*display);
        ASSERT_VIEW_UNCHANGED("LayoutTestScreen");
    }

    SECTION("Test things that need prefs") {
        PreferencesStore prefs;
        prefs.begin("./test/CatchTests/TestData/simprefs.ini");
        prefs.invertSetting.remove();
        prefs.font.remove();
        prefs.lineSpacing.remove();
        prefs.rectX.remove();
        prefs.rectY.remove();
        prefs.rectW.remove();
        prefs.rectH.remove();
        prefs.noSetupMode.set(true);
        PosixFileSystem fileSystem("./test/CatchTests/TestData");
        Telemetry telemetry = Telemetry(fileSystem, prefs);

        SECTION("Test OffsetAdjustScreen") {
            OffsetAdjustScreen screen(*display, prefs, telemetry);
            screen.render(*display);
            ASSERT_VIEW_UNCHANGED("OffsetAdjustScreen");
        }

        SECTION("Test ResizeBoxScreen") {
            ResizeBoxScreen screen(*display, prefs, telemetry);
            screen.render(*display);
            ASSERT_VIEW_UNCHANGED("ResizeBoxScreen");
        }

        SECTION("Test LineSpacingScreen") {
            LineSpacingScreen screen(*display, prefs, telemetry);
            screen.render(*display);
            ASSERT_VIEW_UNCHANGED("LineSpacingScreen");
        }

        SECTION("Test ReadableTableOfContentsScreen") {
            auto readable = std::make_shared<Readable>();
            auto readablesJsonDoc = LoadJsonDoc(fileSystem, "/readables.json");
            readable->fromJson((*readablesJsonDoc)["readables"][1]);

            Renderer renderer(*display);
            auto [path, size] = LocalStore::readableFilePathForReading(fileSystem, readable);
            auto stream = fileSystem.newEPubRendererStream(path.c_str(), readable);
            renderer.setStream(stream);
            ReadableTableOfContentsScreen screen(prefs, readable, renderer, *display, telemetry);
            screen.loadMenuItems();

            screen.render(*display);
            ASSERT_VIEW_UNCHANGED("ReadableTableOfContentsScreen");
        }

        LightingController lightingController;

        SECTION("Test Last Question in CES mode") {
            prefs.cesMode.set(true);

            auto readable = std::make_shared<Readable>();
            auto readablesJsonDoc = LoadJsonDoc(fileSystem, "/readables.json");
            readable->fromJson((*readablesJsonDoc)["readables"][0]);

            Renderer renderer(*display);
            auto [path, size] = LocalStore::readableFilePathForReading(fileSystem, readable);
            auto stream = fileSystem.newGzipRendererStream(path.c_str());
            renderer.setStream(stream);

            ReadableScreen readableScreen(stream, *display, readable, prefs, telemetry, fileSystem,
                                          lightingController);
            readableScreen.render(*display);
            ASSERT_VIEW_UNCHANGED("CesMode");
        }

        SECTION("Test EPUB image rendering with alternate bounding box") {
            display->setBoundingBox(DisplayRect{10, 20, 236, 236});

            auto readable = std::make_shared<Readable>();
            auto readablesJsonDoc = LoadJsonDoc(fileSystem, "/readables-lq-epub.json");
            readable->fromJson((*readablesJsonDoc)["readables"][0]);

            Renderer renderer(*display);
            auto [path, size] = LocalStore::readableFilePathForReading(fileSystem, readable);
            auto stream = fileSystem.newEPubRendererStream(path.c_str(), readable);
            renderer.setStream(stream);

            ReadableScreen readableScreen(stream, *display, readable, prefs, telemetry, fileSystem,
                                          lightingController);
            readableScreen.render(*display);
            ASSERT_VIEW_UNCHANGED("EpubImage");
        }

        SECTION("Test ReleaseNotesScreen") {
            PosixFileSystem fs("./test/CatchTests/TestData/ReleaseNotesScreen/");
            Telemetry telemetry(fs, prefs);
            Battery battery(fs, prefs, telemetry);
            LightingController lightingController;
            PowerManager power(fs, prefs, lightingController, *display, telemetry);
            LocalStore localStore(fs, prefs, battery, telemetry);

            (*display).style.setFontFamily((*display).style.solSansSmall);

            Firmware firmware;
            auto upgradeAvailable = localStore.getPendingFirmwareUpdate(firmware);
            REQUIRE(upgradeAvailable);
            ReleaseNotesScreen screen(*display, prefs, power, localStore, firmware);
            screen.render(*display);
            ASSERT_VIEW_UNCHANGED("ReleaseNotesScreen");
        }

        SECTION("Test DemoScreen") {
            LightingController lightingController;
            DemoScreen screen(*display, prefs, lightingController);
            screen.render(*display);
            ASSERT_VIEW_UNCHANGED("DemoScreen");
            screen.processCommand(UP);
            screen.render(*display);
            ASSERT_VIEW_UNCHANGED("DemoScreenSerial");
        }
    }
}

TEST_CASE("Test bmp file rendering") {
    PosixFileSystem fs("./test/CatchTests/TestData");
    fs.begin();
    // create and render a full screen bitmap view
    auto display = createDisplay();
    FakeEinkET013TT1SkipSignalling = true;
    FakeEinkET013TT1SkipWaveform = true;
    defer {
        FakeEinkET013TT1SkipWaveform = false;
        FakeEinkET013TT1SkipSignalling = false;
    };

    SECTION("Test bitmap view Books (grayscale)") {
        SFile file = fs.open("/books.bmp");
        ContainerStream<SFile> stream(file);
        BMPImage image;
        image.readFromStream(stream);

        BitmapScreen screen(image.getBuffer(), image.getWidth(), image.getHeight());
        screen.render(*display);
        ASSERT_VIEW_UNCHANGED("BitmapViewBooks");
    }

    SECTION("Test bitmap view Books (pre-dithered)") {
        SFile file = fs.open("/books_dithered.bmp");
        ContainerStream<SFile> stream(file);
        BMPImage image;
        image.readFromStream(stream);

        BitmapScreen screen(image.getBuffer(), image.getWidth(), image.getHeight());
        screen.render(*display);
        ASSERT_VIEW_UNCHANGED("BitmapViewBooks");
    }
}
