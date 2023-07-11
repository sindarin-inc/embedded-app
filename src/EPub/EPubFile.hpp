#include "EPubOpf.hpp"
#include "Misc/ZipFile.hpp"
#include "Renderers/RendererStream.hpp"

class EPubFile : public RendererStream {
public:
    EPubFile(FileSystem &fs, const char *filename)
        : RendererStream(StreamType::EPUB), epubZipFile_(fs, filename) {}
    ~EPubFile() override { close(); };

    auto open() -> bool;
    void close();

    auto readBytes(uint8_t *buf, int size) -> int override;

    // TODO: Maybe this could be the default implememtation of readBytes?
    auto read() -> int override;

    auto seek(uint32_t pos) -> bool override;

    auto position() -> size_t override { return position_; };
    auto size() -> size_t override { return contentLength_; };

protected:
    ZipFile epubZipFile_;

    EPubOpfData opfData_{};

    int contentLength_ = 0;
    int position_ = 0;
    int currentXhtmlIndex_ = 0;
};
