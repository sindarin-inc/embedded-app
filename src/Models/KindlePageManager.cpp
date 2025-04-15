#include "Models/KindlePageManager.hpp"

#include <cinttypes>

#include "Communications/SindarinAPI.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/StringUtil.hpp"
#include "sindarin-debug.h"

KindlePageManager::KindlePageManager(const std::shared_ptr<KindleBook> &kindleBook)
    : kindleBook_(kindleBook) {
    log_i("Created KindlePageManager for book: %s by %s", kindleBook->title.c_str(),
          kindleBook->author.c_str());

    // Initialize with lastCallWasPreview_=false so first call can be a preview
    lastCallWasPreview_ = false;
    previewRequestInProgress_ = false;
    lastPreviewOffset_ = 0;
}

void KindlePageManager::loadInitialPage(
    const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle,
    std::function<void(bool success, const std::shared_ptr<KindlePageResponse> &,
                       const std::string &)>
        callback) {

    log_i("Loading initial page for Kindle book: %s", kindleBook_->title.c_str());

    auto requestFunc = [this](SindarinAPI *api, KindlePageResponse *response,
                              const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle, bool) {
        return api->openKindleBook(kindleBook_->title, response, cancelFlag, useBle);
    };

    makeRequest(requestFunc, cancelFlag, useBle,
                [this, callback](bool success, const std::shared_ptr<KindlePageResponse> &response,
                                 const std::string &errorMessage) {
        if (success) {
            // Initial open book call is like a navigate
            lastCallWasPreview_ = false;
            // Reset any preview-related flags on initial load
            previewRequestInProgress_ = false;
            lastPreviewOffset_ = 0;

            log_i("Initial page loaded successfully. Current page: %d/%d (%d%%)",
                  response->progress.currentPage, response->progress.totalPages,
                  response->progress.percentage);

            updateBookMetadata(response);
        }

        callback(success, response, errorMessage);
    });
}

void KindlePageManager::loadNextPreview(
    const std::shared_ptr<Place> &currentPlace, const std::shared_ptr<CancelFlag> &cancelFlag,
    bool useBle,
    std::function<void(bool success, const std::shared_ptr<KindlePageResponse> &,
                       const std::string &)>
        callback) {

    if (previewRequestInProgress_) {
        log_w("Preview request already in progress, ignoring new request");
        callback(false, nullptr, "Preview request already in progress");
        return;
    }

    if (lastCallWasPreview_) {
        log_w("Last call was already a preview, must call navigate first");
        callback(false, nullptr, "Must call navigate first");
        return;
    }

    previewRequestInProgress_ = true;

    uint32_t currentOffset = currentPlace ? currentPlace->getOffset() : 0;

    log_i("Loading next preview for Kindle book at offset %" PRIu32, currentOffset);

    auto requestFunc = [](SindarinAPI *api, KindlePageResponse *response,
                          const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle,
                          bool) { return api->previewKindleNext(response, cancelFlag, useBle); };

    makeRequest(requestFunc, cancelFlag, useBle,
                [this, currentOffset, callback](bool success,
                                                const std::shared_ptr<KindlePageResponse> &response,
                                                const std::string &errorMessage) {
        previewRequestInProgress_ = false;

        if (success && response && !response->ocrText.empty()) {
            // Mark that we've received preview content
            lastCallWasPreview_ = true;

            // Set the preview boundary to be the current position + text size
            // This marks where the new preview content begins in the buffer
            lastPreviewOffset_ = currentOffset + response->ocrText.length();

            // log_i("Preview loaded successfully. New text size: %zu bytes, Preview boundary set
            // at: %" PRIu32,
            //       response->ocrText.length(), lastPreviewOffset_);

            updateBookMetadata(response);
        } else {
            log_w("Preview request failed or returned empty response");
        }

        callback(success, response, errorMessage);
    });
}

void KindlePageManager::syncWithServer(
    const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle,
    std::function<void(bool success, const std::shared_ptr<KindlePageResponse> &,
                       const std::string &)>
        callback) {

    if (previewRequestInProgress_) {
        log_w("Preview request in progress, cannot sync with server yet");
        callback(false, nullptr, "Preview request in progress");
        return;
    }

    log_i("Syncing with server - preview boundary: %" PRIu32 ", lastCallWasPreview: %d",
          lastPreviewOffset_, lastCallWasPreview_);

    auto requestFunc = [](SindarinAPI *api, KindlePageResponse *response,
                          const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle, bool) {
        // Use ocr=false to sync server state without getting content
        return api->navigateKindleNext(response, cancelFlag, useBle, false);
    };

    makeRequest(requestFunc, cancelFlag, useBle,
                [this, callback](bool success, const std::shared_ptr<KindlePageResponse> &response,
                                 const std::string &errorMessage) {
        if (success) {
            // Mark that the last API call was a navigate
            lastCallWasPreview_ = false;

            // Reset the preview boundary after successful syncing
            // This prevents us from re-syncing against the same boundary
            log_i("Reset preview boundary from %" PRIu32 " to 0 after successful sync",
                  lastPreviewOffset_);
            lastPreviewOffset_ = 0;

            updateBookMetadata(response);
        }

        callback(success, response, errorMessage);
    });
}

bool KindlePageManager::hasCrossedPreviewBoundary(uint32_t currentOffset) const {
    return (lastPreviewOffset_ > 0 && currentOffset >= lastPreviewOffset_);
}

bool KindlePageManager::wouldNextTurnCrossBoundary(uint32_t currentOffset,
                                                   uint32_t pageSize) const {
    if (lastPreviewOffset_ <= 0) {
        return false; // No boundary to cross
    }

    uint32_t nextTurnOffset = currentOffset + pageSize;
    return (nextTurnOffset >= lastPreviewOffset_);
}

bool KindlePageManager::needsNavigateCall(uint32_t currentOffset) const {
    // If we've crossed a boundary, we MUST sync with server
    if (hasCrossedPreviewBoundary(currentOffset)) {
        return true;
    }

    // Otherwise, follow strict alternation
    return lastCallWasPreview_;
}

void KindlePageManager::resetPreviewBoundary() { lastPreviewOffset_ = 0; }

void KindlePageManager::setPreviewBoundary(uint32_t offset) { lastPreviewOffset_ = offset; }

uint32_t KindlePageManager::getPreviewBoundary() const { return lastPreviewOffset_; }

bool KindlePageManager::isPreviewRequestInProgress() const { return previewRequestInProgress_; }

bool KindlePageManager::wasLastCallPreview() const { return lastCallWasPreview_; }

void KindlePageManager::updateBookMetadata(const std::shared_ptr<KindlePageResponse> &response) {
    if (!response || !kindleBook_) {
        return;
    }

    // Update the book progress in the KindleBook object
    kindleBook_->progressPercent = response->progress.percentage;
    kindleBook_->progress = StringFormat("%d%%", response->progress.percentage);

    // log_i("Updated book metadata: page %d/%d (%d%%)", response->progress.currentPage,
    //       response->progress.totalPages, response->progress.percentage);
}

void KindlePageManager::makeRequest(
    const std::function<int(SindarinAPI *, KindlePageResponse *,
                            const std::shared_ptr<CancelFlag> &, bool, bool)> &requestFunc,
    const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle,
    std::function<void(bool success, const std::shared_ptr<KindlePageResponse> &,
                       const std::string &)>
        callback) {

    // Execute the API call on the background thread
    DispatchGetCore0BackgroundQueue()->dispatch([requestFunc, cancelFlag, useBle, callback]() {
        // Create a response object
        auto response = std::make_shared<KindlePageResponse>();
        auto sindarinApi = SindarinAPI::getInstance();

        // Make the request
        int httpCode = requestFunc(sindarinApi.get(), response.get(), cancelFlag, useBle, false);

        // Process the response on the UI thread
        DispatchGetUiQueue()->dispatch([httpCode, response, callback]() {
            if (httpCode != 200 || !response->success) {
                // Customize error message based on HTTP code
                std::string errorMessage;

                if (httpCode == 500) {
                    errorMessage = "Server error. Please try again later.";
                } else if (httpCode == 401 || httpCode == 403) {
                    errorMessage = "Authentication error. Please try signing in again.";
                } else if (httpCode == 404) {
                    errorMessage = "Book or page not found in your Kindle library.";
                } else if (httpCode >= 400 && httpCode < 500) {
                    errorMessage = "Request error. Please check your settings.";
                } else if (httpCode >= 500) {
                    errorMessage = "Server error. Please try again later.";
                } else {
                    errorMessage = "Failed to load book content.";
                }

                log_i("HTTP error %d when making Kindle API request", httpCode);
                callback(false, response, errorMessage);
            } else {
                // Call the callback with the successful response
                callback(true, response, "");
            }
        });
    });
}
