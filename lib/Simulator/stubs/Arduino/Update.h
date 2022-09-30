#ifndef __UPDATE_H__
#define __UPDATE_H__

#include <stdint.h>

#define U_FLASH 0

class update {
public:
    void begin(int totalSize, int type) {}
    int write(uint8_t* buffer, int size) {
        return size;
    }
    bool setMD5(const char* md5) {
        return true;
    }

    bool end() {
        return true;
    }

};

class esp {
public:
    void restart() {}
};

update Update;
esp ESP;

#endif