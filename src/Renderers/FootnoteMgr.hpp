#pragma once

#include <set>
#include <utility>

#include "EPub/EPubFile.hpp"
#include "Misc/SpiramAllocator.hpp"
#include "RendererDefs.hpp"

using namespace renderer_defs;

#define FOOTNOTE_PROFILING 1
#define FOOTNOTE_DEBUGGING 0

#if SIMULATOR
// If 0, will process spine entries one by one instead of whole chapters.
// This is to sumplify the debugging process through the simulator capability.
#define FOOTNOTE_CHAPTER_BASED 0
#else
#define FOOTNOTE_CHAPTER_BASED 1
#endif

#if FOOTNOTE_PROFILING
#define FOOTNOTE_PROFILE_START(v) PROFILE_START(v)
#define FOOTNOTE_PROFILE_END(v) PROFILE_END_LOG_MS(v)
#else
#define FOOTNOTE_PROFILE_START(v)
#define FOOTNOTE_PROFILE_END(v)
#endif

const constexpr bool FootnoteDebugging =
#if FOOTNOTE_DEBUGGING
    true;
#else
    false;
#endif

class Footnote {
public:
    StreamPlace anchorStreamPlace; // Where the note is being referenced
    SpiramString id;               // Note id
    SpiramString filename;         // Note filename
    SpiramString content;          // Note paragraph(s)
    bool contentFound;             // true if content populated
#if SIMULATOR
    SpiramString anchorSpineFilename; // Easier for debuging

    Footnote(StreamPlace anchorStreamPlace, SpiramString anchorSpineFilename, SpiramString id,
             SpiramString filename, SpiramString content, bool contentFound)
        : anchorStreamPlace(anchorStreamPlace), id(std::move(std::move(id))),
          filename(std::move(std::move(filename))), content(std::move(std::move(content))),
          contentFound(contentFound), anchorSpineFilename(anchorSpineFilename) {}
#else
    Footnote(StreamPlace anchorStreamPlace, SpiramString id, SpiramString filename,
             SpiramString content, bool contentFound)
        : anchorStreamPlace(anchorStreamPlace), id(std::move(std::move(id))),
          filename(std::move(std::move(filename))), content(std::move(std::move(content))),
          contentFound(contentFound) {}
#endif

    void show(int i, std::ostream &outstream = std::cout, bool html = false) {
        SpiramString none = "[none]";
        if (html) {
            outstream << "<small><i>" << i << ": <b>Id</b>: " << id
                      << " <b>Filename</b>: " << (filename.empty() ? none : filename)
                      << " <b>Location</b>: " << anchorStreamPlace.str();
#if SIMULATOR
            outstream << " <b>in file</b>: " << anchorSpineFilename;
#endif
            if (contentFound) {
                outstream << " <b>Content</b>:"
                          << "</i></small><br/>" << content;
            } else {
                outstream
                    << " <b>Content: <span class=\"error\">Empty</span></b></i></small><br/><br/>";
            }
        } else {
            outstream << i << ": Id: " << id
                      << " Filename: " << (filename.empty() ? none : filename)
                      << " Location: " << anchorStreamPlace.str();
#if SIMULATOR
            outstream << " in file: " << anchorSpineFilename;
#endif
            if (contentFound) {
                outstream << "   Content:" << content << std::endl;
            } else {
                outstream << " Content: Empty" << std::endl;
            }
        }
    }
};

class FootnoteMgr {

private:
    SpiramVector<std::shared_ptr<Footnote>> footnotes_;

    SpineIdx currentSpineIdx_{};

    /**
     * @brief Filter relative filename
     *
     * Removes relative filename's constructs from the filename. Sometimes, in anchor's href
     * attribute, the filename contains relative constructs ("../xxxx" or "xxxx/../xxx"). Can also
     * contain characters in a hexadecimal format (like %xx).
     *
     * @param filename [in/out] The filename to filter
     * @param currentFolder The folder where the spine file is located, from the manifest
     */
    void cleanupPath(SpiramString &filename, const char *currentFolder) const;

public:
    /**
     * @brief Parse Spine/Chapter for footnote tags
     *
     * Parse a Spine or complete chapter to retrieve footnote tags information. The notes content is
     * *not* retrieved. The processing is non-recursive.
     *
     * In a first pass, the addEntry() or parseChapter() is call to generate Footnote entries
     * without their content.
     *
     * @param stream The EPub file to retrieve HTML files
     * @param spineIdx The chapter spine index or Spine to parse. When FOOTNOTE_CHAPTER_BASED=1
     * parsing is done Chapter base, else Spine based. If it's not the first index in the chapter,
     * the first chapter index will be retrieved.
     * @return true The chapter was properly parse
     * @return false There was problems parsing the chapter
     */
    [[nodiscard]] auto parse(const std::shared_ptr<EPubFile> &stream, SpineIdx spineIdx) -> bool;

    /**
     * @brief Generates Footnote entries
     *
     * In a first pass, the addEntry() or parse() is call to generate Footnote entries
     * without their content.
     *
     * @param href Anchor's href that point at the note location.
     * @param stream The EPub file to validate heuristics on anchor validity.
     * @param anchorStreamPlace Tag location in the spine.
     * @param idsInSpine Map of ids present in current spine before current location.
     * @return true The footnote has been added successfully.
     */
    [[nodiscard]] auto addEntry(const char *href, const std::shared_ptr<EPubFile> &stream,
                                StreamPlace anchorStreamPlace,
                                const SpiramMap<SpiramString, uint32_t> &idsInSpine) -> bool;

    /**
     * @brief Retrieve notes content
     *
     * In a second pass, the retrieveNotes() method will find and retrieve each note, putting them
     * in the Footnote entry.
     *
     * @param stream The EPub file.
     *
     * @return true All note content have been retrieved successfully.
     */
    [[nodiscard]] auto retrieveNotes(const std::shared_ptr<EPubFile> &stream) -> bool;

    [[nodiscard]] inline auto isEmpty() const -> bool { return footnotes_.empty(); }

    inline void clear() { footnotes_.clear(); }

    [[nodiscard]] auto getFootnoteHTML(int index) const -> std::shared_ptr<SpiramString>;
    [[nodiscard]] auto getAllFootnotesHTML() const -> std::shared_ptr<SpiramString>;

    // Debugging code

    void show(std::ostream &outstream = std::cout, bool html = false) {
        if (!footnotes_.empty()) {
            int i = 1;
            for (const std::shared_ptr<Footnote> &entry : footnotes_) {
                entry->show(i++, outstream, html);
            }
        }
    }

#if SIMULATOR
    void testEPubFiles();
    void testEPubFilesHTML();
#endif
};