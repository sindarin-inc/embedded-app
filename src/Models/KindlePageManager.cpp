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
    const std::function<void(bool success, const std::shared_ptr<KindlePageResponse> &,
                             const std::string &)> &callback) {

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
    const std::function<void(bool success, const std::shared_ptr<KindlePageResponse> &,
                             const std::string &)> &callback) {

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

    // Log the current offset and preview boundary for debugging
    if (lastPreviewOffset_ > 0) {
        uint32_t distanceToBoundary =
            (currentOffset < lastPreviewOffset_) ? (lastPreviewOffset_ - currentOffset) : 0;
        log_i("[PREVIEW BOUNDARY] Loading preview at offset %" PRIu32
              ", previous boundary: %" PRIu32 ", distance to boundary: %" PRIu32 " bytes",
              currentOffset, lastPreviewOffset_, distanceToBoundary);
    } else {
        log_i("Loading next preview for Kindle book at offset %" PRIu32 " (no boundary set)",
              currentOffset);
    }

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
            uint32_t previousBoundary = lastPreviewOffset_;

            // IMPORTANT: We need to be careful with the boundary calculation, since the content
            // may be modified when appended (e.g., by adding newlines or spaces).
            // The KindleReadableScreen will correct this if needed using setPreviewBoundary()
            // but we log both the original and computed values for debugging
            uint32_t calculatedBoundary = currentOffset + response->ocrText.length();
            lastPreviewOffset_ = calculatedBoundary;

            log_i("[PREVIEW BOUNDARY INITIAL] Setting preview boundary at: %" PRIu32
                  " (offset %" PRIu32 " + content size %zu)",
                  calculatedBoundary, currentOffset, response->ocrText.length());

            // Log detailed information about the preview boundary
            log_i("[PREVIEW BOUNDARY] Preview loaded successfully: %zu bytes. "
                  "Previous boundary: %" PRIu32 ", New boundary: %" PRIu32,
                  response->ocrText.length(), previousBoundary, lastPreviewOffset_);

            // Log the first few characters of the new content to verify boundary
            if (!response->ocrText.empty()) {
                // Count newlines and calculate content lines
                int newlineCount = 0;
                for (char c : response->ocrText) {
                    if (c == '\n') {
                        newlineCount++;
                    }
                }

                // Get a larger sample of the start of content
                std::string previewStart = response->ocrText.substr(
                    0, std::min(static_cast<size_t>(50), response->ocrText.length()));
                for (char &i : previewStart) {
                    if (i == '\n') {
                        i = ' '; // Replace newlines for cleaner logging
                    }
                }

                // Get exact line numbers if present at the start
                std::string firstLineNumber;
                std::istringstream iss(previewStart);
                std::string word;
                while (iss >> word) {
                    if (std::all_of(word.begin(), word.end(), ::isdigit)) {
                        firstLineNumber = word;
                        break;
                    }
                }

                // Log content summary
                log_i("[PREVIEW CONTENT] Preview content (%zu bytes, ~%d newlines) begins with: "
                      "'%s...'",
                      response->ocrText.length(), newlineCount, previewStart.c_str());

                if (!firstLineNumber.empty()) {
                    log_i("[PREVIEW CONTENT] First line number detected: %s",
                          firstLineNumber.c_str());
                }

                // Log the exact bytes at the start
                log_i("[PREVIEW CONTENT] First 10 bytes of preview content:");
                for (int i = 0; i < std::min(static_cast<size_t>(10), response->ocrText.size());
                     i++) {
                    char c = response->ocrText[i];
                    log_i("  Byte %d: '%c' (ASCII %d)", i, (c >= 32 && c < 127) ? c : '.',
                          static_cast<int>(c) & 0xFF);
                }
            }

            updateBookMetadata(response);
        } else {
            log_w("Preview request failed or returned empty response");
        }

        callback(success, response, errorMessage);
    });
}

void KindlePageManager::syncWithServer(
    const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle,
    const std::function<void(bool success, const std::shared_ptr<KindlePageResponse> &,
                             const std::string &)> &callback) {

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

auto KindlePageManager::hasCrossedPreviewBoundary(uint32_t currentOffset,
                                                  uint32_t pageEndOffset) const -> bool {
    // Use pageEndOffset if specified, otherwise use currentOffset
    uint32_t effectiveOffset = pageEndOffset > 0 ? pageEndOffset : currentOffset;

    // Check if either the current position or the page end has crossed the boundary
    bool currentHasCrossed = (lastPreviewOffset_ > 0 && currentOffset >= lastPreviewOffset_);
    bool pageEndHasCrossed = (lastPreviewOffset_ > 0 && effectiveOffset >= lastPreviewOffset_);
    bool hasCrossed = currentHasCrossed || pageEndHasCrossed;

    if (lastPreviewOffset_ > 0) {
        // Calculate how far page end is from boundary
        int32_t pageEndDistanceToBoundary = lastPreviewOffset_ - effectiveOffset;

        if (pageEndOffset > 0 && pageEndHasCrossed && !currentHasCrossed) {
            // Special case: page end has crossed but current position hasn't
            log_i("[BOUNDARY CROSSED] Page end at %" PRIu32 " has crossed boundary at %" PRIu32
                  " (%" PRId32 " bytes beyond), while current position %" PRIu32 " has not",
                  effectiveOffset, lastPreviewOffset_, -pageEndDistanceToBoundary, currentOffset);
        } else if (hasCrossed) {
            log_i("[BOUNDARY CROSSED] Position %" PRIu32 " has crossed boundary at %" PRIu32
                  " (%" PRId32 " bytes beyond boundary)",
                  effectiveOffset, lastPreviewOffset_, -pageEndDistanceToBoundary);
        } else {
            log_i("[BOUNDARY NOT CROSSED] Position %" PRIu32 " has not crossed boundary at %" PRIu32
                  " (%" PRId32 " bytes before boundary)",
                  effectiveOffset, lastPreviewOffset_, pageEndDistanceToBoundary);
        }
    }

    return hasCrossed;
}

auto KindlePageManager::wouldNextTurnCrossBoundary(uint32_t currentOffset, uint32_t pageSize) const
    -> bool {
    if (lastPreviewOffset_ <= 0) {
        return false; // No boundary to cross
    }

    uint32_t nextTurnOffset = currentOffset + pageSize;
    bool wouldCross = (nextTurnOffset >= lastPreviewOffset_);

    // Calculate distance to boundary in bytes and pages
    uint32_t distanceToBoundary =
        (currentOffset < lastPreviewOffset_) ? (lastPreviewOffset_ - currentOffset) : 0;
    uint32_t pagesToBoundary =
        (pageSize > 0) ? ((distanceToBoundary + pageSize - 1) / pageSize) : 0;

    // Log detailed boundary prediction information
    log_i("[BOUNDARY PREDICTION] Current: %" PRIu32 ", next turn would reach: %" PRIu32
          ", boundary: %" PRIu32 ", distance: %" PRIu32 " bytes (~%" PRIu32 " pages), "
          "would cross: %d",
          currentOffset, nextTurnOffset, lastPreviewOffset_, distanceToBoundary, pagesToBoundary,
          wouldCross);

    return wouldCross;
}

auto KindlePageManager::needsNavigateCall(uint32_t currentOffset, uint32_t pageEndOffset) const
    -> bool {
    // If either the current position or page end has crossed a boundary, we MUST sync with server
    if (hasCrossedPreviewBoundary(currentOffset, pageEndOffset)) {
        log_i("[PAGE MANAGER] Need navigate: CRITICAL - current position or page end has crossed "
              "boundary at %" PRIu32,
              lastPreviewOffset_);
        return true;
    }

    // Otherwise, follow strict alternation
    // BUT: Only require alternation if we're close to the preview boundary
    // This prevents forcing a sync when we're nowhere near the boundary
    if (lastCallWasPreview_) {
        // Check if we're within reasonable distance of the boundary
        // This prevents unnecessary syncs when going backward then forward
        if (lastPreviewOffset_ > 0) {
            // More conservative approach - only require sync when we're VERY close to boundary
            uint32_t pageSize = 400; // Reasonable approximation
            uint32_t distanceToBoundary =
                (currentOffset < lastPreviewOffset_) ? lastPreviewOffset_ - currentOffset : 0;

            // Calculate pages to boundary for logging
            uint32_t pagesToBoundary = (distanceToBoundary + pageSize - 1) / pageSize;

            // FIXED: Only enforce alternation if we're within half a page of the boundary
            // This prevents premature syncing and ensures we see more content before syncing
            if (distanceToBoundary < pageSize / 2) {
                log_i("[PAGE MANAGER] Need navigate: Yes - lastCallWasPreview=true and VERY close "
                      "to boundary "
                      "(%" PRIu32 " bytes, <1 page from boundary)",
                      distanceToBoundary);
                return true;
            }

            // If we're not in immediate danger of crossing the boundary in the current page,
            // don't force a sync. This should prevent the issue of skipping content.
            log_i("[PAGE MANAGER] Need navigate: No - not close enough to boundary "
                  "(%" PRIu32 " bytes, ~%" PRIu32 " pages) despite lastCallWasPreview=true",
                  distanceToBoundary, pagesToBoundary);
            return false;
        }

        log_i("[PAGE MANAGER] Need navigate: Yes - lastCallWasPreview=true (no boundary set)");
        return true; // Default if no boundary
    }

    log_i("[PAGE MANAGER] Need navigate: No - lastCallWasPreview=false");
    return false; // Not after a preview
}

void KindlePageManager::resetPreviewBoundary() { lastPreviewOffset_ = 0; }

void KindlePageManager::setPreviewBoundary(uint32_t offset) { lastPreviewOffset_ = offset; }

auto KindlePageManager::getPreviewBoundary() const -> uint32_t { return lastPreviewOffset_; }

auto KindlePageManager::isPreviewRequestInProgress() const -> bool {
    return previewRequestInProgress_;
}

auto KindlePageManager::wasLastCallPreview() const -> bool { return lastCallWasPreview_; }

void KindlePageManager::resetLastCallWasPreview() {
    log_i("Resetting lastCallWasPreview_ from %d to false", lastCallWasPreview_);
    lastCallWasPreview_ = false;
}

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
    const std::function<void(bool success, const std::shared_ptr<KindlePageResponse> &,
                             const std::string &)> &callback) {

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
