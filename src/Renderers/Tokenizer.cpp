#include "Tokenizer.hpp"

#include "EPubTokenizer.hpp"
#include "Renderers/RendererStream.hpp"
#include "TxtTokenizer.hpp"
#include "NullTokenizer.hpp"

auto Tokenizer::factory(std::shared_ptr<RendererStream> stream, DisplaySystem &display)
    -> std::shared_ptr<Tokenizer> {
    std::shared_ptr<Tokenizer> tokenizer;

    if (stream->isA(StreamType::TXT)) {
        tokenizer = std::make_shared<TxtTokenizer>(display);
    } else if (stream->isA(StreamType::EPUB)) {
        tokenizer = std::make_shared<EPubTokenizer>(display);
    } else {
        log_e("Stream is of wrong type (must be TXT or EPUB)!");
        if constexpr (ON_ERROR_EXIT) {
            exit(1);
        }
        tokenizer = std::make_shared<NullTokenizer>(display);
    }
    tokenizer->stream_ = std::move(stream);

    return tokenizer;
}
