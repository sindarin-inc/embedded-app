#include "PBMImage.hpp"

auto SkipSpace(RendererStream &stream) -> bool {
    uint8_t tmp = ' ';
    while (isspace(tmp)) {
        if (stream.readBytes(&tmp, 1) != 1) {
            return false;
        }
    }

    // back up one byte
    if (!stream.seek(stream.position() - 1)) {
        return false;
    }
    return true;
}

auto SkipToNewLine(RendererStream &stream) -> bool {
    uint8_t tmp = 0;
    while (tmp != '\n') {
        if (stream.readBytes(&tmp, 1) != 1) {
            return false;
        }
    }
    return true;
}

// attempt to read a text integer value from the stream
auto ReadInt(RendererStream &stream, int &val) -> bool {
    uint8_t tmp[5];
    uint8_t *str = tmp;
    int size = 0;
    int maxSize = 4;
    // read first character
    if (stream.readBytes(str, 1) != 1) {
        return false;
    }

    while (!isspace(*str)) {
        str++;
        if (++size > maxSize) {
            return false;
        }
        if (stream.readBytes(str, 1) != 1) {
            return false;
        }
    }
    // terminate string
    *str = 0;
    val = strtol((const char *)tmp, nullptr, sizeof(tmp));
    return true;
}

auto SkipComment(RendererStream &stream, bool &error) -> bool {
    uint8_t tmp;
    error = true;
    // read first character of next line
    if (stream.readBytes(&tmp, 1) != 1) {
        return false;
    }

    if (tmp != '#') {
        if (!stream.seek(stream.position() - 1)) {
            return false;
        }
        error = false;
        return false;
    }

    if (!SkipToNewLine(stream)) {
        error = true;
        return false;
    }

    return true;
}

auto SkipComments(RendererStream &stream) -> bool {
    bool error = false;
    while (SkipComment(stream, error)) {
    }

    if (error) {
        return false;
    }

    return true;
}

auto ReadPbmType(RendererStream &stream) -> bool {
    uint8_t tmp[2];
    if (stream.readBytes(tmp, 2) != 2) {
        return false;
    }

    // pbm format, 1 bit binary only
    if (tmp[0] != 'P' || tmp[1] != '4') {
        return false;
    }

    // skip whitespace
    if (!SkipSpace(stream)) {
        return false;
    }

    return true;
}

auto ParsePbmHeader(RendererStream &stream, int &width, int &height) -> bool {
    if (!ReadPbmType(stream)) {
        return false;
    }
    if (!SkipComments(stream)) {
        return false;
    }
    if (!ReadInt(stream, width)) {
        return false;
    }
    if (!ReadInt(stream, height)) {
        return false;
    }
    return true;
}