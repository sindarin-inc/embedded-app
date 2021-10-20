#pragma once
#include <Preferences.h>

class PreferencesStore {
public:
    Preferences prefs;

    int local_x_offset;
    void setLocalXOffset(int local_x_offset) {
        this->local_x_offset = local_x_offset;
        prefs.putInt("x-offset", local_x_offset);
    }

    int local_y_offset;
    void setLocalYOffset(int local_y_offset) {
        this->local_y_offset = local_y_offset;
        prefs.putInt("y-offset", local_y_offset);
    }

    // Default area for browsing should be MAIN MENU which is readables length + 2 (length is
    // SETTINGS MENU)
    int area_in_browsing;
    void setAreaInBrowsing(int area_in_browsing) {
        this->area_in_browsing = area_in_browsing;
        prefs.putInt("browse-area", area_in_browsing);
    }

    char pocket_api_token[40];
    void setPocketApiToken(const char *pocketApiToken) {
        strcpy(pocket_api_token, pocketApiToken);
        prefs.putString("pocket-token", pocket_api_token);
    }

    void begin();

    auto getInstance() -> PreferencesStore * {
        if (instance == nullptr) instance = new PreferencesStore();
        return instance;
    }

protected:
    static PreferencesStore *instance;
};
