#ifndef __UPDATE_H__
#define __UPDATE_H__

#include <stdint.h>

#include "sindarin-debug.h"

#define U_FLASH 0

class update {
public:
    bool begin(int totalSize, int type) {
        totalSize_ = totalSize;
        return true;
    }
    int write(uint8_t *buffer, int size) {
        size_ += size;
        return size;
    }
    bool setMD5(const char *md5) { return true; }
    uint8_t getError() { return 0; }
    void abort() {
        size_ = 0;
        totalSize_ = 0;
    }

    bool end() {
        bool success = size_ == totalSize_;
        if (!success) {
            log_e("Update.end failed with size_=%d totalSize_=%d", size_, totalSize_);
        }
        size_ = 0;
        totalSize_ = 0;
        return success;
    }
    bool isRunning() { return size_ > 0; }

protected:
    int size_ = 0;
    int totalSize_ = 0;
};

class esp {
public:
    void restart() {}
};

update Update;
esp ESP;

#endif