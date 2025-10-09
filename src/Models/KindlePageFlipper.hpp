#pragma once

#include <Renderer/Place.hpp>
#include <Renderer/RendererStream.hpp>
#include <memory>
#include <string>
#include <utility>

#include "Communications/SindarinAPI.hpp"
#include "Misc/CancelFlag.hpp"
#include "Misc/Telemetry.hpp"
#include "Models/KindleBook.hpp"
#include "Models/KindlePageResponse.hpp"

class DispatchQueue;

struct KindleError {
    enum class Type {
        None,                // No error
        ConnectionError,     // Network connection issue (retryable)
        Timeout,             // Request timeout (retryable)
        ServerError,         // Server error (potentially retryable)
        SessionKeyMismatch,  // Session key out of sync (retryable - resets to position 0)
        AuthenticationError, // Authentication failure
        NotFoundError,       // Resource not found
        ClientError,         // Other client errors
        UnknownError         // Unknown error
    };

    Type type = Type::None;
    std::string message;

    [[nodiscard]] auto hasError() const -> bool { return type != Type::None; }
    [[nodiscard]] auto isRetryable() const -> bool {
        return type == Type::ConnectionError || type == Type::Timeout ||
               type == Type::ServerError || type == Type::SessionKeyMismatch;
    }
    [[nodiscard]] auto isConnectionError() const -> bool { return type == Type::ConnectionError; }
};

class KindlePage {
public:
    SpiramString content;
    int serverPageNumber = -1; // The page number on the server that this content came from
};

// TODO: Maybe it would be more exact (albeit more complex) to instead have a KindleTokenizer and a
// KindlePlace. But this lets us reuse most of the existing Renderer code by pretending the
// collection of pages is a TXT document.
class KindlePageStream : public RendererStream {
public:
    KindlePageStream() : RendererStream(DocType::TXT) {}

    auto read() -> int override {
        uint8_t out;
        readBytes(&out, 1);
        return out;
    }

    auto readBytes(uint8_t *buf, int size) -> int override {
        if (currentPageIndex_ >= pages_.size()) {
            return 0;
        }

        // Copy as much as we can from the current page
        auto page = pages_[currentPageIndex_];
        auto content = page.content;
        auto bytesToCopy = std::min(static_cast<size_t>(size),
                                    static_cast<size_t>(content.size() - currentPageOffset_));
        memcpy(buf, content.data() + currentPageOffset_, bytesToCopy);

        // Update the offset
        currentPageOffset_ += bytesToCopy;

        // If we've reached the end of the page, move to the next page
        if (currentPageOffset_ >= content.size()) {
            currentPageIndex_++;
            currentPageOffset_ = 0;

            // If there are still more bytes to copy from this page, copy in the remaining bytes
            int bytesCopied = readBytes(buf + bytesToCopy, size - bytesToCopy);
            bytesToCopy += bytesCopied;
        }

        return bytesToCopy;
    }

    auto seek(uint32_t pos) -> bool override {
        // Pages are non-uniform, iterate through all pages to find the right spot
        for (int i = 0; i < pages_.size(); i++) {
            if (pos < pages_[i].content.size()) {
                currentPageIndex_ = i;
                currentPageOffset_ = pos;
                return true;
            }
            pos -= pages_[i].content.size();
        }
        return false;
    }

    auto position() -> size_t override {
        // Pages are non-uniform, iterate through all pages to find the byte position
        size_t pos = 0;
        for (int i = 0; i < currentPageIndex_; i++) {
            pos += pages_[i].content.size();
        }
        pos += currentPageOffset_;
        return pos;
    }

    auto size() -> size_t override {
        size_t size = 0;
        for (auto &page : pages_) {
            size += page.content.size();
        }
        return size;
    }

    void clearAndSetContent(const SpiramString &content) {
        clear();
        pages_.push_back({content, 0});
        currentPageIndex_ = 0;
        currentPageOffset_ = 0;
        log_i("Set new content of size %zu", content.size());
    }

    auto pageIndexForOffset(uint32_t offset) -> int {
        for (int i = 0; i < pages_.size(); i++) {
            if (offset < pages_[i].content.size()) {
                return i;
            }
            offset -= pages_[i].content.size();
        }
        log_w("Offset %" PRIu32 " is out of bounds for KindlePageStream", offset);
        return -1;
    }

    void appendPageFromResponse(KindlePageResponse &response, int serverPageNumber) {
        // Add a space to the end of the page so that they concatenate cleanly
        response.ocrText += " ";
        pages_.push_back({response.ocrText, serverPageNumber});
    }

    auto getServerPageNumberForOffset(uint32_t offset) -> int {
        int pageIndex = pageIndexForOffset(offset);
        if (pageIndex >= 0 && pageIndex < pages_.size()) {
            return pages_[pageIndex].serverPageNumber;
        }
        return -1;
    }

    auto getOffsetForServerPageNumber(int serverPageNumber) -> int {
        size_t cumulativeOffset = 0;
        for (auto &page : pages_) {
            if (page.serverPageNumber == serverPageNumber) {
                return cumulativeOffset;
            }
            cumulativeOffset += page.content.size();
        }
        return -1;
    }

    auto lastPageIndex() -> int { return pages_.size() - 1; }

    auto getOcrTextLength() -> size_t {
        size_t totalLength = 0;
        for (const auto &page : pages_) {
            totalLength += page.content.size();
        }
        return totalLength;
    }

    void clear() {
        pages_.clear();
        currentPageIndex_ = 0;
        currentPageOffset_ = 0;
    }

    auto getBytesRemainingFrom(uint32_t offset) -> size_t {
        size_t totalSize = size();
        return offset < totalSize ? totalSize - offset : 0;
    }

    auto getPagesRemainingFrom(uint32_t offset) -> int {
        int pageIndex = pageIndexForOffset(offset);
        if (pageIndex < 0) {
            return 0;
        }
        return pages_.size() - pageIndex;
    }

    void debugDumpPageMappings() {
        size_t cumulativeOffset = 0;
        log_d("=== Buffer Page Mappings ===");
        for (size_t i = 0; i < pages_.size(); i++) {
            size_t pageSize = pages_[i].content.size();
            log_d("  Page %zu: offset_range=[%zu-%zu], size=%zu, server_page=%d", i,
                  cumulativeOffset, cumulativeOffset + pageSize - 1, pageSize,
                  pages_[i].serverPageNumber);
            cumulativeOffset += pageSize;
        }
        log_d("  Total buffer size: %zu bytes", cumulativeOffset);
    }

protected:
    SpiramVector<KindlePage> pages_;

    int currentPageIndex_ = 0;
    uint32_t currentPageOffset_ = 0;
};

class KindlePageFlipper {
public:
    /**
     * Constructor
     *
     * @param kindleBook The Kindle book to manage pages for
     * @param cancelFlag Cancel flag for async operations
     * @param telemetry Telemetry instance for logging metrics
     * @param useBle Whether to use BLE connection
     */
    explicit KindlePageFlipper(const std::shared_ptr<KindleBook> &kindleBook,
                               std::shared_ptr<CancelFlag> cancelFlag, Telemetry &telemetry,
                               bool useBle = false);

    /**
     * Loads the initial page of a Kindle book
     */
    void loadInitialPage();

    /**
     * Contains all the logic to decide when to navigate pages and preview upcoming pages.
     *
     * @param place The place to update to
     * @param atEndOfContent Whether we're at the end of the content
     */
    void updatePlace(const std::shared_ptr<Place> &place);

    auto getStream() -> std::shared_ptr<RendererStream> { return pageStream_; }

    void setOnContentReady(std::function<void(size_t ocrTextLength)> onContentReady) {
        onContentReady_ = std::move(onContentReady);
    }

#ifdef TESTS
    void setOnTestContentReady(std::function<void(size_t ocrTextLength)> onTestContentReady) {
        onTestContentReady_ = std::move(onTestContentReady);
    }

    void notifyTestContentReady() {
        if (onTestContentReady_) {
            size_t ocrTextLength = pageStream_ ? pageStream_->getOcrTextLength() : 0;
            onTestContentReady_(ocrTextLength);
        }
    }
#endif

    void setOnErrorFunction(std::function<void(const std::string &errorMessage)> onErrorFunction) {
        onErrorFunction_ = std::move(onErrorFunction);
    }

    void setOnConnectionErrorFunction(std::function<void()> onConnectionErrorFunction) {
        onConnectionErrorFunction_ = std::move(onConnectionErrorFunction);
    }

    // Set callback for when a last read dialog is detected
    void setOnLastReadDialogFunction(
        std::function<void(const SpiramString &dialogText)> onLastReadDialogFunction) {
        onLastReadDialogFunction_ = std::move(onLastReadDialogFunction);
    }

    void updateProgressFromServer(const KindlePageProgress &progress) {
        applyProgressUpdate(progress);
    }

    void setOnProgressUpdated(
        std::function<void(const KindlePageProgress &progress)> onProgressUpdated) {
        onProgressUpdated_ = std::move(onProgressUpdated);
    }

    [[nodiscard]] auto getServerPageNumberForOffset(uint32_t offset) const -> int {
        if (!pageStream_) {
            return -1;
        }
        return pageStream_->getServerPageNumberForOffset(offset);
    }

    void retryNetworkOperation() {
        // Retry loading the initial page if we're at the beginning
        if (pageStream_->size() == 0) {
            loadInitialPage();
        } else if (lastServerSyncPlace_ != nullptr) {
            // Otherwise retry from the current place
            updatePlace(lastServerSyncPlace_);
        }
    }

    void setUseBle(bool useBle) {
        useBle_ = useBle;
        log_i("KindlePageFlipper now using BLE: %s", useBle_ ? "true" : "false");
    }

    [[nodiscard]] auto hasContent() const -> bool { return pageStream_ && pageStream_->size() > 0; }

    void setCancelFlag(const std::shared_ptr<CancelFlag> &cancelFlag) { cancelFlag_ = cancelFlag; }

    void clearBuffer() {
        if (pageStream_) {
            pageStream_->clear();
        }
        lastServerSyncPlace_ = nullptr;
        lastRenderedPlace_ = nullptr;
        lastPreviewedPage_ = -1;
        lastNavigatedPage_ = -1;
        bookSessionKey_.clear();
    }

    [[nodiscard]] auto getPagesRemainingFromOffset(uint32_t offset) const -> int {
        if (pageStream_) {
            return pageStream_->getPagesRemainingFrom(offset);
        }
        return 0;
    }

    [[nodiscard]] auto isUsingBleConnection() const -> bool { return useBle_; }

    // When navigating to a new chapter, we need to reset the book session key
    // and navigation state so that we can start fresh at the beginning of the chapter
    void resetBookSessionKey() {
        bookSessionKey_.clear();
        lastNavigatedPage_ = -1;
        lastPreviewedPage_ = -1;
        log_i("Reset book session key and navigation state");
    }

    void setNavigateTo(int navigateTo) { lastNavigatedPage_ = navigateTo; }
    void setPreviewTo(int previewTo) { lastPreviewedPage_ = previewTo; }
    void setBookSessionKey(const SpiramString &key) { bookSessionKey_ = key; }

    auto navigateToChapter(const std::string &chapterUuid, int currentPage)
        -> std::shared_ptr<KindleError>;

private:
    std::shared_ptr<KindleBook> kindleBook_;
    bool useBle_ = false;
    std::shared_ptr<CancelFlag> cancelFlag_;
    std::shared_ptr<KindlePageStream> pageStream_;
    Telemetry &telemetry_;

    std::shared_ptr<TxtPlace> lastServerSyncPlace_; // Last position synced with server
    std::shared_ptr<TxtPlace> lastRenderedPlace_;   // Last position rendered to the user
    int lastPreviewedPage_ = -1;  // Track the last page we successfully previewed from server
    int lastNavigatedPage_ = -1;  // Track the last page we sent navigate_to for
    SpiramString bookSessionKey_; // Session key from server denoting navigation zero point
    std::function<void(size_t ocrTextLength)> onContentReady_;
#ifdef TESTS
    std::function<void(size_t ocrTextLength)> onTestContentReady_;
#endif
    std::function<void(const std::string &errorMessage)> onErrorFunction_;
    std::function<void()> onConnectionErrorFunction_;
    std::function<void(const SpiramString &dialogText)> onLastReadDialogFunction_;
    std::function<void(const KindlePageProgress &progress)> onProgressUpdated_;

    [[nodiscard]] auto dispatchQueue() const -> DispatchQueue *;
    auto checkForError(int httpCode, std::shared_ptr<KindlePageResponse> &response)
        -> std::shared_ptr<KindleError>;
    auto checkForLastReadDialog(const std::shared_ptr<KindlePageResponse> &response) -> bool;

    // Helper to perform navigation with retry logic
    // Returns error (nullptr for success)
    auto navigateWithRetry(int navigateTo, int previewTo,
                           std::shared_ptr<KindlePageResponse> &response,
                           const std::shared_ptr<CancelFlag> &cancelFlag, int maxRetries = 3)
        -> std::shared_ptr<KindleError>;

    // Returns error (nullptr for success)
    auto openBookWithRetry(std::shared_ptr<KindlePageResponse> &response,
                           const std::shared_ptr<CancelFlag> &cancelFlag, int maxRetries = 2)
        -> std::shared_ptr<KindleError>;

    // Unified method to sync pages with server - handles both navigation changes and buffer refills
    void syncPagesWithServer(uint32_t currentOffset, bool navigationChanged,
                             const std::shared_ptr<CancelFlag> &cancelFlag);

    void applyProgressUpdate(const KindlePageProgress &progress);
};
