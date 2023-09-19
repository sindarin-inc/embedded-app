#include "EPubOpf.hpp"

#include <cstring>

#include "EPubDefs.hpp"
#include "pugixml.hpp"

void EPubOpf::parse(std::shared_ptr<uint8_t[]> buffer, uint32_t length) {

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

                spine_.push_back(SpineItem{.item = &it->second,
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
    } else {
        log_e("Failed to parse EPub manifest %s: %s", path_, result.description());
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
