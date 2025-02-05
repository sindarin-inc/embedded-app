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
     * @brief Cleans up a given file path by resolving relative path components and decoding
     * percent-encoded characters.
     *
     * This function takes a file path and a current folder path, and processes the file path to:
     * - Resolve relative path components (e.g., "." and "..").
     * - Decode percent-encoded characters (e.g., "%20" to space).
     * - Combine the current folder path with the file path if the file path is not absolute.
     *
     * @param filename The file path to be cleaned up. This is both an input and output parameter.
     * @param currentFolder The current folder path to be used if the file path is not absolute.
     */
    void cleanupPath(SpiramString &filename, const char *currentFolder) const;

public:
    /**
     * @brief Parses the EPUB file stream to extract footnotes.
     *
     * This function clears any existing footnotes and then processes the EPUB file stream
     * starting from the given spine index to extract footnotes. The function supports both
     * chapter-based and non-chapter-based parsing, depending on the FOOTNOTE_CHAPTER_BASED
     * macro.
     *
     * @param stream A shared pointer to the EPubFile stream to be parsed.
     * @param spineIdx The starting spine index for parsing.
     * @return true if parsing is successful, false otherwise.
     *
     * The function traverses the XHTML content of the EPUB file, looking for footnote
     * references within <sup> and <a> tags. It adds entries for these footnotes to the
     * FootnoteMgr instance.
     *
     * The Interpret struct is used as an XML tree walker to traverse the XHTML content and
     * extract footnote references. It handles different cases of footnote references, such as:
     * - <sup>...<a href="xxx">yyy</a>...</sup>
     * - <a href="xxx"><sup>yyy</sup></a>
     * - <a href="xxx" id="iii">yyy</a>
     *
     * If the current file is empty or the end of the current file is reached, the function
     * moves to the next file in the spine and continues parsing until the end of the book
     * or the start of a new chapter (if chapter-based parsing is enabled).
     */
    [[nodiscard]] auto parse(const std::shared_ptr<EPubFile> &stream, SpineIdx spineIdx) -> bool;

    /**
     * @brief Adds a footnote entry.
     *
     * This function processes the given href to extract the filename and id of the footnote,
     * validates the href and id, and adds the footnote to the internal list if valid.
     *
     * @param href The href string pointing to the footnote location.
     * @param stream A shared pointer to the EPubFile stream.
     * @param anchorStreamPlace The stream place of the anchor.
     * @param idsInSpine A map of ids in the spine with their corresponding offsets.
     * @return true if the footnote entry is successfully added, false otherwise.
     */
    [[nodiscard]] auto addEntry(const char *href, const std::shared_ptr<EPubFile> &stream,
                                StreamPlace anchorStreamPlace,
                                const SpiramMap<SpiramString, uint32_t> &idsInSpine) -> bool;

    /**
     * @brief Retrieves footnotes from the provided EPub file stream.
     *
     * This function processes the footnotes stored in the `footnotes_` member variable,
     * extracts their filenames and IDs, and then parses the corresponding XML files
     * to retrieve the footnote content. The content is extracted and stored in the
     * `footnotes_` vector.
     *
     * @param stream A shared pointer to the EPubFile stream from which to retrieve the footnotes.
     * @return true if found footnotes were successfully retrieved and parsed, false otherwise.
     *
     * The function performs the following steps:
     * 1. Extracts filenames and IDs from the `footnotes_` vector.
     * 2. Defines a custom XML tree walker to traverse the XML nodes and extract footnote content.
     * 3. Iterates over the extracted filenames, loads the corresponding XML files, and uses the
     *    custom XML tree walker to find and extract the footnote content.
     * 4. Handles different XML structures to ensure the footnote content is correctly extracted.
     * 5. Logs errors and returns false if any issues are encountered during the process.
     */
    [[nodiscard]] auto retrieveNotes(const std::shared_ptr<EPubFile> &stream) -> bool;

    [[nodiscard]] inline auto isEmpty() const -> bool { return footnotes_.empty(); }
    [[nodiscard]] inline auto size() const -> size_t { return footnotes_.size(); }

    inline void clear() { footnotes_.clear(); }

    /**
     * @brief Generates the HTML representation of a footnote.
     *
     * This function creates an HTML document for the footnote at the specified index.
     * If the footnote cannot be found, an empty string is returned.
     *
     * @param index The index of the footnote to retrieve.
     *
     * @return A shared pointer to a SpiramString containing the HTML representation of the
     * footnote. Will be an empty sring if the footnote content is not found.
     */
    [[nodiscard]] auto getFootnoteHTML(int index) const -> std::shared_ptr<SpiramString>;

    /**
     * @brief Generates an HTML representation of all footnotes.
     *
     * This function creates an HTML document containing all the footnotes.
     * If there are no footnotes, it will include a message indicating that
     * no footnotes are available. Each footnote that has content found will
     * be appended to the HTML body.
     *
     * @return A shared pointer to a SpiramString containing the HTML document.
     */
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
    /**
     * This method is no longer being used.
     *
     * @brief Tests EPub files for footnotes.
     *
     * This function tests all EPub files located in a folder specified by the
     * environment variable FOOTNOTE_TEST_FOLDER. It logs the results to a file
     * specified by the environment variable FOOTNOTE_TEST_LOGFILE.
     *
     * The function performs the following steps:
     * 1. Retrieves the folder path and log file path from environment variables.
     * 2. Checks if the environment variables are defined. If not, it logs an error
     *    message and returns.
     * 3. Opens the log file for writing.
     * 4. Iterates over each file in the specified folder.
     * 5. For each file, it creates an EPubFile stream and retrieves the spine count.
     * 6. Iterates over each spine index and parses the content.
     * 7. Logs any errors encountered during parsing or note retrieval.
     * 8. Closes the EPubFile stream and the log file.
     *
     * @note The function requires the following environment variables to be set:
     * - FOOTNOTE_TEST_FOLDER: Path to the folder containing EPub files.
     * - FOOTNOTE_TEST_LOGFILE: Path to the log file where results will be written.
     */
    void testEPubFiles();

    /**
     * @brief Tests EPub files for footnotes and logs the results in an HTML file.
     *
     * This function reads all EPub files from a directory specified by the environment variable
     * FOOTNOTE_TEST_FOLDER and logs the results of footnote parsing and retrieval to an HTML file
     * specified by the environment variable FOOTNOTE_TEST_LOGFILE. The log file will be created
     * with a .html extension if not already specified.
     *
     * The function performs the following steps:
     * 1. Checks for the required environment variables FOOTNOTE_TEST_FOLDER and
     * FOOTNOTE_TEST_LOGFILE.
     * 2. Opens the log file for writing.
     * 3. Iterates through each EPub file in the specified folder.
     * 4. For each EPub file, it parses the spine and retrieves footnotes.
     * 5. Logs the results, including any errors or warnings, to the HTML log file.
     *
     * @note The function will return early if the required environment variables are not set.
     *
     * @warning Ensure that the environment variables FOOTNOTE_TEST_FOLDER and FOOTNOTE_TEST_LOGFILE
     * are defined before running this function.
     */
    void testEPubFilesHTML();
#endif
};