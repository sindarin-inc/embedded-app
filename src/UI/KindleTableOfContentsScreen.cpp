#include "KindleTableOfContentsScreen.hpp"

#include <ArduinoJson.h>

#include "Communications/SindarinAPI.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/StringUtil.hpp"
#include "Models/Json.hpp"
#include "sindarin-debug.h"

KindleTableOfContentsScreen::KindleTableOfContentsScreen(
    DisplaySystem &display, const std::shared_ptr<KindleBook> &kindleBook, PreferencesStore &prefs,
    Telemetry &telemetry, KindlePageFlipper &pageFlipper, int currentPage,
    const std::shared_ptr<CancelFlag> &cancelFlag)
    : EinkView(display), kindleBook_(kindleBook), telemetry_(telemetry), pageFlipper_(pageFlipper),
      currentPage_(currentPage), cancelFlag_(cancelFlag), backgroundView_(display, 5),
      menuView_(display), backLabelView_(display, "Back"), loadingMessageView_(display, 5),
      errorMessageView_(display, 5) {

    menuView_.padding = 1;
    menuView_.itemsPerPage = 11;

    loadingMessageView_.setText("Loading table of contents...");
    loadingMessageView_.alignCenter = true;
    loadingMessageView_.dynamicWidth = false;

    errorMessageView_.setText("");
    errorMessageView_.alignCenter = true;
}

void KindleTableOfContentsScreen::begin() {
    setState(State::LOADING);
    loadTableOfContents();
}

void KindleTableOfContentsScreen::setState(State newState) {
    state_ = newState;
    setNeedsLayout();
    setNeedsRerender();
}

void KindleTableOfContentsScreen::loadTableOfContents() {
    log_i("Loading table of contents for book: %s", kindleBook_->title.c_str());

    // Log telemetry for opening table of contents
    telemetry_.logMetric(TelemetryField::METRIC_EVENT, kindleBook_->uuid,
                         {{"event", std::string("table_of_contents_opened")}, {"kindle", true}});

    DispatchGetCore0BackgroundQueue()->dispatch([this]() {
        if (CancelFlag::isCancelled(cancelFlag_)) {
            return;
        }

        // Make network request for table of contents
        auto api = SindarinAPI::getInstance();

        // Use BLE if that's what we're currently using
        bool isUsingBle = pageFlipper_.isUsingBleConnection();

        // Use POST with empty chapter_uuid to get chapters WITH UUIDs
        auto response = api->fetchKindleChaptersWithUuids(std::string(kindleBook_->uuid),
                                                          currentPage_, isUsingBle, cancelFlag_);

        if (CancelFlag::isCancelled(cancelFlag_)) {
            return;
        }

        DispatchGetUiQueue()->dispatch([this, response]() {
            if (response.empty()) {
                log_e("Failed to get table of contents");
                errorMessageView_.setText("Failed to load table of contents");
                setState(State::ERROR);
                render(display_);
            } else {
                handleTableOfContentsResponse(response);
            }
        });
    });
}

void KindleTableOfContentsScreen::handleTableOfContentsResponse(const std::string &response) {
    log_d("TOC Response: %s", response.c_str());

    // Allocate more memory for JSON parsing - response might have nested objects
    auto doc = std::make_shared<SpiRamJsonDocument>(
        std::max(static_cast<size_t>(4096), response.size() * 3));
    DeserializationError error = deserializeJson(*doc, response);

    if (error) {
        log_e("Failed to parse table of contents response: %s", error.c_str());
        errorMessageView_.setText("Failed to parse table of contents");
        setState(State::ERROR);
        render(display_);
        return;
    }

    auto root = (*doc).as<JsonObject>();

    if (!root["success"] || !root["success"].as<bool>()) {
        const char *errorText = root["error"] | "Unknown error";
        log_e("Table of contents request failed: %s", errorText);
        errorMessageView_.setText(StringFormat("Error: %s", errorText));
        setState(State::ERROR);
        render(display_);
        return;
    }

    // Parse position if available
    if (root.containsKey("position") && root["position"].is<JsonObject>()) {
        JsonObject position = root["position"];
        if (position.containsKey("page")) {
            currentPosition_ = position["page"];
            log_i("Current position: page %d", currentPosition_);
        }
    }

    // Parse chapters
    if (!root.containsKey("chapters") || !root["chapters"].is<JsonArray>()) {
        log_e("No chapters array in response");
        errorMessageView_.setText("No chapters found");
        setState(State::ERROR);
        render(display_);
        return;
    }

    chapters_.clear();
    JsonArray chaptersArray = root["chapters"];

    for (JsonVariant value : chaptersArray) {
        ChapterInfo chapter;

        if (value.containsKey("title")) {
            chapter.title = value["title"].as<std::string>();
        }

        if (value.containsKey("uuid")) {
            chapter.uuid = value["uuid"].as<std::string>();
        }

        if (value.containsKey("page")) {
            chapter.page = value["page"];
        }

        if (!chapter.title.empty()) {
            log_d("Chapter: title='%s', uuid='%s', page=%d", chapter.title.c_str(),
                  chapter.uuid.c_str(), chapter.page);
            chapters_.push_back(chapter);
        }
    }

    log_i("Loaded %zu chapters", chapters_.size());

    if (chapters_.empty()) {
        errorMessageView_.setText("No chapters found");
        setState(State::ERROR);
        render(display_);
    } else {
        populateMenuItems();
        // Don't use setHidden - just change state and let drawInRect handle what to draw
        setState(State::DISPLAYING);
        firstLoad_ = true; // Force full redraw to clear loading message
        render(display_);
    }
}

void KindleTableOfContentsScreen::populateMenuItems() {
    menuView_.resetMenuItems();

    for (const auto &chapter : chapters_) {
        std::string displayText = chapter.title;
        if (chapter.page >= 0) {
            displayText += StringFormat(" (p. %d)", chapter.page);
        }

        auto chapterTitle = chapter.title; // Capture for lambda
        auto chapterUuid = chapter.uuid;

        menuView_.addMenuItem(
            std::make_shared<MenuItemSingleLine>(displayText, [this, chapterTitle, chapterUuid]() {
                log_i("Selected chapter: %s (UUID: %s)", chapterTitle.c_str(), chapterUuid.c_str());
                selectChapter();
            }));
    }
}

void KindleTableOfContentsScreen::selectChapter() {
    if (menuView_.getSelectedIndex() >= 0 && menuView_.getSelectedIndex() < chapters_.size()) {
        const auto &chapter = chapters_[menuView_.getSelectedIndex()];

        log_i("Selected chapter index %zu: title='%s', uuid='%s'", menuView_.getSelectedIndex(),
              chapter.title.c_str(), chapter.uuid.c_str());

        // Log telemetry for chapter selection
        telemetry_.logMetric(TelemetryField::METRIC_EVENT, kindleBook_->uuid,
                             {{"event", std::string("table_of_contents_chapter_selected")},
                              {"kindle", true},
                              {"chapter", chapter.title},
                              {"current_page", currentPage_}});

        // Call the callback to navigate to the chapter with UUID
        if (chapterSelectedCallback_) {
            chapterSelectedCallback_(chapter.uuid, currentPage_);
        }

        // Dismiss this screen
        back();
    }
}

void KindleTableOfContentsScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}

auto KindleTableOfContentsScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    const auto inset = 2;
    auto fullFrame = SolRect::fromSize(size);

    // Always layout the background first to cover everything
    context.setViewFrame(backgroundView_, fullFrame);

    if (state_ == State::LOADING || state_ == State::ERROR) {
        // Center the loading or error message
        auto messageView = (state_ == State::LOADING) ? static_cast<View *>(&loadingMessageView_)
                                                      : static_cast<View *>(&errorMessageView_);

        context.setViewFrame(*messageView, SolRect::fromSize(size),
                             LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
        return size;
    }

    // Layout back button
    context.setViewFrame(backLabelView_, SolRect::fromSize(size),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);

    // Layout menu view
    auto insetRect =
        context.setViewFrame(menuView_, SolRect::fromSize(size).inset(inset),
                             LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);

    return {size.width, insetRect.size.height + 2 * inset};
}

void KindleTableOfContentsScreen::drawInRect(SolRect rect) {
    // Always draw background first to clear any previous content
    backgroundView_.fillColor = display_.bg;
    backgroundView_.strokeColor = display_.bg; // No visible border
    backgroundView_.draw();

    if (state_ == State::LOADING) {
        loadingMessageView_.draw();
    } else if (state_ == State::ERROR) {
        errorMessageView_.draw();
    } else {
        backLabelView_.draw();
        menuView_.draw();
    }
}

void KindleTableOfContentsScreen::render(DisplaySystem &display) {
    setNeedsLayout();
    setFrame(defaultFrameRect());

    draw();

    if (firstLoad_ || state_ != State::DISPLAYING) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    // Always draw fast elements when displaying the menu
    if (state_ == State::DISPLAYING) {
        menuView_.drawFastElements();
        display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
    }
}

void KindleTableOfContentsScreen::processCommand(RemoteCommand command) {
    if (state_ != State::DISPLAYING) {
        if (command == RemoteCommand::BACK || command == RemoteCommand::LEFT) {
            back();
        }
        return;
    }

    switch (command) {
    case RemoteCommand::UP:
        moveSelection(-1);
        break;
    case RemoteCommand::DOWN:
        moveSelection(1);
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::RIGHT:
        selectChapter();
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        back();
        break;
    default:
        break;
    }
}
