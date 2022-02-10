#ifndef MEDITATION_SCREEN_HPP
#define MEDITATION_SCREEN_HPP

#include "Displays/DisplaySystem.hpp"
#include "UI/Screen.hpp"

class MeditationScreen : public Screen {
public:
    MeditationScreen() : Screen() {}

    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;

protected:
    std::string messagesArray[10] =  { "let's meditate", "breathe in", "breathe out", "gratitude", "presence", "breathe in", "breathe out", "focus", "smile", " " };
    int placeInArray = 0;
    int numItemsInArray = 10;

};

#endif