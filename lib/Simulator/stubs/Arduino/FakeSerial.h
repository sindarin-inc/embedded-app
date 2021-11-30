#include <inttypes.h>

class FakeSerial {
public:
    void begin(int rate);
    int printf(const char *format, ...);

    int pp2(const char* format, ...);
    int print(const char* str);
    int println();
    int println(const char* str);
    int write(uint8_t* data, int bytes);
    operator bool() const;
};

extern FakeSerial Serial;
