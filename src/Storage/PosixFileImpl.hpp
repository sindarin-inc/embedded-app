#include <esp32-hal-log.h>
#include <fstream>

#include "FSImpl.h"

extern "C" {
#include <sys/stat.h>
}

class PosixFileImpl : public fs::FileImpl {
protected:
    std::fstream file_;
    char path_[200];
    size_t pos_ = 0;

public:
    PosixFileImpl(const char *path) : file_(path) { strncpy(path_, path, sizeof(path_)); };
    ~PosixFileImpl() override = default;
    size_t write(const uint8_t *buf, size_t size) override {
        if (!file_.is_open()) {
            file_.open(path_, std::fstream::out);
        }
        file_.write((const char *)buf, size);
        if (!file_.good()) {
            log_e("Problem writing to file %s: %s", path_, strerror(errno));
        }
        return size;
    };
    size_t read(uint8_t *buf, size_t size) override {
        file_.read((char *)buf, size);
        auto numRead = file_.gcount();
        pos_ += numRead;
        return numRead;
    };
    void flush() override { file_.flush(); }
    bool seek(uint32_t pos, SeekMode mode) override {
        file_.seekg(pos);
        pos_ = pos;
        return file_.good();
    };
    // Normally we would use `tellg` here, but since position is `const` we can't
    // use `tellg` since it could potentially modify the FILE :/
    size_t position() const override { return pos_; };
    size_t size() const override {
        struct stat st;
        stat(path_, &st);
        return st.st_size;
    };
    void close() override {
        log_d("Closing file %s", path_);
        file_.close();
    };
    const char *name() const override { return ""; };
    time_t getLastWrite() override { return 0; };
    boolean isDirectory(void) override { return false; };
    fs::FileImplPtr openNextFile(const char *mode) override { return nullptr; };
    void rewindDirectory(void) override{};
    operator bool() override { return true; };
};
