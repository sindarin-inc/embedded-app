#include "EPubOpf.hpp"

#include <cstring>
#include <pugixml.hpp>

#include "EPubDefs.hpp"

void EPubOpf::parse(const std::shared_ptr<uint8_t[]> &buffer, uint32_t length,
                    const std::shared_ptr<Readable> &readable) {

    basePath_ = "";
    extractPath(path_, basePath_);
    // log_i("Base Path: %s", basePath_.c_str());

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer_inplace(buffer.get(), length);

    if (result) {

        pugi::xml_node package = doc.child("package");
        pugi::xml_node manifest = package.child("manifest");
        pugi::xml_node metadata = package.child("metadata");
        pugi::xml_node spine = package.child("spine");

        title_ = metadata.child("dc:title").value();
        creator_ = metadata.child("dc:creator").value();

        for (auto item : manifest.children("item")) {
            manifest_[item.attribute("id").value()] =
                ManifestItem{.href = item.attribute("href").value(),
                             .mediaType = item.attribute("media-type").value()};
        }

        for (auto itemref : spine.children("itemref")) {
            std::string idref = itemref.attribute("idref").value();

            auto it = manifest_.find(idref);
            if (it != manifest_.end()) {
                spine_.push_back(SpineItem{.item = &it->second, .readableToCChapterIndex = -1});
            } else {
                log_e("Spine idref not found in manifest: %s", idref.c_str());
            }
        }

        for (int32_t i = 0; i < readable->toc.size(); i++) {
            int32_t spineIdx = getSpineIdx(readable->toc[i].href);
            spine_[spineIdx].readableToCChapterIndex = i;
        }
    } else {
        log_e("Failed to parse EPub opf %s: %s", path_, result.description());
    }
}

void EPubOpf::extractPath(const char *fname, std::string &path) {
    path.clear();
    int i = strlen(fname) - 1;
    while ((i > 0) && (fname[i] != '/')) {
        i--;
    }
    if (i > 0) {
        path.assign(fname, ++i);
    }
}

auto EPubOpf::getSpineIdx(const std::string &href) const -> int {
    for (int spineIdx = 0; spineIdx < spine_.size(); spineIdx++) {
        if (spine_[spineIdx].item->href == href) {
            return spineIdx;
        }
    }
    return 0;
}

auto EPubOpf::getReadableToCChapterIndex(const std::string &href) const -> int {
    int spineIdx = getSpineIdx(href);

    while (spineIdx >= 0) {
        if (spine_[spineIdx].readableToCChapterIndex != -1) {
            return spine_[spineIdx].readableToCChapterIndex;
        }
        spineIdx -= 1;
    }

    return -1;
}
