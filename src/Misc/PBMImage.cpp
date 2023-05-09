#include "PBMImage.hpp"

bool skipSpace(RendererStream &stream) {
    uint8_t tmp = ' ';
    while (isspace(tmp)) {
        if (stream.readBytes(&tmp, 1) != 1) return false;
    }

    // back up one byte
    if (!stream.seek(stream.position() - 1)) return false;
    return true;
}

bool skipToNewLine(RendererStream &stream) {
    uint8_t tmp = 0;
    while (tmp != '\n') {
        if (stream.readBytes(&tmp, 1) != 1) return false;
    }
    return true;
}

// attempt to read a text integer value from the stream
bool readInt(RendererStream &stream, int &val) {
    uint8_t tmp[5];
    uint8_t *str = tmp;
    int size = 0;
    int maxSize = 4;
    // read first character
    if (stream.readBytes(str, 1) != 1) return false;

    while (!isspace(*str)) {
        str++;
        if (++size > maxSize) return false;
        if (stream.readBytes(str, 1) != 1) return false;
    }
    // terminate string
    *str = 0;
    val = atoi((const char *)tmp);
    return true;
}

bool skipComment(RendererStream &stream, bool &error) {
    uint8_t tmp;
    error = true;
    // read first character of next line
    if (stream.readBytes(&tmp, 1) != 1) return false;

    if (tmp != '#') {
        if (!stream.seek(stream.position() - 1)) return false;
        error = false;
        return false;
    }

    if (!skipToNewLine(stream)) {
        error = true;
        return false;
    }

    return true;
}

bool skipComments(RendererStream &stream) {
    bool error = false;
    while (skipComment(stream, error)) {
    }

    if (error) return false;

    return true;
}

bool readPBMType(RendererStream &stream) {
    uint8_t tmp[2];
    if (stream.readBytes(tmp, 2) != 2) return false;

    // pbm format, 1 bit binary only
    if (tmp[0] != 'P' || tmp[1] != '4') return false;

    // skip whitespace
    if (!skipSpace(stream)) return false;

    return true;
}

bool parsePBMHeader(RendererStream &stream, int &width, int &height) {
    if (!readPBMType(stream)) return false;
    if (!skipComments(stream)) return false;
    if (!readInt(stream, width)) return false;
    if (!readInt(stream, height)) return false;
    return true;
}