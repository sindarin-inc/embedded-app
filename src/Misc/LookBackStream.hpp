#pragma once
#include "Misc/CircBuffer.hpp"
#include "Models/DocType.hpp"
#include "Renderers/RendererStream.hpp"

#define LOOKBACK_BUFFER_SIZE 32768

class LookBackStream : public RendererStream {
public:
    LookBackStream(std::unique_ptr<RendererStream> stream, int lookBackSize,
                   DocType docType = DocType::NONE)
        : RendererStream(docType), stream_(std::move(stream)), lookBackBuffer_(lookBackSize) {}

    auto available() -> int override;
    auto readBytes(uint8_t *buf, int size) -> int override;
    auto read() -> int override;
    auto seek(uint32_t pos) -> bool override;
    auto position() -> size_t override;
    auto size() -> size_t override;

private:
    std::unique_ptr<RendererStream> stream_;
    CircBuffer lookBackBuffer_;
    int lookBackBytes_{0};
    int position_{0};
};