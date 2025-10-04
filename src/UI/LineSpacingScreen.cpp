#include "UI/LineSpacingScreen.hpp"

#include "UI/Fonts.hpp"
#include "sindarin-debug.h"

#define MIN_LINE_SPACING (-2)
#define MAX_LINE_SPACING 5

LineSpacingScreen::LineSpacingScreen(DisplaySystem &display, PreferencesStore &prefs,
                                     Telemetry &telemetry)
    : EinkView(display), display_(display),
      renderer_(display, GetTextRectFromPrefs(display, prefs),
                FontFromPrefs(prefs.font.get().value_or(PREFS_SOL_SMALL))),
      prefs_(prefs), telemetry_(telemetry), lineSpacing_(prefs.lineSpacing.get().value_or(0)),
      rendererRef_(renderer_), outerBoxView_(display, 5), backgroundBoxView_(display, 5),
      explanationView_(display, 3) {

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
    rendererRef_.setStream(stream);

    setupInstructions();
}

LineSpacingScreen::LineSpacingScreen(DisplaySystem &display, PreferencesStore &prefs,
                                     Telemetry &telemetry, Renderer &renderer,
                                     std::shared_ptr<Place> place)
    : EinkView(display), display_(display),
      renderer_(display, GetTextRectFromPrefs(display, prefs),
                FontFromPrefs(prefs.font.get().value_or(PREFS_SOL_SMALL))),
      place_(std::move(place)), prefs_(prefs), telemetry_(telemetry),
      lineSpacing_(prefs.lineSpacing.get().value_or(0)), rendererRef_(renderer),
      outerBoxView_(display, 5), backgroundBoxView_(display, 5), explanationView_(display, 5) {
    setupInstructions();
}

auto LineSpacingScreen::setupInstructions() -> void {
    std::string instructions = "\ue003  Increase\n";
    instructions += "\ue004  Decrease\n";
    instructions += "\ue000  Done";

    explanationView_.setText(instructions);
    explanationView_.dynamicWidth = true;

    setNeedsLayout();
}

auto LineSpacingScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    auto explanationFrame = context.setViewFrame(explanationView_, SolRect::fromSize(size),
                                                 LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
    auto outerBoxViewFrame = context.setViewFrame(outerBoxView_, explanationFrame.inset(-5));
    context.setViewFrame(backgroundBoxView_, outerBoxViewFrame.inset(-8));

    return size;
}

void LineSpacingScreen::drawInRect(SolRect rect) {
    outerBoxView_.fillColor = display_.bg;
    outerBoxView_.strokeColor = display_.fg;
    backgroundBoxView_.fillColor = display_.bg;
    backgroundBoxView_.strokeColor = display_.bg;

    backgroundBoxView_.draw();
    outerBoxView_.draw();
    explanationView_.draw();
}

void LineSpacingScreen::render(DisplaySystem &display) {
    // Get the bounding box
    auto textBox = GetTextRectFromPrefs(display, prefs_);
    setFrame(defaultFrameRect());

    rendererRef_.setRect(textBox);
    rendererRef_.setLineSpacing(lineSpacing_);
    if (place_ == nullptr) {
        rendererRef_.renderFirstPage(0, true);
    } else {
        rendererRef_.renderPage(place_, 0);
    }

    draw();
    display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
}

void LineSpacingScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::LEFT:
    case RemoteCommand::RIGHT:
        // Intentionally left out for now
        break;
    case RemoteCommand::UP:
        lineSpacing_++;
        if (lineSpacing_ > MAX_LINE_SPACING) {
            lineSpacing_ = MAX_LINE_SPACING;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::DOWN:
        lineSpacing_--;
        if (lineSpacing_ < MIN_LINE_SPACING) {
            lineSpacing_ = MIN_LINE_SPACING;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::BACK:
        // If LEFT is implemented above,
        // undo that here, since you get both PREV and BACK on a longpress
    case RemoteCommand::ENTER:
        prefs_.lineSpacing.set(lineSpacing_);
        log_d("Saving lineSpacing to preferences: (%d)", lineSpacing_);

        telemetry_.logMetricData(TelemetryField::METRIC_STYLE, {{"line_spacing", lineSpacing_}});

        back();
        return;
    default:
        break;
    }

    log_d("setting new lineSpacing to be %d", lineSpacing_);
}
