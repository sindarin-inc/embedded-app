#include "GZip.hpp"

#include <sindarin-debug.h>

#define GZIP_CHUNK_SIZE 1024

auto GZip::initWithOutputFileName(const std::string &outputFileName) -> bool {
    // open output filename
    FILE *fp = fopen(outputFileName.c_str(), "wb");
    if (!fp) {
        log_e("GZip error: could not open %s", outputFileName.c_str());
        return false;
    }

    return initWithOutputFile(fp);
}

auto GZip::initWithOutputFile(SFile &f) -> bool { return initWithOutputFile(f.getFilePtr()); }

auto GZip::initWithOutputFile(FILE *fp) -> bool {
    int fd = fileno(fp);
    if (fd == -1) {
        log_e("GZip error: could not get file descriptor");
        return false;
    }

    gf_ = gzdopen(fd, "w");
    if (!gf_) {
        log_e("GZip error: gzdopen failed");
        return false;
    }

    return true;
}

auto GZip::writeMemory(uint8_t *mem, int size) -> bool {
    if (!gf_) {
        log_e("GZip error: no output file was opened");
        return false;
    }

    int bytesWritten = gzwrite(gf_, mem, size);
    if (bytesWritten != size) {
        log_e("GZip error: should have written %d bytes, return was %d", size, bytesWritten);
        return false;
    }

    return true;
}

auto GZip::close() -> bool {
    if (!gf_) {
        log_e("GZip error: close(), no output file was opened");
        return false;
    }

    int ret = gzclose(gf_);
    gf_ = nullptr;

    if (ret != Z_OK) {
        log_e("GZip error: close(), failed gzclose");
        return false;
    }

    return true;
}

auto GZip::compressFile(const std::string &inputFileName) -> bool {
    if (!gf_) {
        log_e("GZip error: no output file was opened");
        return false;
    }

    // open input filename
    FILE *infp = fopen(inputFileName.c_str(), "rb");
    if (!infp) {
        log_e("GZip error: could not open input file %s", inputFileName.c_str());
        return false;
    }

    return compressFile(infp);
}

auto GZip::compressFile(SFile &f) -> bool { return compressFile(f.getFilePtr()); }

auto GZip::compressFile(FILE *fp) -> bool {
    // read chunks and write to compressed file
    uint8_t buf[GZIP_CHUNK_SIZE];
    bool done = false;
    while (!done) {
        int bytesRead = fread(buf, 1, GZIP_CHUNK_SIZE, fp);
        if (bytesRead > 0) {
            if (!writeMemory(buf, bytesRead)) {
                log_e("GZip error: failed to write %d bytes", bytesRead);
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