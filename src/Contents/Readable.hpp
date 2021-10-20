#pragma once
#ifndef READABLE_H
#define READABLE_H

class Readable {
public:
    const char *id;
    const char *name;
    const char *path;
    unsigned long place, place_prev, place_screed;
    Readable(const char *id, const char *name, const char *path)
        : id(id), name(name), path(path), place(0), place_prev(0), place_screed(0){};
    Readable(const Readable &r) = default;
    ;
};
#endif
