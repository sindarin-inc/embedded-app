#include "Models/KindlePageFlipper.hpp"

#include <cinttypes>
#include <esp_timer.h>
#include <utility>

#include "Communications/SindarinAPI.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/Telemetry.hpp"
#include "sindarin-debug.h"

KindlePageFlipper::KindlePageFlipper(const std::shared_ptr<KindleBook> &kindleBook,
                                     std::shared_ptr<CancelFlag> cancelFlag, Telemetry &telemetry,
                                     bool useBle)
    : kindleBook_(kindleBook), useBle_(useBle), cancelFlag_(std::move(cancelFlag)),
      telemetry_(telemetry) {
    // Capture string copies to avoid use-after-free
    std::string bookTitle = kindleBook ? std::string(kindleBook->title) : "Unknown";
    std::string bookAuthor = kindleBook ? std::string(kindleBook->author) : "Unknown";

    log_i("Created KindlePageFlipper for book: %s by %s", bookTitle.c_str(), bookAuthor.c_str());

    pageStream_ = std::make_shared<KindlePageStream>();
}

auto KindlePageFlipper::checkForError(int httpCode, std::shared_ptr<KindlePageResponse> &response)
    -> std::string {
    if (httpCode == 200 && response->success && response->authenticated) {
        return "";
    }

    // Check for authentication failure in response
    if ((httpCode == 200 || httpCode == 401) && !response->authenticated) {
        // When authenticated: false and we have kindle.json, use the message from response
        if (!response->message.empty()) {
            return std::string(response->message);
        }
        return "Your Kindle session has expired. Please try signing in on the mobile app again.";
    }

    // Check for connection issues
    if (httpCode <= 0) {
        return "CONNECTION_ERROR"; // Special marker for connection issues that should be retried
    } else if (httpCode == 401 || httpCode == 403) {
        return "Your Kindle session has expired. Please try signing in on the mobile app again.";
    } else if (httpCode == 404) {
        return "Book or page not found in your Kindle library.";
    } else if (httpCode >= 400 && httpCode < 500) {
        // Use server's error message if available
        if (!response->message.empty()) {
            return std::string(response->message);
        }
        return "Request error. Please check your settings.";
    } else if (httpCode >= 500) {
        // Use server's error message if available
        if (!response->message.empty()) {
            return std::string(response->message);
        }
        return "Server error. Please try again later.";
    } else {
        // Use server's error message if available
        if (!response->message.empty()) {
            return std::string(response->message);
        }
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
    // Capture string copies to avoid use-after-free
    SpiramString bookTitle = kindleBook_ ? kindleBook_->title : SpiramString("Unknown");
    std::string bookUuid = kindleBook_ ? std::string(kindleBook_->uuid) : "";

    log_i("Loading initial page for Kindle book: %s", bookTitle.c_str());

    telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, bookUuid,
                         {{"event", std::string("book-open")},
                          {"action", std::string("open-begin")},
                          {"kindle", true}});

    auto cancelFlag = cancelFlag_;
    // Execute the API call on the background thread
    DispatchGetCore0BackgroundQueue()->dispatch([this, cancelFlag, bookTitle, bookUuid]() {
        // Track book open start time
        int64_t bookOpenStartTime = esp_timer_get_time();
        // Create a response object
        auto response = std::make_shared<KindlePageResponse>();

        int httpCode = SindarinAPI::getInstance()->openKindleBook(bookTitle, response.get(),
                                                                  cancelFlag, useBle_);
        if (CancelFlag::isCancelled(cancelFlag)) {
            return;
        }

        auto errorMessage = checkForError(httpCode, response);
        if (!errorMessage.empty()) {
            log_w("Error loading initial page: %s", errorMessage.c_str());

            int64_t elapsedMs = (esp_timer_get_time() - bookOpenStartTime) / 1000;
            telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, bookUuid,
                                 {{"event", std::string("book-open")},
                                  {"action", std::string("open-failed")},
                                  {"kindle", true},
                                  {"error", errorMessage},
                                  {"elapsed_ms", static_cast<int>(elapsedMs)}});

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

        size_t ocrTextLength = 0;
        // If this is a last read dialog, don't append the page (which would be empty)
        if (!hasDialog) {
            ocrTextLength = response->ocrText.size();
            pageStream_->appendPageFromResponse(*response);
        }

        // Always signal that we got content (to dismiss loading)
        gotContentFunction_(ocrTextLength);
    });
}

void KindlePageFlipper::updatePlace(const std::shared_ptr<Place> &place, bool atEndOfContent) {
    // Capture needed values to avoid use-after-free
    SpiramString bookTitle = kindleBook_ ? kindleBook_->title : SpiramString("Unknown");
    std::string bookUuid = kindleBook_ ? std::string(kindleBook_->uuid) : "";

    auto cancelFlag = cancelFlag_;

    // Perform the logic in a queue so we always have the latest data before making API calls
    DispatchGetCore0BackgroundQueue()->dispatch(
        [this, place, cancelFlag, atEndOfContent, bookTitle, bookUuid]() {
        if (CancelFlag::isCancelled(cancelFlag)) {
            log_i("Cancelled while updating place");
            return;
        }

        if (serverSyncPlace_ == nullptr) {
            // First time we're setting the place
            serverSyncPlace_ = std::dynamic_pointer_cast<TxtPlace>(place);
        }

        log_i("Updating place from %s to %s %s", serverSyncPlace_->str().c_str(),
              place->str().c_str(), atEndOfContent ? "(at end of content)" : "");

        auto previousPageIndex = pageStream_->pageIndexForOffset(serverSyncPlace_->getOffset());
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
            // Track navigation start time
            int64_t navigationStartTime = esp_timer_get_time();

            auto beginTelemetryData = std::unordered_map<std::string, TelemetryVariant>{
                {"event", std::string("book-navigate")},
                {"action", std::string("navigate-begin")},
                {"kindle", true},
                {"direction", pagesToAdvance},
                {"po", static_cast<int>(place->getOffset())}};
            if (pageToPreview > 0) {
                beginTelemetryData["preview"] = pageToPreview;
            }
            telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, bookUuid,
                                 beginTelemetryData);

            auto response = std::make_shared<KindlePageResponse>();

            int httpCode = SindarinAPI::getInstance()->navigateKindle(
                bookTitle, response.get(), cancelFlag, useBle_, pagesToAdvance, pageToPreview);
            if (CancelFlag::isCancelled(cancelFlag)) {
                return;
            }
            auto errorMessage = checkForError(httpCode, response);
            if (!errorMessage.empty()) {
                log_e("Error moving to next page: %s", errorMessage.c_str());

                int64_t elapsedMs = (esp_timer_get_time() - navigationStartTime) / 1000;
                auto failedTelemetryData = std::unordered_map<std::string, TelemetryVariant>{
                    {"event", std::string("book-navigate")},
                    {"action", std::string("navigate-failed")},
                    {"kindle", true},
                    {"direction", pagesToAdvance},
                    {"po", static_cast<int>(place->getOffset())},
                    {"error", errorMessage},
                    {"elapsed_ms", static_cast<int>(elapsedMs)}};
                if (pageToPreview > 0) {
                    failedTelemetryData["preview"] = pageToPreview;
                }
                telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, bookUuid,
                                     failedTelemetryData);

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

            size_t ocrTextLength = 0;
            // If this is a last read dialog, don't append the page
            if (!hasDialog) {
                // If we're navigating only, don't expect a page
                if (pageToPreview > 0) {
                    ocrTextLength = response->ocrText.size();
                    pageStream_->appendPageFromResponse(*response);
                }
            }

            int64_t elapsedMs = (esp_timer_get_time() - navigationStartTime) / 1000;
            auto successTelemetryData = std::unordered_map<std::string, TelemetryVariant>{
                {"event", std::string("book-navigate")},
                {"action", std::string("navigate-success")},
                {"kindle", true},
                {"direction", pagesToAdvance},
                {"po", static_cast<int>(place->getOffset())},
                {"ocr_text_length", static_cast<int>(ocrTextLength)},
                {"elapsed_ms", static_cast<int>(elapsedMs)}};
            if (pageToPreview > 0) {
                successTelemetryData["preview"] = pageToPreview;
            }
            telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, bookUuid,
                                 successTelemetryData);

            // Always signal that we got content (to dismiss loading)
            gotContentFunction_(ocrTextLength);

            // Update server sync position only after successful navigation
            serverSyncPlace_ = std::dynamic_pointer_cast<TxtPlace>(place);
        } else {
            log_d("Skipping navigation request - already have buffered content (currentPage=%d, "
                  "lastPage=%d, pagesToAdvance=%d)",
                  currentPageIndex, lastPageIndex, pagesToAdvance);

            // For cached content, still call the gotContentFunction to maintain consistent behavior
            if (gotContentFunction_) {
                // Get the current content length from the page stream
                size_t contentLength = pageStream_->getOcrTextLength();
                log_d("Calling gotContentFunction for cached content with length=%zu",
                      contentLength);
                gotContentFunction_(contentLength);
            }
        }
    });
}
