#pragma once

#include <Spiram/Memory.hpp>

#include "Renderers/RendererStream.hpp"

class BMPImage {
public:
    BMPImage() : outputBuffer_(nullptr) { createLUT(); }
    ~BMPImage() = default;

    auto readFromBytes(uint8_t *bmpbytes, int size) -> bool;
    auto readFromStream(RendererStream &stream) -> bool;
    [[nodiscard]] auto getWidth() const -> int;
    [[nodiscard]] auto getHeight() const -> int;
    [[nodiscard]] auto getBitsPerPixel() const -> int;
    auto getBuffer() -> uint8_t *;

private:
    std::shared_ptr<uint8_t[]> outputBuffer_;
    int bitsPerPixel_{0};
    int width_{0};
    int height_{0};

    void createLUT();
};