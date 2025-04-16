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

    // Log the current offset and preview boundaries for debugging
    if (!previewBoundaries_.empty()) {
        // Generate a string with all boundaries
        std::string boundaryList;
        for (uint32_t boundary : previewBoundaries_) {
            if (!boundaryList.empty()) {
                boundaryList += ", ";
            }
            boundaryList += std::to_string(boundary);
        }

        // Find the next boundary ahead of current position, if any
        uint32_t nextBoundary = 0;
        for (uint32_t boundary : previewBoundaries_) {
            if (boundary > currentOffset) {
                if (nextBoundary == 0 || boundary < nextBoundary) {
                    nextBoundary = boundary;
                }
            }
        }

        if (nextBoundary > 0) {
            uint32_t distanceToBoundary = nextBoundary - currentOffset;
            log_i("[PREVIEW BOUNDARY] Loading preview at offset %" PRIu32
                  ", next boundary ahead: %" PRIu32 ", distance: %" PRIu32
                  " bytes, all boundaries: [%s]",
                  currentOffset, nextBoundary, distanceToBoundary, boundaryList.c_str());
        } else {
            log_i("[PREVIEW BOUNDARY] Loading preview at offset %" PRIu32
                  ", all boundaries passed: [%s]",
                  currentOffset, boundaryList.c_str());
        }
    } else {
        log_i("[PREVIEW BOUNDARY] Loading next preview for Kindle book at offset %" PRIu32
              " (no boundaries set)",
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

            // We've synced with the server at this position, so all previous
            // boundaries are no longer relevant, but we shouldn't just throw away
            // the boundary information since we still have the content in our buffer.
            // Keep the boundaries but log the sync point for debugging.

            if (!previewBoundaries_.empty()) {
                std::string boundaryList;
                for (uint32_t boundary : previewBoundaries_) {
                    if (!boundaryList.empty()) {
                        boundaryList += ", ";
                    }
                    boundaryList += std::to_string(boundary);
                }
                log_i("[SYNC BOUNDARIES] Synced at position, preserving boundaries: [%s]",
                      boundaryList.c_str());
            } else {
                // For legacy support, still log the old format
                log_i("Reset preview boundary from %" PRIu32 " to 0 after successful sync",
                      lastPreviewOffset_);
                lastPreviewOffset_ = 0;
            }

            updateBookMetadata(response);
        }

        callback(success, response, errorMessage);
    });
}

void KindlePageManager::navigatePrevious(
    const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle,
    const std::function<void(bool success, const std::shared_ptr<KindlePageResponse> &,
                             const std::string &)> &callback) {

    if (previewRequestInProgress_) {
        log_w("Preview request in progress, cannot navigate previous yet");
        callback(false, nullptr, "Preview request in progress");
        return;
    }

    log_i("Navigating to previous page - preview boundary: %" PRIu32 ", lastCallWasPreview: %d",
          lastPreviewOffset_, lastCallWasPreview_);

    auto requestFunc = [](SindarinAPI *api, KindlePageResponse *response,
                          const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle, bool) {
        // Use ocr=false when navigating backward since we already have the content locally
        return api->navigateKindlePrevious(response, cancelFlag, useBle, false);
    };

    makeRequest(requestFunc, cancelFlag, useBle,
                [this, callback](bool success, const std::shared_ptr<KindlePageResponse> &response,
                                 const std::string &errorMessage) {
        if (success) {
            // Mark that the last API call was a navigate (not a preview)
            lastCallWasPreview_ = false;

            // We should keep the boundary information since it's still valid
            // for when we navigate forward again
            if (!previewBoundaries_.empty()) {
                std::string boundaryList;
                for (uint32_t boundary : previewBoundaries_) {
                    if (!boundaryList.empty()) {
                        boundaryList += ", ";
                    }
                    boundaryList += std::to_string(boundary);
                }
                log_i("[NAVIGATE BACKWARDS] Preserving boundaries after navigate-previous: [%s]",
                      boundaryList.c_str());
            } else {
                // Legacy support for old format
                log_i("Reset preview boundary from %" PRIu32 " to 0 after navigate-previous",
                      lastPreviewOffset_);
                lastPreviewOffset_ = 0;
            }

            updateBookMetadata(response);
        }

        callback(success, response, errorMessage);
    });
}

auto KindlePageManager::hasCrossedPreviewBoundary(uint32_t currentOffset,
                                                  uint32_t pageEndOffset) const -> bool {
    // Use pageEndOffset if specified, otherwise use currentOffset
    uint32_t effectiveOffset = pageEndOffset > 0 ? pageEndOffset : currentOffset;

    // First check the vector of preview boundaries
    bool hasCrossed = false;
    uint32_t crossedBoundary = 0;

    // Generate a boundary report for logging
    std::string boundaryList;
    for (uint32_t boundary : previewBoundaries_) {
        if (!boundaryList.empty()) {
            boundaryList += ", ";
        }
        boundaryList += std::to_string(boundary);

        // Check if we crossed this boundary
        if (boundary > 0 && effectiveOffset >= boundary) {
            hasCrossed = true;
            crossedBoundary = boundary;
        }
    }

    // If using the vector approach didn't find a crossed boundary, fall back to legacy approach
    if (!hasCrossed) {
        // Check if either the current position or the page end has crossed the boundary
        bool currentHasCrossed = (lastPreviewOffset_ > 0 && currentOffset >= lastPreviewOffset_);
        bool pageEndHasCrossed = (lastPreviewOffset_ > 0 && effectiveOffset >= lastPreviewOffset_);
        hasCrossed = currentHasCrossed || pageEndHasCrossed;
        crossedBoundary = lastPreviewOffset_;
    }

    // Detailed logging about boundary status
    if (hasCrossed) {
        // Calculate how far we are beyond the boundary
        int32_t distanceBeyondBoundary = effectiveOffset - crossedBoundary;

        log_i("[BOUNDARY MAP] Position %" PRIu32 " has crossed boundary at %" PRIu32 " (%" PRId32
              " bytes beyond). All boundaries: [%s]",
              effectiveOffset, crossedBoundary, distanceBeyondBoundary, boundaryList.c_str());
    } else if (!previewBoundaries_.empty() || lastPreviewOffset_ > 0) {
        // Calculate distance to the nearest boundary ahead
        uint32_t nearestBoundary = 0;
        for (uint32_t boundary : previewBoundaries_) {
            if (boundary > effectiveOffset) {
                if (nearestBoundary == 0 || boundary < nearestBoundary) {
                    nearestBoundary = boundary;
                }
            }
        }

        // If no boundary found in vector, use the legacy one
        if (nearestBoundary == 0 && lastPreviewOffset_ > effectiveOffset) {
            nearestBoundary = lastPreviewOffset_;
        }

        if (nearestBoundary > 0) {
            uint32_t distanceToBoundary = nearestBoundary - effectiveOffset;
            log_i("[BOUNDARY MAP] Position %" PRIu32 " has not crossed boundary at %" PRIu32
                  " (%" PRIu32 " bytes before). All boundaries: [%s]",
                  effectiveOffset, nearestBoundary, distanceToBoundary, boundaryList.c_str());
        } else {
            log_i("[BOUNDARY MAP] Position %" PRIu32 " - no boundaries ahead. All boundaries: [%s]",
                  effectiveOffset, boundaryList.c_str());
        }
    }

    return hasCrossed;
}

auto KindlePageManager::wouldNextTurnCrossBoundary(uint32_t currentOffset, uint32_t pageSize) const
    -> bool {
    // Find the nearest boundary ahead
    uint32_t nearestBoundary = 0;

    // First check the vector of boundaries
    for (uint32_t boundary : previewBoundaries_) {
        if (boundary > currentOffset) {
            if (nearestBoundary == 0 || boundary < nearestBoundary) {
                nearestBoundary = boundary;
            }
        }
    }

    // If no boundary found in vector, fall back to legacy approach
    if (nearestBoundary == 0 && lastPreviewOffset_ > currentOffset) {
        nearestBoundary = lastPreviewOffset_;
    }

    // If no boundary ahead, we won't cross one
    if (nearestBoundary == 0) {
        log_i("[BOUNDARY PREDICTION] No boundary ahead to cross");
        return false;
    }

    // Calculate if next turn would cross the boundary
    uint32_t nextTurnOffset = currentOffset + pageSize;
    bool wouldCross = (nextTurnOffset >= nearestBoundary);

    // Calculate distance to boundary in bytes and pages
    uint32_t distanceToBoundary = nearestBoundary - currentOffset;
    uint32_t pagesToBoundary =
        (pageSize > 0) ? ((distanceToBoundary + pageSize - 1) / pageSize) : 0;

    // Generate boundary list for logging
    std::string boundaryList;
    for (uint32_t boundary : previewBoundaries_) {
        if (!boundaryList.empty()) {
            boundaryList += ", ";
        }
        boundaryList += std::to_string(boundary);
    }

    // Log detailed boundary prediction information
    log_i("[BOUNDARY PREDICTION] Current: %" PRIu32 ", next turn would reach: %" PRIu32
          ", nearest boundary: %" PRIu32 ", distance: %" PRIu32 " bytes (~%" PRIu32 " pages), "
          "would cross: %d",
          currentOffset, nextTurnOffset, nearestBoundary, distanceToBoundary, pagesToBoundary,
          wouldCross);

    log_i("[BOUNDARY MAP] All boundaries: [%s]", boundaryList.c_str());

    return wouldCross;
}

auto KindlePageManager::needsNavigateCall(uint32_t currentOffset, uint32_t pageEndOffset) const
    -> bool {
    // Get total stream size to determine if we're navigating through cached content
    uint32_t distanceToEnd = 0;
    uint32_t totalSize = 0; // Will be set by hasPrecachedContentAhead
    bool hasCachedContent = hasPrecachedContentAhead(currentOffset, distanceToEnd, totalSize);

    // CASE 1: If we've crossed a preview boundary, we MUST sync with server
    if (hasCrossedPreviewBoundary(currentOffset, pageEndOffset)) {
        log_i("[PAGE MANAGER] Need navigate: CRITICAL - current position or page end has crossed "
              "boundary at %" PRIu32,
              lastPreviewOffset_);
        return true;
    }

    // CASE 2: We have cached content ahead from previous forward navigation
    // This happens when we've gone forward, then backward, then forward again
    if (hasCachedContent) {
        // Calculate how much content we have before we need to make API calls
        uint32_t distanceToBoundary = (lastPreviewOffset_ > 0 && currentOffset < lastPreviewOffset_)
                                          ? (lastPreviewOffset_ - currentOffset)
                                          : 0;

        // We have two thresholds:
        // 1. Far from boundary: We definitely don't need any API calls
        // 2. Close to boundary but still have enough content: We might need to prepare for loading

        // Definitely have enough cached content (multiple pages ahead)
        if (distanceToEnd > 2000) {
            log_i("[PAGE MANAGER] Need navigate: No - plenty of cached content (%" PRIu32
                  " bytes ahead, boundary at %" PRIu32 ")",
                  distanceToEnd, lastPreviewOffset_);
            log_i("[PAGE DECISION] Using cached content for forward navigation after "
                  "going backward - already visited these pages");
            return false;
        }

        // Getting close to boundary, but still have some content
        if (distanceToEnd > 1000 || distanceToBoundary > 500) {
            log_i("[PAGE MANAGER] Need navigate: No - still have some cached content "
                  "(%" PRIu32 " bytes ahead, %" PRIu32 " bytes to boundary at %" PRIu32 ")",
                  distanceToEnd, distanceToBoundary, lastPreviewOffset_);
            return false;
        }

        // Very close to the boundary, may need to consider the alternation pattern
        log_i("[PAGE MANAGER] Getting close to end of cached content "
              "(%" PRIu32 " bytes ahead, %" PRIu32 " bytes to boundary)",
              distanceToEnd, distanceToBoundary);
    }

    // CASE 3: Follow alternation pattern only when approaching the end of cached content
    // This ensures we maintain proper server synchronization when loading new content
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

            // Only enforce alternation if we're within half a page of the boundary
            // This prevents premature syncing and ensures we see more content before syncing
            if (distanceToBoundary < pageSize / 2) {
                log_i("[PAGE MANAGER] Need navigate: Yes - lastCallWasPreview=true and VERY close "
                      "to boundary (%" PRIu32 " bytes, <1 page from boundary)",
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

void KindlePageManager::resetPreviewBoundary() {
    // Log the boundaries being reset
    if (!previewBoundaries_.empty()) {
        std::string boundaryList;
        for (uint32_t boundary : previewBoundaries_) {
            if (!boundaryList.empty()) {
                boundaryList += ", ";
            }
            boundaryList += std::to_string(boundary);
        }
        log_i("[PREVIEW BOUNDARIES] Resetting all preview boundaries: [%s]", boundaryList.c_str());
    } else {
        log_i("[PREVIEW BOUNDARIES] No boundaries to reset");
    }

    // Clear the vector of boundaries
    previewBoundaries_.clear();
    lastPreviewOffset_ = 0;
}

void KindlePageManager::setPreviewBoundary(uint32_t offset) {
    // Don't add duplicate boundaries
    if (!previewBoundaries_.empty() && previewBoundaries_.back() == offset) {
        log_i("[PREVIEW BOUNDARIES] Skipping duplicate boundary at %" PRIu32, offset);
        return;
    }

    // Add to the vector of boundaries
    previewBoundaries_.push_back(offset);

    // Update the legacy field for backward compatibility
    lastPreviewOffset_ = offset;

    // Log the current state of boundaries
    std::string boundaryList;
    for (uint32_t boundary : previewBoundaries_) {
        if (!boundaryList.empty()) {
            boundaryList += ", ";
        }
        boundaryList += std::to_string(boundary);
    }
    log_i("[PREVIEW BOUNDARIES] Current boundaries: [%s]", boundaryList.c_str());
}

auto KindlePageManager::getPreviewBoundary() const -> uint32_t { return lastPreviewOffset_; }

auto KindlePageManager::hasPrecachedContentAhead(uint32_t currentOffset, uint32_t &distanceToEnd,
                                                 uint32_t &totalSize) const -> bool {
    // This method determines if we've already loaded content ahead of the current position
    // This typically happens in forward-backward-forward navigation patterns

    // Initialize with defaults
    totalSize = 0;
    distanceToEnd = 0;

    // CASE 1: Using the vector of preview boundaries for precise tracking
    if (!previewBoundaries_.empty()) {
        // Find the next boundary ahead of our current position
        uint32_t nextBoundary = 0;
        bool foundNextBoundary = false;

        for (uint32_t boundary : previewBoundaries_) {
            if (boundary > currentOffset) {
                nextBoundary = boundary;
                foundNextBoundary = true;
                break;
            }
        }

        // Log the relationship between current position and boundaries
        std::string boundaryList;
        for (uint32_t boundary : previewBoundaries_) {
            if (!boundaryList.empty()) {
                boundaryList += ", ";
            }
            boundaryList += std::to_string(boundary);
        }

        if (foundNextBoundary) {
            // We have a next boundary ahead, so we definitely have cached content
            uint32_t distanceToBoundary = nextBoundary - currentOffset;

            // Assume total size includes at least the next boundary plus some margin
            totalSize = nextBoundary + 1000; // Conservative estimate

            // Calculate total distance to end of cached content
            distanceToEnd = (currentOffset < totalSize) ? (totalSize - currentOffset) : 0;

            // Check if we have meaningful content ahead (at least a page worth)
            if (distanceToEnd > 500) {
                log_i("[BOUNDARY MAP] Position %" PRIu32 " → Next boundary %" PRIu32 " (%" PRIu32
                      " bytes ahead)",
                      currentOffset, nextBoundary, distanceToBoundary);

                log_i("[BOUNDARY MAP] All boundaries: [%s]", boundaryList.c_str());

                log_i("[CACHED CONTENT] Using cached content between position %" PRIu32
                      " and boundary %" PRIu32,
                      currentOffset, nextBoundary);

                return true;
            }
        } else {
            // We don't have a next boundary, but we might still have content ahead
            log_i("[BOUNDARY MAP] Position %" PRIu32 " → No boundaries ahead. All boundaries: [%s]",
                  currentOffset, boundaryList.c_str());
        }
    }

    // CASE 2: Legacy single boundary check (for backward compatibility)
    if (lastPreviewOffset_ > 0 && currentOffset < lastPreviewOffset_) {
        // Distance to boundary (how far we are from content we've already loaded)
        uint32_t distanceToBoundary = lastPreviewOffset_ - currentOffset;

        // Assume total size is at least boundary plus some margin
        totalSize = lastPreviewOffset_ + 1000; // Conservative estimate

        // Calculate total distance to end of cached content
        distanceToEnd = (currentOffset < totalSize) ? (totalSize - currentOffset) : 0;

        // Check if we have meaningful content ahead (at least a page worth)
        if (distanceToEnd > 500) {
            uint32_t cachedContentAfterBoundary = distanceToEnd - distanceToBoundary;

            log_i("[CACHED CONTENT] Position %" PRIu32 " has %" PRIu32 " bytes ahead (%" PRIu32
                  " bytes to boundary at %" PRIu32 ", ~%" PRIu32
                  " bytes of cached content after boundary)",
                  currentOffset, distanceToEnd, distanceToBoundary, lastPreviewOffset_,
                  cachedContentAfterBoundary);

            // Additional logging to help debug why we're not making API calls
            log_i("[API DECISION] Using cached content instead of making API call - "
                  "we already visited these pages and have content in buffer");

            return true;
        }
    }

    // CASE 3: No boundaries but we're likely still navigating through cached content
    // This happens after navigate-previous resets the boundaries but we still have the content

    // Check if buffer looks large enough to assume we have cached content ahead
    // This is a heuristic based on the RendererStream's current size - anything over ~1000 bytes
    // suggests we've previously loaded content ahead that we can reuse
    const uint32_t minAssumedBufferSize = 1000;

    // In the absence of boundary markers, use heuristics:
    // 1. We've just done navigate-previous (lastCallWasPreview_=false)
    // 2. We're at a position that suggests we have forward content available
    if (previewBoundaries_.empty() && !lastCallWasPreview_ &&
        currentOffset < minAssumedBufferSize) {
        // Make a conservative assumption about how much content we might have
        // This should be tuned based on actual buffer sizes observed in the field
        totalSize = 3000; // Conservative estimate after navigate-previous
        distanceToEnd = (currentOffset < totalSize) ? (totalSize - currentOffset) : 0;

        log_i("[CACHED CONTENT] Position %" PRIu32 " likely has cached content ahead - "
              "after navigate-previous (estimated %" PRIu32 " bytes ahead)",
              currentOffset, distanceToEnd);

        // In this scenario, we shouldn't immediately make API calls since we're likely to have
        // enough content for at least a few page turns after navigating backward
        log_i("[API DECISION] Using assumed cached content after navigate-previous - "
              "deferring API calls until definitely needed");

        return true;
    }

    // Default case - we don't have evidence of precached content
    log_i("[CACHED CONTENT CHECK] No definitive evidence of cached content ahead of position "
          "%" PRIu32,
          currentOffset);
    return false;
}

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
