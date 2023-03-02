#pragma once

#include <cstring>
#include <fstream>
#include <iostream>

#include "IBMFDefs.hpp"
#include "RLEExtractor.hpp"

using namespace IBMFDefs;

/**
 * @brief Access to a IBMF face.
 *
 * This is a low-level class to allow acces to a IBMF faces generated through METAFONT
 *
 */
class IBMFFaceLow {

private:
    static constexpr char const *TAG = "IBMFFaceLow";

    static constexpr uint8_t MAX_GLYPH_COUNT = 254; // Index Value 0xFE and 0xFF are reserved

    bool initialized;

    FaceHeaderPtr faceHeader;
    RLEBitmap bitmaps[MAX_GLYPH_COUNT];
    GlyphInfoPtr glyphs[MAX_GLYPH_COUNT];
    LigKernStep *lig_kern_steps;
    FIX16 *kerns;

public:
    IBMFFaceLow() : initialized(false) {}

    ~IBMFFaceLow() {}

    bool load(MemoryPtr data_start, MemoryPtr data_end) {
        memset(glyphs, 0, sizeof(glyphs));

        MemoryPtr memory_ptr = data_start;
        MemoryPtr memory_end = data_end;

        faceHeader = (FaceHeaderPtr)memory_ptr;

        memory_ptr += sizeof(FaceHeader);
        for (int i = 0; i < faceHeader->glyph_count; i++) {
            glyphs[i] = (GlyphInfoPtr)memory_ptr;
            memory_ptr += sizeof(GlyphInfo);

            bitmaps[i].pixels = memory_ptr;
            memory_ptr += glyphs[i]->packet_length;

            bitmaps[i].dim = Dim(glyphs[i]->bitmap_width, glyphs[i]->bitmap_height);
            bitmaps[i].length = glyphs[i]->packet_length;

            if (memory_ptr > memory_end) return false;
        }

        lig_kern_steps = (LigKernStep *)memory_ptr;
        memory_ptr += sizeof(LigKernStep) * faceHeader->lig_kern_step_count;
        if (memory_ptr > memory_end) return false;

        kerns = (FIX16 *)memory_ptr;

        memory_ptr += sizeof(FIX16) * faceHeader->kern_count;
        if (memory_ptr > memory_end) return false;

        return true;
    }

    inline bool is_initialized() { return initialized; }
    inline uint8_t get_face_pt_size() { return faceHeader->point_size; }
    inline uint16_t get_line_height() { return faceHeader->line_height; }
    inline uint16_t get_em_height() { return faceHeader->em_size >> 6; }
    inline uint8_t get_max_hight() { return faceHeader->max_hight; }
    inline int16_t get_descender_height() { return -(int16_t)faceHeader->descender_height; }
    inline LigKernStep *get_lig_kern(uint8_t idx) { return &lig_kern_steps[idx]; }
    inline FIX16 get_kern(uint8_t i) { return kerns[i]; }
    inline GlyphInfo *get_glyph_info(uint8_t glyph_code) { return glyphs[glyph_code]; }

    /**
     * @brief Translate unicode in an internal char code
     *
     * The class allow for latin1+ characters to be plotted into a bitmap. As the
     * font doesn't contains all accented glyphs, a combination of glyphs must be
     * used to draw a single bitmap. This method translate some of the supported
     * unicode values to that combination. The least significant byte will contains
     * the main glyph code and the next byte will contain the accent code.
     *
     * @param charcode The character code in unicode
     * @return The internal representation of a character
     */
    uint32_t translate(uint32_t charcode) {
        uint32_t glyph_code;

        if ((charcode > 0x20) && (charcode < 0x7F)) {
            glyph_code = 0xFF00 | charcode;
        } else if ((charcode >= 0xA1) && (charcode <= 0xFF)) {
            glyph_code = set2_translation_latin_1[charcode - 0xA1];
        } else if ((charcode >= 0x100) && (charcode <= 0x1FF)) {
            glyph_code = set2_translation_latin_A[charcode - 0x100];
        } else {
            switch (charcode) {
            case 0x2013:
                glyph_code = 0xFF15;
                break; // endash
            case 0x2014:
                glyph_code = 0xFF16;
                break;   // emdash
            case 0x2018: // quote left
            case 0x02BB: // reverse apostrophe
                glyph_code = 0xFF60;
                break;
            case 0x2019: // quote right
            case 0x02BC: // apostrophe
                glyph_code = 0xFF27;
                break;
            case 0x201C:
                glyph_code = 0xFF10;
                break; // quoted left "
            case 0x201D:
                glyph_code = 0xFF11;
                break; // quoted right
            case 0x02C6:
                glyph_code = 0xFF5E;
                break; // circumflex
            case 0x02DA:
                glyph_code = 0xFF06;
                break; // ring
            case 0x02DC:
                glyph_code = 0xFF7E;
                break; // tilde ~
            case 0x201A:
                glyph_code = 0xFF0D;
                break; // comma like ,
            case 0x2032:
                glyph_code = 0xFF27;
                break; // minute '
            case 0x2033:
                glyph_code = 0xFF22;
                break; // second "
            case 0x2044:
                glyph_code = 0xFF2F;
                break; // fraction /
            case 0x20AC:
                glyph_code = 0xFFAD;
                break; // euro
            default:
                glyph_code = 0xFFFE;
            }
        }

        return glyph_code;
    }

    bool put_glyph(uint32_t char_code, GlyphMetrics &glyph_metrics, Bitmap &canvas, Pos atPos) {
        Glyph glyph;
        glyph.bitmap = canvas;
        if (get_glyph(char_code, glyph, true, false, atPos)) {
            glyph_metrics = glyph.metrics;
            return true;
        }
        return false;
    }

    bool get_glyph(uint32_t charCode, Glyph &appGlyph, bool loadBitmap, bool bitmapInit = true,
                   Pos atPos = Pos(0, 0)) {

        uint32_t glyph_code = translate(charCode);

        if (bitmapInit) appGlyph.clear();
        appGlyph.metrics.line_height = faceHeader->line_height;

        uint8_t accent_code = (glyph_code & 0x0000FF00) >> 8;
        uint8_t accent_idx = accent_code - faceHeader->first_code;
        GlyphInfoPtr accent_info = (accent_code != 0xFF) ? glyphs[accent_idx] : nullptr;
        RLEBitmapPtr accent_bitmap = (accent_code != 0xFF) ? &bitmaps[accent_idx] : nullptr;

        uint8_t glyph_idx = (glyph_code & 0xFF) - faceHeader->first_code;
        if (((glyph_code & 0xFF) == 0xFF) ||
            ((glyph_idx < faceHeader->glyph_count) && (glyphs[glyph_idx] == nullptr))) {
            std::cerr << "No entry for char code 0x" << std::hex << glyph_code << std::endl;
            glyph_code = 0xFFFE;
        }

        if (glyph_code == 0xFFFE) {
            appGlyph.metrics.advance = faceHeader->space_size;
            appGlyph.point_size = faceHeader->point_size;
            return true;
        }

        if (glyph_idx >= faceHeader->glyph_count) return false;

        GlyphInfoPtr glyph_info = glyphs[glyph_idx];
        RLEBitmapPtr glyph_bitmap = &bitmaps[glyph_idx];

        if (glyph_info == nullptr) return false;

        Dim dim = Dim(glyph_info->bitmap_width, glyph_info->bitmap_height);
        Pos offsets = Pos(0, 0);

        uint8_t added_left = 0;

        if (accent_info != nullptr) {

            if (glyph_code == 0x276E) { // Apostrophe n
                // offsets.x = 0; // already set
                added_left = accent_info->bitmap_width + 1 -
                             (((faceHeader->x_height >> 6) * faceHeader->slant_correction) >> 6);
                dim.width = added_left + glyph_info->bitmap_width;
            } else {
                // Horizontal adjustment
                if (glyph_code == 0x0C41) { // Ą
                    offsets.x = glyph_info->bitmap_width - accent_info->bitmap_width;
                } else if ((glyph_code == 0x0C61) || (glyph_code == 0x0C45)) { // ą or Ę
                    offsets.x =
                        glyph_info->bitmap_width - accent_info->bitmap_width -
                        ((((int32_t)glyph_info->bitmap_height) * faceHeader->slant_correction) >>
                         6);
                } else {
                    offsets.x =
                        ((glyph_info->bitmap_width > accent_info->bitmap_width)
                             ? ((glyph_info->bitmap_width - accent_info->bitmap_width) >> 1)
                             : 0) +
                        ((accent_info->vertical_offset < 5)
                             ? -(((faceHeader->x_height >> 6) * faceHeader->slant_correction) >> 6)
                             : ((((int32_t)glyph_info->bitmap_height) *
                                 faceHeader->slant_correction) >>
                                6))
                        /*- (accent_info->horizontal_offset - glyph_info->horizontal_offset)*/;
                }
                if ((offsets.x == 0) && (glyph_info->bitmap_width < accent_info->bitmap_width)) {
                    added_left = (accent_info->bitmap_width - glyph_info->bitmap_width) >> 1;
                    dim.width = accent_info->bitmap_width;
                }
                if (dim.width < (offsets.x + accent_info->bitmap_width)) {
                    dim.width = offsets.x + accent_info->bitmap_width;
                }
            }

            // Vertical adjustment
            if (accent_info->vertical_offset >= (faceHeader->x_height >> 6)) {
                // Accents that are on top of a main glyph
                dim.height += (accent_info->vertical_offset - (faceHeader->x_height >> 6));
            } else if (accent_info->vertical_offset < 5) {
                // Accents below the main glyph (cedilla)
                int16_t added_height =
                    (glyph_info->bitmap_height - glyph_info->vertical_offset) -
                    ((-accent_info->vertical_offset) + accent_info->bitmap_height);
                if (added_height < 0) dim.height += -added_height;
                offsets.y = glyph_info->vertical_offset - accent_info->vertical_offset;
            }
            // else if ((glyph_code == 0x1648) || (glyph_code == 0x1568)) { // Ħ or ħ
            //   offsets.y = (glyph_info->bitmap_height * 1) >> 2;
            //   offsets.x = ((glyph_info->bitmap_height - offsets.y) *
            //   faceHeader->slant_correction) >> 6;
            // }
            // else if ((glyph_code == 0x1554) || (glyph_code == 0x1574)) { // Ŧ or ŧ
            //   offsets.y = (glyph_info->bitmap_height * 1) >> 1;
            //   offsets.x = (offsets.y * faceHeader->slant_correction) >> 6;
            // }
        }

        if (bitmapInit) {
            uint16_t size = (resolution == PixelResolution::ONE_BIT)
                                ? dim.height * ((dim.width + 7) >> 3)
                                : dim.height * dim.width;

            appGlyph.bitmap.pixels = new uint8_t[size];
            memset(appGlyph.bitmap.pixels, 0, size);
            appGlyph.bitmap.dim = dim;
        }

        RLEExtractor rle;

        if (loadBitmap && (glyph_bitmap != nullptr)) {
            Pos outPos = Pos(atPos.x + offsets.x, atPos.y + offsets.y);
            if (accent_bitmap != nullptr) {
                rle.retrieve_bitmap(*accent_bitmap, appGlyph.bitmap, outPos,
                                    accent_info->rle_metrics);

                offsets.y = (accent_info->vertical_offset >= (faceHeader->x_height >> 6))
                                ? (accent_info->vertical_offset - (faceHeader->x_height >> 6))
                                : 0;
                offsets.x = added_left;
            }

            outPos = Pos(atPos.x + offsets.x, atPos.y + offsets.y);
            rle.retrieve_bitmap(*glyph_bitmap, appGlyph.bitmap, offsets, glyph_info->rle_metrics);

            appGlyph.metrics.xoff = -(glyph_info->horizontal_offset + offsets.x);
            appGlyph.metrics.yoff = -(glyph_info->vertical_offset + offsets.y);
            appGlyph.metrics.advance = glyph_info->advance >> 6;
            appGlyph.metrics.ligature_and_kern_pgm_index = glyph_info->lig_kern_pgm_index;
            appGlyph.metrics.line_height = faceHeader->line_height;
        }
        show_glyph(appGlyph, glyph_code & 0xFF);
        return true;
    }

    void show_glyph(const Glyph &glyph, uint32_t char_code) {
        if constexpr (DEBUG) {
            std::cout << "Glyph Char Code: " << std::hex << char_code << std::dec << std::endl
                      << "  Metrics: [" << std::dec << glyph.bitmap.dim.width << ", "
                      << glyph.bitmap.dim.height << "] " << std::endl
                      << "  Position: [" << glyph.metrics.xoff << ", " << glyph.metrics.yoff << ']'
                      << std::endl
                      << "  Bitmap available: " << ((glyph.bitmap.pixels == nullptr) ? "No" : "Yes")
                      << std::endl;

            if (glyph.bitmap.pixels != nullptr) {

                uint32_t row, col;
                MemoryPtr row_ptr;

                std::cout << '+';
                for (col = 0; col < glyph.bitmap.dim.width; col++)
                    std::cout << '-';
                std::cout << '+' << std::endl;

                if (resolution == PixelResolution::ONE_BIT) {
                    uint32_t row_size = (glyph.bitmap.dim.width + 7) >> 3;
                    for (row = 0, row_ptr = glyph.bitmap.pixels; row < glyph.bitmap.dim.height;
                         row++, row_ptr += row_size) {
                        std::cout << '|';
                        for (col = 0; col < glyph.bitmap.dim.width; col++) {
                            std::cout << ((row_ptr[col >> 3] & (0x80 >> (col & 7))) ? 'X' : ' ');
                        }
                        std::cout << '|';
                        std::cout << std::endl;
                    }
                } else {
                    uint32_t row_size = glyph.bitmap.dim.width;
                    for (row = 0, row_ptr = glyph.bitmap.pixels; row < glyph.bitmap.dim.height;
                         row++, row_ptr += row_size) {
                        std::cout << '|';
                        for (col = 0; col < glyph.bitmap.dim.width; col++) {
                            std::cout << ((row_ptr[col] == 0) ? ' ' : 'X');
                        }
                        std::cout << '|';
                        std::cout << std::endl;
                    }
                }

                std::cout << '+';
                for (col = 0; col < glyph.bitmap.dim.width; col++) {
                    std::cout << '-';
                }
                std::cout << '+' << std::endl << std::endl;
            }
        }
    }

    void show_glyph_info(uint16_t i, GlyphInfo &g) {
        if constexpr (DEBUG) {
            std::cout << "  [" << i << "]: "
                      << "char_code : " << +g.char_code << ", width: " << +g.bitmap_width
                      << ", height: " << +g.bitmap_height << ", hoffset: " << +g.horizontal_offset
                      << ", voffset: " << +g.vertical_offset << ", pkt_len: " << +g.packet_length
                      << ", advance: " << +((float)g.advance / 64.0)
                      << ", dyn_f: " << +g.rle_metrics.dyn_f
                      << ", first_black: " << +g.rle_metrics.first_is_black
                      << ", Lig_kern_pgm_idx: " << +g.lig_kern_pgm_index << std::endl;
        }
    }

    void show_lig_kerns() {
        if constexpr (DEBUG) {
            std::cout << std::endl
                      << "----------- Ligature / Kern programs: ----------" << std::endl;
            LigKernStepPtr entry;
            uint16_t i;
            for (entry = lig_kern_steps, i = 0; i < faceHeader->lig_kern_step_count; entry++, i++) {
                if (entry->skip.whole > 128) {
                    std::cout << "  [" << i << "]:  "
                              << "Whole skip: " << +entry->skip.whole << ", "
                              << "Goto: "
                              << +((entry->op_code.d.displ_high << 8) + entry->remainder.displ_low);
                } else {
                    std::cout << "  [" << i << "]:  "
                              << "Whole skip: " << +entry->skip.whole << ", "
                              << "Stop: " << (entry->skip.s.stop ? "Yes" : "No") << ", "
                              << "NxtChar: " << +entry->next_char << ", "
                              << "IsKern: " << (entry->op_code.d.is_a_kern ? "Yes" : "No") << ", "
                              << (entry->op_code.d.is_a_kern ? "Kern Idx: " : "Lig char: ")
                              << (entry->op_code.d.is_a_kern
                                      ? +((entry->op_code.d.displ_high << 8) +
                                          entry->remainder.displ_low)
                                      : +entry->remainder.replacement_char)
                              << std::dec;
                    if (!entry->op_code.d.is_a_kern) {
                        std::cout << ", OpCodes: a:" << +entry->op_code.op.a_op
                                  << ", b:" << +entry->op_code.op.b_op
                                  << ", c:" << +entry->op_code.op.c_op;
                    }
                }

                std::cout << std::endl;
            }
        }
    }

    void show_kerns() {
        if constexpr (DEBUG) {
            std::cout << std::endl << "----------- Kerns: ----------" << std::endl;

            for (int i = 0; i < faceHeader->kern_count; i++) {
                std::cout << "  [" << i << "]:  " << (kerns[i] / 64.0) << std::endl;
            }
        }
    }

    void show_face() {
        if constexpr (DEBUG) {

            std::cout << std::endl << "----------- Header: ----------" << std::endl;

            std::cout << "DPI: " << faceHeader->dpi << ", point size: " << +faceHeader->point_size
                      << ", first char code: " << +faceHeader->first_code
                      << ", last char code: " << +faceHeader->last_code
                      << ", line height: " << +faceHeader->line_height
                      << ", max height: " << +faceHeader->max_hight
                      << ", x height: " << +((float)faceHeader->x_height / 64.0)
                      << ", em size: " << +((float)faceHeader->em_size / 64.0)
                      << ", space size: " << +((float)faceHeader->space_size / 64.0)
                      << ", glyph count: " << +faceHeader->glyph_count
                      << ", lig kern count: " << +faceHeader->lig_kern_step_count
                      << ", kern_count: " << +faceHeader->kern_count
                      << ", slant corr: " << +faceHeader->slant_correction
                      << ", descender height: " << +faceHeader->descender_height << std::endl;

            std::cout << std::endl << "----------- Glyphs: ----------" << std::endl;

            for (int i = 0; i < faceHeader->glyph_count; i++) {
                show_glyph_info(i, *glyphs[i]);
            }

            show_lig_kerns();

            show_kerns();
        }
    }
};
