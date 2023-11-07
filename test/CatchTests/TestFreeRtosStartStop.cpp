#include <catch_amalgamated.hpp>
#include <chrono>
#include <thread>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "ViewTestCommon.hpp"
#include "sindarin-debug.h"

TEST_CASE("Start and stop freertos", "[.][freertos]") {
    for (int i = 0; i < 3; i++) {
        bool done = false;
        DispatchQueue::dispatchOneOff([&done]() {
            log_i("Start 1");
            Delay(1000);
            log_i("Done 1");
            done = true;
        });

        StartFreeRtos(false);
        while (!done) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        StopFreeRtos();
    }
}
