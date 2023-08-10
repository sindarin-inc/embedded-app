#include "EPubOpf.hpp"

#include <pugixml.hpp>

void EPubOpf::parse() {
    if (zipFile_.openFile(path_) != UNZ_OK) {
        log_e("Failed to open ePub manifest %s", path_);
    } else {

        unz_file_info info;
        zipFile_.getFileInfo(&info, nullptr, 0, nullptr, 0);

        char *buffer = new char[info.uncompressed_size];
        size_t len = zipFile_.readFromFile((uint8_t *)buffer, info.uncompressed_size);
        zipFile_.closeFile();

        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_string(buffer);

        if (result) {
            totalSize_ = 0;

            pugi::xml_node package = doc.child("package");
            pugi::xml_node metadata = package.child("metadata");
            title_ = metadata.child("dc:title").value();
            creator_ = metadata.child("dc:creator").value();

            for (auto item : package.child("manifest").children("item")) {
                manifest_[item.attribute("id").value()] =
                    ManifestItem{.href = item.attribute("href").value(),
                                 .mediaType = item.attribute("media-type").value()};
                // log_w("------------> adding manifest: href:%s, id:%s, mediaType:%s",
                //       item.attribute("href").value(), item.attribute("id").value(),
                //       item.attribute("media-type").value());
            }

            for (auto itemref : package.child("spine").children("itemref")) {
                std::string idref = itemref.attribute("idref").value();

                ManifestIterator it = manifest_.find(idref);
                if (it != manifest_.end()) {
                    zipFile_.openFile(it->second.fullPath().c_str());
                    unz_file_info info;
                    zipFile_.getFileInfo(&info, nullptr, 0, nullptr, 0);
                    uint32_t usize = info.uncompressed_size;
                    totalSize_ += usize;
                    zipFile_.closeFile();

                    spine_.push_back(SpineItem{.idRef = idref, .uncompressedSize = usize});
                    // log_w("------------> adding spine: idref:%s",
                    // itemref.attribute("idref").value());
                } else {
                    log_e("Spine idref not found in manifest: %s", idref.c_str());
                }
            }
        } else {
            log_e("Faile to parse EPub manifest %s: %s", path_, result.description());
        }

        delete[] buffer;
    }
}
