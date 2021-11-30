#include "FakeSerial.h"
#include <stdarg.h>
#include <stdio.h>

void FakeSerial::begin(int rate) 
{
    this->printf("rate is %d\n", rate);
}
int FakeSerial::printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = 0;
    ret = vprintf(format, args);
    va_end(args);
    return ret;
}

int FakeSerial::print(const char* str)
{
    this->printf(str);
    this->printf("\n");
    return 0;
}

int FakeSerial::println()
{
    printf("\n");
    return 0;
}

int FakeSerial::println(const char* str)
{
    this->printf(str);
    return 0;
}

int FakeSerial::write(uint8_t* data, int bytes)
{
    this->printf("write called %d bytes\n", bytes);
    return 0;
}

FakeSerial::operator bool() const
{
    return true;
}
