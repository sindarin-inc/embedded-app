#include <catch_amalgamated.hpp>

#include "ViewTestCommon.hpp"

TEST_CASE("First few screens render correctly and handle remote commands") {
    StartFreeRtos();

    // First screen renders twice once with a full refresh and once with a AU4
    WaitForDraw();
    WaitForDraw();

    AssertViewUnchanged("FirstScreen");

    SECTION("Navigate to settings") {
        // Send a remote command for left (for Settings)
        SendRemoteCommand(CHANNEL_MINUS);

        WaitForDraw();
        WaitForDraw();

        AssertViewUnchanged("Settings");
    }

    StopFreeRtos();
}
