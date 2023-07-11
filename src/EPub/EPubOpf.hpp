#include <expat.h>
#include <map>

#include "EPubOpfData.hpp"
#include "Misc/ZipFile.hpp"
#include "Models/Place.hpp"

class EPubOpf {
public:
    EPubOpf(ZipFile &zipFile, const char *path) : path_(path), zipFile_(zipFile) {}

    [[nodiscard]] auto parse(EPubOpfData & opfData) -> bool;

private:
    static bool parsingTitle;
    static bool parsingCreator;
    static bool parsingManifest;
    static bool parsingSpine;

protected:
    const char *path_;
    ZipFile &zipFile_;

    static void XMLCALL startElementHandler(void *userData, const char *name, const char **attrs);
    static void XMLCALL endElementHandler(void *userData, const char *name);
    static void XMLCALL characterDataHandler(void *userData, const char *s, int len);
};
