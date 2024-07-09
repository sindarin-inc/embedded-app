#include "UI/ReadablePlacemarkHistoryScreen.hpp"

#include "Misc/StringUtil.hpp"

auto ReadablePlacemarkHistoryScreen::layout(LayoutContext &context, Size size) -> Size {
    const auto inset = 2;

    // This mirrors what happens in MenuScreen right now. We may want to refactor this into a
    // helper so that it is reused and consistent.
    context.setViewFrame(leftArrowView_, {{0, 1}, {5, 10}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);
    context.setViewFrame(upperLeftTextView_, {{10, 0}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);

    auto insetRect = context.setSublayoutFrame(
        Rect{{0, 0}, size}.inset(inset),
        LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;

            for (auto &view : menuItemViews_) {
                auto viewFrame = context.setViewFrame(
                    *view, {{x, y}, size}, LayoutOptionsSizeToFitVertical | LayoutOptionsAlignTop);
                y += viewFrame.size.height + 1;
            }

            return {size.width, y};
        });

    context.setSublayoutFrame(
        {{size.width + 10, 0}, {10, size.height}},
        LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical | LayoutOptionsAlignRight,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;
            Size boxSize = {5, 5};
            for (auto &view : menuPageIndicatorViews_) {
                auto viewFrame = context.setViewFrame(*view, {{x, y}, boxSize});
                y += viewFrame.size.height + 5;
            }

            return {size.width, y};
        });

    return {size.width, insetRect.size.height + 2 * inset};
};

void ReadablePlacemarkHistoryScreen::drawInRect(Rect rect) {
    leftArrowView_.draw();
    upperLeftTextView_.draw();

    for (auto &view : menuItemViews_) {
        (*view).draw();
    }

    if (menuPageIndicatorViews_.size() > 1) {
        for (auto &view : menuPageIndicatorViews_) {
            (*view).draw();
        }
    }
}

void ReadablePlacemarkHistoryScreen::render(DisplaySystem &display) {
    setNeedsLayout();

    setFrame(
        {{display.xAbsolute(display.defaultXPadding), display.yAbsolute(display.defaultYPadding)},
         {display.defaultScreenWidth - 2 * display.defaultXPadding,
          display.defaultScreenHeight - 2 * display.defaultYPadding}});

    if (firstLoad_) {
        PROFILE_START(ReadablePlacemarkHistoryScreenLayoutAndDraw);
        menuItemsPage_.clear();
        menuItemViews_.clear();
        menuPageIndicatorViews_.clear();

        populateMenuItems(menuItemsPage_);

        for (auto &menuItem : menuItemsPage_) {
            auto menuItemView = std::make_shared<MenuItemView>(
                display, menuItem, myStyle_.navigationFont, false, false);
            menuItemView->setCircleIndicator(true);
            menuItemView->setRightText(menuItem.subtitle);
            menuItemViews_.push_back(menuItemView);
        }

        int numPages = getNumItems() / getMyStyle().numberOfItemsPerPage + 1;
        if (getNumItems() % getMyStyle().numberOfItemsPerPage == 0) {
            numPages--;
        }

        for (int i = 0; i < numPages; i++) {
            auto menuPageView = std::make_shared<BoxView>(display, 3);

            if (i == (getCurrSelection() / getMyStyle().numberOfItemsPerPage)) {
                menuPageView->fillColor = display.fg;
            }
            menuPageIndicatorViews_.push_back(menuPageView);
        }

        draw();
        PROFILE_END_LOG_US(ReadablePlacemarkHistoryScreenLayoutAndDraw);
        display.updateDisplay(DisplaySystem::UpdateType::FULL);
    }

    if (menuItemViews_.empty()) {
        log_w("No views to draw in ReadablePlacemarkHistoryScreen");
    }

    std::shared_ptr<MenuItemView> selectedMenuItemView =
        menuItemViews_[getCurrSelection() % getMyStyle().numberOfItemsPerPage];
    for (auto &menuItemView : menuItemViews_) {
        if (menuItemView && menuItemView == selectedMenuItemView) {
            menuItemView->setSelected(true);
        } else {
            menuItemView->setSelected(false);
        }
    }

    // Redraw with a different vcom setting for the menu item bullet
    draw();

    display.updateDisplay(DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);

    firstLoad_ = false;
}

void ReadablePlacemarkHistoryScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        if (prevSelection()) {
            firstLoad_ = true;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::DOWN:
        if (nextSelection()) {
            firstLoad_ = true;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::RIGHT:
        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        firstLoad_ = true;
        back();
        setNeedsRerender();
        break;
    default:
        break;
    }
}

void ReadablePlacemarkHistoryScreen::loadMenuItems() {
    resetMenuItems();
    auto placemarkHistory = UserReadablePlacemarkHistory::loadForReadable(fileSystem_, readable_);
    if (placemarkHistory.empty()) {
        log_e("Failed to load placemark history");
        back();
        return;
    }

    // Load a map of chapter href to title and accumulated offset
    auto stream = std::dynamic_pointer_cast<EPubFile>(renderer_.getStream());
    std::unordered_map<SpiramString, std::string> chapterHrefToTitle;
    std::unordered_map<SpiramString, int> chapterHrefToOffset;
    for (const ReadableTableOfContentsChapter &chapter : stream->getToc()) {
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
        addMenuItem(MenuItem(chapterTitle,
                             Commify(SOL_PAGE_NUMBER(
                                 chapterHrefToOffset[SpiramString(placemark.ePubPlace->getHRef())] +
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
    auto stream = std::dynamic_pointer_cast<EPubFile>(renderer_.getStream());
    readable_->place = place;
    renderer_.setCurrentPlace(readable_->place);
    prefs_.readablePlace.set(readable_->shortenedUuid.c_str(), readable_->place->str());
    prefs_.readablePlaceChanged.set(readable_->shortenedUuid.c_str(), true);
    prefs_.lastReadTime.set(readable_->shortenedUuid.c_str(),
                            SolClock::getInstance().getEpochTime());
    telemetry_.logMetric(TelemetryField::METRIC_SET_PLACEMARK, readable_->shortenedUuid.c_str(),
                         readable_->telemetryPlacemarkMetrics());
}
