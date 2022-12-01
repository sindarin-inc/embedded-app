#include "ZipFile.hpp"

// static map needed here to interface with C api
std::map<std::string, ZipFile *> ZipFile::zipRegistry;

void *ZipFile::zipOpen(const char *filename, int32_t *size) {
    // get ZipFile instance
    ZipFile *iZF = ZipFile::zipRegistry[filename];
    if (!iZF) {
        *size = 0;
        return NULL;
    }

    iZF->fp_ = iZF->fileSystem_.open(filename, FILE_READ);
    *size = iZF->fp_.size();

    return iZF;
}

void ZipFile::zipClose(void *p) {
    ZIPFILE *pzf = (ZIPFILE *)p;
    ZipFile *iZF = (ZipFile *)pzf->fHandle;
    if (!iZF) {
        return;
    }

    if (iZF && iZF->fp_) {
        iZF->fp_.close();
    }
}

int32_t ZipFile::zipRead(void *p, uint8_t *buffer, int32_t length) {
    ZIPFILE *pzf = (ZIPFILE *)p;
    ZipFile *iZF = (ZipFile *)pzf->fHandle;
    if (!iZF) {
        return -1;
    }

    File *fp = &(iZF->fp_);
    if (!fp) {
        return -1;
    }

    int r = fp->readBytes((char *)buffer, length);
    return r;
}

int32_t ZipFile::zipSeek(void *p, int32_t position, int iType) {
    ZIPFILE *pzf = (ZIPFILE *)p;
    ZipFile *iZF = (ZipFile *)pzf->fHandle;
    if (!iZF) {
        return -1;
    }
    File *fp = &(iZF->fp_);
    if (!fp) {
        return -1;
    }

    if (iType == SEEK_SET) {
        return fp->seek(position);
    } else if (iType == SEEK_END) {
        return fp->seek(position + pzf->iSize);
    } else { // SEEK_CUR
        long l = fp->position();
        return fp->seek(l + position);
    }
}

bool ZipFile::open() {
    ZipFile::zipRegistry[fileName_] = this;
    int rc = zip_.openZIP(fileName_.c_str(), zipOpen, zipClose, zipRead, zipSeek);
    if (rc == UNZ_OK) {
        log_i("unzip %s ok.", fileName_.c_str());
        return true;
    }

    log_i("failed to unzip %s", fileName_.c_str());
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
    log_i("Global comment:");
    log_i("%s", szComment);
    log_i("Files in this archive:");
    zip_.gotoFirstFile();
    rc = UNZ_OK;
    while (rc == UNZ_OK) { // Display all files contained in the archive
        rc = zip_.getFileInfo(&fi, szName, sizeof(szName), NULL, 0, szComment, sizeof(szComment));
        if (rc == UNZ_OK) {
            log_i("%s - %ld / %ld", szName, fi.compressed_size, fi.uncompressed_size);
        }
        rc = zip_.gotoNextFile();
    }
}