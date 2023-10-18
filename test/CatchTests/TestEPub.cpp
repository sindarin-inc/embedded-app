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

#include <sstream>

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

        ASSERT_VIEW_UNCHANGED("OnePageTutorial");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();

        for (int i = 0; i < 20; i++) {
            std::stringstream fname;
            fname << "Book1/" << i;

            ASSERT_VIEW_UNCHANGED(fname.str());

            SendRemoteCommand(ENTER);
            WAIT_FOR_DRAW();
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

        // ASSERT_VIEW_UNCHANGED("OnePageTutorial");
        // SendRemoteCommand(ENTER);
        // WAIT_FOR_DRAW();

        for (int i = 0; i < 20; i++) {
            std::stringstream fname;
            fname << "Book2/" << i;

            ASSERT_VIEW_UNCHANGED(fname.str());

            SendRemoteCommand(ENTER);
            WAIT_FOR_DRAW();
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

        // // SECTION("Book 3") {

        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW();

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW();

        // ASSERT_VIEW_UNCHANGED("OnePageTutorial");
        // SendRemoteCommand(ENTER);
        // WAIT_FOR_DRAW();

        for (int i = 0; i < 20; i++) {
            std::stringstream fname;
            fname << "Book3/" << i;

            ASSERT_VIEW_UNCHANGED(fname.str());

            SendRemoteCommand(ENTER);
            WAIT_FOR_DRAW();
        }
    }

    StopFreeRtos();
}

#endif