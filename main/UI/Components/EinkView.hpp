#pragma once

#include "Displays/DisplaySystem.hpp"
#include "UI/Layout/View.hpp"

class EinkView : public View {
public:
    EinkView(DisplaySystem &display) : View(), display_(display) {}

protected:
    DisplaySystem &display_;
};
