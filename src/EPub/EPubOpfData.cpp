#include "EPubOpfData.hpp"

auto EPubOpfData::getFilePath(renderer_defs::StreamPlace &place) -> std::string {
    if (place.fileNumber < spine_.size()) {
        for (const auto &manifestItem : manifest_) {
            if (manifestItem.id == spine_[place.fileNumber].idRef) {
                return manifestItem.href;
            }
        }
    }
    return std::string();
}