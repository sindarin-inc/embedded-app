#include "ReadingSettingsMenuScreen.hpp"

#include "Misc/Platform.hpp"

ReadingSettingsMenuScreen::ReadingSettingsMenuScreen(DisplaySystem &display,
                                                     const std::string &backTitle)
    : EinkView(display), menuView_(display), backLabelView_(display, backTitle),
      renderer_(display, DisplayRect({0, 0, 0, 0})), rendererView_(display, renderer_) {
    menuView_.padding = 1;
    menuView_.itemsPerPage = 8;
    rendererView_.dottedBox = true;
    rendererView_.setAutoPilotWraparound();

    std::string message;
    message += R"(The Expanse of the Lord of the Rings

    by James S.J.R.R.A. Tolkien Christensen


    "Elen síla lúmenn' omentielvo" Frodo said to Captain James Holden. (A star shines on the hour of our meeting).
    
    "Yes, Frodo." Holden answered. "There are about 1300 stars we could visit whenever you're up for it. But please get rid of that evil jewelry. We wouldn't want the protomolecule to get a hold of that."

    "Yeah, about that..." Frodo said with a frown. "Gandalf wouldn't get those giant eagles involved. Said they were too high key or something. But perhaps they're not high key enough? Perhaps these Rail Guns and Point Defense Cannons of yours work better on orcs and wraiths than the flying spies of Manwe?

    "Sure, I suppose this is essentially an engineering and a tactical conundrum" Holden said considering the proposed mission - "Getting the device to this special volcano past a supernatural despot and his army of zombies. It's not thatdifferent from missions we've had before. At least there were zombies. I'll ask Naomi to look at the details when we're back in her orbit around Eärendil or whatever you called Venus around here..."

    Frodo didn't really know where to begin a response to Holden's peculiar monologue but before he could answer, Amos chimed in with his usual grin and the words "Is there money involved in this? Might come in handy."

    "Well, my uncle Bilbo is sort of wealthy and left it all to me when he disappeared at his birthday party. I also know a lady in an enchanted forest that makes some pretty useful stuff."
    
    "THREE STRANDS OF HER GOLDEN HAIR....1!1ONE11" Gimli exclaimed for the hundredth time, tears streaming happily down his braided beard.

    "Yeah, don't really know what we would do with that, Master Dwarf" Amos answered, a bit puzzled. "But your uncle's old wealth sounds perfectly fine, little man" Amos said to Frodo, polishing the shotgun he procured from his back.

    Whilst this conversation continued, Peregrin Took wandered off, curiosity leading him inside the Martian spaceship parked in the wilderness. Up the dark ladder, past the galley and finally into the company of Alex Kamal. The best pilot in the universe, apparently not interested in exploring Middle Earth instead of watching another movie on his personal terminal. 

    "Whatever you do don't push any buttons in here" Alex said in his Mariner Valley drawl somehow resembling the accent of Texas, which in the third age of this world still was called "Southern Rhûn".

    "Buttons? What buttons?" Pippin asked looking around for winter cloaks, absent-mindedly leaning on a particularly big red half dome on the weirdly twinkling table around Alex' swiveling couch...

    The usually dormant A.I. of the Rocinante suddenly awoke. Sensing the newfound power node wrapped in a little golden circle. "This technology seems preciousss to usss" it thought while firing up its Epstein drive frying the Fellowship straight to Valinor - or wherever mortals end up, beyond the circles of this world.

    And that my friends, is why Eru Ilúvatar invented time travel; with the words booming through the void...

    "FOOL OF A TOOK!"
    )";

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
    // We rely on the animation timer which is set in the renderer view in autopilot mode
    case RemoteCommand::ANIMATION:
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
    if (firstLoad_ || !rendererView_.autoPilotMode()) {
        draw();
        if (firstLoad_) {
            display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
            firstLoad_ = false;
        }
        menuView_.drawIndicator();
        display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
    } else if (rendererView_.autoPilotMode()) {
        draw();
        menuView_.drawIndicator();
        if (rendererView_.newPage()) {
            display.updateDisplay(DisplaySystem::UpdateType::FULL, true);
        } else {
            display.updateDisplay(DisplaySystem::UpdateType::FAST_ORIGINAL_AU4, true);
        }
    }
}
