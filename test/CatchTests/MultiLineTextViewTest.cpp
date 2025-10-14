#include <catch_amalgamated.hpp>

#define protected public
#define private public
#include "UI/Components/MultiLineTextView.hpp"
#include "ViewTestCommon.hpp"
#undef protected
#undef private

TEST_CASE("MultiLineTextView splitLines creation") {
    auto display = createDisplay();

    MultiLineTextView textView(*display, 2);

    // Set the text for testing
    std::string testText = "The Washington Post: Introducing “The Empty Grave of Comrade Bishop”";
    textView.setText(testText);

    // Layout
    textView.setFrame({0, 0, 169, 200});
    textView.layoutIfNeeded();

    // Check the content of splitLines
    REQUIRE(textView.splitLines_[0] == "The Washington Post: Introducing ");
    REQUIRE(textView.splitLines_[1] == "“The Empty Grave of Comrade ");
}

TEST_CASE("MultiLineTextView with multiple newlines", "[MultiLineTextView]") {
    auto display = createDisplay();

    MultiLineTextView textView(*display, 5);

    // Set the text for testing
    std::string testText = "You are currently on page 4. The most recent page read is page 10 from "
                           "Sam's iPad at 3:15 PM today.\n\nGo to that page?";

    textView.setText(testText);

    // Layout
    textView.setFrame({56, 71, 143, 70});
    textView.layoutIfNeeded();
    textView.draw();

    for (auto &line : textView.splitLines_) {
        log_i("Line: %s", line.c_str());
    }

    textView.draw();

    // Check the content of splitLines

    // Print all the lines
    for (auto &line : textView.splitLines_) {
        log_i("Line: %s", line.c_str());
    }

    REQUIRE(textView.splitLines_[0] == "You are currently on page 4. ");
    REQUIRE(textView.splitLines_[1] == "The most recent page read is ");
    REQUIRE(textView.splitLines_[2] == "page 10 from Sam's iPad at 3:15 ");
    REQUIRE(textView.splitLines_[3] == "PM today...");
}

TEST_CASE("MultiLineTextView collapses ellipses off trailing blank line", "[MultiLineTextView]") {
    auto display = createDisplay();

    MultiLineTextView textView(*display, 5);

    // Text includes a forced blank line ("\n\n") and enough content to exceed 5 lines
    std::string testText =
        "You are currently on page 4. The most recent page read is page 10 from Sam's iPad at "
        "3:15 PM today.\n\nGo to that page? Continue reading to find more details in the "
        "subsequent lines that should force truncation.";

    textView.setText(testText);

    // Narrow width to ensure many wraps and overflow beyond 5 lines
    textView.setFrame({0, 0, 120, 200});
    textView.layoutIfNeeded();
    textView.draw();
    textView.draw();

    // Ensure we did overflow
    REQUIRE(textView.splitLines_.size() > 5);

    for (auto &line : textView.splitLines_) {
        log_i("Line: %s", line.c_str());
    }

    // The last displayed line (index 4) should not be blank and should end with ellipses
    REQUIRE(textView.splitLines_[0] == "You are currently on page ");
    REQUIRE(textView.splitLines_[1] == "4. The most recent page ");
    REQUIRE(textView.splitLines_[2] == "read is page 10 from ");
    REQUIRE(textView.splitLines_[3] == "Sam's iPad at 3:15 PM ");
    REQUIRE(textView.splitLines_[4] == "today...");
}
