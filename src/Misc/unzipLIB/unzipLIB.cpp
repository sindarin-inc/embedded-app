//
// unzipLIB - Arduino unzip library
// based on zlib and contrib/minizip/unzip
// written by Larry Bank
// bitbank@pobox.com
//
// Copyright 2021 BitBank Software, Inc. All Rights Reserved.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//===========================================================================
#include "unzipLIB.h"

#include <memory>

auto UNZIP::openZIP(uint8_t *pData, int iDataSize) -> int {
    // mhill added allocation for buffer
    auto buffer = std::make_unique<uint8_t[]>(32768 + 7168);
    zip_.ucFlate = buffer.get();

    zip_.zHandle = UnzOpen(nullptr, pData, iDataSize, &zip_, nullptr, nullptr, nullptr, nullptr);
    if (zip_.zHandle == nullptr) {
        //       printf("Error opening file: %s\n", argv[1]);
        return -1;
    }
    return 0;
} /* open() */

auto UNZIP::openZIP(const char *szFilename, ZIP_OPEN_CALLBACK *pfnOpen,
                    ZIP_CLOSE_CALLBACK *pfnClose, ZIP_READ_CALLBACK *pfnRead,
                    ZIP_SEEK_CALLBACK *pfnSeek) -> int {
    zip_.zHandle = UnzOpen(szFilename, nullptr, 0, &zip_, pfnOpen, pfnRead, pfnSeek, pfnClose);
    if (zip_.zHandle == nullptr) {
        //       printf("Error opening file: %s\n", argv[1]);
        return -1;
    }
    return 0;
} /* open() */

auto UNZIP::closeZIP() -> int {
    zip_.iLastError = UnzClose((unzFile)zip_.zHandle);
    return zip_.iLastError;
} /* closeZIP() */

auto UNZIP::openCurrentFile() -> int {
    zip_.iLastError = UnzOpenCurrentFile((unzFile)zip_.zHandle);
    return zip_.iLastError;
} /* openCurrentFile() */

auto UNZIP::closeCurrentFile() -> int {
    zip_.iLastError = UnzCloseCurrentFile((unzFile)zip_.zHandle);
    return zip_.iLastError;
} /* closeCurrentFile() */

auto UNZIP::readCurrentFile(uint8_t *buffer, int iLength) -> int {
    return UnzReadCurrentFile((unzFile)zip_.zHandle, buffer, iLength);
} /* readCurrentFile() */
auto UNZIP::getCurrentFilePos() -> int {
    return (int)Unztell((unzFile)zip_.zHandle);
} /* getCurrentFilePos() */

auto UNZIP::gotoFirstFile() -> int {
    return UnzGoToFirstFile((unzFile)zip_.zHandle);
} /* gotoFirstFile() */
auto UNZIP::gotoNextFile() -> int {
    zip_.iLastError = UnzGoToNextFile((unzFile)zip_.zHandle);
    return zip_.iLastError;
} /* gotoNextFile() */
auto UNZIP::locateFile(const char *szFilename) -> int {
    zip_.iLastError = UnzLocateFile((unzFile)zip_.zHandle, szFilename, 2);
    return zip_.iLastError;
} /* locateFile() */

auto UNZIP::getFileInfo(unz_file_info *pFileInfo, char *szFileName, int iFileNameBufferSize,
                        void *extraField, int iExtraFieldBufferSize, char *szComment,
                        int iCommentBufferSize) -> int // get info about the current file

{
    return UnzGetCurrentFileInfo((unzFile)zip_.zHandle, pFileInfo, szFileName, iFileNameBufferSize,
                                 extraField, iExtraFieldBufferSize, szComment, iCommentBufferSize);
} /* getFileInfo() */

auto UNZIP::getLastError() -> int { return zip_.iLastError; } /* getLastError() */

auto UNZIP::getGlobalComment(char *destBuffer, int iBufferSize) -> int {
    zip_.iLastError = UnzGetGlobalComment((unzFile)zip_.zHandle, destBuffer, iBufferSize);
    return zip_.iLastError;
} /* getComment() */
