#include "GZipFile.hpp"

#include <sindarin-debug.h>

#define GZIP_CHUNK_SIZE 1024

auto GZipFile::initWithOutputFileName(const std::string &outputFileName) -> bool {
    // open output filename
    FILE *fp = fopen(outputFileName.c_str(), "wb");
    if (!fp) {
        log_e("GZipFile error: could not open %s", outputFileName.c_str());
        return false;
    }

    return initWithOutputFile(fp);
}

auto GZipFile::initWithOutputFile(SFile &f) -> bool { return initWithOutputFile(f.getFilePtr()); }

auto GZipFile::initWithOutputFile(FILE *fp) -> bool {
    int fd = fileno(fp);
    if (fd == -1) {
        log_e("GZipFile error: could not get file descriptor");
        return false;
    }

    gf_ = gzdopen(fd, "w");
    if (!gf_) {
        log_e("GZipFile error: gzdopen failed");
        return false;
    }

    return true;
}

auto GZipFile::writeMemory(uint8_t *mem, int size) -> bool {
    if (!gf_) {
        log_e("GZipFile error: no output file was opened");
        return false;
    }

    int bytesWritten = gzwrite(gf_, mem, size);
    if (bytesWritten != size) {
        log_e("GZipFile error: should have written %d bytes, return was %d", size, bytesWritten);
        return false;
    }

    return true;
}

auto GZipFile::close() -> bool {
    if (!gf_) {
        log_e("GZipFile error: close(), no output file was opened");
        return false;
    }

    int ret = gzclose(gf_);
    gf_ = nullptr;

    if (ret != Z_OK) {
        log_e("GZipFile error: close(), failed gzclose");
        return false;
    }

    return true;
}

auto GZipFile::compressFile(const std::string &inputFileName) -> bool {
    if (!gf_) {
        log_e("GZipFile error: no output file was opened");
        return false;
    }

    // open input filename
    FILE *infp = fopen(inputFileName.c_str(), "rb");
    if (!infp) {
        log_e("GZipFile error: could not open input file %s", inputFileName.c_str());
        return false;
    }

    return compressFile(infp);
}

auto GZipFile::compressFile(SFile &f) -> bool { return compressFile(f.getFilePtr()); }

auto GZipFile::compressFile(FILE *fp) -> bool {
    // read chunks and write to compressed file
    uint8_t buf[GZIP_CHUNK_SIZE];
    bool done = false;
    while (!done) {
        int bytesRead = fread(buf, 1, GZIP_CHUNK_SIZE, fp);
        if (bytesRead > 0) {
            if (!writeMemory(buf, bytesRead)) {
                log_e("GZipFile error: failed to write %d bytes", bytesRead);
                return false;
            }
        }

        if (bytesRead != GZIP_CHUNK_SIZE) {
            // end of file
            done = true;
        }
    }

    gzclose(gf_);
    gf_ = nullptr;
    return true;
}