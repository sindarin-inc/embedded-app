#include "EPubOpf.hpp"
#include "Misc/ZipFile.hpp"
#include "Renderers/RendererStream.hpp"

class EPubFile : public RendererStream {
public:
    EPubFile(FileSystem &fs, const char *filename) : epubZipFile_(fs, filename) {}
    ~EPubFile() override { close(); };

    auto open() -> bool;
    void close();

    // RendererStream overrides
    auto readBytes(uint8_t *buf, int size) -> int override {
        // While we have bytes to read, read them!
        size_t len = epubZipFile_.readFromFile(buf, size);

        if (len < size) {
            epubZipFile_.closeFile();

            currentXhtmlIndex_++;
            // If there's another file to open
            if (currentXhtmlIndex_ < xhtmlItems_.size()) {
                // Open the next file
                auto &item = xhtmlItems_[currentXhtmlIndex_];
                log_d("Opening %s", item.fullPath().c_str());
                epubZipFile_.openFile(item.fullPath().c_str());
                // Read the bytes
                size_t secondLen = epubZipFile_.readFromFile(buf + len, size - len);
                len += secondLen;
            } else {
                log_i("No more files to read");
            }
        }

        position_ += len;
        // We're done!
        return len;
    };
    // TODO: Maybe this could be the default implememtation of readBytes?
    auto read() -> int override {
        int out;
        readBytes((uint8_t *)&out, 1);
        return out;
    };
    auto seek(uint32_t pos) -> bool override {
        if (pos < position_) {
            log_w("Seeking backwards in ePub from %d to %d", position_, pos);
            position_ = 0;
            currentXhtmlIndex_ = 0;
            epubZipFile_.closeFile();
            epubZipFile_.openFile(xhtmlItems_[0].fullPath().c_str());
        }

        // Seek forward to position
        while (position_ < pos && position_ < contentLength_) {
            // Read a byte at a time until we get to the position
            read();
        }
        return true;
    };
    auto position() -> size_t override { return position_; };
    auto size() -> size_t override { return contentLength_; };

protected:
    ZipFile epubZipFile_;

    // Text content
    std::vector<EPubData::ManifestItem> xhtmlItems_;

    int contentLength_ = 0;
    int position_ = 0;
    int currentXhtmlIndex_ = 0;
};
