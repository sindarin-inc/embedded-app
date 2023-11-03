#include <catch_amalgamated.hpp>

// Allow access to protected methods from tests
#define protected public
#define private public
#include "Misc/BMPImage.hpp"
#include "Misc/Telemetry.hpp"
#include "Renderers/RendererDefs.hpp"
#include "Renderers/RendererStream.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "UI/BitmapScreen.hpp"
#include "UI/LayoutTestScreen.hpp"
#include "UI/LineSpacingScreen.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/PowerDepletedScreen.hpp"
#include "UI/ReadableScreen.hpp"
#include "UI/ReadableTableOfContentsScreen.hpp"
#include "UI/ReleaseNotesScreen.hpp"
#include "UI/ResizeBoxScreen.hpp"
#include "UI/Screen.hpp"
#include "ViewTestCommon.hpp"
#undef protected
#undef private

#include <sstream>

extern auto UIGetCurrentScreen() -> Screen *;

TEST_CASE("Test EPub Books Rendering", "[.][epub]") {

    SetupFileSystemMock("EPubTestData");
    SetImagesFolder("EPubImages");

    StartAppMain(FONT_CHOICES::PREFS_SOL_SMALL, 10, 10, -2);

    // First screen renders twice once with a full refresh and once with a AU4
    WAIT_FOR_DRAW();
    WAIT_FOR_DRAW();

    ASSERT_VIEW_UNCHANGED("FirstScreen");

    SECTION("Books") {
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        // WAIT_FOR_DRAW();

        auto screen = UIGetCurrentScreen();
        auto readableScreen = dynamic_cast<ReadableScreen *>(screen);

        if (readableScreen != nullptr) {

            ASSERT_VIEW_UNCHANGED("OnePageTutorial");
            SendRemoteCommand(ENTER);
            WAIT_FOR_DRAW();

            int pageNbr = 1;
            ChapterIdx chapterIdx = 0;
            auto epubFile = std::dynamic_pointer_cast<EPubFile>(readableScreen->getStream());

            if (epubFile != nullptr) {
                for (;;) {
                    StreamPlace streamPlace = readableScreen->getStreamPlace();
                    ChapterIdx idx = epubFile->getChapterIdx(streamPlace.getSpineIdx());
                    if (idx != chapterIdx) {
                        chapterIdx = idx;
                        pageNbr = 1;
                    }

                    std::stringstream fname;
                    fname << chapterIdx + 1 << "-" << pageNbr++;

                    ASSERT_VIEW_UNCHANGED(fname.str());

                    if (readableScreen->checkAtEnd()) break;

                    SendRemoteCommand(ENTER);
                    WAIT_FOR_DRAW();
                }
            }

        } else {
            UNSCOPED_INFO("Unable to get ReadableScreen instance for Book1.");
            CHECK(false);
        }

        SendRemoteCommand(UP);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();

        ASSERT_VIEW_UNCHANGED("MainMenu");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();

        ASSERT_VIEW_UNCHANGED("SecondScreen");

#if 0
        // The following is kept as it would be useful if other books are integrated to the test.
        // }

        // SECTION("Book 2") {

        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW();

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();

        screen = UIGetCurrentScreen();
        readableScreen = dynamic_cast<ReadableScreen *>(screen);

        if (readableScreen != nullptr) {

            // ASSERT_VIEW_UNCHANGED("OnePageTutorial");
            // SendRemoteCommand(ENTER);
            // WAIT_FOR_DRAW();

            int pageNbr = 1;
            uint16_t spineIdx = 0;

            for (;;) {
                StreamPlace streamPlace = readableScreen->getStreamPlace();

                if (streamPlace.getSpineIdx() != spineIdx) {
                    spineIdx = streamPlace.getSpineIdx();
                    pageNbr = 1;
                }

                std::stringstream fname;
                fname << "Book2/" << spineIdx + 1 << "-" << pageNbr++;

                ASSERT_VIEW_UNCHANGED(fname.str());

                if (readableScreen->checkAtEnd()) break;

                SendRemoteCommand(ENTER);
                WAIT_FOR_DRAW();
            }

        } else {
            UNSCOPED_INFO("Unable to get ReadableScreen instance for Book2.");
            CHECK(false);
        }

        SendRemoteCommand(UP);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();

        ASSERT_VIEW_UNCHANGED("MainMenu");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();
        WAIT_FOR_DRAW();

        ASSERT_VIEW_UNCHANGED("ThirdScreen");

        // }

        // SECTION("Book 3") {

        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW();

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();

        screen = UIGetCurrentScreen();
        readableScreen = dynamic_cast<ReadableScreen *>(screen);

        if (readableScreen != nullptr) {

            // ASSERT_VIEW_UNCHANGED("OnePageTutorial");
            // SendRemoteCommand(ENTER);
            // WAIT_FOR_DRAW();

            int pageNbr = 1;
            uint16_t spineIdx = 0;

            for (;;) {
                StreamPlace streamPlace = readableScreen->getStreamPlace();

                if (streamPlace.getSpineIdx() != spineIdx) {
                    spineIdx = streamPlace.getSpineIdx();
                    pageNbr = 1;
                }

                std::stringstream fname;
                fname << "Book3/" << spineIdx + 1 << "-" << pageNbr++;

                ASSERT_VIEW_UNCHANGED(fname.str());

                if (readableScreen->checkAtEnd()) break;

                SendRemoteCommand(ENTER);
                WAIT_FOR_DRAW();
            }
        } else {
            UNSCOPED_INFO("Unable to get ReadableScreen instance for Book3.");
            CHECK(false);
        }
#endif
    }

    StopFreeRtos();
}
