#include "EPubMetaFile.hpp"

#include "pugixml.hpp"

void EPubMetaFile::parse() {

    if (zipFile_.openFile(path_) != UNZ_OK) {
        log_e("Failed to open ePub manifest %s", path_);
    } else {
        unz_file_info info;
        zipFile_.getFileInfo(&info, nullptr, 0, nullptr, 0);

        char *buffer = new char[info.uncompressed_size + 1];
        uint32_t len = zipFile_.readFromFile((uint8_t *)buffer, info.uncompressed_size);

        if (len != info.uncompressed_size) {
            log_e("Failed to read file %s: read %" PRIu32 " but expected %" PRIu32 " bytes", path_,
                  len, static_cast<uint32_t>(info.uncompressed_size));
        }
        buffer[len] = '\0';

        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_string(buffer);

        if (result) {
            for (pugi::xml_node rootfile :
                 doc.child("container").child("rootfiles").children("rootfile")) {
                rootfilePaths.emplace_back(rootfile.attribute("full-path").value());
                // log_w("-----------> adding %s", rootfile.attribute("full-path").value());
            }
        } else {
            log_e("Faile to parse EPub manifest %s: %s", path_, result.description());
        }

        delete[] buffer;

        if (zipFile_.closeFile() != UNZ_OK) {
            log_e("Failed to close ePub manifest %s", path_);
        }
    }
}
