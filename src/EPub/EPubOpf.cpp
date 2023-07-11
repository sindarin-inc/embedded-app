
#include "EPubOpf.hpp"

bool EPubOpf::parsingTitle;
bool EPubOpf::parsingCreator;
bool EPubOpf::parsingManifest;
bool EPubOpf::parsingSpine;

auto EPubOpf::parse(EPubOpfData & opfData) -> bool {
    XML_Parser parser = XML_ParserCreate(nullptr);
    XML_SetUserData(parser, &opfData);
    XML_SetElementHandler(parser, startElementHandler, endElementHandler);
    XML_SetCharacterDataHandler(parser, characterDataHandler);

    if (zipFile_.openFile(path_) != UNZ_OK) {
        log_e("Failed to open ePub manifest %s", path_);
        return false;
    }

    parsingTitle = false;
    parsingCreator = false;
    parsingManifest = false;
    parsingSpine = false;

    opfData.setTitle("");
    opfData.setCreator("");

    char buffer[1024];
    int done;
    do {
        size_t len = zipFile_.readFromFile((uint8_t *)buffer, sizeof(buffer));
        done = len < sizeof(buffer);
        if (XML_Parse(parser, buffer, len, done) == XML_STATUS_ERROR) {
            // Handle parse error
            log_e("Parse error!");
            return false;
        }
    } while (!done);

    XML_ParserFree(parser);
    if (zipFile_.closeFile() != UNZ_OK) {
        log_e("Failed to close ePub manifest %s", path_);
        return false;
    }

    return true;
}

void XMLCALL EPubOpf::startElementHandler(void *userData, const char *name,
                                                 const char **attrs) {
    auto *data = static_cast<EPubOpfData *>(userData);
    if (parsingManifest && strcmp(name, "item") == 0) {
        EPubOpfData::ManifestItem item;
        for (int i = 0; attrs[i]; i += 2) {
            if (strcmp(attrs[i], "href") == 0) {
                item.href = attrs[i + 1];
            } else if (strcmp(attrs[i], "id") == 0) {
                item.id = attrs[i + 1];
            } else if (strcmp(attrs[i], "media-type") == 0) {
                item.mediaType = attrs[i + 1];
            }
        }
        data->addManifestItem(item);
    } else if (parsingSpine && strcmp(name, "itemRef") == 0) {
        EPubOpfData::SpineItem item;
        if (strcmp(attrs[0], "idref") == 0) {
            item.idRef = attrs[1];
        }
        data->addSpineItem(item);
    } else if (strcmp(name, "dc:title") == 0) {
        parsingTitle = true;
    } else if (strcmp(name, "dc:creator") == 0) {
        parsingCreator = true;
    } else if (strcmp(name, "manifest") == 0) {
        parsingManifest = true;
    } else if (strcmp(name, "spine") == 0) {
        parsingSpine = true;
    }
}

void XMLCALL EPubOpf::endElementHandler(void *userData, const char *name) {
    auto *data = static_cast<EPubOpfData *>(userData);
    if (strcmp(name, "dc:title") == 0) {
        parsingTitle = false;
    } else if (strcmp(name, "dc:creator") == 0) {
        parsingCreator = false;
    } else if (strcmp(name, "manifest") == 0) {
        parsingManifest = false;
    } else if (strcmp(name, "spine") == 0) {
        parsingSpine = false;
    }
}

void XMLCALL EPubOpf::characterDataHandler(void *userData, const char *s, int len) {
    auto *data = static_cast<EPubOpfData *>(userData);
    if (parsingTitle) {
        data->setTitle(std::string(s, len));
    } else if (parsingCreator) {
        data->setCreator(std::string(s, len));
    }
}