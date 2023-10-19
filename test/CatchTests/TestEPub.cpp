#if 0

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

TEST_CASE("Test EPub Books Rendering") {

    SetupFileSystemMock("EPubTestData");
    SetImagesFolder("EPubImages");

    StartFreeRtos(true, FONT_CHOICES::PREFS_SOL_SMALL, 10, 10, -2);

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
            uint16_t fileIdx = 0;

            for (;;) {
                StreamPlace streamPlace = readableScreen->getStreamPlace();

                if (streamPlace.getFileIdx() != fileIdx) {
                    fileIdx = streamPlace.getFileIdx();
                    pageNbr = 1;
                }

                std::stringstream fname;
                fname << "Book1/" << fileIdx + 1 << "-" << pageNbr++;

                ASSERT_VIEW_UNCHANGED(fname.str());

                if (readableScreen->checkAtEnd()) break;

                SendRemoteCommand(ENTER);
                WAIT_FOR_DRAW();
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
            uint16_t fileIdx = 0;

            for (;;) {
                StreamPlace streamPlace = readableScreen->getStreamPlace();

                if (streamPlace.getFileIdx() != fileIdx) {
                    fileIdx = streamPlace.getFileIdx();
                    pageNbr = 1;
                }

                std::stringstream fname;
                fname << "Book2/" << fileIdx + 1 << "-" << pageNbr++;

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
            uint16_t fileIdx = 0;

            for (;;) {
                StreamPlace streamPlace = readableScreen->getStreamPlace();

                if (streamPlace.getFileIdx() != fileIdx) {
                    fileIdx = streamPlace.getFileIdx();
                    pageNbr = 1;
                }

                std::stringstream fname;
                fname << "Book3/" << fileIdx + 1 << "-" << pageNbr++;

                ASSERT_VIEW_UNCHANGED(fname.str());

                if (readableScreen->checkAtEnd()) break;

                SendRemoteCommand(ENTER);
                WAIT_FOR_DRAW();
            }
        } else {
            UNSCOPED_INFO("Unable to get ReadableScreen instance for Book3.");
            CHECK(false);
        }
    }

    StopFreeRtos();
}

#endif