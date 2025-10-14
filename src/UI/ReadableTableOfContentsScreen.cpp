#include "UI/ReadableTableOfContentsScreen.hpp"

#include <StringUtil/StringUtil.hpp>

#include "Displays/TextInsets.hpp"
#include "sindarin-debug.h"

void ReadableTableOfContentsScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}

auto ReadableTableOfContentsScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    const auto inset = 2;

    context.setViewFrame(backLabelView_, SolRect::fromSize(size),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);

    auto insetRect =
        context.setViewFrame(menuView_, SolRect::fromSize(size).inset(inset),
                             LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);

    return {size.width, insetRect.size.height + 2 * inset};
};

void ReadableTableOfContentsScreen::drawInRect(SolRect rect) {
    backLabelView_.draw();
    menuView_.draw();
}

void ReadableTableOfContentsScreen::render(DisplaySystem &display) {
    setNeedsLayout();
    setFrame(defaultFrameRect());
    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    menuView_.drawFastElements();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}

void ReadableTableOfContentsScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        moveSelection(-1);
        setNeedsRerender();
        break;
    case RemoteCommand::DOWN:
        moveSelection(1);
        setNeedsRerender();
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::RIGHT:
        setPlacemark();
        menuView_.executeSelectedItem();
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        firstLoad_ = true;
        setNeedsRerender();
        back();
        return;
    default:
        break;
    }
}

void ReadableTableOfContentsScreen::loadMenuItems() {
    menuView_.resetMenuItems();
    auto epubFile =
        std::dynamic_pointer_cast<EPubFileRendererStream>(renderer_.getStream())->epubFile;

    for (const EPubTocChapter &chapter : epubFile->getToc()) {
        std::string title = std::string(chapter.title);
        if (!chapter.inSubsectionHref.empty()) {
            title = StringFormat("  %s", chapter.title.c_str());
        }
        addMenuItem(std::make_shared<MenuItemTOC>(
            title, Commify(SOL_PAGE_NUMBER(chapter.epubOffset)), [this]() { back(); }));
    }
}

void ReadableTableOfContentsScreen::setPlacemark() {
    auto epubFile =
        std::dynamic_pointer_cast<EPubFileRendererStream>(renderer_.getStream())->epubFile;
    auto chapter = epubFile->getToc()[menuView_.getSelectedIndex()];
    if (renderer_.getStream()->isFor(DocType::EPUB)) {
        readable_->place = std::make_shared<EPubPlace>(0, std::string(chapter.href));
    }
    renderer_.setCurrentPlace(readable_->place);
    prefs_.readablePlace.set(readable_->shortenedUuid.c_str(), readable_->place->str());
    prefs_.readablePlaceChanged.set(readable_->shortenedUuid.c_str(), true);
    prefs_.lastReadTime.set(readable_->shortenedUuid.c_str(),
                            SolClock::getInstance().getEpochTime());
    telemetry_.logMetric(TelemetryField::METRIC_SET_PLACEMARK, readable_->shortenedUuid.c_str(),
                         readable_->telemetryPlacemarkMetrics());
}

void ReadableTableOfContentsScreen::setNearestChapterSelection(
    const std::shared_ptr<Place> &placeInFile) {
    int nearestChapter = 0;
    if (placeInFile->isFor(DocType::EPUB)) {
        auto epubFile =
            std::dynamic_pointer_cast<EPubFileRendererStream>(renderer_.getStream())->epubFile;
        auto ePubPlace = std::dynamic_pointer_cast<EPubPlace>(placeInFile);

        nearestChapter = epubFile->getChapterIdx(ePubPlace->getHRef());

        if (nearestChapter == -1) {
            log_w("Unable to find nearest chapter for %s", ePubPlace->getHRef().c_str());
            nearestChapter = 0;
        }
    }
    menuView_.setSelectedIndex(nearestChapter);
}
