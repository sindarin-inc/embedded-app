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
    explicit KindlePageManager(const std::shared_ptr<KindleBook>& kindleBook);
    
    /**
     * Loads the initial page of a Kindle book
     * 
     * @param cancelFlag Flag to cancel the request
     * @param useBle Whether to use BLE for the request
     * @param callback Callback function for when the request completes
     */
    void loadInitialPage(const std::shared_ptr<CancelFlag>& cancelFlag, bool useBle,
                         std::function<void(bool success, const std::shared_ptr<KindlePageResponse>&, const std::string&)> callback);
    
    /**
     * Loads the next preview page for a Kindle book
     * 
     * @param currentPlace Current place in the renderer
     * @param cancelFlag Flag to cancel the request
     * @param useBle Whether to use BLE for the request
     * @param callback Callback function for when the request completes
     */
    void loadNextPreview(const std::shared_ptr<Place>& currentPlace,
                         const std::shared_ptr<CancelFlag>& cancelFlag, bool useBle,
                         std::function<void(bool success, const std::shared_ptr<KindlePageResponse>&, const std::string&)> callback);
    
    /**
     * Syncs the local position with the server using navigate-next
     * 
     * @param cancelFlag Flag to cancel the request
     * @param useBle Whether to use BLE for the request
     * @param callback Callback function for when the request completes
     */
    void syncWithServer(const std::shared_ptr<CancelFlag>& cancelFlag, bool useBle,
                        std::function<void(bool success, const std::shared_ptr<KindlePageResponse>&, const std::string&)> callback);
    
    /**
     * Checks if the current position is at or has crossed the preview boundary
     * 
     * @param currentOffset Current offset in the renderer
     * @return True if the position has crossed the preview boundary
     */
    bool hasCrossedPreviewBoundary(uint32_t currentOffset) const;
    
    /**
     * Checks if the next turn would cross the preview boundary
     * 
     * @param currentOffset Current offset in the renderer
     * @param pageSize Size of the current page in bytes
     * @return True if the next turn would cross the preview boundary
     */
    bool wouldNextTurnCrossBoundary(uint32_t currentOffset, uint32_t pageSize) const;
    
    /**
     * Gets the next content to load, respecting the alternation pattern
     * 
     * @param currentOffset Current offset in the renderer
     * @return True if a navigate call is needed, false if preview is needed
     */
    bool needsNavigateCall(uint32_t currentOffset) const;
    
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
    uint32_t getPreviewBoundary() const;
    
    /**
     * Checks if a preview request is in progress
     * 
     * @return True if a preview request is in progress
     */
    bool isPreviewRequestInProgress() const;
    
    /**
     * Gets whether the last API call was a preview
     * 
     * @return True if the last API call was a preview
     */
    bool wasLastCallPreview() const;
    
    /**
     * Updates the Kindle book metadata from a page response
     * 
     * @param response The page response
     */
    void updateBookMetadata(const std::shared_ptr<KindlePageResponse>& response);
    
private:
    std::shared_ptr<KindleBook> kindleBook_; // The Kindle book being managed
    bool lastCallWasPreview_{false}; // Tracks if the last API call was a preview to enforce strict alternation
    bool previewRequestInProgress_{false}; // Tracks if a preview request is currently in progress
    uint32_t lastPreviewOffset_{0}; // Tracks the offset where preview content was added
    
    // Helper method to make API requests
    void makeRequest(const std::function<int(SindarinAPI*, KindlePageResponse*, const std::shared_ptr<CancelFlag>&, bool, bool)>& requestFunc,
                     const std::shared_ptr<CancelFlag>& cancelFlag, bool useBle,
                     std::function<void(bool success, const std::shared_ptr<KindlePageResponse>&, const std::string&)> callback);
};