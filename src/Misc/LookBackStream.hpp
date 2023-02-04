#pragma once
#include "Renderers/RendererStream.hpp"
#include "Misc/CircBuffer.hpp"

class LookBackStream : public RendererStream {
public:
    LookBackStream(RendererStream* stream, int lookBackSize) 
    : stream_(stream)
    , lookBackBuffer_(lookBackSize) 
    , lookBackBytes_(0)
    , position_(0) {}
    ~LookBackStream() {}

    auto available() -> int;
    auto readBytes(uint8_t* buf, int size) -> int;
    auto read() -> int;
    auto seek(uint32_t pos) -> bool;
    auto position() -> size_t;
    auto size() -> size_t;

private:
    RendererStream* stream_;
    CircBuffer lookBackBuffer_;
    int lookBackBytes_;
    int position_;
};