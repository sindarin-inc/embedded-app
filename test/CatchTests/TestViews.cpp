#include <catch_amalgamated.hpp>

#include "UI/LayoutTestScreen.hpp"
#include "UI/PowerDepletedScreen.hpp"
#include "ViewTestCommon.hpp"

TEST_CASE("First few screens render correctly and handle remote commands") {
    StartFreeRtos();

    SetupFileSystemMock("TestData");

    // First screen renders twice once with a full refresh and once with a AU4
    WaitForDraw();
    WaitForDraw();

    ASSERT_VIEW_UNCHANGED("FirstScreen");

    // TODO: Currently there's a problem with starting and stopping the scheduler that makes the
    // tests lock up with multiple SECTIONs, otherwise we should split up indivitual test flows into
    // multiple sections so that all the state resets between tests.
    SECTION("Render a bunch of views") {
        // Send a remote command for left (for Settings)
        SendRemoteCommand(CHANNEL_MINUS);
        WaitForDraw();
        WaitForDraw();

        ASSERT_VIEW_UNCHANGED("Settings");

        // Go back to the main screen
        SendRemoteCommand(CHANNEL_PLUS);
        WaitForDraw();
        WaitForDraw();

        // Render a bunch of readable pages
        SendRemoteCommand(ENTER);
        WaitForDraw();

        ASSERT_VIEW_UNCHANGED("ReadableIntro");

        SendRemoteCommand(ENTER);
        WaitForDraw();
        ASSERT_VIEW_UNCHANGED("LastQuestion1");

        SendRemoteCommand(CHANNEL_PLUS);
        WaitForDraw();
        ASSERT_VIEW_UNCHANGED("LastQuestion1Bookmarked");

        SendRemoteCommand(ENTER);
        WaitForDraw();
        ASSERT_VIEW_UNCHANGED("LastQuestion2");

        SendRemoteCommand(ENTER);
        WaitForDraw();
        ASSERT_VIEW_UNCHANGED("LastQuestion3");

        SendRemoteCommand(ENTER);
        WaitForDraw();
        ASSERT_VIEW_UNCHANGED("LastQuestion4");

        SendRemoteCommand(ENTER);
        WaitForDraw();
        ASSERT_VIEW_UNCHANGED("LastQuestion5");

        SendRemoteCommand(ENTER);
        WaitForDraw();
        ASSERT_VIEW_UNCHANGED("LastQuestion6");

        SendRemoteCommand(ENTER);
        WaitForDraw();
        ASSERT_VIEW_UNCHANGED("LastQuestion7");

        // Render the popup menu
        SendRemoteCommand(NEXT);
        WaitForDraw();
        ASSERT_VIEW_UNCHANGED("PopupMenu");
    }

    StopFreeRtos();
}

TEST_CASE("Test some views independently") {
    // Don't spin up the whole app (runAppMain = false)
    StartFreeRtos(false);

    // Just create and render a PowerDepletedScreen
    auto display = createDisplay();

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

    StopFreeRtos();
}
