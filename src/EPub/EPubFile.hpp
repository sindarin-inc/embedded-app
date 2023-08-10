#include "EPubOpf.hpp"
#include "Misc/ZipFile.hpp"
#include "Renderers/RendererStream.hpp"

class EPubFile {
public:
    EPubFile(FileSystem &fs, const char *filename) : epubZipFile_(fs, filename) {}
    ~EPubFile() { close(); };

    auto open() -> bool;
    void close();

private:
    ZipFile epubZipFile_;

    std::shared_ptr<EPubOpf> opf_;
};
