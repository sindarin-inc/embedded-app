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

    log_i("Created KindlePageFlipper for book: %s by %s", kindleBook_->title.c_str(),
          kindleBook_->author.c_str());

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
    log_i("Loading initial page for Kindle book: %s", kindleBook_->title.c_str());

    // Reset navigation tracking when loading initial page
    lastNavigatedPage_ = -1;
    lastPreviewedPage_ = -1;

    telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                         {{"event", std::string("book-open")},
                          {"action", std::string("open-begin")},
                          {"kindle", true},
                          {"elapsed_ms", 0}});

    auto cancelFlag = cancelFlag_;
    // Execute the API call on the background thread
    DispatchGetCore0BackgroundQueue()->dispatch([this, cancelFlag]() {
        if (CancelFlag::isCancelled(cancelFlag)) {
            log_i("Cancelled while loading initial page");
            return;
        }

        // Track book open start time
        int64_t bookOpenStartTime = esp_timer_get_time();
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

            int64_t elapsedMs = (esp_timer_get_time() - bookOpenStartTime) / 1000;
            telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
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

            // Log first 50 chars of initial content to see what page it really is
            std::string fullText = std::string(response->ocrText);
            std::string preview =
                fullText.substr(0, std::min(static_cast<size_t>(50), fullText.size()));
            log_i("\033[1;36m>>> INITIAL PAGE: storing_as_page=0, content_preview='%s...'\033[0m",
                  preview.c_str());

            // For initial page, it's always page 0
            pageStream_->appendPageFromResponse(*response, 0);
            // Initialize the last previewed page to 0 on initial load
            lastPreviewedPage_ = 0;
            pageStream_->debugDumpPageMappings();
        }

        // Always signal that we got content (to dismiss loading)
        gotContentFunction_(ocrTextLength);
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

        if (serverSyncPlace_ == nullptr) {
            // First time we're setting the place
            serverSyncPlace_ = std::dynamic_pointer_cast<TxtPlace>(place);
        }

        log_i("Updating place from %s to %s %s", serverSyncPlace_->str().c_str(),
              place->str().c_str(), atEndOfContent ? "(at end of content)" : "");

        auto previousPageIndex = pageStream_->pageIndexForOffset(serverSyncPlace_->getOffset());
        auto currentPageIndex = pageStream_->pageIndexForOffset(place->getOffset());
        auto lastPageIndex = pageStream_->lastPageIndex();

        // Calculate buffer state
        uint32_t currentOffset = place->getOffset();
        size_t bytesRemaining = pageStream_->getBytesRemainingFrom(currentOffset);
        int pagesRemaining = pageStream_->getPagesRemainingFrom(currentOffset);

        log_d("Buffer analysis: %" PRIu32 " bytes from offset %" PRIu32
              ", %zu bytes remaining, %d pages remaining",
              static_cast<uint32_t>(pageStream_->size()), currentOffset, bytesRemaining,
              pagesRemaining);

        // Define buffer threshold
        constexpr size_t bufferThresholdBytes = 1250;

        // Get the server page number for our current reading position
        auto serverPageForCurrentOffset =
            pageStream_->getServerPageNumberForOffset(place->getOffset());

        // Debug: Show the mapping
        log_i("\033[1;33m>>> OFFSET MAPPING: offset=%" PRIu32
              " -> page_index=%d -> server_page=%d\033[0m",
              place->getOffset(), currentPageIndex, serverPageForCurrentOffset);

        // Use absolute navigation with navigate_to and preview_to
        // Navigate to the server page that corresponds to our current reading position
        auto navigateTo = serverPageForCurrentOffset >= 0 ? serverPageForCurrentOffset : 0;

        // When making preview requests, we should advance navigate_to to stay close to preview_to
        // We can skip intermediate navigation pages, but we want to minimize the gap
        // Use the highest contiguous server page we have as our navigation position
        if (lastPreviewedPage_ > navigateTo && lastPreviewedPage_ > lastNavigatedPage_) {
            // We have pages beyond our current position, advance navigation to catch up
            // This prevents large gaps between navigate_to and preview_to
            navigateTo =
                std::min(lastPreviewedPage_, navigateTo + 3); // Don't jump too far ahead at once
        }

        // For preview, we must load sequentially - always the next page after what we have
        // preview_to must be sequential: 1, 2, 3, 4... can't skip
        auto previewTo = lastPreviewedPage_ + 1;

        // Check if we need to send a navigation request
        // We need to navigate if:
        // 1. User has moved to a different page (navigate_to changed) - can skip intermediate pages
        // 2. We need more preview pages (low buffer, at end, or user jumped ahead of our previews)
        bool navigationChanged = (navigateTo != lastNavigatedPage_);
        bool needMorePreviews =
            (bytesRemaining < bufferThresholdBytes) || atEndOfContent ||
            (navigateTo >= lastPreviewedPage_); // User is at or past our last preview

        log_d("Absolute navigation: navigateTo=%d, previewTo=%d (current=%d/%d, previous=%d/%d, "
              "lastNavigated=%d, lastPreviewed=%d, navChanged=%d, needPreviews=%d, "
              "atEndOfContent=%d, serverPage=%d)",
              navigateTo, previewTo, currentPageIndex, lastPageIndex, previousPageIndex,
              lastPageIndex, lastNavigatedPage_, lastPreviewedPage_, navigationChanged,
              needMorePreviews, atEndOfContent, serverPageForCurrentOffset);

        // Send navigation request if we need to update navigation position OR load more previews
        bool needsNavigation = navigationChanged || needMorePreviews;

        if (needsNavigation) {
            log_i("\033[1;35m>>> NAVIGATION REQUEST: navigate_to=%d, preview_to=%d (bytes "
                  "remaining: %zu, threshold: %zu)\033[0m",
                  navigateTo, previewTo, bytesRemaining, bufferThresholdBytes);
            // Track navigation start time
            int64_t navigationStartTime = esp_timer_get_time();

            auto beginTelemetryData = std::unordered_map<std::string, TelemetryVariant>{
                {"event", std::string("book-navigate")},
                {"action", std::string("navigate-begin")},
                {"kindle", true},
                {"navigate_to", navigateTo},
                {"preview_to", previewTo},
                {"po", static_cast<int>(place->getOffset())},
                {"elapsed_ms", 0}};
            telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                                 beginTelemetryData);

            auto response = std::make_shared<KindlePageResponse>();

            int httpCode = SindarinAPI::getInstance()->navigateKindle(
                kindleBook_->title, response.get(), cancelFlag, useBle_, navigateTo, previewTo);
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
                    {"navigate_to", navigateTo},
                    {"preview_to", previewTo},
                    {"po", static_cast<int>(place->getOffset())},
                    {"error", errorMessage},
                    {"elapsed_ms", static_cast<int>(elapsedMs)}};
                telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
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
            if (!hasDialog) {
                if (previewTo > navigateTo) {
                    ocrTextLength = response->ocrText.size();
                    // When we navigate with preview, the response contains content for the preview
                    // page preview_to=1 means we're getting content for page 1 (not page 0)
                    int serverPageNumber = previewTo;

                    // Log first 50 chars of content to see what page it really is
                    std::string fullText = std::string(response->ocrText);
                    std::string preview =
                        fullText.substr(0, std::min(static_cast<size_t>(50), fullText.size()));
                    log_i("\033[1;36m>>> PREVIEW RESPONSE: navigate_to=%d, preview_to=%d, "
                          "storing_as_page=%d, content_preview='%s...'\033[0m",
                          navigateTo, previewTo, serverPageNumber, preview.c_str());

                    pageStream_->appendPageFromResponse(*response, serverPageNumber);
                    // Update the last previewed page to match the server page we just stored
                    lastPreviewedPage_ = serverPageNumber;
                    log_i("\033[1;36m>>> APPENDED PAGE: server_page=%d, content_length=%zu, "
                          "buffer_size_now=%zu\033[0m",
                          serverPageNumber, ocrTextLength, pageStream_->size());
                    // Debug: show full buffer state
                    pageStream_->debugDumpPageMappings();
                }
                // Update the last navigated page to reflect what we told the server
                lastNavigatedPage_ = navigateTo;
                log_d("Updated lastNavigatedPage_ to %d after successful navigation",
                      lastNavigatedPage_);
            }

            int64_t elapsedMs = (esp_timer_get_time() - navigationStartTime) / 1000;
            auto successTelemetryData = std::unordered_map<std::string, TelemetryVariant>{
                {"event", std::string("book-navigate")},
                {"action", std::string("navigate-success")},
                {"kindle", true},
                {"navigate_to", navigateTo},
                {"preview_to", previewTo},
                {"po", static_cast<int>(place->getOffset())},
                {"ocr_text_length", static_cast<int>(ocrTextLength)},
                {"elapsed_ms", static_cast<int>(elapsedMs)}};
            telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                                 successTelemetryData);

            // Always signal that we got content (to dismiss loading)
            gotContentFunction_(ocrTextLength);

            // Update server sync position after successful navigation
            // We track the actual navigation position, not the preview position
            // This ensures we properly track what page the server thinks we're on
            serverSyncPlace_ = std::dynamic_pointer_cast<TxtPlace>(place);
        } else {
            if (!navigationChanged && !needMorePreviews) {
                log_d("Skipping navigation request - no change needed (currentPage=%d, "
                      "lastPage=%d, navigateTo=%d, previewTo=%d, bytesRemaining=%zu)",
                      currentPageIndex, lastPageIndex, navigateTo, previewTo, bytesRemaining);
                log_i("\033[1;33m>>> NAVIGATION SKIPPED: No change needed (navigate_to=%d, "
                      "preview_to=%d, bytes: %zu)\033[0m",
                      navigateTo, previewTo, bytesRemaining);
            }

            // Only call gotContentFunction if we've actually changed position
            // This prevents infinite loops when repeatedly at the same position
            if (navigateTo != previousPageIndex && gotContentFunction_) {
                size_t contentLength = pageStream_->getOcrTextLength();
                log_d("Calling gotContentFunction for position change with cached content "
                      "(length=%zu)",
                      contentLength);
                gotContentFunction_(contentLength);
            }
        }
    });
}
