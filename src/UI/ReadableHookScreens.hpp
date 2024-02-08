#ifndef READABLE_HOOK_SCREENS_HPP
#define READABLE_HOOK_SCREENS_HPP

#include <memory>
#include <string>
#include <unordered_map>

#include "Displays/DisplaySystem.hpp"
#include "Misc/Telemetry.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/FirstHookTestScreen.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/ResizeBoxScreen.hpp"
#include "UI/Screen.hpp"

#define FIRST_HOOK_TEST_SCREEN_TAG "first-hook-test"
#define OFFSET_ADJUST_SCREEN_TAG "offset-adjust"
#define RESIZE_BOX_SCREEN_TAG "resize-box"

// This is a singleton class that holds a map of string names that are
// the tags that will be embedded in epubs as hooks that allow us
// to redirect the reader to different screens when they hit that part
// of the epub.
//
// Right now it's implemented as a simple vector of strings (the tags), such
// that when the ReadableScreen parses the epub and finds the tag, it can
// request a new Screen object from this class. When this class returns
// that Screen Object, the ReadableScreen will then render that Screen.
//
class ReadableHookScreens {
public:
    //
    // Create a new Screen object and return a shared_ptr to it (static method)
    // This method is called by the ReadableScreen when it finds a tag
    // in the epub that matches one of the names in the vector.
    //
    static auto getNewHookScreen(const std::string &name, DisplaySystem &display,
                                 PreferencesStore &prefs, Telemetry &telemetry)
        -> std::shared_ptr<Screen> {
        if (name == FIRST_HOOK_TEST_SCREEN_TAG) {
            return std::make_shared<FirstHookTestScreen>(display, prefs, telemetry);
        } else if (name == OFFSET_ADJUST_SCREEN_TAG) {
            return std::make_shared<OffsetAdjustScreen>(display, prefs, telemetry);
        } else if (name == RESIZE_BOX_SCREEN_TAG) {
            return std::make_shared<ResizeBoxScreen>(display, prefs, telemetry);
        } else {
            log_e("No screen object known for name: %s", name.c_str());
            return nullptr; // Return nullptr if the name is not found
        }
    }
};

#endif