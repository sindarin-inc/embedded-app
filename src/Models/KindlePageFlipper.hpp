#pragma once

#include <memory>
#include <string>
#include <utility>

#include "Communications/SindarinAPI.hpp"
#include "Misc/CancelFlag.hpp"
#include "Misc/Telemetry.hpp"
#include "Models/KindleBook.hpp"
#include "Models/KindlePageResponse.hpp"
#include "Models/Place.hpp"
#include "Renderers/RendererStream.hpp"

class KindlePage {
public:
    SpiramString content;
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

    void appendPageFromResponse(KindlePageResponse &response) {
        // Add a space to the end of the page so that they concatenate cleanly
        response.ocrText += " ";
        pages_.push_back({response.ocrText});
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
    void updatePlace(const std::shared_ptr<Place> &place, bool atEndOfContent);

    auto getStream() -> std::shared_ptr<RendererStream> { return pageStream_; }

    void setGotContentFunction(std::function<void(size_t ocrTextLength)> gotContentFunction) {
        gotContentFunction_ = std::move(gotContentFunction);
    }

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

    void retryNetworkOperation(bool atEndOfContent) {
        // Retry loading the initial page if we're at the beginning
        if (pageStream_->size() == 0) {
            loadInitialPage();
        } else if (serverSyncPlace_ != nullptr) {
            // Otherwise retry from the current place
            updatePlace(serverSyncPlace_, atEndOfContent);
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
        serverSyncPlace_ = nullptr;
    }

private:
    // const int bufferThreshold_ = 2000; // Byte threshold to preload more content

    std::shared_ptr<KindleBook> kindleBook_;
    bool useBle_ = false;
    std::shared_ptr<CancelFlag> cancelFlag_;
    std::shared_ptr<KindlePageStream> pageStream_;
    Telemetry &telemetry_;

    std::shared_ptr<TxtPlace> serverSyncPlace_; // Last position synced with server
    std::function<void(size_t ocrTextLength)> gotContentFunction_;
    std::function<void(const std::string &errorMessage)> onErrorFunction_;
    std::function<void()> onConnectionErrorFunction_;
    std::function<void(const SpiramString &dialogText)> onLastReadDialogFunction_;

    auto checkForError(int httpCode, std::shared_ptr<KindlePageResponse> &response) -> std::string;
    auto checkForLastReadDialog(const std::shared_ptr<KindlePageResponse> &response) -> bool;
};
