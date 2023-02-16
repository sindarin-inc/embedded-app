#include "EPubMetaFile.hpp"

void EPubMetaFile::parse() {
    XML_Parser parser = XML_ParserCreate(nullptr);
    XML_SetUserData(parser, &rootfilePaths);
    XML_SetElementHandler(parser, startElementHandler, endElementHandler);
    XML_SetCharacterDataHandler(parser, characterDataHandler);

    if (zipFile_.openFile(path_) != UNZ_OK) {
        log_e("Failed to open ePub manifest %s", path_);
    }

    char buffer[128];
    int done;
    do {
        size_t len = zipFile_.readFromFile((uint8_t *)buffer, sizeof(buffer));
        done = len < sizeof(buffer);
        if (XML_Parse(parser, buffer, len, done) == XML_STATUS_ERROR) {
            log_e("Parser error");
            // Handle parse error
            break;
        }
    } while (!done);

    XML_ParserFree(parser);
    if (zipFile_.closeFile() != UNZ_OK) {
        log_e("Failed to close ePub manifest %s", path_);
    }
}
