#include "Models/KindlePageFlipper.hpp"

#include <cinttypes>
#include <utility>

#include "Communications/SindarinAPI.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/StringUtil.hpp"
#include "sindarin-debug.h"

KindlePageFlipper::KindlePageFlipper(const std::shared_ptr<KindleBook> &kindleBook,
                                     std::shared_ptr<CancelFlag> cancelFlag, bool useBle)
    : kindleBook_(kindleBook), useBle_(useBle), cancelFlag_(std::move(cancelFlag)) {
    log_i("Created KindlePageFlipper for book: %s by %s", kindleBook->title.c_str(),
          kindleBook->author.c_str());

    pageStream_ = std::make_shared<KindlePageStream>();
}

auto KindlePageFlipper::checkForError(int httpCode, std::shared_ptr<KindlePageResponse> &response)
    -> std::string {
    if (httpCode == 200 && response->success) {
        return "";
    }

    if (httpCode == 500) {
        return "Server error. Please try again later.";
    } else if (httpCode == 401 || httpCode == 403) {
        return "Authentication error. Please try signing in again.";
    } else if (httpCode == 404) {
        return "Book or page not found in your Kindle library.";
    } else if (httpCode >= 400 && httpCode < 500) {
        return "Request error. Please check your settings.";
    } else if (httpCode >= 500) {
        return "Server error. Please try again later.";
    } else {
        return "Failed to load book content.";
    }
}

void KindlePageFlipper::loadInitialPage() {
    log_i("Loading initial page for Kindle book: %s", kindleBook_->title.c_str());

    auto cancelFlag = cancelFlag_;
    // Execute the API call on the background thread
    DispatchGetCore0BackgroundQueue()->dispatch([this, cancelFlag]() {
        // Create a response object
        auto response = std::make_shared<KindlePageResponse>();

        int httpCode = SindarinAPI::getInstance()->openKindleBook(
            kindleBook_->title, response.get(), cancelFlag, useBle_);
        if (CancelFlag::isCancelled(cancelFlag)) {
            return;
        }

        auto errorMessage = checkForError(httpCode, response);
        if (!errorMessage.empty()) {
            log_w("Error loading initial page: %s", errorMessage.c_str());
            onErrorFunction_(errorMessage);
            return;
        }

        pageStream_->appendPageFromResponse(*response);
        gotContentFunction_();

        // Preload the next page
        httpCode =
            SindarinAPI::getInstance()->previewKindleNext(response.get(), cancelFlag, useBle_);
        if (CancelFlag::isCancelled(cancelFlag)) {
            return;
        }
        pageStream_->appendPageFromResponse(*response);
        gotContentFunction_();

        // TODO(john): Automatically retry
        errorMessage = checkForError(httpCode, response);
        if (!errorMessage.empty()) {
            log_w("Error loading preview after initial page: %s", errorMessage.c_str());
            onErrorFunction_(errorMessage);
            return;
        }
    });
}

void KindlePageFlipper::updatePlace(const std::shared_ptr<Place> &place) {
    if (place_ == nullptr) {
        // First time we're setting the place
        place_ = std::dynamic_pointer_cast<TxtPlace>(place);
        return;
    }

    // Log the places
    log_i("Updating place from %s to %s", place_->str().c_str(), place->str().c_str());

    auto cancelFlag = cancelFlag_;
    DispatchGetCore0BackgroundQueue()->dispatch([this, place, cancelFlag]() {
        // Perform the logic in the same queue as the requests, so we always have the latest data
        // when we need to make decisions
        auto currentPageIndex = pageStream_->pageIndexForOffset(place->getOffset());
        // If the start of this page is in the last loaded page, then go ahead and load the next
        // page
        if (currentPageIndex < pageStream_->lastPageIndex()) {
            log_d("On page %d/%d, no need to preload next page", currentPageIndex,
                  pageStream_->lastPageIndex());
            return;
        }
        log_d("Now on page %d/%d, preloading next page", currentPageIndex,
              pageStream_->lastPageIndex());
        auto response = std::make_shared<KindlePageResponse>();

        // TODO(john): Would be better to combine these into a single request
        int httpCode =
            SindarinAPI::getInstance()->navigateKindleNext(response.get(), cancelFlag, useBle_);
        if (CancelFlag::isCancelled(cancelFlag)) {
            return;
        }

        auto errorMessage = checkForError(httpCode, response);
        if (!errorMessage.empty()) {
            log_w("Error moving to next page: %s", errorMessage.c_str());
            onErrorFunction_(errorMessage);
            return;
        }

        httpCode =
            SindarinAPI::getInstance()->previewKindleNext(response.get(), cancelFlag, useBle_);
        if (CancelFlag::isCancelled(cancelFlag)) {
            return;
        }

        errorMessage = checkForError(httpCode, response);
        if (!errorMessage.empty()) {
            log_w("Error loading preview after navigating to next page: %s", errorMessage.c_str());
            onErrorFunction_(errorMessage);
            return;
        }

        pageStream_->appendPageFromResponse(*response);
        gotContentFunction_();
    });

    // Update the place
    place_ = std::dynamic_pointer_cast<TxtPlace>(place);
}
