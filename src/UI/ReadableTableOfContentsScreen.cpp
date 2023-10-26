#include "ReadableTableOfContentsScreen.hpp"

#include "Misc/StringUtil.hpp"

auto ReadableTableOfContentsScreen::layout(LayoutContext &context, Size size) -> Size {
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

void ReadableTableOfContentsScreen::drawInRect(Rect rect) {
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

void ReadableTableOfContentsScreen::render(DisplaySystem &display) {
    setNeedsLayout();

    setFrame(
        {{display.xAbsolute(display.defaultXPadding), display.yAbsolute(display.defaultYPadding)},
         {display.defaultScreenWidth - 2 * display.defaultXPadding,
          display.defaultScreenHeight - 2 * display.defaultYPadding}});

    if (firstLoad_) {
        PROFILE_START(ReadableTableOfContentsScreenLayoutAndDraw);
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
        PROFILE_END_LOG_US(ReadableTableOfContentsScreenLayoutAndDraw);
        display.updateDisplay(DisplaySystem::UpdateType::FULL);
    }

    if (menuItemViews_.size() <= 0) {
        log_w("No views to draw in ReadableTableOfContentsScreen");
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

void ReadableTableOfContentsScreen::processCommand(RemoteCommand command) {
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
        setPlacemark();
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

void ReadableTableOfContentsScreen::loadMenuItems() {
    resetMenuItems();
    for (const ReadableTableOfContentsChapter &chapter : readable_->toc) {
        std::string title = std::string(chapter.title);
        if (!chapter.inSubsectionHref.empty()) {
            title = StringFormat("  %s", chapter.title.c_str());
        }
        addMenuItem(
            MenuItem(title, Commify(SOL_PAGE_NUMBER(chapter.txtOffset)), [this]() { back(); }));
    }
}

void ReadableTableOfContentsScreen::setPlacemark() {
    auto chapter = readable_->toc[getCurrSelection()];
    if (readable_->place->isFor(DocType::TXT)) {
        readable_->place->setOffset(chapter.txtOffset);
    } else if (readable_->place->isFor(DocType::EPUB)) {
        readable_->place->setOffset(0);
        std::shared_ptr<EPubPlace> ePubPlace =
            std::dynamic_pointer_cast<EPubPlace>(readable_->place);
        ePubPlace->setHRef(chapter.href);
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
    if (placeInFile->isFor(DocType::TXT)) {
        for (int i = 0; i < readable_->toc.size(); i++) {
            if (readable_->toc[i].txtOffset <= placeInFile->getOffset()) {
                nearestChapter = i;
            } else {
                break;
            }
        }
    } else if (placeInFile->isFor(DocType::EPUB)) {
        auto stream = std::dynamic_pointer_cast<EPubFile>(renderer_.getStream());
        auto ePubPlace = std::dynamic_pointer_cast<EPubPlace>(placeInFile);

        nearestChapter = stream->getReadableToCChapterIndex(ePubPlace);

        if (nearestChapter == -1) {
            log_w("Unable to find nearest chapter for %s", ePubPlace->getHRef().c_str());
            nearestChapter = 0;
        }

        // std::shared_ptr<EPubPlace> ePubPlace = std::dynamic_pointer_cast<EPubPlace>(placeInFile);
        // std::string href = ePubPlace->getHRef();
        // for (int i = 0; i < readable_->toc.size(); i++) {
        //     if (readable_->toc[i].href == href) {
        //         nearestChapter = i;
        //         break;
        //     }
        // }
    }
    setCurrSelection(nearestChapter);
}
