#include "ZipFile.hpp"

#include "Misc/StringUtil.hpp"

// static map needed here to interface with C api
std::map<std::string, ZipFile *> ZipFile::zipRegistry;

auto ZipFile::zipOpen(const char *filename, int32_t *size) -> void * {
    // get ZipFile instance
    ZipFile *iZF = ZipFile::zipRegistry[filename];
    if (!iZF) {
        *size = 0;
        return nullptr;
    }

    iZF->fp_ = iZF->fileSystem_.open(filename, FILE_READ);
    *size = iZF->fp_.size();

    return iZF;
}

void ZipFile::zipClose(void *p) {
    auto *pzf = (ZIPFILE *)p;
    auto *iZF = (ZipFile *)pzf->fHandle;
    if (!iZF) {
        return;
    }

    if (iZF && iZF->fp_) {
        iZF->fp_.close();
    }
}

auto ZipFile::zipRead(void *p, uint8_t *buffer, int32_t length) -> int32_t {
    auto *pzf = (ZIPFILE *)p;
    auto *iZF = (ZipFile *)pzf->fHandle;
    if (!iZF) {
        return -1;
    }

    File *fp = &(iZF->fp_);
    if (!fp) {
        return -1;
    }

    // reading one byte at a time from the open zip file
    // somehow, reading the full length results in read() returning
    // 0 on the first read.
    // This is a workaround, would be great to fix whatever the
    // underlying issue is
    int r = 0;
    for (int j = 0; j < length; j++) {
        int rr = fp->read(&buffer[j], 1);
        if (rr < 1) {
            break;
        }

        r += rr;
    }

    return r;
}

auto ZipFile::zipSeek(void *p, int32_t position, int iType) -> int32_t {
    auto *pzf = (ZIPFILE *)p;
    auto *iZF = (ZipFile *)pzf->fHandle;
    if (!iZF) {
        return -1;
    }
    File *fp = &(iZF->fp_);
    if (!fp) {
        return -1;
    }

    int32_t dest = 0;
    if (iType == SEEK_SET) {
        dest = position;
    } else if (iType == SEEK_END) {
        dest = position + pzf->iSize;
    } else { // SEEK_CUR
        long l = fp->position();
        dest = l + position;
    }
    bool res = fp->seek(dest);
    return res;
}

auto ZipFile::open() -> bool {
    ZipFile::zipRegistry[fileName_] = this;
    int rc = zip_.openZIP(fileName_.c_str(), zipOpen, zipClose, zipRead, zipSeek);
    if (rc == UNZ_OK) {
        log_i("unzip %s ok.", fileName_.c_str());
        return true;
    }

    log_i("failed to unzip %s, err %d", fileName_.c_str(), rc);
    return false;
}

void ZipFile::close() {
    log_i("closing %s", fileName_.c_str());
    zip_.closeZIP();
    ZipFile::zipRegistry.erase(fileName_);
}

void ZipFile::listFiles() {
    char szComment[256];
    char szName[256];
    unz_file_info fi;
    int rc = zip_.getGlobalComment(szComment, sizeof(szComment));
    if (rc != UNZ_OK) {
        log_i("no zip comment");
    } else {
        log_i("Global comment:");
        log_i("%s", szComment);
    }
    log_i("Files in this archive:");
    zip_.gotoFirstFile();
    rc = UNZ_OK;
    while (rc == UNZ_OK) { // Display all files contained in the archive
        rc =
            zip_.getFileInfo(&fi, szName, sizeof(szName), nullptr, 0, szComment, sizeof(szComment));
        if (rc == UNZ_OK) {
            log_i("%s - %ld / %ld", szName, fi.compressed_size, fi.uncompressed_size);
        }
        rc = zip_.gotoNextFile();
    }
}

void ZipFile::extractCurrentFile(const char *outFname) {

    uint8_t buf[1024];
    bool done = false;

    File fp = fileSystem_.open(outFname, "w", true);
    zip_.openCurrentFile();

    while (!done) {
        int bytesRead = zip_.readCurrentFile(buf, sizeof(buf));
        fp.write(buf, bytesRead);

        done = true;
        if (bytesRead < sizeof(buf)) {
            done = true;
        }
    }

    zip_.closeCurrentFile();
    fp.close();
}

void ZipFile::extractAllToDirectory(const char *dirname) {
    // extract everything in the zip file to specified directory

    zip_.gotoFirstFile();
    int rc = UNZ_OK;
    unz_file_info fi;
    char szName[256];
    char fullDir[512];

    if (!fileSystem_.exists(dirname)) {
        fileSystem_.createDir(dirname);
    }

    while (rc == UNZ_OK) {
        rc = zip_.getFileInfo(&fi, szName, sizeof(szName), nullptr, 0, nullptr, 0);
        if (rc == UNZ_OK) {
            log_i("%s - %ld / %ld", szName, fi.compressed_size, fi.uncompressed_size);
            int nameLen = strlen(szName);
            if (nameLen > 0) {
                SafeSnprintf(fullDir, sizeof(fullDir), "%s/%s", dirname, szName);
                if (szName[nameLen - 1] == '/') {
                    if (!fileSystem_.exists(fullDir)) {
                        log_i("creating %s", fullDir);
                        fileSystem_.createDir(fullDir);
                    }
                } else {
                    // try to extract this file
                    extractCurrentFile(fullDir);
                }
            }
        }
        rc = zip_.gotoNextFile();
    }
}