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
        if (gotContentFunction_) {
            log_i("Calling gotContentFunction after initial page load (ocrTextLength=%zu)",
                  ocrTextLength);
            gotContentFunction_(ocrTextLength);
        } else {
            log_w("gotContentFunction_ is null after initial page load");
        }
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

        // For preview, we must load sequentially - always the next page after what we have
        // preview_to must be sequential: 1, 2, 3, 4... can't skip
        auto previewTo = lastPreviewedPage_ + 1;

        // Check if we need to send a navigation request
        // We need to navigate if:
        // 1. User has moved to a different page (navigate_to changed from last reported position)
        // 2. We need more preview pages (low buffer or at end of content)
        // 3. User jumped ahead past our previews
        bool navigationChanged = (navigateTo != lastNavigatedPage_);
        bool userJumpedAhead =
            (navigateTo >= lastPreviewedPage_); // User is at or past our last preview
        bool needMorePreviews = (bytesRemaining < bufferThresholdBytes) || atEndOfContent;

        // Determine what kind of request to make
        bool needsNavigation = false;
        bool includePreview = false;

        if (userJumpedAhead) {
            // User jumped ahead - need to navigate and get preview
            needsNavigation = true;
            includePreview = true;
        } else if (navigationChanged && needMorePreviews) {
            // User moved and we need more content - navigate with preview
            needsNavigation = true;
            includePreview = true;
        } else if (navigationChanged && !needMorePreviews) {
            // User moved but we have enough buffer - just update position, no preview needed
            needsNavigation = true;
            includePreview = false;
        } else if (!navigationChanged && needMorePreviews) {
            // Same position but need more buffer - skip the request entirely
            // The additional preview logic after page load will handle this
            needsNavigation = false;
        }

        log_d("Navigation analysis: navigateTo=%d, previewTo=%d (current=%d/%d, previous=%d/%d, "
              "lastNavigated=%d, lastPreviewed=%d, navChanged=%d, userJumped=%d, needPreviews=%d, "
              "atEndOfContent=%d, serverPage=%d)",
              navigateTo, previewTo, currentPageIndex, lastPageIndex, previousPageIndex,
              lastPageIndex, lastNavigatedPage_, lastPreviewedPage_, navigationChanged,
              userJumpedAhead, needMorePreviews, atEndOfContent, serverPageForCurrentOffset);

        if (needsNavigation) {
            // Determine the actual preview_to value to send
            int actualPreviewTo = includePreview ? previewTo : navigateTo;

            log_i("\033[1;35m>>> NAVIGATION REQUEST: navigate_to=%d, preview_to=%d (bytes "
                  "remaining: %zu, threshold: %zu, includePreview=%d)\033[0m",
                  navigateTo, actualPreviewTo, bytesRemaining, bufferThresholdBytes,
                  includePreview);

            // Track navigation start time
            int64_t navigationStartTime = esp_timer_get_time();

            auto beginTelemetryData = std::unordered_map<std::string, TelemetryVariant>{
                {"event", std::string("book-navigate")},
                {"action", std::string("navigate-begin")},
                {"kindle", true},
                {"navigate_to", navigateTo},
                {"preview_to", actualPreviewTo},
                {"po", static_cast<int>(place->getOffset())},
                {"elapsed_ms", 0}};
            telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                                 beginTelemetryData);

            auto response = std::make_shared<KindlePageResponse>();

            // Retry logic for failed requests - since navigation is idempotent, we can safely retry
            constexpr int maxRetries = 3;
            int retryCount = 0;
            std::string errorMessage;
            int httpCode = 0;

            while (retryCount < maxRetries) {
                if (retryCount > 0) {
                    log_w("Retrying navigation request (attempt %d/%d): navigate_to=%d, "
                          "preview_to=%d",
                          retryCount + 1, maxRetries, navigateTo, actualPreviewTo);

                    // Log retry telemetry
                    auto retryTelemetryData = std::unordered_map<std::string, TelemetryVariant>{
                        {"event", std::string("book-navigate")},
                        {"action", std::string("navigate-retry")},
                        {"kindle", true},
                        {"navigate_to", navigateTo},
                        {"preview_to", actualPreviewTo},
                        {"po", static_cast<int>(place->getOffset())},
                        {"retry_attempt", retryCount + 1},
                        {"max_retries", maxRetries}};
                    telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                                         retryTelemetryData);
                }

                httpCode = SindarinAPI::getInstance()->navigateKindle(
                    kindleBook_->title, response.get(), cancelFlag, useBle_, navigateTo,
                    actualPreviewTo);

                if (CancelFlag::isCancelled(cancelFlag)) {
                    return;
                }

                errorMessage = checkForError(httpCode, response);

                // If successful or non-retryable error, break out
                if (errorMessage.empty()) {
                    if (retryCount > 0) {
                        log_i("Navigation request succeeded after %d retries", retryCount);
                    }
                    break;
                }

                // Check if this is a retryable error (network/timeout issues)
                bool isRetryable = (errorMessage == "CONNECTION_ERROR" ||
                                    errorMessage == "TIMEOUT" || httpCode == 0 || // No response
                                    httpCode >= 500);                             // Server errors

                if (!isRetryable) {
                    log_e("Non-retryable error: %s (HTTP %d)", errorMessage.c_str(), httpCode);
                    break;
                }

                retryCount++;
                if (retryCount < maxRetries) {
                    // Wait before retry (exponential backoff)
                    int delayMs = 1000 * retryCount; // 1s, 2s, 3s
                    log_w("Retryable error: %s (HTTP %d). Waiting %dms before retry...",
                          errorMessage.c_str(), httpCode, delayMs);
                    vTaskDelay(pdMS_TO_TICKS(delayMs));
                }
            }

            if (!errorMessage.empty()) {
                log_e("Navigation request failed after %d attempts: %s",
                      retryCount > 0 ? retryCount : 1, errorMessage.c_str());

                int64_t elapsedMs = (esp_timer_get_time() - navigationStartTime) / 1000;
                auto failedTelemetryData = std::unordered_map<std::string, TelemetryVariant>{
                    {"event", std::string("book-navigate")},
                    {"action", std::string("navigate-failed")},
                    {"kindle", true},
                    {"navigate_to", navigateTo},
                    {"preview_to", actualPreviewTo},
                    {"po", static_cast<int>(place->getOffset())},
                    {"error", errorMessage},
                    {"elapsed_ms", static_cast<int>(elapsedMs)},
                    {"retry_count", retryCount > 0 ? retryCount : 0},
                    {"all_retries_failed", true}};
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
                if (actualPreviewTo > navigateTo) {
                    ocrTextLength = response->ocrText.size();
                    // When we navigate with preview, the response contains content for the preview
                    // page preview_to=1 means we're getting content for page 1 (not page 0)
                    int serverPageNumber = actualPreviewTo;

                    // Log first 50 chars of content to see what page it really is
                    std::string fullText = std::string(response->ocrText);
                    std::string preview =
                        fullText.substr(0, std::min(static_cast<size_t>(50), fullText.size()));
                    log_i("\033[1;36m>>> PREVIEW RESPONSE: navigate_to=%d, preview_to=%d, "
                          "storing_as_page=%d, content_preview='%s...'\033[0m",
                          navigateTo, actualPreviewTo, serverPageNumber, preview.c_str());

                    pageStream_->appendPageFromResponse(*response, serverPageNumber);
                    // Update the last previewed page to match the server page we just stored
                    lastPreviewedPage_ = serverPageNumber;
                    log_i("\033[1;36m>>> APPENDED PAGE: server_page=%d, content_length=%zu, "
                          "buffer_size_now=%zu\033[0m",
                          serverPageNumber, ocrTextLength, pageStream_->size());
                    // Debug: show full buffer state
                    pageStream_->debugDumpPageMappings();

                    // Check if we need one more preview page after successfully appending this one
                    // Only request one additional page at a time to avoid complexity
                    constexpr size_t bufferThresholdBytes = 1250;
                    size_t bytesRemainingNow = pageStream_->getBytesRemainingFrom(currentOffset);

                    if (bytesRemainingNow < bufferThresholdBytes && !atEndOfContent) {
                        // We need one more preview page - request the next page
                        int nextPreviewPage = lastPreviewedPage_ + 1;
                        log_i("\033[1;35m>>> BUFFER LOW: %zu bytes remaining < %zu threshold, "
                              "requesting one more preview page: preview_to=%d\033[0m",
                              bytesRemainingNow, bufferThresholdBytes, nextPreviewPage);

                        // Make one additional preview request (navigate_to stays the same)
                        auto nextResponse = std::make_shared<KindlePageResponse>();

                        // Retry logic for additional preview request
                        constexpr int maxPreviewRetries = 2;
                        int previewRetryCount = 0;
                        std::string nextErrorMessage;
                        int nextHttpCode = 0;

                        while (previewRetryCount < maxPreviewRetries) {
                            if (previewRetryCount > 0) {
                                log_w("Retrying additional preview request (attempt %d/%d): "
                                      "preview_to=%d",
                                      previewRetryCount + 1, maxPreviewRetries, nextPreviewPage);
                            }

                            nextHttpCode = SindarinAPI::getInstance()->navigateKindle(
                                kindleBook_->title, nextResponse.get(), cancelFlag, useBle_,
                                navigateTo, nextPreviewPage);

                            if (CancelFlag::isCancelled(cancelFlag)) {
                                break;
                            }

                            nextErrorMessage = checkForError(nextHttpCode, nextResponse);

                            if (nextErrorMessage.empty()) {
                                if (previewRetryCount > 0) {
                                    log_i("Additional preview request succeeded after %d retries",
                                          previewRetryCount);
                                }
                                break;
                            }

                            // Check if retryable
                            bool isRetryable = (nextErrorMessage == "CONNECTION_ERROR" ||
                                                nextErrorMessage == "TIMEOUT" ||
                                                nextHttpCode == 0 || nextHttpCode >= 500);

                            if (!isRetryable) {
                                break;
                            }

                            previewRetryCount++;
                            if (previewRetryCount < maxPreviewRetries) {
                                int delayMs = 1000 * previewRetryCount;
                                log_w("Preview request failed: %s. Waiting %dms before retry...",
                                      nextErrorMessage.c_str(), delayMs);
                                vTaskDelay(pdMS_TO_TICKS(delayMs));
                            }
                        }

                        if (!CancelFlag::isCancelled(cancelFlag)) {
                            if (nextErrorMessage.empty() && !checkForLastReadDialog(nextResponse)) {
                                size_t nextOcrTextLength = nextResponse->ocrText.size();

                                if (nextOcrTextLength > 0) {
                                    // Log preview of next page content
                                    std::string nextFullText = std::string(nextResponse->ocrText);
                                    std::string nextPreview = nextFullText.substr(
                                        0, std::min(static_cast<size_t>(50), nextFullText.size()));
                                    log_i("\033[1;36m>>> ADDITIONAL PREVIEW LOADED: preview_to=%d, "
                                          "content_preview='%s...'\033[0m",
                                          nextPreviewPage, nextPreview.c_str());

                                    pageStream_->appendPageFromResponse(*nextResponse,
                                                                        nextPreviewPage);
                                    lastPreviewedPage_ = nextPreviewPage;
                                    ocrTextLength += nextOcrTextLength; // Add to total loaded

                                    log_i("\033[1;36m>>> APPENDED ADDITIONAL PAGE: server_page=%d, "
                                          "content_length=%zu, buffer_size_now=%zu\033[0m",
                                          nextPreviewPage, nextOcrTextLength, pageStream_->size());
                                    pageStream_->debugDumpPageMappings();
                                } else {
                                    log_i("Received empty preview page %d - may be at end of book",
                                          nextPreviewPage);
                                }
                            } else if (!nextErrorMessage.empty()) {
                                log_w("Failed to load additional preview page %d: %s",
                                      nextPreviewPage, nextErrorMessage.c_str());
                            }
                        }
                    }
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
                {"preview_to", actualPreviewTo},
                {"po", static_cast<int>(place->getOffset())},
                {"ocr_text_length", static_cast<int>(ocrTextLength)},
                {"elapsed_ms", static_cast<int>(elapsedMs)}};

            // Add retry count if there were retries
            if (retryCount > 0) {
                successTelemetryData["retry_count"] = retryCount;
            }

            telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                                 successTelemetryData);

            // Always signal that we got content (to dismiss loading)
            if (gotContentFunction_) {
                log_i("Calling gotContentFunction after successful navigation (ocrTextLength=%zu)",
                      ocrTextLength);
                gotContentFunction_(ocrTextLength);
            } else {
                log_w("gotContentFunction_ is null after successful navigation");
            }

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

            // Update sync position and call callback when navigating with cached content
            auto currentOffset = place->getOffset();
            auto previousOffset = serverSyncPlace_ ? serverSyncPlace_->getOffset() : 0;
            
            log_d("Checking cached navigation callback: currentOffset=%u, previousOffset=%u, "
                  "gotContentFunction_=%p", currentOffset, previousOffset, 
                  gotContentFunction_ ? (void*)1 : nullptr);
            
            if (currentOffset != previousOffset) {
                // Update server sync position even when using cached content
                serverSyncPlace_ = std::dynamic_pointer_cast<TxtPlace>(place);
                
                // Call gotContentFunction for UI updates and tests
                if (gotContentFunction_) {
                    size_t contentLength = pageStream_->getOcrTextLength();
                    log_i("Calling gotContentFunction for position change with cached content "
                          "(offset %u -> %u, length=%zu)",
                          previousOffset, currentOffset, contentLength);
                    gotContentFunction_(contentLength);
                } else {
                    log_w("gotContentFunction_ is null when navigating with cached content");
                }
            }
        }
    });
}
