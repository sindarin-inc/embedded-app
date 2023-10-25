#if 1
#include <catch_amalgamated.hpp>

// Allow access to protected methods from tests
#define protected public
#define private public
#include "Misc/BMPImage.hpp"
#include "Misc/Telemetry.hpp"
#include "Renderers/RendererStream.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "UI/BitmapScreen.hpp"
#include "UI/LayoutTestScreen.hpp"
#include "UI/LineSpacingScreen.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/PowerDepletedScreen.hpp"
#include "UI/ReadableTableOfContentsScreen.hpp"
#include "UI/ReleaseNotesScreen.hpp"
#include "UI/ResizeBoxScreen.hpp"
#include "ViewTestCommon.hpp"
#undef protected
#undef private

TEST_CASE("First few screens render correctly and handle remote commands") {

    ViewTestCommon *viewTestCommon = new ViewTestCommon();

    viewTestCommon->SetupFileSystemMock("TestData");
    viewTestCommon->SetImagesFolder("Images");

    viewTestCommon->StartFreeRtos();

    // First screen renders twice once with a full refresh and once with a AU4
    WAIT_FOR_DRAW(viewTestCommon);
    WAIT_FOR_DRAW(viewTestCommon);

    ASSERT_VIEW_UNCHANGED(viewTestCommon, "FirstScreen");

    // TODO: Currently there's a problem with starting and stopping the scheduler that makes the
    // tests lock up with multiple SECTIONs, otherwise we should split up indivitual test flows into
    // multiple sections so that all the state resets between tests.
    SECTION("Render a bunch of views") {
        // Send a remote command for left (for Settings)
        viewTestCommon->SendRemoteCommand(LEFT);
        WAIT_FOR_DRAW(viewTestCommon);
        WAIT_FOR_DRAW(viewTestCommon);

        ASSERT_VIEW_UNCHANGED(viewTestCommon, "Settings");

        viewTestCommon->SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(viewTestCommon);

        viewTestCommon->SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(viewTestCommon);
        WAIT_FOR_DRAW(viewTestCommon);

        ASSERT_VIEW_UNCHANGED(viewTestCommon, "ContentSettings");

        viewTestCommon->SendRemoteCommand(LEFT);
        WAIT_FOR_DRAW(viewTestCommon);
        WAIT_FOR_DRAW(viewTestCommon);

        // Go back to the main screen
        viewTestCommon->SendRemoteCommand(RIGHT);
        WAIT_FOR_DRAW(viewTestCommon);
        WAIT_FOR_DRAW(viewTestCommon);

        // Render a bunch of readable pages
        viewTestCommon->SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(viewTestCommon);

        ASSERT_VIEW_UNCHANGED(viewTestCommon, "ReadableIntro");

        viewTestCommon->SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(viewTestCommon);
        ASSERT_VIEW_UNCHANGED(viewTestCommon, "LastQuestion1");

        // Check to make sure going forward from page 1 and then back
        // results in the same page.
        viewTestCommon->SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(viewTestCommon);
        viewTestCommon->SendRemoteCommand(LEFT);
        WAIT_FOR_DRAW(viewTestCommon);
        ASSERT_VIEW_UNCHANGED(viewTestCommon, "LastQuestion1");

        // This is the new default way to get a bookmark on a page
        viewTestCommon->SendRemoteCommand(UP);
        WAIT_FOR_DRAW(viewTestCommon);
        WAIT_FOR_DRAW(viewTestCommon);
        viewTestCommon->SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(viewTestCommon);
        viewTestCommon->SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(viewTestCommon);
        viewTestCommon->SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(viewTestCommon);
        ASSERT_VIEW_UNCHANGED(viewTestCommon, "LastQuestion1Bookmarked");

        // Render the popup menu to confirm page 1
        viewTestCommon->SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(viewTestCommon);
        WAIT_FOR_DRAW(viewTestCommon);
        ASSERT_VIEW_UNCHANGED(viewTestCommon, "PopupMenu1");

        // Return to book on page 1, then start paging through the book
        viewTestCommon->SendRemoteCommand(LEFT);
        WAIT_FOR_DRAW(viewTestCommon);
        viewTestCommon->SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(viewTestCommon);
        ASSERT_VIEW_UNCHANGED(viewTestCommon, "LastQuestion2");

        viewTestCommon->SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(viewTestCommon);
        ASSERT_VIEW_UNCHANGED(viewTestCommon, "LastQuestion3");

        viewTestCommon->SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(viewTestCommon);
        ASSERT_VIEW_UNCHANGED(viewTestCommon, "LastQuestion4");

        viewTestCommon->SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(viewTestCommon);
        ASSERT_VIEW_UNCHANGED(viewTestCommon, "LastQuestion5");

        viewTestCommon->SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(viewTestCommon);
        ASSERT_VIEW_UNCHANGED(viewTestCommon, "LastQuestion6");

        viewTestCommon->SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(viewTestCommon);
        ASSERT_VIEW_UNCHANGED(viewTestCommon, "LastQuestion7");

        // Render the popup menu
        viewTestCommon->SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(viewTestCommon);
        WAIT_FOR_DRAW(viewTestCommon);
        ASSERT_VIEW_UNCHANGED(viewTestCommon, "PopupMenu2");

        // Seek to the end of the book, then confirm page number
        viewTestCommon->SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(viewTestCommon);
        viewTestCommon->SendRemoteCommand(DOWN); // 1% granularity
        WAIT_FOR_DRAW(viewTestCommon);
        viewTestCommon->SendRemoteCommand(DOWN); // 0.1% granularity
        WAIT_FOR_DRAW(viewTestCommon);
        viewTestCommon->SendRemoteCommand(DOWN); // 10% granularity
        WAIT_FOR_DRAW(viewTestCommon);
        for (int i = 0; i < 10; i++) {
            viewTestCommon->SendRemoteCommand(RIGHT);
            WAIT_FOR_DRAW(viewTestCommon);
        }
        viewTestCommon->SendRemoteCommand(UP);
        WAIT_FOR_DRAW(viewTestCommon);
        viewTestCommon->SendRemoteCommand(UP); // Show popup menu
        WAIT_FOR_DRAW(viewTestCommon);
        WAIT_FOR_DRAW(viewTestCommon);
        ASSERT_VIEW_UNCHANGED(viewTestCommon, "PopupMenu3");

        // Go back to the main menu
        viewTestCommon->SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(viewTestCommon);
        WAIT_FOR_DRAW(viewTestCommon);

        // Go to the second readable (which has a TOC)
        viewTestCommon->SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(viewTestCommon);

        viewTestCommon->SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(viewTestCommon);

        // Render the popup menu
        viewTestCommon->SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(viewTestCommon);
        WAIT_FOR_DRAW(viewTestCommon);

        viewTestCommon->SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(viewTestCommon);
        WAIT_FOR_DRAW(viewTestCommon);

        for (int i = 0; i < 6; i++) {
            viewTestCommon->SendRemoteCommand(DOWN);
            WAIT_FOR_DRAW(viewTestCommon);
        }

        ASSERT_VIEW_UNCHANGED(viewTestCommon, "PopupTOC");

        viewTestCommon->SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(viewTestCommon);

        ASSERT_VIEW_UNCHANGED(viewTestCommon, "Dawn");

        // Render the popup menu
        viewTestCommon->SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(viewTestCommon);
        WAIT_FOR_DRAW(viewTestCommon);
        ASSERT_VIEW_UNCHANGED(viewTestCommon,
                              "PopupMenu4"); // Should show the updated seeked spot from the TOC
    }

    viewTestCommon->StopFreeRtos();
    delete viewTestCommon;
}

TEST_CASE("Test some views independently") {

    ViewTestCommon *viewTestCommon = new ViewTestCommon();

    // Just create and render a PowerDepletedScreen
    auto display = viewTestCommon->createDisplay();

    SECTION("Test PowerDepletedScreen") {
        PowerDepletedScreen screen;
        screen.render(*display);
        ASSERT_VIEW_UNCHANGED(viewTestCommon, "PowerDepletedScreen");
    }

    /* With the main menu refactor, this test hangs and does not complete. */
    SECTION("Test LayoutTestScreen") {
        LayoutTestScreen screen(*display);
        screen.render(*display);
        ASSERT_VIEW_UNCHANGED(viewTestCommon, "LayoutTestScreen");
    }

    SECTION("Test things that need prefs") {
        PreferencesStore prefs;
        prefs.begin();
        prefs.invertSetting.remove();
        prefs.font.remove();
        prefs.lineSpacing.remove();
        prefs.rectX.remove();
        prefs.rectY.remove();
        prefs.rectW.remove();
        prefs.rectH.remove();
        prefs.noSetupMode.set(true);
        PosixFileSystem fileSystem;
        Telemetry telemetry = Telemetry(fileSystem, prefs);

        SECTION("Test OffsetAdjustScreen") {
            OffsetAdjustScreen screen(*display, prefs, telemetry);
            screen.render(*display);
            ASSERT_VIEW_UNCHANGED(viewTestCommon, "OffsetAdjustScreen");
        }

        SECTION("Test ResizeBoxScreen") {
            ResizeBoxScreen screen(*display, prefs, telemetry);
            screen.render(*display);
            ASSERT_VIEW_UNCHANGED(viewTestCommon, "ResizeBoxScreen");
        }

        SECTION("Test LineSpacingScreen") {
            LineSpacingScreen screen(*display, prefs, telemetry);
            screen.render(*display);
            ASSERT_VIEW_UNCHANGED(viewTestCommon, "LineSpacingScreen");
        }

        SECTION("Test ReadableTableOfContentsScreen") {
            auto readable = std::make_shared<Readable>();
            auto readablesJsonDoc = viewTestCommon->LoadJsonDoc(fileSystem, "/readables.json");
            readable->fromJson((*readablesJsonDoc)["readables"][1]);

            Renderer renderer(*display);
            ReadableTableOfContentsScreen screen(prefs, readable, renderer, *display, telemetry);
            screen.loadMenuItems();

            // This view renders twice, once with a full refresh and once with a AU4 so we need to
            // not wait on the signaling otherwise we'll hang here.
            FakeEinkET013TT1SkipSignalling = true;
            screen.render(*display);
            FakeEinkET013TT1SkipSignalling = false;
            ASSERT_VIEW_UNCHANGED(viewTestCommon, "ReadableTableOfContentsScreen");
        }
    }

    delete viewTestCommon;
}

TEST_CASE("Test bmp file rendering") {

    ViewTestCommon *viewTestCommon = new ViewTestCommon();

    viewTestCommon->SetupFileSystemMock("TestData");

    PosixFileSystem fs;
    fs.begin();
    // create and render a full screen bitmap view
    auto display = viewTestCommon->createDisplay();

    SECTION("Test bitmap view Books (grayscale)") {
        SFile file = fs.open("/books.bmp");
        ContainerStream<SFile> stream(file);
        BMPImage image;
        image.readFromStream(stream);

        BitmapScreen screen(image.getBuffer(), image.getWidth(), image.getHeight());
        screen.render(*display);
        ASSERT_VIEW_UNCHANGED(viewTestCommon, "BitmapViewBooks");
    }

    SECTION("Test bitmap view Books (pre-dithered)") {
        SFile file = fs.open("/books_dithered.bmp");
        ContainerStream<SFile> stream(file);
        BMPImage image;
        image.readFromStream(stream);

        BitmapScreen screen(image.getBuffer(), image.getWidth(), image.getHeight());
        screen.render(*display);
        ASSERT_VIEW_UNCHANGED(viewTestCommon, "BitmapViewBooks");
    }

    delete viewTestCommon;
}

#endif