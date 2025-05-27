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

    // Check for connection issues
    if (httpCode <= 0) {
        return "CONNECTION_ERROR"; // Special marker for connection issues that should be retried
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

auto KindlePageFlipper::checkForLastReadDialog(const std::shared_ptr<KindlePageResponse> &response)
    -> bool {
    if (response->lastReadDialog && !response->dialogText.empty() && onLastReadDialogFunction_) {
        onLastReadDialogFunction_(response->dialogText);
        return true;
    }
    return false;
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
            // Only propagate non-connection errors
            if (errorMessage != "CONNECTION_ERROR") {
                onErrorFunction_(errorMessage);
            } else {
                log_i("Network connection issue detected, initiating reconnection");
                if (onConnectionErrorFunction_) {
                    onConnectionErrorFunction_();
                }
            }
            return;
        }

        auto hasDialog = checkForLastReadDialog(response);

        // If this is a last read dialog, don't append the page (which would be empty)
        if (!hasDialog) {
            pageStream_->appendPageFromResponse(*response);
        }

        // Always signal that we got content (to dismiss loading)
        gotContentFunction_();
    });
}

void KindlePageFlipper::updatePlace(const std::shared_ptr<Place> &place, bool atEndOfContent) {
    auto cancelFlag = cancelFlag_;

    // Perform the logic in a queue so we always have the latest data before making API calls
    DispatchGetCore0BackgroundQueue()->dispatch([this, place, cancelFlag, atEndOfContent]() {
        if (CancelFlag::isCancelled(cancelFlag)) {
            log_i("Cancelled while updating place");
            return;
        }

        if (place_ == nullptr) {
            // First time we're setting the place
            place_ = std::dynamic_pointer_cast<TxtPlace>(place);
        }

        log_i("Updating place from %s to %s %s", place_->str().c_str(), place->str().c_str(),
              atEndOfContent ? "(at end of content)" : "");

        auto previousPageIndex = pageStream_->pageIndexForOffset(place_->getOffset());
        auto currentPageIndex = pageStream_->pageIndexForOffset(place->getOffset());
        auto lastPageIndex = pageStream_->lastPageIndex();

        auto pagesToAdvance = currentPageIndex - previousPageIndex;
        // Cases:
        // 1. We don't have a full page of content ahead (load one preview page)
        // 2. We have a full page of content but it is too short and we're at the end of the content
        // (load another page into the preview)
        auto pageToPreview = 0;
        if (currentPageIndex == lastPageIndex || atEndOfContent) {
            pageToPreview = lastPageIndex - currentPageIndex + 1;
        }

        log_d("Now on page %d/%d, previously on page %d/%d, advancing %d pages and previewing page "
              "%d (atEndOfContent=%d)",
              currentPageIndex, lastPageIndex, previousPageIndex, lastPageIndex, pagesToAdvance,
              pageToPreview, atEndOfContent);

        if (pagesToAdvance > 0 || pageToPreview > 0) {
            auto response = std::make_shared<KindlePageResponse>();

            int httpCode = SindarinAPI::getInstance()->navigateKindle(
                response.get(), cancelFlag, useBle_, pagesToAdvance, pageToPreview);
            if (CancelFlag::isCancelled(cancelFlag)) {
                return;
            }
            auto errorMessage = checkForError(httpCode, response);
            if (!errorMessage.empty()) {
                log_e("Error moving to next page: %s", errorMessage.c_str());
                // Only propagate non-connection errors
                if (errorMessage != "CONNECTION_ERROR") {
                    onErrorFunction_(errorMessage);
                } else {
                    log_e("Network connection issue detected while navigating, initiating "
                          "reconnection");
                    if (onConnectionErrorFunction_) {
                        onConnectionErrorFunction_();
                    }
                }
                return;
            }

            auto hasDialog = checkForLastReadDialog(response);

            // If this is a last read dialog, don't append the page
            if (!hasDialog) {
                // If we're navigating only, don't expect a page
                if (pageToPreview > 0) {
                    pageStream_->appendPageFromResponse(*response);
                }
            }

            // Always signal that we got content (to dismiss loading)
            gotContentFunction_();
        }

        place_ = std::dynamic_pointer_cast<TxtPlace>(place);
    });
}
