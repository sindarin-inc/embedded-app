#include <expat.h>
#include <string>
#include <vector>

#include "Misc/ZipFile.hpp"

class EPubMetaFile {
public:
    EPubMetaFile(ZipFile &zipFile, const char *path) : path_(path), zipFile_(zipFile) {}

    void parse();

    std::vector<std::string> rootfilePaths;

private:
    const char *path_;
    ZipFile &zipFile_;

    static void XMLCALL startElementHandler(void *userData, const char *name, const char **attrs) {
        // TODO: There might be some variation here in structure
        if (strcmp(name, "rootfile") == 0) {
            for (int i = 0; attrs[i]; i += 2) {
                if (strcmp(attrs[i], "full-path") == 0) {
                    auto *rootfilePaths = static_cast<std::vector<std::string> *>(userData);
                    rootfilePaths->push_back(attrs[i + 1]);
                    break;
                }
            }
        }
    }

    static void XMLCALL endElementHandler(void *userData, const char *name) {}

    static void XMLCALL characterDataHandler(void *userData, const char *s, int len) {}
};
