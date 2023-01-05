#pragma once

#include <memory>
#include <vector>

#include "Models/Readable.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Bitmaps/Lightbulb.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/MenuItemView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Components/StackView.hpp"
#include "UI/Layout/View.hpp"
#include "UI/MenuScreen.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

// Example of a more complex view that combines other views in a layout
class ArchiveMenuScreen : public MenuScreen, public EinkView {
public:
    ArchiveMenuScreen(Power &power, Readable &readable, PreferencesStore &prefs,
                      DisplaySystem &display)
        : MenuScreen(power), EinkView(display) {}

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void render(DisplaySystem &display) override;

    bool firstLoad = true;

protected:
    std::vector<std::shared_ptr<MenuItemView>> menuItemViews_;
};
