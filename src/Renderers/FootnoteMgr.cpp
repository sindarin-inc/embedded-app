#include "FootnoteMgr.hpp"

#include <sstream>

#define HEXTONIBBLE(c) ((c) >= 'A' ? ((c) >= 'a' ? ((c) - 'a') : ((c) - 'A')) + 10 : ((c) - '0'))

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
void FootnoteMgr::cleanupPath(SpiramString &filename, const char *currentFolder) const {
    // Allocate memory for the result
    SpiramString fullPath{};
    bool absolute{false};

    // If filename starts with '/', it is an absolute path
    if (filename[0] == '/') {
        fullPath = filename.c_str();
        absolute = true;
    } else {
        // Create a combined path
        fullPath = currentFolder;
        fullPath.push_back('/');
        fullPath.append(filename.c_str());
    }

    std::vector<SpiramString> tokens;
    std::istringstream f(fullPath.c_str());

    SpiramString s;
    while (std::getline(f, s, '/')) {
        //  std::cout << "-> " << s << std::endl;
        if (s.empty()) {
        } else if (s == "..") {
            if (!tokens.empty()) {
                tokens.pop_back();
            }
        } else if (s == ".") {
        } else {
            SpiramString t;
            int pos = -1;
            int lastPos = -1;
            while ((pos = s.find('%', pos + 1)) != -1) {
                if (pos != lastPos) {
                    t.append(s.substr(lastPos + 1, pos - (lastPos + 1)));
                }
                t.push_back(static_cast<char>(HEXTONIBBLE(s[pos + 1]) << 4) +
                            HEXTONIBBLE(s[pos + 2]));
                pos = lastPos = pos + 2;
            }
            if ((lastPos + 1) < s.size()) {
                t.append(s.substr(lastPos + 1, s.size() - lastPos + 1));
            }
            tokens.push_back(t);
        }
    }

    // Rebuild the cleaned path
    filename.clear();
    if (absolute) {
        filename.push_back('/');
    }
    for (auto &token : tokens) {
        filename.append(token);
        filename.push_back('/');
    }

    filename.pop_back();

    // std::cout << "Cleaned path: " << *filename << std::endl;
}

/**
 * @brief Adds a footnote entry to the manager.
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
auto FootnoteMgr::addEntry(const char *href, const std::shared_ptr<EPubFile> &stream,
                           StreamPlace anchorStreamPlace,
                           const SpiramMap<SpiramString, uint32_t> &idsInSpine) -> bool {

    if ((href == nullptr) || (href[0] == '\0')) {
        if constexpr (FootnoteDebugging) {
            log_e("href is empty!!");
        }
        return false;
    }

    SpiramString id{""}, filename{""};

    const char *ptr = href;
    while ((*ptr != '\0') && (*ptr != '#')) {
        filename.push_back(*ptr++);
    }

    if (filename.empty()) {
        // This happen when a href contains only a '#' followed by the id. This means that
        // the footnote his in the same spine.
        filename = stream->getSpine(anchorStreamPlace.getSpineIdx()).item->href.c_str();
    } else {
        cleanupPath(filename,
                    stream->getSpineRelativeFolderPath(anchorStreamPlace.getSpineIdx()).c_str());
        SpineIdx spineIdx = stream->getSpineIdx(filename.c_str());
        if (spineIdx < anchorStreamPlace.getSpineIdx()) {
            // Heuristic part 1: a footnote anchor cannot be pointing at a previous location.
            if constexpr (FootnoteDebugging) {
                log_w("Potential Footnote at spineIdx %d is locate before current location %s. "
                      "Rejected!",
                      spineIdx, anchorStreamPlace.str().c_str());
            }
            return false;
        }
    }

    if ((*ptr == '\0') || (*ptr != '#')) {
        if constexpr (FootnoteDebugging) {
            log_e("Invalid href: %s", href);
        }
        return false;
    }

    ptr += 1; // pass the '#'
    if (*ptr == '\0') {
        if constexpr (FootnoteDebugging) {
            log_e("Invalid Id after '#': %s", href);
        }
        return false;
    }

    while (*ptr != '\0') {
        id.push_back(*ptr++);
    }

    // If the anchor point at a location preceding in the spine, it is considered non-valid
    auto it = idsInSpine.find(id);
    if ((it != idsInSpine.end()) && (it->second < anchorStreamPlace.getOffset())) {
        // Heuristic part 2: a footnote anchor cannot be pointing at a previous location.
        if constexpr (FootnoteDebugging) {
            log_w("Potential Footnote at offset %" PRIu32
                  " is locate before current location %s. Rejected!",
                  it->second, anchorStreamPlace.str().c_str());
        }
        return false;
    }

#if SIMULATOR
    footnotes_.push_back(SpiRamPtr<Footnote>(
        anchorStreamPlace, stream->getSpine(anchorStreamPlace.getSpineIdx()).item->href, id,
        filename, SpiramString{}, false));
#else
    footnotes_.push_back(
        SpiRamPtr<Footnote>(anchorStreamPlace, id, filename, SpiramString{}, false));
#endif

    return true;
}

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
auto FootnoteMgr::parse(const std::shared_ptr<EPubFile> &stream, SpineIdx spineIdx) -> bool {

    clear();

#if FOOTNOTE_CHAPTER_BASED
    if (!stream->spineIdxIsChapterStart(spineIdx)) {
        spineIdx = stream->getChapterIdx(spineIdx);
    }
#endif

    currentSpineIdx_ = spineIdx;

    FOOTNOTE_PROFILE_START(parse);

    do {
        REND_PROFILE_START(loadXMLFile)
        const EPubOpf::SpineItem &spineItem = stream->getSpine(currentSpineIdx_);
        pugi::xml_document &doc =
            stream->getXHTMLFile(static_cast<std::string>(spineItem.item->href));
        REND_PROFILE_END(loadXMLFile)

        pugi::xml_node node = doc.child("html").child("body").first_child();

        struct Interpret : pugi::xml_tree_walker {
            SpineIdx currentSpineIdx{};
            FootnoteMgr *footnoteMgr{};
            const std::shared_ptr<EPubFile> *stream{};
            SpiramMap<SpiramString, uint32_t> idsInSpine;
            bool skipNext{};

            struct FindSubNode : pugi::xml_tree_walker {
                auto for_each(pugi::xml_node &node) -> bool override {
                    if (nodeName == node.name()) {
                        theNode = node;
                        found = true;
                        return false;
                    }
                    return true;
                }
                std::string_view nodeName;
                pugi::xml_node theNode;
                bool found{};

                inline void setup(const char *nodeName) {
                    this->nodeName = nodeName;
                    found = false;
                }
            } findSubNode;

            auto for_each(pugi::xml_node &node) -> bool override {
                if (node.type() == pugi::node_element) {

                    bool result = true;

                    SpiramString id = node.attribute("id").value();
                    if (strcmp(node.name(), "sup") == 0) {
                        // case 1: <sup>...<a href ="xxx">yyy</a>...</sup>
                        findSubNode.setup("a");
                        node.traverse(findSubNode);
                        if (findSubNode.found) {
                            pugi::xml_node aNode = findSubNode.theNode;
                            const char *href = aNode.attribute("href").value();
                            if ((href[0] != '\0') && (strncmp(href, "http://", 7) != 0) &&
                                (strncmp(href, "https://", 8) != 0)) {
                                result = footnoteMgr->addEntry(
                                    href, *stream,
                                    {static_cast<uint32_t>(node.offset_debug()), currentSpineIdx},
                                    idsInSpine);
                            }
                            SpiramString aId = aNode.attribute("id").value();
                            if (!aId.empty() && id.empty()) {
                                id = aId;
                            }
                            skipNext = true;
                        }
                    } else if (strcmp(node.name(), "a") == 0) {
                        if (skipNext) {
                            skipNext = false;
                        } else {
                            // case 2: <a href="xxx"><sup>yyy</sup></a>
                            // case 3: <a href="xxx" id="iii">yyy</a>
                            const char *href = node.attribute("href").value();
                            if ((href[0] != '0') && (strncmp(href, "http://", 7) != 0) &&
                                (strncmp(href, "https://", 8) != 0)) {
                                if (node.attribute("id") || node.child("sup")) {
                                    result = footnoteMgr->addEntry(
                                        href, *stream,
                                        {static_cast<uint32_t>(node.offset_debug()),
                                         currentSpineIdx},
                                        idsInSpine);
                                }
                                // if (node.child("sup") && (href[0] != '\0')) {
                                //     result = footnoteMgr->addEntry(
                                //         href,
                                //         {static_cast<uint32_t>(node.offset_debug()),
                                //         currentSpineIdx});
                                // }
                            }
                        }
                    }

                    if (!id.empty()) {
                        idsInSpine[id] = node.offset_debug();
                    }

                    if (!result) {
                        if constexpr (FootnoteDebugging) {
                            log_w("Footnote entry not added.");
                        }
                    }
                }

                return true;
            }

            void setup(SpineIdx spineIdx, FootnoteMgr *mgr,
                       const std::shared_ptr<EPubFile> &stream) {
                currentSpineIdx = spineIdx;
                footnoteMgr = mgr;
                this->stream = &stream;
                idsInSpine.clear();
                skipNext = false;
            }
        } interpret;

        interpret.setup(currentSpineIdx_, this, stream);

        if (node) {
            do {
                node.traverse(interpret);
                node = node.next_sibling();
            } while (node);
        } else {
            if constexpr (FootnoteDebugging) {
                log_w("HTML file %s is empty.", spineItem.item->href.c_str());
            }
        }

        if (!node) {
            // We are at the end of the current file
            // Pointing at next file, we will try to complete the current
            // page if it was not completely processed by the current file.
            currentSpineIdx_ += 1;

            // We break the process of paging if
            // - we are passed the end of the book
            // - the new current file is the beginning of a chapter if Chapter based
        }
    }
#if FOOTNOTE_CHAPTER_BASED
    while ((currentSpineIdx_ < stream->getSpineCount()) &&
           !stream->spineIdxIsChapterStart(currentSpineIdx_));
#else
    while (false);
#endif

    FOOTNOTE_PROFILE_END(parse);

    return true;
}

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
auto FootnoteMgr::retrieveNotes(const std::shared_ptr<EPubFile> &stream) -> bool {

    std::set<std::string> filenames;
    SpiramMap<SpiramString, int> idMap;
    bool status{true};

    FOOTNOTE_PROFILE_START(retrieveNotes);

    int idx = 0;
    for (auto &entry : footnotes_) {
        filenames.insert(entry->filename.c_str());
        idMap[entry->id.c_str()] = idx++;
    }

    // std::cout << "----- Files to check for footnotes -----" << std::endl;
    // for (const auto &entryFilename : filenames) {
    //     std::cout << "filename: " << entryFilename << std::endl;
    // }
    // std::cout << "----- [end] -----" << std::endl;

    struct Walker : pugi::xml_tree_walker {
        struct XmlStringWriter : pugi::xml_writer {
            SpiramString *result{};

            void write(const void *data, size_t size) override {
                result->append(static_cast<const char *>(data), size);
            }

            void setup(SpiramString *res) { result = res; }
        } writer;

        auto for_each(pugi::xml_node &node) -> bool override {
            pugi::xml_attribute id = node.attribute("id");
            if (id) {
                auto it = idMap->find(id.value());
                if (it != idMap->end()) {
                    pugi::xml_node pNode = node;
                    writer.setup(&((*footnotes)[it->second]->content));

                    // The footnote could be inside <br/> ... <br/> tags:

                    bool brDelimitedFound = false;

                    while (pNode && (strcmp(pNode.name(), "br") != 0)) {
                        pNode = pNode.previous_sibling();
                    }

                    if (pNode) {
                        pNode = pNode.next_sibling();

                        // Be sure that there is an ending <br/> tag
                        pugi::xml_node eNode = pNode;

                        while (eNode && (strcmp(eNode.name(), "br") != 0)) {
                            eNode = eNode.next_sibling();
                        }

                        if (eNode) {
                            // put the result inside a <p> tag
                            writer.write("<p>", 3);
                            while (pNode && (strcmp(pNode.name(), "br") != 0)) {
                                pNode.print(writer, "", pugi::format_raw);
                                pNode = pNode.next_sibling();
                            }
                            writer.write("</p>", 4);
                            (*footnotes)[it->second]->contentFound = true;
                            brDelimitedFound = true;
                        }
                    }

                    if (!brDelimitedFound) {

                        // Check for <p>, <div> or <li> tags

                        pugi::xml_node pNode = node;
                        while (pNode && ((strcmp(pNode.name(), "p") != 0) &&
                                         (strcmp(pNode.name(), "div") != 0) &&
                                         (strcmp(pNode.name(), "li") != 0))) {
                            pNode = pNode.parent();
                        }
                        if (pNode) {
                            pNode.print(writer, "", pugi::format_raw);
                            (*footnotes)[it->second]->contentFound = true;
                        }
                    }
                }
            }
            return true;
        }

        void setup(SpiramMap<SpiramString, int> *idMap,
                   SpiramVector<std::shared_ptr<Footnote>> *footnotes) {
            this->idMap = idMap;
            this->footnotes = footnotes;
        }

        SpiramMap<SpiramString, int> *idMap{};
        SpiramVector<std::shared_ptr<Footnote>> *footnotes{};
    } fileWalker;

    fileWalker.setup(&idMap, &footnotes_);

    for (const auto &entryFilename : filenames) {
        pugi::xml_document doc;
        auto [buffer, length] = stream->getFile(stream->getFullPath(entryFilename));
        if (buffer != nullptr) {
            if constexpr (FootnoteDebugging) {
                log_i("File %s, length = %" PRIu32 " ...", entryFilename.c_str(), length);
            }
            pugi::xml_parse_result result = doc.load_buffer_inplace(
                buffer.get(), length,
                (pugi::parse_default | pugi::parse_ws_pcdata) & ~pugi::parse_escapes);

            if (result) {
                pugi::xml_node body = doc.child("html").child("body");
                if (body) {

                    body.traverse(fileWalker);

                } else {
                    if constexpr (FootnoteDebugging) {
                        log_e("<body> tag not found in %s.", entryFilename.c_str());
                    }
                    status = false;
                    break;
                }
            } else {
                if constexpr (FootnoteDebugging) {
                    log_e("Unable to parse file %s with pugixml.",
                          stream->getFullPath(entryFilename).c_str());
                }
                status = false;
                break;
            }
        } else {
            if constexpr (FootnoteDebugging) {
                log_e("Unable to retrieve file %s.", stream->getFullPath(entryFilename).c_str());
            }
            status = false;
            break;
        }
    }

    FOOTNOTE_PROFILE_END(retrieveNotes);

    return status;
}

/**
 * @brief Generates the HTML representation of a footnote.
 *
 * This function creates an HTML document for the footnote at the specified index.
 * If the footnote cannot be found, an empty string is returned.
 *
 * @param index The index of the footnote to retrieve.
 *
 * @return A shared pointer to a SpiramString containing the HTML representation of the footnote.
 *         Will be an empty sring if the footnote content is not found.
 */
auto FootnoteMgr::getFootnoteHTML(int index) const -> std::shared_ptr<SpiramString> {
    auto result = std::make_shared<SpiramString>();

    if ((index < footnotes_.size()) && footnotes_[index]->contentFound) {
        result->assign("<?xml version='1.0' encoding='utf-8'?><!DOCTYPE html><html><head/><body>");
        result->append(footnotes_[index]->content);
        result->append("</body></html>");
    }

    return result;
}

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
auto FootnoteMgr::getAllFootnotesHTML() const -> std::shared_ptr<SpiramString> {
    auto result = std::make_shared<SpiramString>();

    result->assign("<?xml version='1.0' encoding='utf-8'?><!DOCTYPE html><html><head/><body>");

    if (footnotes_.empty()) {
        result->append("<p>No Footnote</p>");
    } else {
        for (auto &footnote : footnotes_) {
            if (footnote->contentFound) {
                result->append(footnote->content);
            }
        }
    }

    result->append("</body></html>");

    return result;
}

#if SIMULATOR

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

/**
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
void FootnoteMgr::testEPubFiles() {
    bool error{false};
    char *folder{nullptr};
    char *logFile{nullptr};

    folder = getenv("FOOTNOTE_TEST_FOLDER");

    if (folder == nullptr) {
        std::cerr << "Please define environment variable FOOTNOTE_TEST_FOLDER where "
                  << "EPub ebook files reside to be use for testing the FootnoteMgr." << std::endl;
        error = true;
    }

    logFile = getenv("FOOTNOTE_TEST_LOGFILE");

    if (logFile == nullptr) {
        std::cerr << "Please define environment variable FOOTNOTE_TEST_LOGFILE where "
                  << "the result log will be generated." << std::endl;
        error = true;
    }

    if (error) {
        return;
    }

    std::cout << "FOOTNOTE_TEST_FOLDER = " << folder << std::endl
              << "FOOTNOTE_TEST_LOGFILE = " << logFile << std::endl;

    std::ofstream logStream;

    logStream.open(logFile);

    for (const auto &entry : fs::directory_iterator(folder)) {
        logStream << "===== " << entry.path().c_str() << " =====" << std::endl;

        auto file = SFile(std::make_shared<VFSFile>(entry.path().c_str(), FILE_READ, false));
        auto readable = std::make_shared<Readable>();
        readable->readableType = "book";

        auto prefs = PreferencesStore();
        SolDrm solDrm(prefs);

        auto stream = std::make_shared<EPubFile>(file, readable, solDrm);

        auto spineCount = stream->getSpineCount();
        for (SpineIdx idx = 0; idx < spineCount; idx++) {
#if FOOTNOTE_CHAPTER_BASED
            if (stream->spineIdxIsChapterStart(idx)) {
                logStream << "----- Chapter at spineIdx " << idx << " -----" << std::endl;
                if (!parse(stream, idx)) {
                    logStream << "Some errors parsing the chapter." << std::endl;
                } else if (!retrieveNotes(stream)) {
                    logStream << "Some errors retrieving notes." << std::endl;
                }
                show(logStream);
            }
#else
            logStream << "----- SpineIdx " << idx << " -----" << std::endl;
            if (!parse(stream, idx)) {
                logStream << "Some errors parsing the spine." << std::endl;
            } else if (!retrieveNotes(stream)) {
                logStream << "Some errors retrieving notes." << std::endl;
            }
            show(logStream);
#endif
        }

        stream->close();
    }

    logStream.close();
    std::cout << "Testing FootnoteMgr completed." << std::endl;
}

/**
 * @brief Tests EPub files for footnotes and logs the results in an HTML file.
 *
 * This function reads all EPub files from a directory specified by the environment variable
 * FOOTNOTE_TEST_FOLDER and logs the results of footnote parsing and retrieval to an HTML file
 * specified by the environment variable FOOTNOTE_TEST_LOGFILE. The log file will be created
 * with a .html extension if not already specified.
 *
 * The function performs the following steps:
 * 1. Checks for the required environment variables FOOTNOTE_TEST_FOLDER and FOOTNOTE_TEST_LOGFILE.
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
void FootnoteMgr::testEPubFilesHTML() {
    bool error{false};
    char *folder{nullptr};
    std::string logFile{};

    folder = getenv("FOOTNOTE_TEST_FOLDER");

    if (folder == nullptr) {
        std::cerr << "Please define environment variable FOOTNOTE_TEST_FOLDER where "
                  << "EPub ebook files reside to be use for testing the FootnoteMgr." << std::endl;
        error = true;
    }

    char *logFileName = getenv("FOOTNOTE_TEST_LOGFILE");

    if (logFileName == nullptr) {
        std::cerr << "Please define environment variable FOOTNOTE_TEST_LOGFILE where "
                  << "the result log will be generated." << std::endl;
        error = true;
    }

    if (error) {
        return;
    }

    logFile.append(logFileName);

    if (logFile.find(".html") == std::string::npos) {
        logFile.append(".html");
    }

    std::cout << "FOOTNOTE_TEST_FOLDER = " << folder << std::endl
              << "FOOTNOTE_TEST_LOGFILE = " << logFile << std::endl;

    std::ofstream logStream;

    logStream.open(logFile);

    logStream << "<?xml version='1.0' encoding='utf-8'?><!DOCTYPE html><html><head/><body><style "
                 "type=\"text/css\"> p {margin-bottom: 0.5em;  margin-top: -0.3em;} .warning { "
                 "color: orange } .error { color: red } </style>"
              << std::endl;

    for (const auto &entry : fs::directory_iterator(folder)) {

        logStream << "<br/><hr/><h1>" << entry.path().c_str() << "</h1>" << std::endl;

        std::cout << "===== OPENING " << entry.path().c_str() << " =====" << std::endl;

        auto file = SFile(std::make_shared<VFSFile>(entry.path().c_str(), FILE_READ, false));
        auto readable = std::make_shared<Readable>();
        readable->readableType = "book";

        auto prefs = PreferencesStore();
        SolDrm solDrm(prefs);

        auto stream = std::make_shared<EPubFile>(file, readable, solDrm);

        auto spineCount = stream->getSpineCount();

        std::cout << "Spine count: " << spineCount << std::endl;

        for (SpineIdx idx = 0; idx < spineCount; idx++) {
#if FOOTNOTE_CHAPTER_BASED
            if (stream->spineIdxIsChapterStart(idx)) {
                logStream << "<h2>Chapter at spineIdx " << idx << "</h2>" << std::endl;
                if (!parse(stream, idx)) {
                    logStream
                        << "<p class=\"error\"><b><i>Some errors parsing the chapter.</i></b></p>"
                        << std::endl;
                } else if (!retrieveNotes(stream)) {
                    logStream
                        << "<p class=\"error\"><b><i>Some errors retrieving notes.<i/></b></p>"
                        << std::endl;
                }
                if (isEmpty()) {
                    logStream << "<p class=\"warning\"><b><i>No Footnote</i></b></p>" << std::endl;
                } else {
                    logStream << "<p><small>Entries count: " << footnotes_.size() << ".</small></p>"
                              << std::endl;
                }
                show(logStream, true);
            }
#else
            logStream << "<h2>SpineIdx " << idx << "</h2>" << std::endl;
            if (!parse(stream, idx)) {
                logStream << "<p class=\"error\"><b><i>Some errors parsing the spine.</i></b></p>"
                          << std::endl;
            } else if (!retrieveNotes(stream)) {
                logStream << "<p class=\"error\"><b><i>Some errors retrieving notes.<i/></b></p>"
                          << std::endl;
            }
            if (isEmpty()) {
                logStream << "<p class=\"warning\"><b><i>No Footnote</i></b></p>" << std::endl;
            } else {
                logStream << "<p><small>Entries count: " << footnotes_.size() << ".</small></p>"
                          << std::endl;
            }
            show(logStream, true);
#endif
        }

        stream->close();
    }

    logStream << "<br/><hr/></body></html>" << std::endl;

    logStream.close();
    std::cout << "Testing FootnoteMgr completed." << std::endl;
}

#endif
