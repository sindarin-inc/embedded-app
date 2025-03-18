#pragma once

#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/LineMenuScreen.hpp"

enum TutorialScreenBitsDeleted {
    TUTORIAL_SCREEN_BOOKS = 1 << 0,
    TUTORIAL_SCREEN_ARTICLES = 1 << 1,
    TUTORIAL_SCREEN_BIBLE = 1 << 2,
    TUTORIAL_SCREEN_SETTINGS = 1 << 3,
    TUTORIAL_SCREEN_READING_SETTINGS = 1 << 4,
    TUTORIAL_SCREEN_SYSTEM_SETTINGS = 1 << 5,
    TUTORIAL_SCREEN_KINDLE = 1 << 6
};

class TutorialScreen : public Screen {
public:
    TutorialScreen(DisplaySystem &display, PreferencesStore &prefs, SpiramString text, int bit);

protected:
    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;
    LineMenuScreen menu_;

    bool inMenu_ = false;

    std::shared_ptr<Place> place_, newPlace_;

    auto atEnd() -> bool { return newPlace_->getOffset() == 0; };
};
