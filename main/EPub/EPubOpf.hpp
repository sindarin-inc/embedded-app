#include <expat.h>
#include <map>
#include <string>
#include <vector>

#include "Misc/ZipFile.hpp"

class EPubData {
public:
    struct ManifestItem {
        std::string href;
        std::string id;
        std::string mediaType;
        int uncompressedSize; // Must be loaded seprately
        [[nodiscard]] auto fullPath() const -> std::string { return "OEBPS/" + href; }
    };

    std::string title;
    std::string creator;
    std::vector<ManifestItem> manifest;

    // Don't love that these state things are here
    bool parsingTitle = false;
    bool parsingCreator = false;
    bool parsingManifest = false;
};

class EPubOpf {
public:
    EPubOpf(ZipFile &zipFile, const char *path) : path_(path), zipFile_(zipFile) {}

    auto parse() -> EPubData {
        EPubData data;
        XML_Parser parser = XML_ParserCreate(nullptr);
        XML_SetUserData(parser, &data);
        XML_SetElementHandler(parser, startElementHandler, endElementHandler);
        XML_SetCharacterDataHandler(parser, characterDataHandler);

        if (zipFile_.openFile(path_) != UNZ_OK) {
            log_e("Failed to open ePub manifest %s", path_);
            return data;
        }

        char buffer[1024];
        int done;
        do {
            size_t len = zipFile_.readFromFile((uint8_t *)buffer, sizeof(buffer));
            done = len < sizeof(buffer);
            if (XML_Parse(parser, buffer, len, done) == XML_STATUS_ERROR) {
                // Handle parse error
                break;
            }
        } while (!done);

        XML_ParserFree(parser);
        if (zipFile_.closeFile() != UNZ_OK) {
            log_e("Failed to close ePub manifest %s", path_);
        }

        return data;
    }

protected:
    const char *path_;
    ZipFile &zipFile_;

    static void XMLCALL startElementHandler(void *userData, const char *name, const char **attrs) {
        auto *data = static_cast<EPubData *>(userData);
        if (strcmp(name, "dc:title") == 0) {
            data->parsingTitle = true;
        } else if (strcmp(name, "dc:creator") == 0) {
            data->parsingCreator = true;
        } else if (strcmp(name, "manifest") == 0) {
            data->parsingManifest = true;
        } else if (data->parsingManifest && strcmp(name, "item") == 0) {
            EPubData::ManifestItem item;
            for (int i = 0; attrs[i]; i += 2) {
                if (strcmp(attrs[i], "href") == 0) {
                    item.href = attrs[i + 1];
                } else if (strcmp(attrs[i], "id") == 0) {
                    item.id = attrs[i + 1];
                } else if (strcmp(attrs[i], "media-type") == 0) {
                    item.mediaType = attrs[i + 1];
                }
            }
            data->manifest.push_back(item);
        }
    }

    static void XMLCALL endElementHandler(void *userData, const char *name) {
        auto *data = static_cast<EPubData *>(userData);
        if (strcmp(name, "dc:title") == 0) {
            data->parsingTitle = false;
        } else if (strcmp(name, "dc:creator") == 0) {
            data->parsingCreator = false;
        } else if (strcmp(name, "manifest") == 0) {
            data->parsingManifest = false;
        }
    }

    static void XMLCALL characterDataHandler(void *userData, const char *s, int len) {
        auto *data = static_cast<EPubData *>(userData);
        if (data->parsingTitle) {
            data->title.append(s, len);
        } else if (data->parsingCreator) {
            data->creator.append(s, len);
        }
    }
};
