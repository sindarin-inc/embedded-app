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

/**
 * @brief Footnotes retrieval and formatting
 *
 * This class is responsible of retrieving footnote information that is part of the current Book.
 *
 * Usage
 * -----
 *
 * The normal usage of this class is as follow:
 *
 * 1) Call the FootnoteMgr::parse(epubfile, chapterSpineIdx) method to find the footnote tags
 *    present in the chapter spineIdx.
 *
 * 2) The FootnoteMgr::isEmpty() can be used to check if any footnote tags were found. Please look
 *    at the caveats section below for more information. Will return *false* if there is tag entries
 *    found in the current chapter.
 *
 * 3) If the user wants to look at the footnotes, and there is some footnotes found by the parse()
 *    method, you can then call the FootnoteMgr::retrieveNotes() method that will then search and
 *    retrieve the footnotes as pointed to by the current chapter.
 *
 * Two possible use:
 *
 * 4a) [To show one footnote at a time]: Call the FootnoteMgr::size() to get the number of footnotes
 *     and then call the FootnoteMgr::getFootnoteHTML(index) method in sequence to retrieve one
 *     footnote at a time in a htnml format.
 *
 * 4b) [To show all footnotes at once]: Call the getAllFootnotesHTML().
 *
 * TAGS PARSING HEURISTICS
 * -----------------------
 *
 * - An anchor (<a href="xxx"/>) is present in the text and is potentially pointing at a note.
 *
 * - The href attribute must contain a '#' followed by the note id.
 *
 * - If a URL is present in the href attribute before the '#', it must be pointing at a spine that
 *   is located *after* the current chapter's spine.
 *
 * - The URL, if present, must not be containing 'http(s)://' prefix.
 *
 * - If there is no URL in the href attribute, the note must be located in the same spine as the
 *   current one and must be located *after* the location of the anchor.
 *
 * FOOTNOTES PARSING HEURISTICS
 * ----------------------------
 *
 * Footnotes are extracted from the text if:
 *
 * - The anchor is preceeded by a <br/> tag. A <br/> tag is than expected to follow the footnote.
 *   If not, the footnote is not retrieved.
 *
 * - The anchor in inside a <p>...</p>, a <li>...</li> or a <div>...</div> tag.
 *
 * CAVEATS
 * -------
 *
 * - The two methods parse() and retrieveNotes() are seperated to take into account the time
 *   required to retrieve the footnotes themselves (could take several seconds depending on the
 *   amount of them present in the chapter). The time to parse a chapter is much shorter.
 *
 * - The class doesn't distinguish real footnotes located in the same spine as the chapter, or the
 *   notes located in another spine (often located at the end of the book).
 *
 * - Tags used to signify a footnote or a note can be using different numbering schemes, or specific
 *   letters. They may appear in various order in the resulting list of notes.
 *
 * - As the epub formatting is based on html, there is no provision in the
 *   EPUB standard to keep proper meta-data information about each piece of text: the parsing of
 *   footnotes is using some heuristics to retrieve the information. These heuristics are not able
 *   to cover all cases as some formatting could mislead the discovery of the tags or the footnotes
 *   themselves, or get some tags that are not really footnote related. As the parse() method
 *   complete its task, there is the potential that there is no footnote present even if the
 *   isEmpty() method returns false.
 *
 * - Footnote text can contain url pointing at some other locations (like the internet). Nothing is
 *   done to prevent that case.
 */

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
    [[nodiscard]] inline auto size() const -> size_t { return footnotes_.size(); }

    inline void clear() { footnotes_.clear(); }

    [[nodiscard]] auto getFootnoteHTML(int index) const -> std::shared_ptr<SpiramString>;
    [[nodiscard]] auto getAllFootnotesHTML() const -> std::shared_ptr<SpiramString>;

    // Debugging code

    void show(std::ostream &outstream = std::cout, bool html = false) {
        if (!footnotes_.empty()) {
            int i = 1;
            for (const std::shared_ptr<Footnote> &entry : footnotes_) {
                if (entry->contentFound) {
                    entry->show(i++, outstream, html);
                }
            }
        }
    }

#if SIMULATOR
    void testEPubFiles();
    void testEPubFilesHTML();
#endif
};