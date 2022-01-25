#ifndef RAW_RENDERER_H
#define RAW_RENDERER_H

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "Contents/Readable.hpp"
#include "Renderer.hpp"

class StreamRenderer : public Renderer {
public:
    StreamRenderer(DisplaySystem &display) : Renderer(display) {}

    void setString(std::string &string);

    auto available() -> int override { return stream_.peek() != EOF; }
    auto read() -> int override { return stream_.get(); };
    auto seek(uint32_t pos) -> bool override {
        stream_.seekg(pos);
        return stream_.good();
    };
    auto position() -> size_t override { return stream_.tellg(); }
    auto size() -> size_t override { return 0; }

protected:
    std::istringstream stream_;
};

#endif
