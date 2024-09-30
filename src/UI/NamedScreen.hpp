#pragma once

#include <string>

#include "Displays/DisplaySystem.hpp"
#include "UI/Screen.hpp "

// This is a screen that simply has a name that can be used for navigation.
class NamedScreen : public Screen {
public:
    NamedScreen(DisplaySystem &display, std::string name);

    std::string getName() const { return name_; }
    void setName(std::string name) { name_ = name; }

private:
    std::string name_;
};
