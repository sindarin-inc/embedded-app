#include <catch_amalgamated.hpp>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <ctime>

namespace {
    // Custom event listener that logs test names as they start
    class TestEventLogger : public Catch::EventListenerBase {
    private:
        std::chrono::steady_clock::time_point testStartTime;
        std::string currentTestName;
        
    public:
        using EventListenerBase::EventListenerBase;

        void testCaseStarting(Catch::TestCaseInfo const& testInfo) override {
            testStartTime = std::chrono::steady_clock::now();
            currentTestName = testInfo.name;
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;
            
            // Format timestamp
            char timeStr[100];
            std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", 
                         std::localtime(&time_t));
            
            // Log test starting
            std::cout << "\n[" << timeStr << "." 
                      << std::setfill('0') << std::setw(3) << ms.count() 
                      << "] Starting test: \"" << testInfo.name << "\"";
            
            if (!testInfo.tags.empty()) {
                std::cout << " [tags:";
                for (const auto& tag : testInfo.tags) {
                    std::cout << " " << tag.original;
                }
                std::cout << "]";
            }
            std::cout << std::endl;
        }

        void testCaseEnded(Catch::TestCaseStats const& testCaseStats) override {
            auto endTime = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                endTime - testStartTime);
            auto ms = duration.count();
            
            std::cout << "[Test completed] \"" << testCaseStats.testInfo->name << "\" - ";
            
            if (testCaseStats.totals.assertions.failed > 0) {
                std::cout << "FAILED (" << testCaseStats.totals.assertions.failed 
                          << " assertions failed)";
            } else if (testCaseStats.totals.assertions.passed > 0) {
                std::cout << "PASSED (" << testCaseStats.totals.assertions.passed 
                          << " assertions)";
            } else {
                std::cout << "NO ASSERTIONS";
            }
            
            std::cout << " - Duration: " << ms << "ms" << std::endl;
        }

        void sectionStarting(Catch::SectionInfo const& sectionInfo) override {
            // Only log sections that are different from the test name
            if (sectionInfo.name != currentTestName) {
                std::cout << "  [Section] " << sectionInfo.name << std::endl;
            }
        }
    };
}

// Register the event listener
CATCH_REGISTER_LISTENER(TestEventLogger)

// Define main function for the test executable
#define CATCH_CONFIG_MAIN