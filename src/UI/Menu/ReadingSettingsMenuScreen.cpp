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
        "skirmishes and treasure hunts, he'd pen verses inspired by the sea's beauty. His "
        "weathered "
        "hands, more accustomed to wielding cutlasses and steering ships, now delicately held "
        "quills "
        "and parchment. The crew initially scoffed at their captain's peculiar hobby, but soon "
        "found "
        "themselves drawn to the lyrical tales of their adventures.\n\n"
        "A stolen, enchanted quill from a captured merchant vessel soon revealed its magic, making "
        "his "
        "poems come to life. The images he wrote danced in the air, telling tales of love, "
        "adventure, "
        "and the sea. Shimmering dolphins leapt from ethereal waves, while ghostly ships sailed "
        "through "
        "misty skies. O'Malley's crew, once fierce and merciless, became entranced by the art of "
        "words, turning their ship into a floating haven of poetry and wonder. Hardened pirates "
        "began to explore their own creative sides, composing ballads of their exploits and "
        "sonnets "
        "to distant loves.\n\n"
        "However, the enchanted quill's magic drew the ire of Poseidon, god of the sea. The deity, "
        "feeling his domain was being usurped by mortal creativity, grew jealous and vengeful. "
        "Waves "
        "turned treacherous, testing the pirate crew's newfound love for poetry. Storms battered "
        "their ship, and sea monsters lurked in the depths, threatening to drag them under. Yet, "
        "the pirates found solace and strength in their verses, their words becoming shields "
        "against "
        "the god's wrath.\n\n"
        "Frustrated but undeterred, O'Malley used the quill to write a poem addressing Poseidon "
        "directly. The god appeared in a towering column of water, his trident crackling with "
        "power. Instead of striking the pirates down, Poseidon, intrigued by their audacity, "
        "challenged O'Malley to a duel of verses. The fate of the ship and crew hung in the "
        "balance as god and mortal faced off in a battle of words.\n\n"
        "Poseidon's poetry conjured storms, tsunamis, and maelstroms, his words carrying the "
        "raw power of nature. O'Malley's verses evoked calm seas, the beauty of sunsets, and "
        "the enduring spirit of those who sailed the oceans. The duel raged for days, with "
        "each poem shifting the very fabric of reality around the ship. In the end, Poseidon "
        "conceded, moved by the depth of emotion in O'Malley's words. The god granted the "
        "pirates safe passage and blessed the enchanted quill, ensuring their poetic legacy "
        "would live on through the ages.";
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
    case RemoteCommand::ANIMATION:
        setNeedsRerender();
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

void ReadingSettingsMenuScreen::resetTimer() {
    auto totalLength = renderer_.getTotalLengthOfLines();
    nextInterval_ = totalLength != 0
                        ? pageSpeed_ *
                              renderer_.getLengthOfLine(rendererView_.getNumSkipLines() - 1) /
                              totalLength
                        : 0;
    if (nextInterval_ == 0) {
        nextInterval_ = 1000;
    }

    xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(nextInterval_), 0);
}

void ReadingSettingsMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    bool fromFirstLoad = false;
    if (firstLoad_ || !autoPilotOn_) {
        draw();
        if (firstLoad_) {
            display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
            firstLoad_ = false;
        }
        menuView_.drawIndicator();
        display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
        fromFirstLoad = true;
    } else if (autoPilotOn_) {
        draw();
        menuView_.drawIndicator();
        // Because the increment happens after the render, so this is the beginning of a new page
        if (rendererView_.getNumSkipLines() == 1) {
            display.updateDisplay(DisplaySystem::UpdateType::FULL, true);
        } else {
            display.updateDisplay(DisplaySystem::UpdateType::FAST_ORIGINAL_AU4, true);
        }
        resetTimer();
    }
    if (fromFirstLoad && autoPilotOn_) {
        resetTimer();
    }
}
