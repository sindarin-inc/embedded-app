#include <Arduino.h>
#include <FS.h>
#include <FSImpl.h>
#include <istream>

#include "sindarin-debug.h"

// WARNING: This is a very experimental class to bridge std::stream to Arduino. It's minimally
// tested.
class IstreamFileImpl : public fs::FileImpl {
public:
    IstreamFileImpl(std::istream &stream) : stream_(stream) {}

    auto read(uint8_t *buf, size_t size) -> size_t override {
        stream_.read((char *)buf, size);
        return stream_.gcount();
    }

    auto write(const uint8_t *buf, size_t size) -> size_t override {
        return 0; // Not supported
    }

    auto seek(uint32_t pos, SeekMode mode) -> bool override {
        std::ios::seekdir dir;
        switch (mode) {
        case SeekSet:
            dir = std::ios::beg;
            break;
        case SeekCur:
            dir = std::ios::cur;
            break;
        case SeekEnd:
            dir = std::ios::end;
            break;
        }
        stream_.seekg(pos, dir);
        return stream_.good();
    }

    auto position() const -> size_t override { return stream_.tellg(); }

    auto size() const -> size_t override {
        log_w("REQUESTING SIZE AND THATS BAD");
        // TODO: How could we pass through requests for size?
        return 0;
        // auto pos = stream_.tellg();
        // stream_.seekg(0, std::ios::end);
        // auto size = stream_.tellg();
        // stream_.seekg(pos);
        // return size;
    }

    void flush() override {
        // Not supported
    }

    auto close() -> void override {
        // Nothing to do
        // stream_.close();
    }

    auto getLastWrite() -> time_t override {
        return 0; // Not supported
    }

    auto path() const -> const char * override {
        return ""; // Not supported
    }

    auto name() const -> const char * override {
        return ""; // Not supported
    }

    auto isDirectory() -> boolean override {
        return false; // Not supported
    }

    auto openNextFile(const char *mode) -> fs::FileImplPtr override {
        return nullptr; // Not supported
    }

    auto rewindDirectory() -> void override {
        // Not supported
    }

    operator bool() override { return stream_.good(); }

private:
    std::istream &stream_;
};
