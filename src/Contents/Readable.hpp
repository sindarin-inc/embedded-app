#pragma once
#ifndef READABLE_H
#define READABLE_H

class Readable {
public:
    const char *id;
    const char *name;
    const char *path;
    unsigned long place, placePrev;
    Readable(const char *id, const char *name, const char *path)
        : id(id), name(name), path(path), place(0), placePrev(0){};
    Readable(const Readable &r) = default;
    ;
};
#endif
