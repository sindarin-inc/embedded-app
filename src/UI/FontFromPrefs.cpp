#include "FontFromPrefs.hpp"

#include <Fonts.hpp>

#include "Storage/PreferencesStore.hpp"

auto FontFromPrefs(int choice) -> Font * {
    switch (choice) {
    case PREFS_SOL_MEDIUM:
        return static_cast<Font *>(&fontFace1);
    case PREFS_SOL_LARGE:
        return static_cast<Font *>(&fontFace2);
    case PREFS_SOL_SMALL:
        return static_cast<Font *>(&fontFace0);
    default:
        break;
    }
    log_e("Invalid font choice: probably a bug");
    return nullptr;
}
