#pragma once

#include <set>

#include "EPub/EPubFile.hpp"
#include "Misc/SpiramAllocator.hpp"
#include "RendererDefs.hpp"

using namespace renderer_defs;

class Footnote {
public:
    StreamPlace streamPlace_; // Where the note is being referenced
    SpiramString id_;         // Note id
    SpiramString filename_;   // Note filename
    SpiramString content_;    // Note paragraph(s)
    bool contentFound_;       // true if content populated

    Footnote(StreamPlace streamPlace, SpiramString id, SpiramString filename, SpiramString content,
             bool contentFound)
        : streamPlace_(streamPlace), id_(id), filename_(filename), content_(content),
          contentFound_(contentFound) {}

    void show(int i) {
        SpiramString none = "<none>";
        std::cout << i << ": Id:" << id_ << " filename:" << (filename_.empty() ? none : filename_)
                  << " chapter place:" << streamPlace_.str() << std::endl
                  << std::flush;
        if (contentFound_) {
            std::cout << "   Content:" << content_ << std::endl;
        }
    }
};

class FootnoteMgr {

private:
    SpiramVector<std::shared_ptr<Footnote>> footnotes_;

    SpineIdx currentSpineIdx_;

    /**
     * @brief Filter relative filename
     *
     * Removes relative filename's constructs from the filename. Sometimes, in anchor's href
     * attribute, the filename contains relative constructs ("../xxxx" or "xxxx/../xxx"). Can also
     * contain characters in a hexadecimal format (like %xx).
     *
     * @param filename The filename to filter
     * @return SpiramString The filtered filename
     */
    [[nodiscard]] auto adjustFilename(const char *filename) -> SpiramString;

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
    [[nodiscard]] auto parseChapter(std::shared_ptr<EPubFile> stream, SpineIdx spineIdx) -> bool;

    /**
     * @brief Generates Footnote entries
     *
     * In a first pass, the addEntry() or parseChapter() is call to generate Footnote entries
     * without their content.
     *
     * @param href Anchor's href that point at the note location.
     * @param streamPlace Tag location in the chapter.
     */
    void addEntry(const char *href, StreamPlace streamPlace);

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
    [[nodiscard]] auto retrieveNotes(std::shared_ptr<EPubFile> stream) -> bool;

    [[nodiscard]] inline auto is_empty() -> bool { return footnotes_.empty(); }

    inline void clear() { footnotes_.clear(); }

    // Debugging code

    void show() {
        std::cout << "----- FootNote Entries -----" << std::endl << std::flush;

        int i = 1;
        for (std::shared_ptr<Footnote> entry : footnotes_) {
            entry->show(i++);
        }

        std::cout << "----- end -----" << std::endl << std::flush;
    }
};