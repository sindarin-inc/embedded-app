#include "PreferencesStore.hpp"

#include "sindarin-debug.h"

#define POCKET_API_TOKEN_PREFS_KEY "pocket-token"

void PreferencesStore::begin() {
    prefs.begin("sindarin-app", false);

    // Only uncomment this if you need to reset the prefs.
    // Then recomment after loading onto board and restart.
    // prefs.clear();

    local_x_offset = prefs.getInt("x-offset");
    local_y_offset = prefs.getInt("y-offset");
    area_in_browsing = prefs.getInt("browse-area");

    prefs.getString(POCKET_API_TOKEN_PREFS_KEY, pocket_api_token, sizeof(pocket_api_token));
}
