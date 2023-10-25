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
