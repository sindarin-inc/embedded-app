#include "UI/ReadablePlacemarkHistoryScreen.hpp"

#include <EPubFile/EPubToc.hpp>
#include <StringUtil/StringUtil.hpp>

void ReadablePlacemarkHistoryScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}

auto ReadablePlacemarkHistoryScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    const auto inset = 2;

    context.setViewFrame(backLabelView_, SolRect::fromSize(size),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);

    auto insetRect =
        context.setViewFrame(menuView_, SolRect::fromSize(size).inset(inset),
                             LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);

    return {size.width, insetRect.size.height + 2 * inset};
};

void ReadablePlacemarkHistoryScreen::drawInRect(SolRect rect) {
    backLabelView_.draw();
    menuView_.draw();
}

void ReadablePlacemarkHistoryScreen::render(DisplaySystem &display) {
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

void ReadablePlacemarkHistoryScreen::processCommand(RemoteCommand command) {
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

void ReadablePlacemarkHistoryScreen::loadMenuItems() {
    menuView_.resetMenuItems();
    auto placemarkHistory = UserReadablePlacemarkHistory::loadForReadable(fileSystem_, readable_);
    if (placemarkHistory.empty()) {
        log_e("Failed to load placemark history");
        back();
        return;
    }

    // Load a map of chapter href to title and accumulated offset
    auto epubFile =
        std::dynamic_pointer_cast<EPubFileRendererStream>(renderer_.getStream())->epubFile;
    std::unordered_map<SpiramString, std::string> chapterHrefToTitle;
    std::unordered_map<SpiramString, int> chapterHrefToOffset;
    for (const EPubTocChapter &chapter : epubFile->getToc()) {
        SpiramString href = chapter.href;
        if (chapter.title.empty()) {
            // Use book title if chapter title is empty
            chapterHrefToTitle[href] = readable_->title;
        } else {
            chapterHrefToTitle[href] = chapter.title;
        }
        chapterHrefToOffset[href] = chapter.epubOffset;
    }

    const UserReadablePlacemarkHistory *recentPlacemark = nullptr;
    for (const auto &placemark : placemarkHistory) {
        // log_d("Placemark: %s", placemark.ePubPlace->getHRef().c_str());
        auto chapterTitle = chapterHrefToTitle[SpiramString(placemark.ePubPlace->getHRef())];
        if (chapterTitle.empty()) {
            log_d("Placemark not found in TOC, using readable title: %s",
                  placemark.ePubPlace->getHRef().c_str());
            chapterTitle = readable_->title;
        }
        // Compare individual offset+href, since the operator== overload also uses the date, which
        // we don't care about here
        if (recentPlacemark != nullptr &&
            placemark.ePubPlace->getOffset() == recentPlacemark->ePubPlace->getOffset() &&
            placemark.ePubPlace->getHRef() == recentPlacemark->ePubPlace->getHRef()) {
            // Don't repeat the same placemark
            continue;
        }
        addMenuItem(std::make_shared<MenuItemTOC>(
            chapterTitle,
            Commify(
                SOL_PAGE_NUMBER(chapterHrefToOffset[SpiramString(placemark.ePubPlace->getHRef())] +
                                placemark.ePubPlace->getOffset())),
            [this, placemark]() {
            // Set readable's placemark to selected placemark history
            setPlacemark(placemark.ePubPlace);
            back();
            }));
        recentPlacemark = &placemark;
    }
}

void ReadablePlacemarkHistoryScreen::setPlacemark(const std::shared_ptr<Place> &place) {
    auto epubFile =
        std::dynamic_pointer_cast<EPubFileRendererStream>(renderer_.getStream())->epubFile;
    readable_->place = place;
    renderer_.setCurrentPlace(readable_->place);
    prefs_.readablePlace.set(readable_->shortenedUuid.c_str(), readable_->place->str());
    prefs_.readablePlaceChanged.set(readable_->shortenedUuid.c_str(), true);
    prefs_.lastReadTime.set(readable_->shortenedUuid.c_str(),
                            SolClock::getInstance().getEpochTime());
    telemetry_.logMetric(TelemetryField::METRIC_SET_PLACEMARK, readable_->shortenedUuid.c_str(),
                         readable_->telemetryPlacemarkMetrics());
}
