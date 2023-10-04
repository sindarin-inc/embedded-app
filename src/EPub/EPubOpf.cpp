#include "EPubOpf.hpp"

#include <cstring>

#include "EPubDefs.hpp"
#include "pugixml.hpp"

void EPubOpf::parse(const std::shared_ptr<uint8_t[]> &buffer, uint32_t length,
                    const std::shared_ptr<Readable> readable) {

    basePath_ = "";
    extractPath(path_, basePath_);
    // log_i("Base Path: %s", basePath_.c_str());

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer_inplace(buffer.get(), length);

    if (result) {
        // totalSize_ = 0;

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
            // log_w("------------> adding manifest: href:%s, id:%s, mediaType:%s",
            //       item.attribute("href").value(), item.attribute("id").value(),
            //       item.attribute("media-type").value());
        }

        for (auto itemref : spine.children("itemref")) {
            std::string idref = itemref.attribute("idref").value();

            auto it = manifest_.find(idref);
            if (it != manifest_.end()) {

                spine_.push_back(SpineItem{.item = &it->second, .readableChapterIndex = -1
                                           /* .uncompressedSize = usize,  .offset = totalSize_ */});
                // log_w("------------> adding spine: idref:%s, href:%s, size:%" PRIu32 "
                // offset:%" PRIu32,
                //       itemref.attribute("idref").value(), it->second.href.c_str(), usize,
                //       totalSize_);

                // totalSize_ += usize;
            } else {
                log_e("Spine idref not found in manifest: %s", idref.c_str());
            }
        }

        if (readable->toc.size() == 0) {
            log_e("Readable TOC size is 0!!");
        }
        for (int32_t i = 0; i < readable->toc.size(); i++) {
            log_w("toc href: %s", readable->toc[i].href.c_str());
            int32_t spineIdx = getSpineIdx(readable->toc[i].href);
            spine_[spineIdx].readableChapterIndex = i;
            log_w("Found spineIdx %" PRIi32 "for Chapter Idx %" PRIi32, spineIdx, i);
        }
    } else {
        log_e("Failed to parse EPub opf %s: %s", path_, result.description());
    }
}

// auto EPubOpf::getSpineIdxForOffset(uint32_t offset) const -> Idx {
//     int spineIdx = 0;
//     while (spineIdx < spine_.size()) {
//         if (spine_[spineIdx].offset > offset) break;
//         spineIdx += 1;
//     }

//     return spineIdx > 0 ? spineIdx - 1 : 0;
// }

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

auto EPubOpf::getReadableChapterIndex(const std::string &href) const -> int {
    int spineIdx = getSpineIdx(href);
    while (spineIdx >= 0) {
        if (spine_[spineIdx].readableChapterIndex != -1) {
            return spine_[spineIdx].readableChapterIndex;
        }
        spineIdx -= 1;
    }

    return 0;
}
