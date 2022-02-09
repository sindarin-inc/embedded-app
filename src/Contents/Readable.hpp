#pragma once
#ifndef READABLE_H
#define READABLE_H

#include <cstdint>

class Readable {
public:
    const char *id;
    const char *name;
    const char *path;
    uint32_t place, placePrev;
    Readable(const char *id, const char *name, const char *path)
        : id(id), name(name), path(path), place(0), placePrev(0){};
    Readable(const Readable &r) = default;
    ;
};
#endif
