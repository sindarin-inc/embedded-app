#pragma once

#include <stdio.h>
#include <stdint.h>
#include <memory>
#include <string>
#include "sindarin-debug.h"

#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"
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

class SStream {
public:
    SStream() {}
    ~SStream() {}

    virtual size_t write(uint8_t c) = 0;
    size_t write(const char *str)
    {
        if(str == NULL) {
            return 0;
        }
        return write((const uint8_t *) str, strlen(str));
    }

    virtual size_t write(const uint8_t *buf, size_t size) = 0;

    virtual int available() = 0;
    virtual int read() = 0;

    virtual std::string readString() { return ""; }
    std::string readStringUntil(char terminator) {
        std::string res = "";
        int value;
        while(true){
            value = read();
            if(value <= 0 || value == terminator) {
                return res;
            }
            res += (char)value;
        }
        return res;
    }
};

class SFile : public SStream {
public:
    SFile() : _p(nullptr) {}
    SFile(std::shared_ptr<SFileImpl> p) : _p(p) {}

    size_t write(uint8_t c);
    size_t write(const uint8_t *buf, size_t size);
    int available() override;
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
    operator bool();
    //time_t getLastWrite();
    const char *name() const;
    const char *path() const;

    boolean isDirectory(void);
    //File openNextFile(const char *mode = FILE_READ);
    void rewindDirectory(void);

protected:
    std::shared_ptr<SFileImpl> _p;
};

class VFSFile : public SFileImpl {
public:
    VFSFile(const char* fname, const char *mode, const bool create) {
        log_i("opening %s", fname);

        std::string modeStr(mode);
        std::string openMode = "";
        bool read = false;
        if (modeStr.find(FILE_READ) != std::string::npos) {
            openMode = "rb";
            read = true;
        } else if (modeStr.find(FILE_WRITE) != std::string::npos) {
            openMode = "wb";
        } else if (modeStr.find(FILE_APPEND) != std::string::npos) {
            openMode = "ab+";
        }
        log_d("Opening file %s with mode string %s", fname, openMode.c_str());
        fp_ = fopen(fname, openMode.c_str());

        if (fp_) {
            log_i("opened");
            if (read) {
                getFileSize();
            }
        } else {
            log_i("not opened");
        }
    }
    ~VFSFile() {}

    size_t write(uint8_t);
    size_t write(const uint8_t *buf, size_t size);
    int available();
    void flush();
    size_t read(uint8_t *buf, size_t size);
    //size_t readBytes(char *buffer, size_t length);

    bool seek(uint32_t pos, SSeekMode mode);
    size_t position() const;
    size_t size() const;
    void close();
    operator bool();
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