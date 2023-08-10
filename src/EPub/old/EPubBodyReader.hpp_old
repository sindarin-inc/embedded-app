#include <expat.h>
#include <map>
#include <string>
#include <vector>

#include "Misc/ZipFile.hpp"

class EPubBodyReader : public {
public:
    EPubBodyReader(ZipFile &zipFile, const char *path) : path_(path), zipFile_(zipFile) {}

    EPubData parse() {
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
        EPubData *data = static_cast<EPubData *>(userData);
        if (strcmp(name, "dc:title") == 0) {
            data->parsing_title_ = true;
        } else if (strcmp(name, "dc:creator") == 0) {
            data->parsing_creator_ = true;
        } else if (strcmp(name, "manifest") == 0) {
            data->parsing_manifest_ = true;
        } else if (data->parsing_manifest_ && strcmp(name, "item") == 0) {
            EPubData::ManifestItem item;
            for (int i = 0; attrs[i]; i += 2) {
                if (strcmp(attrs[i], "href") == 0) {
                    item.href = attrs[i + 1];
                } else if (strcmp(attrs[i], "id") == 0) {
                    item.id = attrs[i + 1];
                } else if (strcmp(attrs[i], "media-type") == 0) {
                    item.media_type = attrs[i + 1];
                }
            }
            data->manifest.push_back(item);
        }
    }

    static void XMLCALL endElementHandler(void *userData, const char *name) {
        EPubData *data = static_cast<EPubData *>(userData);
        if (strcmp(name, "dc:title") == 0) {
            data->parsing_title_ = false;
        } else if (strcmp(name, "dc:creator") == 0) {
            data->parsing_creator_ = false;
        } else if (strcmp(name, "manifest") == 0) {
            data->parsing_manifest_ = false;
        }
    }

    static void XMLCALL characterDataHandler(void *userData, const char *s, int len) {
        EPubData *data = static_cast<EPubData *>(userData);
        if (data->parsing_title_) {
            data->title.append(s, len);
        } else if (data->parsing_creator_) {
            data->creator.append(s, len);
        }
    }
};
