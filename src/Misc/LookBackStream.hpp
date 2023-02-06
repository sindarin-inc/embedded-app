#pragma once
#include "Misc/CircBuffer.hpp"
#include "Renderers/RendererStream.hpp"

class LookBackStream : public RendererStream {
public:
    LookBackStream(RendererStream *stream, int lookBackSize)
        : stream_(stream), lookBackBuffer_(lookBackSize) {}

    ~LookBackStream() override {
        if (stream_) {
            delete stream_;
        }
    }

    auto available() -> int override;
    auto readBytes(uint8_t *buf, int size) -> int override;
    auto read() -> int override;
    auto seek(uint32_t pos) -> bool override;
    auto position() -> size_t override;
    auto size() -> size_t override;

private:
    RendererStream *stream_;
    CircBuffer lookBackBuffer_;
    int lookBackBytes_{0};
    int position_{0};
};