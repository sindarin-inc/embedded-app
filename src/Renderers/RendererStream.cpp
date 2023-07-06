#include "RendererStream.hpp"

#include "Misc/StringUtil.hpp"

auto RendererStream::getType(const char *path) -> StreamType {
    if (HasSuffix(path, ".txt") || HasSuffix(path, ".txt.gz")) {
        return StreamType::TXT;
    }
    return StreamType::OTHER;
}
