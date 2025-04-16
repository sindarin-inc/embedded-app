#pragma once

#include <memory>
#include <string>

#include "Misc/CancelFlag.hpp"
#include "Models/KindleBook.hpp"
#include "Models/KindlePageResponse.hpp"
#include "Models/Place.hpp"

// Forward declarations
class SindarinAPI;

/**
 * KindlePageManager
 *
 * Manages the loading and synchronization of Kindle pages for reading.
 * Handles the preview/navigate alternation pattern required by the Kindle API:
 *
 * 1. API Call Alternation:
 *    - Calls to /preview and /navigate must strictly alternate
 *    - After any /preview call, the next API call must be /navigate
 *    - After any /navigate call, the next API call must be /preview
 *
 * 2. Content Buffering:
 *    - When /preview-next returns text, the position where preview text begins is tracked
 *    - When user reaches the preview boundary, we MUST sync server position
 *    - This is done by calling /navigate-next, which aligns server with local position
 */
class KindlePageManager {
public:
    /**
     * Constructor
     *
     * @param kindleBook The Kindle book to manage pages for
     */
    explicit KindlePageManager(const std::shared_ptr<KindleBook> &kindleBook);

    /**
     * Loads the initial page of a Kindle book
     *
     * @param cancelFlag Flag to cancel the request
     * @param useBle Whether to use BLE for the request
     * @param callback Callback function for when the request completes
     */
    void loadInitialPage(
        const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle,
        const std::function<void(bool success, const std::shared_ptr<KindlePageResponse> &,
                                 const std::string &)> &callback);

    /**
     * Loads the next preview page for a Kindle book
     *
     * @param currentPlace Current place in the renderer
     * @param cancelFlag Flag to cancel the request
     * @param useBle Whether to use BLE for the request
     * @param callback Callback function for when the request completes
     */
    void loadNextPreview(
        const std::shared_ptr<Place> &currentPlace, const std::shared_ptr<CancelFlag> &cancelFlag,
        bool useBle,
        const std::function<void(bool success, const std::shared_ptr<KindlePageResponse> &,
                                 const std::string &)> &callback);

    /**
     * Syncs the local position with the server using navigate-next
     *
     * @param cancelFlag Flag to cancel the request
     * @param useBle Whether to use BLE for the request
     * @param callback Callback function for when the request completes
     */
    void syncWithServer(
        const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle,
        const std::function<void(bool success, const std::shared_ptr<KindlePageResponse> &,
                                 const std::string &)> &callback);

    /**
     * Loads the previous page from the server using navigate-previous
     *
     * @param cancelFlag Flag to cancel the request
     * @param useBle Whether to use BLE for the request
     * @param callback Callback function for when the request completes
     */
    void navigatePrevious(
        const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle,
        const std::function<void(bool success, const std::shared_ptr<KindlePageResponse> &,
                                 const std::string &)> &callback);

    /**
     * Checks if the current position is at or has crossed the preview boundary
     *
     * @param currentOffset Current offset in the renderer
     * @return True if the position has crossed the preview boundary
     */
    /**
     * Checks if the current position has crossed the preview boundary
     *
     * @param currentOffset Current offset in the renderer
     * @param pageEndOffset Optional offset of the end of the current page (if different from
     * currentOffset)
     * @return True if either position has crossed the preview boundary
     */
    [[nodiscard]] auto hasCrossedPreviewBoundary(uint32_t currentOffset,
                                                 uint32_t pageEndOffset = 0) const -> bool;

    /**
     * Checks if the next turn would cross the preview boundary
     *
     * @param currentOffset Current offset in the renderer
     * @param pageSize Size of the current page in bytes
     * @return True if the next turn would cross the preview boundary
     */
    [[nodiscard]] auto wouldNextTurnCrossBoundary(uint32_t currentOffset, uint32_t pageSize) const
        -> bool;

    /**
     * Checks if there is pre-cached content ahead of the current position
     * This happens when we've gone forward, then backward, then forward again
     *
     * @param currentOffset Current offset in the renderer
     * @param distanceToEnd [OUT] Distance from current position to end of buffer
     * @param totalSize [OUT] Total size of the buffer
     * @return True if there is content ahead that was previously loaded
     */
    auto hasPrecachedContentAhead(uint32_t currentOffset, uint32_t &distanceToEnd,
                                  uint32_t &totalSize) const -> bool;

    /**
     * Gets the next content to load, respecting the alternation pattern
     *
     * @param currentOffset Current offset in the renderer
     * @return True if a navigate call is needed, false if preview is needed
     */
    /**
     * Gets the next content to load, respecting the alternation pattern
     *
     * @param currentOffset Current offset in the renderer
     * @param pageEndOffset Optional offset of the end of the current page
     * @return True if a navigate call is needed, false if preview is needed
     */
    [[nodiscard]] auto needsNavigateCall(uint32_t currentOffset, uint32_t pageEndOffset = 0) const
        -> bool;

    /**
     * Resets the preview boundary
     */
    void resetPreviewBoundary();

    /**
     * Sets the preview boundary
     *
     * @param offset Offset where the preview content begins
     */
    void setPreviewBoundary(uint32_t offset);

    /**
     * Gets the preview boundary
     *
     * @return Offset where the preview content begins
     */
    [[nodiscard]] auto getPreviewBoundary() const -> uint32_t;

    /**
     * Gets all preview boundaries
     *
     * @return Vector of all offsets where preview content begins
     */
    [[nodiscard]] auto getPreviewBoundaries() const -> const std::vector<uint32_t> & {
        return previewBoundaries_;
    }

    /**
     * Checks if a preview request is in progress
     *
     * @return True if a preview request is in progress
     */
    [[nodiscard]] auto isPreviewRequestInProgress() const -> bool;

    /**
     * Gets whether the last API call was a preview
     *
     * @return True if the last API call was a preview
     */
    [[nodiscard]] auto wasLastCallPreview() const -> bool;

    /**
     * Resets the lastCallWasPreview flag to false
     * Used when changing direction from backward to forward to avoid unnecessary API calls
     */
    void resetLastCallWasPreview();

    /**
     * Updates the Kindle book metadata from a page response
     *
     * @param response The page response
     */
    void updateBookMetadata(const std::shared_ptr<KindlePageResponse> &response);

private:
    std::shared_ptr<KindleBook> kindleBook_; // The Kindle book being managed
    bool lastCallWasPreview_{
        false}; // Tracks if the last API call was a preview to enforce strict alternation
    bool previewRequestInProgress_{false}; // Tracks if a preview request is currently in progress

    // Vector of preview boundaries - each entry represents an offset where preview content begins
    // This allows tracking multiple preview boundaries as we navigate back and forth
    std::vector<uint32_t> previewBoundaries_{};

    // For backward compatibility - maps to the last boundary in the vector
    uint32_t lastPreviewOffset_{0};

    // Helper method to make API requests
    void makeRequest(
        const std::function<int(SindarinAPI *, KindlePageResponse *,
                                const std::shared_ptr<CancelFlag> &, bool, bool)> &requestFunc,
        const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle,
        const std::function<void(bool success, const std::shared_ptr<KindlePageResponse> &,
                                 const std::string &)> &callback);
};
