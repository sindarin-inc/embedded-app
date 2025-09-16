#include "Models/KindlePageFlipper.hpp"

#include <cinttypes>
#include <esp_timer.h>
#include <utility>

#include "Communications/SindarinAPI.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/Telemetry.hpp"
#include "sindarin-debug.h"

// Buffer threshold for determining when to fetch more content
static constexpr size_t KINDLE_BUFFER_THRESHOLD_BYTES = 1500;

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
    -> std::shared_ptr<KindleError> {
    // First assume all is well
    if (httpCode == 200 && response->success && response->authenticated) {
        return nullptr;
    }

    // Check for session key mismatch error
    if (httpCode == 400 && response->error == KINDLE_ERROR_SESSION_KEY_MISMATCH) {
        return std::make_shared<KindleError>(
            KindleError{KindleErrorType::SessionKeyMismatch, std::string(response->message)});
    }

    // Check for authentication failure in response
    if ((httpCode == 200 || httpCode == 401) && !response->authenticated) {
        // When authenticated: false and we have kindle.json, use the message from response
        std::string message =
            !response->message.empty()
                ? std::string(response->message)
                : "Your Kindle session has expired. Please try signing in on the mobile app again.";
        return std::make_shared<KindleError>(
            KindleError{KindleErrorType::AuthenticationError, message});
    }

    // Check for connection issues
    if (httpCode <= 0) {
        return std::make_shared<KindleError>(
            KindleError{KindleErrorType::ConnectionError, "Network connection error"});
    } else if (httpCode == 401 || httpCode == 403) {
        return std::make_shared<KindleError>(KindleError{
            KindleErrorType::AuthenticationError,
            "Your Kindle session has expired. Please try signing in on the mobile app again."});
    } else if (httpCode == 404) {
        return std::make_shared<KindleError>(KindleError{
            KindleErrorType::NotFoundError, "Book or page not found in your Kindle library."});
    } else if (httpCode >= 400 && httpCode < 500) {
        // Use server's error message if available
        std::string message = !response->message.empty()
                                  ? std::string(response->message)
                                  : "Request error. Please check your settings.";
        return std::make_shared<KindleError>(KindleError{KindleErrorType::ClientError, message});
    } else if (httpCode >= 500) {
        // Use server's error message if available
        std::string message = !response->message.empty() ? std::string(response->message)
                                                         : "Server error. Please try again later.";
        return std::make_shared<KindleError>(KindleError{KindleErrorType::ServerError, message});
    } else {
        // Use server's error message if available
        std::string message = !response->message.empty() ? std::string(response->message)
                                                         : "Failed to load book content.";
        return std::make_shared<KindleError>(KindleError{KindleErrorType::UnknownError, message});
    }
}

auto KindlePageFlipper::navigateWithRetry(int navigateTo, int previewTo,
                                          std::shared_ptr<KindlePageResponse> &response,
                                          const std::shared_ptr<CancelFlag> &cancelFlag,
                                          int maxRetries) -> std::shared_ptr<KindleError> {
    int retryCount = 0;
    std::shared_ptr<KindleError> error;
    int httpCode = 0;
    int64_t startTime = esp_timer_get_time();

    while (retryCount < maxRetries) {
        if (retryCount > 0) {
            log_w("Retrying navigation request (attempt %d/%d): navigate_to=%d, preview_to=%d",
                  retryCount + 1, maxRetries, navigateTo, previewTo);

            // Log telemetry for retry attempt
            auto retryTelemetryData = std::unordered_map<std::string, TelemetryVariant>{
                {"event", std::string("book-navigate")},
                {"action", std::string("navigate-retry")},
                {"kindle", true},
                {"navigate_to", navigateTo},
                {"retry_attempt", retryCount + 1},
                {"max_retries", maxRetries},
                {"elapsed_ms", static_cast<int>((esp_timer_get_time() - startTime) / 1000)}};
            if (previewTo >= 0) {
                retryTelemetryData["preview_to"] = previewTo;
            }
            telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                                 retryTelemetryData);
        }

        int64_t attemptStartTime = esp_timer_get_time();
        httpCode = SindarinAPI::getInstance()->navigateKindle(kindleBook_->title, response.get(),
                                                              cancelFlag, useBle_, navigateTo,
                                                              previewTo, bookSessionKey_);

        if (CancelFlag::isCancelled(cancelFlag)) {
            return nullptr; // Caller must check cancelFlag separately
        }

        error = checkForError(httpCode, response);

        if (!error) {
            // Update the book session key if we got a new one (happy path)
            if (!response->bookSessionKey.empty() && response->bookSessionKey != bookSessionKey_) {
                bookSessionKey_ = response->bookSessionKey;
                log_i("Updated book_session_key: %s", bookSessionKey_.c_str());
            }

            if (retryCount > 0) {
                log_i("Navigation request succeeded after %d retries", retryCount);
                // Log success after retry
                auto successTelemetryData = std::unordered_map<std::string, TelemetryVariant>{
                    {"event", std::string("book-navigate")},
                    {"action", std::string("navigate-retry-success")},
                    {"kindle", true},
                    {"navigate_to", navigateTo},
                    {"retry_attempt", retryCount + 1},
                    {"attempt_elapsed_ms",
                     static_cast<int>((esp_timer_get_time() - attemptStartTime) / 1000)},
                    {"total_elapsed_ms",
                     static_cast<int>((esp_timer_get_time() - startTime) / 1000)}};
                if (previewTo >= 0) {
                    successTelemetryData["preview_to"] = previewTo;
                }
                telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                                     successTelemetryData);
            }
            return error; // Success
        }

        // Handle session key mismatch specially
        if (error->type == KindleErrorType::SessionKeyMismatch) {
            // The server provides the correct session key in the current_session_key field
            if (!response->currentSessionKey.empty()) {
                log_w("Session key mismatch detected. Resetting position and retrying with new "
                      "session key");
                bookSessionKey_ = response->currentSessionKey;
                log_i("Updated book_session_key from mismatch response: %s",
                      bookSessionKey_.c_str());

                // Reset our tracking since server has reset to position 0
                lastNavigatedPage_ = 0;
                lastPreviewedPage_ = 0;

                // Clear the page stream to reflect the reset
                pageStream_->clear();

                // Log telemetry for session key mismatch
                telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                                     {{"event", std::string("book-navigate")},
                                      {"action", std::string("session-key-mismatch")},
                                      {"kindle", true},
                                      {"navigate_to", navigateTo},
                                      {"retry_attempt", retryCount + 1}});

                // This is a retryable error, so continue with retry logic below
            } else {
                log_e("Session key mismatch but no new session key provided");
                return std::make_shared<KindleError>(
                    KindleError{KindleErrorType::ClientError, "Session out of sync with server"});
            }
        }

        // Check if this is a retryable error
        if (!error->isRetryable()) {
            log_e("Non-retryable error: %s (HTTP %d)", error->message.c_str(), httpCode);
            return error;
        }

        // Log telemetry for retryable error
        auto failedTelemetryData = std::unordered_map<std::string, TelemetryVariant>{
            {"event", std::string("book-navigate")},
            {"action", std::string("navigate-retry-failed")},
            {"kindle", true},
            {"navigate_to", navigateTo},
            {"retry_attempt", retryCount + 1},
            {"http_code", httpCode},
            {"error", error->message},
            {"will_retry", retryCount + 1 < maxRetries},
            {"attempt_elapsed_ms",
             static_cast<int>((esp_timer_get_time() - attemptStartTime) / 1000)}};
        if (previewTo >= 0) {
            failedTelemetryData["preview_to"] = previewTo;
        }
        telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                             failedTelemetryData);

        retryCount++;
        if (retryCount < maxRetries) {
            int delayMs = 1000 * retryCount; // Exponential backoff
            log_w("Retryable error: %s (HTTP %d). Waiting %dms before retry...",
                  error->message.c_str(), httpCode, delayMs);
            vTaskDelay(pdMS_TO_TICKS(delayMs));
        }
    }

    log_e("Navigation request failed after %d attempts: %s", maxRetries, error->message.c_str());
    return error;
}

auto KindlePageFlipper::openBookWithRetry(std::shared_ptr<KindlePageResponse> &response,
                                          const std::shared_ptr<CancelFlag> &cancelFlag,
                                          int maxRetries) -> std::shared_ptr<KindleError> {
    int retryCount = 0;
    std::shared_ptr<KindleError> error;
    int httpCode = 0;
    int64_t startTime = esp_timer_get_time();

    while (retryCount < maxRetries) {
        if (retryCount > 0) {
            log_w("Retrying open book request (attempt %d/%d): %s", retryCount + 1, maxRetries,
                  kindleBook_->title.c_str());

            // Log telemetry for retry attempt
            telemetry_.logMetric(
                TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                {{"event", std::string("book-open")},
                 {"action", std::string("open-retry")},
                 {"kindle", true},
                 {"retry_attempt", retryCount + 1},
                 {"max_retries", maxRetries},
                 {"elapsed_ms", static_cast<int>((esp_timer_get_time() - startTime) / 1000)}});
        }

        int64_t attemptStartTime = esp_timer_get_time();
        httpCode = SindarinAPI::getInstance()->openKindleBook(kindleBook_->title, response.get(),
                                                              cancelFlag, useBle_);

        if (CancelFlag::isCancelled(cancelFlag)) {
            return nullptr; // Caller must check cancelFlag separately
        }

        // Update the book session key if we got a new one
        if (!response->bookSessionKey.empty() && response->bookSessionKey != bookSessionKey_) {
            bookSessionKey_ = response->bookSessionKey;
            log_i("Updated book_session_key: %s", bookSessionKey_.c_str());
        }

        error = checkForError(httpCode, response);

        if (!error) {
            if (retryCount > 0) {
                log_i("Open book request succeeded after %d retries", retryCount);
                // Log success after retry
                telemetry_.logMetric(
                    TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                    {{"event", std::string("book-open")},
                     {"action", std::string("open-retry-success")},
                     {"kindle", true},
                     {"retry_attempt", retryCount + 1},
                     {"attempt_elapsed_ms",
                      static_cast<int>((esp_timer_get_time() - attemptStartTime) / 1000)},
                     {"total_elapsed_ms",
                      static_cast<int>((esp_timer_get_time() - startTime) / 1000)}});
            }
            return error; // Success
        }

        // Check if this is a retryable error
        if (!error->isRetryable()) {
            log_e("Non-retryable error: %s (HTTP %d)", error->message.c_str(), httpCode);
            return error;
        }

        // Log telemetry for retryable error
        telemetry_.logMetric(
            TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
            {{"event", std::string("book-open")},
             {"action", std::string("open-retry-failed")},
             {"kindle", true},
             {"retry_attempt", retryCount + 1},
             {"http_code", httpCode},
             {"error", error->message},
             {"will_retry", retryCount + 1 < maxRetries},
             {"attempt_elapsed_ms",
              static_cast<int>((esp_timer_get_time() - attemptStartTime) / 1000)}});

        retryCount++;
        if (retryCount < maxRetries) {
            int delayMs = 1000 * retryCount; // Exponential backoff
            log_w("Retryable error: %s (HTTP %d). Waiting %dms before retry...",
                  error->message.c_str(), httpCode, delayMs);
            vTaskDelay(pdMS_TO_TICKS(delayMs));
        }
    }

    log_e("Open book request failed after %d attempts: %s", maxRetries,
          error ? error->message.c_str() : "unknown");
    return error;
}

void KindlePageFlipper::fetchAdditionalPagesIfNeeded(
    uint32_t currentOffset, int navigateTo, const std::shared_ptr<CancelFlag> &cancelFlag) {
    // Continue fetching additional pages while buffer is low, up to 6 additional pages
    const int maxAdditionalPages = 6;
    int additionalPagesFetched = 0;
    size_t bytesRemainingNow = pageStream_->getBytesRemainingFrom(currentOffset);

    while (bytesRemainingNow < KINDLE_BUFFER_THRESHOLD_BYTES &&
           additionalPagesFetched < maxAdditionalPages && !CancelFlag::isCancelled(cancelFlag)) {

        int nextPreviewPage = lastPreviewedPage_ + 1;

        // Always use the current reading position for navigate_to
        auto currentNavigateTo = pageStream_->getServerPageNumberForOffset(currentOffset);
        if (currentNavigateTo < 0) {
            if (pageStream_->size() == 0 || lastPreviewedPage_ < 0) {
                log_w("Cannot determine server page for offset %u, buffer empty; skipping",
                      currentOffset);
                break; // No local context to base a request on
            }

            // We're at the end of the buffered content—fall back to the last page we added.
            currentNavigateTo = lastPreviewedPage_;
            log_i("Using last previewed page %d for boundary offset %u", currentNavigateTo,
                  currentOffset);
        }

        log_i("→ Buffer low (%zu bytes), requesting page %d (additional page %d/%d)",
              bytesRemainingNow, nextPreviewPage, additionalPagesFetched + 1, maxAdditionalPages);

        auto nextResponse = std::make_shared<KindlePageResponse>();
        auto nextError =
            navigateWithRetry(currentNavigateTo, nextPreviewPage, nextResponse, cancelFlag, 2);

        if (CancelFlag::isCancelled(cancelFlag)) {
            break;
        }

        if (nextError) {
            log_w("Failed to load additional preview page %d: %s", nextPreviewPage,
                  nextError->message.c_str());
            break; // Stop fetching on error
        }

        if (checkForLastReadDialog(nextResponse)) {
            continue;
        }

        size_t nextOcrTextLength = nextResponse->ocrText.size();

        if (nextOcrTextLength == 0) {
            log_i("Received empty preview page %d - may be at end of book", nextPreviewPage);
            break; // Stop fetching if we get an empty page
        }

        pageStream_->appendPageFromResponse(*nextResponse, nextPreviewPage);
        lastPreviewedPage_ = nextPreviewPage;
        bytesRemainingNow = pageStream_->getBytesRemainingFrom(currentOffset);
        log_i("← Added page %d: +%zu bytes (buffer now: %zu bytes)", nextPreviewPage,
              nextOcrTextLength, bytesRemainingNow);
        additionalPagesFetched++;

        // Notify UI that we have more content available after each page fetch
        if (gotContentFunction_) {
            gotContentFunction_(pageStream_->getOcrTextLength());
        }
    }

    if (additionalPagesFetched > 0) {
        log_i("Fetched %d additional pages, buffer now: %zu bytes", additionalPagesFetched,
              bytesRemainingNow);
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

    // Reset navigation tracking when loading initial page
    lastNavigatedPage_ = -1;
    lastPreviewedPage_ = -1;
    bookSessionKey_.clear();

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

        auto error = openBookWithRetry(response, cancelFlag, 2);
        if (CancelFlag::isCancelled(cancelFlag)) {
            return;
        }

        if (error) {
            log_w("Error loading initial page: %s", error->message.c_str());

            int64_t elapsedMs = (esp_timer_get_time() - bookOpenStartTime) / 1000;
            telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                                 {{"event", std::string("book-open")},
                                  {"action", std::string("open-failed")},
                                  {"kindle", true},
                                  {"error", error->message},
                                  {"elapsed_ms", static_cast<int>(elapsedMs)}});

            // Only propagate non-connection errors
            if (!error->isConnectionError()) {
                onErrorFunction_(error->message);
            } else {
                log_i("Network connection issue detected, initiating reconnection");
                if (onConnectionErrorFunction_) {
                    onConnectionErrorFunction_();
                }
            }
            return;
        }

        // Store the new book session key
        if (!response->bookSessionKey.empty()) {
            bookSessionKey_ = response->bookSessionKey;
        }

        auto hasDialog = checkForLastReadDialog(response);

        size_t ocrTextLength = 0;
        // If this is a last read dialog, don't append the page (which would be empty)
        if (!hasDialog) {
            ocrTextLength = response->ocrText.size();

            pageStream_->appendPageFromResponse(*response, 0);
            // Update both navigation and preview state since open-book gives us page 0
            lastNavigatedPage_ = 0;
            lastPreviewedPage_ = 0;
        }

        // Log successful book open
        int64_t elapsedMs = (esp_timer_get_time() - bookOpenStartTime) / 1000;
        telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                             {{"event", std::string("book-open")},
                              {"action", std::string("open-success")},
                              {"kindle", true},
                              {"ocr_text_length", static_cast<int>(ocrTextLength)},
                              {"has_dialog", hasDialog},
                              {"elapsed_ms", static_cast<int>(elapsedMs)}});

        // Notify UI that initial content is ready to display
        if (gotContentFunction_) {
            gotContentFunction_(ocrTextLength);
        }

        // Fetch additional pages in the background if needed
        if (!hasDialog && ocrTextLength > 0) {
            DispatchGetCore0BackgroundQueue()->dispatch(
                [this, cancelFlag]() { fetchAdditionalPagesIfNeeded(0, 0, cancelFlag); });
        }
    });
}

void KindlePageFlipper::updatePlace(const std::shared_ptr<Place> &place) {
    auto cancelFlag = cancelFlag_;

    // Perform the logic in a queue so we always have the latest data before making API calls
    DispatchGetCore0BackgroundQueue()->dispatch([this, place, cancelFlag]() {
        if (CancelFlag::isCancelled(cancelFlag)) {
            log_i("Cancelled while updating place");
            return;
        }

        if (lastServerSyncPlace_ == nullptr) {
            lastServerSyncPlace_ = std::dynamic_pointer_cast<TxtPlace>(place);
        }
        if (lastRenderedPlace_ == nullptr) {
            lastRenderedPlace_ = std::dynamic_pointer_cast<TxtPlace>(place);
        }

        log_i("Updating place from %s to %s", lastServerSyncPlace_->str().c_str(),
              place->str().c_str());

        // Calculate buffer state
        uint32_t currentOffset = place->getOffset();
        size_t bytesRemaining = pageStream_->getBytesRemainingFrom(currentOffset);

        // Get the server page number for our current reading position
        auto serverPageForCurrentOffset =
            pageStream_->getServerPageNumberForOffset(place->getOffset());

        // Absolute navigation with navigate_to and preview_to. Navigate to the server page that
        // corresponds to our current reading position, which is between the page boundaries set
        // by the preview_to=<page number> parameter earlier.
        auto navigateTo = serverPageForCurrentOffset >= 0 ? serverPageForCurrentOffset : 0;

        // For preview, we must load sequentially - always the next page after what we have
        // preview_to must be sequential: 1, 2, 3, 4... can't skip
        auto previewTo = lastPreviewedPage_ + 1;

        // Core state checks:
        bool navigationChanged = (navigateTo != lastNavigatedPage_);
        bool needMoreBuffer = (bytesRemaining < KINDLE_BUFFER_THRESHOLD_BYTES);

        // When navigation changes without preview, we get no new content,
        // so check if we'll need buffer after the navigation-only response
        if (navigationChanged && !needMoreBuffer) {
            // Calculate how many bytes will remain after we move to navigateTo position
            int navigateToOffset = pageStream_->getOffsetForServerPageNumber(navigateTo);

            // If we found the offset, calculate remaining bytes from that position
            if (navigateToOffset >= 0) {
                size_t bytesFromNavigateTo = pageStream_->getBytesRemainingFrom(navigateToOffset);
                needMoreBuffer = (bytesFromNavigateTo < KINDLE_BUFFER_THRESHOLD_BYTES);
                log_d(
                    "Navigation would leave %zu bytes from navigate_to=%d position, needBuffer=%d",
                    bytesFromNavigateTo, navigateTo, needMoreBuffer);
            }
        }

        // Make a request if we changed pages or need more buffer
        bool shouldMakeRequest = navigationChanged || needMoreBuffer;

        log_d("Navigation analysis: navigate=%d, preview=%d, lastNav=%d, lastPrev=%d, "
              "navChanged=%d, needBuffer=%d (bytes: %zu/%zu), serverPage=%d",
              navigateTo, previewTo, lastNavigatedPage_, lastPreviewedPage_, navigationChanged,
              needMoreBuffer, bytesRemaining, KINDLE_BUFFER_THRESHOLD_BYTES,
              serverPageForCurrentOffset);

        if (!shouldMakeRequest) {
            // Update rendered position and call callback when navigating with cached content
            auto currentOffset = place->getOffset();
            auto previousOffset = lastRenderedPlace_ ? lastRenderedPlace_->getOffset() : 0;

            log_d("Checking cached navigation callback: currentOffset=%u, previousOffset=%u, "
                  "gotContentFunction_=%s",
                  (unsigned int)currentOffset, (unsigned int)previousOffset,
                  gotContentFunction_ ? "set" : "null");
            if (currentOffset != previousOffset) {
                // Update rendered position when using cached content
                lastRenderedPlace_ = std::dynamic_pointer_cast<TxtPlace>(place);

                if (gotContentFunction_) {
                    size_t contentLength = pageStream_->getOcrTextLength();
                    gotContentFunction_(contentLength);
                }
            }

            return;
        }

        if (needMoreBuffer) {
            log_i("→ Request: navigate_to=%d, preview_to=%d (buffer: %zu bytes)", navigateTo,
                  previewTo, bytesRemaining);
        } else {
            log_i("→ Request: navigate_to=%d (buffer: %zu bytes, no preview)", navigateTo,
                  bytesRemaining);
        }

        // Track navigation start time
        int64_t navigationStartTime = esp_timer_get_time();

        auto beginTelemetryData = std::unordered_map<std::string, TelemetryVariant>{
            {"event", std::string("book-navigate")},
            {"action", std::string("navigate-begin")},
            {"kindle", true},
            {"navigate_to", navigateTo},
            {"po", static_cast<int>(place->getOffset())},
            {"elapsed_ms", 0}};
        if (needMoreBuffer) {
            beginTelemetryData["preview_to"] = previewTo;
        }
        telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                             beginTelemetryData);

        auto response = std::make_shared<KindlePageResponse>();
        auto error =
            navigateWithRetry(navigateTo, needMoreBuffer ? previewTo : -1, response, cancelFlag);

        if (CancelFlag::isCancelled(cancelFlag)) {
            return;
        }

        if (error) {

            int64_t elapsedMs = (esp_timer_get_time() - navigationStartTime) / 1000;
            auto failedTelemetryData = std::unordered_map<std::string, TelemetryVariant>{
                {"event", std::string("book-navigate")},
                {"action", std::string("navigate-failed")},
                {"kindle", true},
                {"navigate_to", navigateTo},
                {"po", static_cast<int>(place->getOffset())},
                {"error", error->message},
                {"elapsed_ms", static_cast<int>(elapsedMs)}};
            if (needMoreBuffer) {
                failedTelemetryData["preview_to"] = previewTo;
            }
            telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                                 failedTelemetryData);

            // Only propagate non-connection errors
            if (!error->isConnectionError()) {
                onErrorFunction_(error->message);
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
            if (needMoreBuffer) {
                ocrTextLength = response->ocrText.size();
                // When we navigate with preview, the response contains content for the preview
                // page
                int serverPageNumber = previewTo;

                pageStream_->appendPageFromResponse(*response, serverPageNumber);
                lastPreviewedPage_ = serverPageNumber;
                log_i("← Added page %d: +%zu bytes (buffer now: %zu bytes)", serverPageNumber,
                      ocrTextLength, pageStream_->getBytesRemainingFrom(currentOffset));

                // Fetch additional pages in the background so UI can update immediately
                DispatchGetCore0BackgroundQueue()->dispatch(
                    [this, currentOffset, navigateTo, cancelFlag]() {
                    fetchAdditionalPagesIfNeeded(currentOffset, navigateTo, cancelFlag);
                });
            }
            // Update the last navigated page to reflect what we told the server
            lastNavigatedPage_ = navigateTo;
        }

        int64_t elapsedMs = (esp_timer_get_time() - navigationStartTime) / 1000;
        auto successTelemetryData = std::unordered_map<std::string, TelemetryVariant>{
            {"event", std::string("book-navigate")},
            {"action", std::string("navigate-success")},
            {"kindle", true},
            {"navigate_to", navigateTo},
            {"po", static_cast<int>(place->getOffset())},
            {"ocr_text_length", static_cast<int>(ocrTextLength)},
            {"elapsed_ms", static_cast<int>(elapsedMs)}};
        if (needMoreBuffer) {
            successTelemetryData["preview_to"] = previewTo;
        }

        telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                             successTelemetryData);

        if (gotContentFunction_) {
            gotContentFunction_(ocrTextLength);
        }

        // Update both positions after successful navigation
        // lastServerSyncPlace_ tracks what the server knows
        // lastRenderedPlace_ tracks what we've displayed to the user
        lastServerSyncPlace_ = std::dynamic_pointer_cast<TxtPlace>(place);
        lastRenderedPlace_ = std::dynamic_pointer_cast<TxtPlace>(place);
    });
}
