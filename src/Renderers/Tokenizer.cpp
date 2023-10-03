#include "Tokenizer.hpp"

#include "EPubTokenizer.hpp"
#include "NullTokenizer.hpp"
#include "Renderers/RendererStream.hpp"
#include "TxtTokenizer.hpp"

auto Tokenizer::factory(const std::shared_ptr<RendererStream> &stream, DisplaySystem &display,
                        Tokens &tokens, Pager &pager) -> std::shared_ptr<Tokenizer> {
    std::shared_ptr<Tokenizer> tokenizer;

    if (stream->isFor(DocType::TXT)) {
        tokenizer = std::make_shared<TxtTokenizer>(display, tokens, pager);
    } else if (stream->isFor(DocType::EPUB)) {
        tokenizer = std::make_shared<EPubTokenizer>(display, tokens, pager);
    } else {
        log_e("Stream is of wrong type (must be TXT or EPUB)!: %" PRIu8,
              static_cast<uint8_t>(stream->getDocType()));
        REND_EXIT_SIM;
        tokenizer = std::make_shared<NullTokenizer>(display, tokens, pager);
    }
    tokenizer->setStream(stream);
    pager.setStream(stream);

    return tokenizer;
}
