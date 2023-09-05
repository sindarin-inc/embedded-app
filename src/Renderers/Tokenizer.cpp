#include "Tokenizer.hpp"

#include "NullTokenizer.hpp"
#include "Renderers/RendererStream.hpp"
#include "TxtTokenizer.hpp"

auto Tokenizer::factory(std::shared_ptr<RendererStream> stream, DisplaySystem &display)
    -> std::shared_ptr<Tokenizer> {
    std::shared_ptr<Tokenizer> tokenizer;

    if (stream->isA(StreamType::TXT)) {
        tokenizer = std::make_shared<TxtTokenizer>(display);
    } else {
        log_e("Stream is of wrong type (must be TXT or EPUB)!: %" PRIu8,
              static_cast<uint8_t>(stream->getStreamType()));
        if constexpr (ON_ERROR_EXIT) {
            exit(1);
        }
        tokenizer = std::make_shared<NullTokenizer>(display);
    }
    tokenizer->stream_ = std::move(stream);

    return tokenizer;
}
