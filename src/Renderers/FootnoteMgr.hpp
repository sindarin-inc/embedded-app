#pragma once

#include <set>
#include <utility>

#include "EPub/EPubFile.hpp"
#include "Misc/SpiramAllocator.hpp"
#include "RendererDefs.hpp"

using namespace renderer_defs;

#define FOOTNOTE_PROFILING 1

#if FOOTNOTE_PROFILING
#define FOOTNOTE_PROFILE_START(v) PROFILE_START(v)
#define FOOTNOTE_PROFILE_END(v) PROFILE_END_LOG_MS(v)
#else
#define FOOTNOTE_PROFILE_START(v)
#define FOOTNOTE_PROFILE_END(v)
#endif

class Footnote {
public:
    StreamPlace streamPlace; // Where the note is being referenced
    SpiramString id;         // Note id
    SpiramString filename;   // Note filename
    SpiramString content;    // Note paragraph(s)
    bool contentFound;       // true if content populated

    Footnote(StreamPlace streamPlace, SpiramString id, SpiramString filename, SpiramString content,
             bool contentFound)
        : streamPlace(streamPlace), id(std::move(std::move(id))),
          filename(std::move(std::move(filename))), content(std::move(std::move(content))),
          contentFound(contentFound) {}

    void show(int i, std::ostream &outstream = std::cout) {
        SpiramString none = "<none>";
        outstream << i << ": Id:" << id << " filename:" << (filename.empty() ? none : filename)
                  << " chapter place:" << streamPlace.str() << std::endl
                  << std::flush;
        if (contentFound) {
            outstream << "   Content:" << content << std::endl;
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
     * @param filename The filename to filter
     * @param currentFolder The folder where the chapter file is located, from the manifest
     * @return SpiramString The filtered filename
     */
    [[nodiscard]] auto cleanupPath(const char *filename, const char *currentFolder)
        -> std::shared_ptr<SpiramString>;

public:
    /**
     * @brief Parse chapter for footnote tags
     *
     * Parse a complete chapter to retrieve footnote tags information. The notes content is *not*
     * retrieved. The processing is non-recursive.
     *
     * In a first pass, the addEntry() or parseChapter() is call to generate Footnote entries
     * without their content.
     *
     * @param stream The EPub file to retrieve HTML files
     * @param spineIdx The chapter spine index to parse. If it's not the first index in the chapter,
     * the first chapter index will be retrieved.
     * @return true The chapter was properly parse
     * @return false There was problems parsing the chapter
     */
    [[nodiscard]] auto parseChapter(const std::shared_ptr<EPubFile> &stream, SpineIdx spineIdx)
        -> bool;

    /**
     * @brief Generates Footnote entries
     *
     * In a first pass, the addEntry() or parseChapter() is call to generate Footnote entries
     * without their content.
     *
     * @param href Anchor's href that point at the note location.
     * @param streamPlace Tag location in the chapter.
     * @return true The footnote has been added successfully.
     */
    [[nodiscard]] auto addEntry(const char *href, StreamPlace streamPlace) -> bool;

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

    [[nodiscard]] inline auto isEmpty() -> bool const { return footnotes_.empty(); }

    inline void clear() { footnotes_.clear(); }

    [[nodiscard]] auto getFootnoteHTML(int index) -> std::shared_ptr<SpiramString> const;
    [[nodiscard]] auto getAllFootnotesHTML() -> std::shared_ptr<SpiramString> const;

    // Debugging code

    void show(std::ostream &outstream = std::cout) {
        if (footnotes_.empty()) {
            outstream << "No footnote" << std::endl;
        } else {
            outstream << "FootNote Entries:" << std::endl << std::flush;

            int i = 1;
            for (const std::shared_ptr<Footnote> &entry : footnotes_) {
                entry->show(i++, outstream);
            }
        }
    }

#if SIMULATOR
    void testEPubFiles();
#endif
};