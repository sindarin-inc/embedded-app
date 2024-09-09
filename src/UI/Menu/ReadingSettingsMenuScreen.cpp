#include "ReadingSettingsMenuScreen.hpp"

#include "Misc/Platform.hpp"

ReadingSettingsMenuScreen::ReadingSettingsMenuScreen(DisplaySystem &display,
                                                     const std::string &backTitle)
    : EinkView(display), menuView_(display), backLabelView_(display, backTitle),
      renderer_(display, DisplayRect({0, 0, 0, 0})), rendererView_(display, renderer_) {
    menuView_.padding = 1;
    menuView_.itemsPerPage = 8;
    rendererView_.dottedBox = true;

    std::string message;
    message +=
        "Beneath a rugged exterior, Pirate Captain O'Malley harbored a love for poetry. Between "
        "skirmishes and treasure hunts, he'd pen verses inspired by the sea's beauty. A stolen, "
        "enchanted quill from a captured merchant vessel soon revealed its magic, making his poems "
        "come to life. The images he wrote danced in the air, telling tales of love, adventure, "
        "and the sea. O'Malley's crew, once fierce and merciless, became entranced by the art of "
        "words, turning their ship into a floating haven of poetry and wonder. "
        "However, the enchanted quill's magic drew the ire of Poseidon, god of the sea. Waves "
        "turned treacherous, testing the pirate crew's newfound love for poetry. Frustrated, "
        "O'Malley used the quill to write a poem addressing Poseidon. The god appeared, "
        "challenging O'Malley to a duel of verses. Poseidon's poetry conjured storms; O'Malley's "
        "verses evoked calm seas.";
    auto stream = std::make_shared<StdStringStream>(message);
    renderer_.setStream(stream);
}

void ReadingSettingsMenuScreen::addMenuItem(const std::shared_ptr<NewMenuItem> &menuItem) {
    menuView_.addMenuItem(menuItem);
}

void ReadingSettingsMenuScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        moveSelection(-1);
        break;
    case RemoteCommand::DOWN:
        moveSelection(1);
        break;
    case RemoteCommand::ENTER:
        menuView_.executeSelectedItem();
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        firstLoad_ = true;
        back();
        setNeedsRerender();
        break;
    default:
        // Handle other commands or ignore
        break;
    }
    setNeedsLayout();
}

auto ReadingSettingsMenuScreen::setSelectedIndex(size_t index) -> bool {
    auto newPage = menuView_.setSelectedIndex(index);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
    return newPage;
}

auto ReadingSettingsMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    int padding = size.height / 20;

    // Layout back label
    auto backBox = context.setViewFrame(backLabelView_, {{0, 0}, {size.width, 0}},
                                        LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);
    // Layout menu
    auto menuBox = context.setViewFrame(
        menuView_, Rect::fromSize(size).inset(size.width / 7, backBox.size.height + padding),
        LayoutOptionsSizeToFitVertical);

    int yOffset = menuBox.bottom() + padding;

    context.setViewFrame(rendererView_, {{0, yOffset}, {size.width, size.height - yOffset}});

    return size;
}

void ReadingSettingsMenuScreen::drawInRect(Rect rect) {
    backLabelView_.draw();
    menuView_.draw();
    rendererView_.draw();
}

void ReadingSettingsMenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}

void ReadingSettingsMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    menuView_.drawIndicator();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}
