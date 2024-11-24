#include <catch_amalgamated.hpp>
#include <chrono>
#include <csignal>
#include <thread>

TEST_CASE("Infinite loop test", "[.][timeout]") {
    // This test will run indefinitely
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

TEST_CASE("Crash test", "[.][crash]") {
    // This test will crash by raising SIGABRT
    std::raise(SIGABRT);
}
