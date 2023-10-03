#include "EPubMetaFile.hpp"

#include "pugixml.hpp"

void EPubMetaFile::parse(const std::shared_ptr<uint8_t[]> &buffer, uint32_t length) {

    pugi::xml_document doc;

    EPUB_PROFILE_START(XMLParseManifest);
    pugi::xml_parse_result result = doc.load_buffer_inplace(buffer.get(), length);
    EPUB_PROFILE_END(XMLParseManifest);

    if (result) {
        for (pugi::xml_node &rootfile :
             doc.child("container").child("rootfiles").children("rootfile")) {
            rootfilePaths.emplace_back(rootfile.attribute("full-path").value());
            // log_w("-----------> adding %s", rootfile.attribute("full-path").value());
        }
    } else {
        log_e("Failed to parse EPub manifest %s: %s", path_, result.description());
    }
}
