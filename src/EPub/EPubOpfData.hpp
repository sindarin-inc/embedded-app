#include <string>
#include <vector>

#include "Renderers/RendererDefs.hpp"

class EPubOpfData {
public:
    struct ManifestItem {
        std::string href;
        std::string id;
        std::string mediaType;
        int uncompressedSize; // Must be loaded seprately
        [[nodiscard]] auto fullPath() const -> std::string { return "OEBPS/" + href; }
    };

    struct SpineItem {
        std::string idRef;
    };

private:
    std::string title_;
    std::string creator_;
    std::vector<ManifestItem> manifest_;
    std::vector<SpineItem> spine_;

public:
    inline void setTitle(std::string theTitle) { title_ = theTitle; }
    inline void setCreator(std::string theCreator) { creator_ = theCreator; }

    inline void addSpineItem(SpineItem &item) { spine_.push_back(item); };
    inline void addManifestItem(ManifestItem &item) { manifest_.push_back(item); }

    [[nodiscard]] inline auto getFileCount() -> uint16_t { return spine_.size(); }

    [[nodiscard]] auto getFilePath(renderer_defs::StreamPlace &place) -> std::string;
};
