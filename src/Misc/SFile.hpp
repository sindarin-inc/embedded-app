#pragma once

#include <stdio.h>
#include <stdint.h>
#include <memory>
#include "sindarin-debug.h"

enum SSeekMode { SSeekSet = 0, SSeekCur = 1, SSeekEnd = 2 };

class SFileImpl
{
public:
    virtual ~SFileImpl() { }
    virtual size_t write(const uint8_t *buf, size_t size) = 0;
    virtual size_t read(uint8_t* buf, size_t size) = 0;
    virtual void flush() = 0;
    virtual bool seek(uint32_t pos, SSeekMode mode) = 0;
    virtual size_t position() const = 0;
    virtual size_t size() const = 0;
    virtual void close() = 0;
    //virtual time_t getLastWrite() = 0;
    virtual const char* name() const = 0;
    virtual const char* path() const = 0;
    virtual boolean isDirectory(void) = 0;
    //virtual FileImplPtr openNextFile(const char* mode) = 0;
    virtual void rewindDirectory(void) = 0;
    virtual operator bool() = 0;
};

class SFile {
public:
    SFile(std::shared_ptr<SFileImpl> p) : _p(p) {}

    size_t write(uint8_t c);
    size_t write(const uint8_t *buf, size_t size);
    int available();
    int read();
    int peek();
    void flush();
    size_t read(uint8_t *buf, size_t size);
    size_t readBytes(char *buffer, size_t length);

    bool seek(uint32_t pos, SSeekMode mode);
    bool seek(uint32_t pos) { return seek(pos, SSeekSet); }
    size_t position() const;
    size_t size() const;
    void close();
    operator bool() const;
    //time_t getLastWrite();
    const char *name() const;
    const char *path() const;

    boolean isDirectory(void);
    //File openNextFile(const char *mode = FILE_READ);
    void rewindDirectory(void);

protected:
    std::shared_ptr<SFileImpl> _p;
};

class VFSFile : SFileImpl {
public:
    VFSFile(const char* fname) {
        log_i("opening %s", fname);
        fp_ = fopen(fname, "rb");

        if (fp_) {
            log_i("opened");
        } else {
            log_i("not opened");
        }
    }
    ~SFile() {}

    size_t write(uint8_t);
    size_t write(const uint8_t *buf, size_t size);
    int available();
    //int read();
    int peek();
    void flush();
    size_t read(uint8_t *buf, size_t size);
    size_t readBytes(char *buffer, size_t length);

    bool seek(uint32_t pos, SSeekMode mode);
    bool seek(uint32_t pos) { return seek(pos, SSeekSet); }
    size_t position() const;
    size_t size() const;
    void close();
    operator bool() const {
        if (fp_) {
            return true;
        }

        return false;
    }
    //time_t getLastWrite();
    const char *name() const;
    const char *path() const;

    bool isDirectory(void);
    //SFile openNextFile(const char *mode = "r");
    void rewindDirectory(void);
protected:

    FILE* fp_{nullptr};
    size_t fSize_{0};

    void getFileSize();
};