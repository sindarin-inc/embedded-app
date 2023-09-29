#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "sindarin-debug.h"

#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"
enum SSeekMode { SSeekSet = 0, SSeekCur = 1, SSeekEnd = 2 };

class SFileImpl {
public:
    virtual ~SFileImpl() = default;
    virtual auto write(const uint8_t *buf, size_t size) -> size_t = 0;
    virtual auto read(uint8_t *buf, size_t size) -> size_t = 0;
    virtual auto flush() -> int = 0;
    virtual auto seek(uint32_t pos, SSeekMode mode) -> bool = 0;
    [[nodiscard]] virtual auto position() const -> size_t = 0;
    [[nodiscard]] virtual auto size() const -> size_t = 0;
    virtual auto close() -> int = 0;
    virtual operator bool() const = 0;
    virtual auto getFilePtr() -> FILE * = 0;
};

class SStream {
public:
    SStream() = default;
    ~SStream() = default;

    virtual auto write(uint8_t c) -> size_t = 0;
    auto write(const char *str) -> size_t {
        if (str == nullptr) {
            return 0;
        }
        return write((const uint8_t *)str, strlen(str));
    }

    virtual auto write(const uint8_t *buf, size_t size) -> size_t = 0;
    virtual auto available() -> int = 0;
    virtual auto read() -> int = 0;
    virtual auto readString() -> std::string {
        std::string ret;
        int c = read();
        while (c >= 0) {
            ret += (char)c;
            c = read();
        }
        return ret;
    }
    auto readStringUntil(char terminator) -> std::string {
        std::string res = "";
        int value;
        while (true) {
            value = read();
            if (value <= 0 || value == terminator) {
                return res;
            }
            res += (char)value;
        }
        return res;
    }
};

class SFile : public SStream {
public:
    SFile() : p_(nullptr) {}
    SFile(std::shared_ptr<SFileImpl> p) : p_(std::move(p)) {}

    auto write(uint8_t c) -> size_t override;
    auto write(const uint8_t *buf, size_t size) -> size_t override;
    auto available() -> int override;
    auto read() -> int override;
    auto peek() -> int;
    auto flush() -> int;
    auto read(uint8_t *buf, size_t size) -> size_t;
    auto readBytes(char *buffer, size_t length) -> size_t;

    auto seek(uint32_t pos, SSeekMode mode) -> bool;
    auto seek(uint32_t pos) -> bool { return seek(pos, SSeekSet); }
    [[nodiscard]] auto position() const -> size_t;
    [[nodiscard]] auto size() const -> size_t;
    auto close() -> int;
    operator bool() const;

    auto getFilePtr() -> FILE * { return p_->getFilePtr(); }

protected:
    std::shared_ptr<SFileImpl> p_;
};

class VFSFile : public SFileImpl {
public:
    VFSFile(const char *fname, const char *mode, const bool create) {
        std::string modeStr(mode);
        std::string openMode = "";
        bool read = false;
        if (modeStr.find('r') != std::string::npos) {
            openMode = "rb";
            read = true;
        } else if (modeStr.find('w') != std::string::npos) {
            openMode = "wb";
        } else if (modeStr.find('a') != std::string::npos) {
            openMode = "ab+";
        }
        log_d("Opening file %s with mode string %s", fname, openMode.c_str());
        fp_ = fopen(fname, openMode.c_str());

        if (fp_) {
            if (read) {
                getFileSize();
            }
        } else {
            log_e("Failed to open file: %s", fname);
        }
    }
    ~VFSFile() override = default;

    auto write(const uint8_t *buf, size_t size) -> size_t override;
    auto available() -> int;
    auto flush() -> int override;
    auto read(uint8_t *buf, size_t size) -> size_t override;

    auto seek(uint32_t pos, SSeekMode mode) -> bool override;
    [[nodiscard]] auto position() const -> size_t override;
    [[nodiscard]] auto size() const -> size_t override;
    auto close() -> int override;
    operator bool() const override;
    auto getFilePtr() -> FILE * override;

protected:
    FILE *fp_{nullptr};
    size_t fSize_{0};

    void getFileSize();
};