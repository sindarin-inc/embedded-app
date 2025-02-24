#if CONFIG_FONT_IBMF

#include "IBMFFontData.hpp"

auto FontData::load(MemoryPtr fontData, uint32_t length) -> bool {

    if (currentFontData_ == fontData) {
        return true;
    }

    initialized_ = false;
    preamble_ = reinterpret_cast<PreamblePtr>(fontData);

    if constexpr (IBMF_TRACING) {
        LOGD("Loading font at location 0x%p of length %u", fontData,
             static_cast<unsigned int>(length));
    }

    if (strncmp("IBMF", preamble_->marker, 4) != 0) {
        LOGE("Preamble in error: IBMF marker absent!!");
        return false;
    }
    if (preamble_->bits.version != IBMF_VERSION) {
        LOGE("Font is of a wrong version. Expected V4, got V%d", preamble_->bits.version);
        return false;
    }

    if (preamble_->bits.fontFormat > 1) {
        LOGE("Unknown font format. Expected 0 (LATIN) or 1 (UTF32), got %d",
             preamble_->bits.fontFormat);
        return false;
    }
    if (preamble_->faceCount >= 10) {
        LOGE("Too many faces in the font. Limit is set to %d in ibmf_defs.h", MAX_FACE_COUNT);
        return false;
    }

    // Bypass the header and the faces point size list padded to 32 bits

    MemoryPtr data = fontData + ((sizeof(Preamble) + preamble_->faceCount + 3) & 0xFFFFFFFC);

    // retrieve offsets from the beginning of the file to the face starts

    auto binFaceOffsets = reinterpret_cast<uint32_t(*)[]>(data);
    data += (sizeof(uint32_t) * preamble_->faceCount);

    // If fontFormat is UTF32, retrieve the CodePoint directory

    if (preamble_->bits.fontFormat == FontFormat::UTF32) {
        planes_ = reinterpret_cast<PlanesPtr>(data);
        data += sizeof(Planes);
        codePointBundles_ = reinterpret_cast<CodePointBundlesPtr>(data);
        data += (((*planes_)[3].codePointBundlesIdx + (*planes_)[3].entriesCount) *
                 sizeof(CodePointBundle));
    } else {
        LOGE("The font format is not UTF32");
        return false; // We only support FontFormat::UTF32
    }

    // Check for discrepancies
    if ((*binFaceOffsets)[0] != static_cast<uint32_t>(data - fontData)) {
        LOGE("Wrong faces offset in font file, got %08x, expected %08x.",
             static_cast<unsigned int>((*binFaceOffsets)[0]),
             static_cast<unsigned int>(data - fontData));
        return false;
    }

    uint32_t binFaceLengths[MAX_FACE_COUNT];

    for (uint8_t i = 0; i < preamble_->faceCount; i++) {
        binFaceLengths[i] = ((i < (preamble_->faceCount - 1)) ? (*binFaceOffsets)[i + 1] : length) -
                            (*binFaceOffsets)[i];

        if (!faces_[i].load(fontData + (*binFaceOffsets)[i], binFaceLengths[i],
                            preamble_->bits.fontFormat)) {
            LOGE("Unable to load face %d", i);
            return false;
        }
    }

    initialized_ = true;
    currentFontData_ = fontData;
    unknownGlyphCode_ = translate(UNKNOWN_CODEPOINT);

    // showFont();
    return true;
}

/**
 * @brief Translate UTF32 codePoint to it's internal representation
 *
 * All supported character sets must use this method to get the internal
 * glyph code correponding to the CodePoint. For the LATIN font format,
 * the glyph code contains both the glyph index and accented information. For
 * UTF32 font format, it contains the index of the glyph in all faces that are
 * part of the font.
 *
 * FontFormat 0 - Latin Char Set Translation
 * -----------------------------------------
 *
 * The class allow for latin1+ CodePoints to be plotted into a bitmap. As the
 * font doesn't contains all accented glyphs, a combination of glyphs must be
 * used to draw a single bitmap. This method translate some of the supported
 * CodePoint values to that combination. The least significant byte will contains
 * the main glyph code index and the next byte will contain the accent code.
 *
 * The supported UTF16 CodePoints are the following:
 *
 *      U+0020 to U+007F
 *      U+00A1 to U+017f
 *
 *   and the following:
 *
 * |  UTF16  | Description          |
 * |:-------:|----------------------|
 * | U+02BB  | reverse apostrophe
 * | U+02BC  | apostrophe
 * | U+02C6  | circumflex
 * | U+02DA  | ring
 * | U+02DC  | tilde ~
 * | U+2013  | endash (Not available with CM Typewriter)
 * | U+2014  | emdash (Not available with CM Typewriter)
 * | U+2018  | quote left
 * | U+2019  | quote right
 * | U+201A  | comma like ,
 * | U+201C  | quoted left "
 * | U+201D  | quoted right
 * | U+2032  | minute '
 * | U+2033  | second "
 * | U+2044  | fraction /
 * | U+20AC  | euro
 *
 * Font Format 1 - UTF32 translation
 * ---------------------------------
 *
 * Using the Planes and CodePointBundles structure, retrieves the
 * glyph code corresponding to the CodePoint.
 *
 * @param codePoint The UTF32 character code
 * @return The internal representation of CodePoint
 */
[[nodiscard]] auto FontData::translate(char32_t codePoint) const -> GlyphCode {
    GlyphCode glyphCode = unknownGlyphCode_;

#if LATIN_SUPPORT
    if (preamble_->bits.fontFormat == FontFormat::LATIN) {
        if ((codePoint > 0x20) && (codePoint < 0x7F)) {
            glyphCode = codePoint; // ASCII codes No accent
        } else if ((codePoint >= 0xA1) && (codePoint <= 0x1FF)) {
            glyphCode = LATIN_TRANSLATION_SET[codePoint - 0xA1];
        } else {
            switch (codePoint) {
            case 0x2013: // endash
                glyphCode = 0x0015;
                break;
            case 0x2014: // emdash
                glyphCode = 0x0016;
                break;
            case 0x2018: // quote left
            case 0x02BB: // reverse apostrophe
                glyphCode = 0x0060;
                break;
            case 0x2019: // quote right
            case 0x02BC: // apostrophe
                glyphCode = 0x0027;
                break;
            case 0x201C: // quoted left "
                glyphCode = 0x0010;
                break;
            case 0x201D: // quoted right
                glyphCode = 0x0011;
                break;
            case 0x02C6: // circumflex
                glyphCode = 0x005E;
                break;
            case 0x02DA: // ring
                glyphCode = 0x0006;
                break;
            case 0x02DC: // tilde ~
                glyphCode = 0x007E;
                break;
            case 0x201A: // comma like ,
                glyphCode = 0x000D;
                break;
            case 0x2032: // minute '
                glyphCode = 0x0027;
                break;
            case 0x2033: // second "
                glyphCode = 0x0022;
                break;
            case 0x2044: // fraction /
                glyphCode = 0x002F;
                break;
            case 0x20AC: // euro
                glyphCode = 0x00AD;
                break;
            }
        }
    }
#endif

    if ((codePoint == ' ') || (codePoint == 0xA0) || (codePoint == 0x202F) ||
        ((codePoint >= 0x2000) && (codePoint <= 0x200F))) {
        glyphCode = SPACE_CODE;
    } else if (codePoint == ZERO_WIDTH_CODEPOINT) {
        glyphCode = ZERO_WIDTH_CODE;
    } else if ((codePoint >= 0xFFF0) && (codePoint <= 0xFFFF)) {
        glyphCode = DONT_CARE_CODE;
    } else {
        auto planeIdx = static_cast<uint16_t>(codePoint >> 16);

        if (planeIdx <= 3) {
            auto u16 = static_cast<char16_t>(codePoint);

            // NOLINTBEGIN(clang-analyzer-core.NullDereference)
            uint16_t codePointBundleIdx = (*planes_)[planeIdx].codePointBundlesIdx;
            uint16_t entriesCount = (*planes_)[planeIdx].entriesCount;
            int gCode = (*planes_)[planeIdx].firstGlyphCode;
            // NOLINTEND(clang-analyzer-core.NullDereference)
            int i = 0;
            while (i < entriesCount) {
                if (u16 <= (*codePointBundles_)[codePointBundleIdx].lastCodePoint) {
                    break;
                }
                gCode += ((*codePointBundles_)[codePointBundleIdx].lastCodePoint -
                          (*codePointBundles_)[codePointBundleIdx].firstCodePoint + 1);
                i++;
                codePointBundleIdx++;
            }
            if ((i < entriesCount) &&
                (u16 >= (*codePointBundles_)[codePointBundleIdx].firstCodePoint)) {
                glyphCode = gCode + u16 - (*codePointBundles_)[codePointBundleIdx].firstCodePoint;
            }
        }
    }

    // The following test could generates many entries in the log, depending on the quantity of
    // unknown code points received to be translated. Could be removed or disabled if required.
    if ((codePoint != UNKNOWN_CODEPOINT) && (glyphCode == unknownGlyphCode_)) {
        log_w("Unknown Code Point received: U+%05" PRIx32, uint32_t(codePoint));
    }

    return glyphCode;
}

void FontData::showCodePointBundles(int firstIdx, int count) const {
    if constexpr (IBMF_DEBUG) {
        for (int idx = firstIdx; count > 0; idx++, count--) {
            std::cout << "[" << idx << "] "
                      << "First CodePoint: " << +(*codePointBundles_)[idx].firstCodePoint
                      << ", Last CodePoint: " << +(*codePointBundles_)[idx].lastCodePoint
                      << std::endl;
        }
    }
}

void FontData::showPlanes() const {
    if constexpr (IBMF_DEBUG) {
        std::cout << "----------- Planes -----------" << std::endl;
        for (int i = 0; i < 4; i++) {
            std::cout << "[" << i
                      << "] CodePoint Bundle Index: " << (*planes_)[i].codePointBundlesIdx
                      << ", Entries Count: " << (*planes_)[i].entriesCount
                      << ", First glyph code: " << (*planes_)[i].firstGlyphCode << std::endl;
            std::cout << "    CodePoint Bundles:" << std::endl;
            showCodePointBundles((*planes_)[i].codePointBundlesIdx, (*planes_)[i].entriesCount);
        }
    }
}

void FontData::showFont() const {
    if constexpr (IBMF_DEBUG) {
        char marker[5];
        memcpy(marker, preamble_->marker, 4);
        marker[4] = 0;
        std::cout << "----------- Font Preamble -----------" << std::endl;
        std::cout << "Marker: " << marker << ", Font Version: " << preamble_->bits.version
                  << ", Font Format: " << preamble_->bits.fontFormat
                  << ", Face Count: " << preamble_->faceCount << std::endl;

        if (preamble_->bits.fontFormat == FontFormat::UTF32) {
            showPlanes();
        }
    }
    for (int i = 0; i < preamble_->faceCount; i++) {
        faces_[i].showFace();
    }
}

#endif