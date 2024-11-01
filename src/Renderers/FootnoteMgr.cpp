#include "FootnoteMgr.hpp"

#include <sstream>

#define HEXTONIBBLE(c) ((c) >= 'A' ? ((c) >= 'a' ? ((c) - 'a') : ((c) - 'A')) + 10 : ((c) - '0'))

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

auto FootnoteMgr::addEntry(const char *href, const std::shared_ptr<EPubFile> &stream,
                           StreamPlace streamPlace,
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
        filename = stream->getSpine(streamPlace.getSpineIdx()).item->href.c_str();
    } else {
        cleanupPath(filename,
                    stream->getSpineRelativeFolderPath(streamPlace.getSpineIdx()).c_str());
        SpineIdx spineIdx = stream->getSpineIdx(filename.c_str());
        if (spineIdx < streamPlace.getSpineIdx()) {
            // Heuristic part 1: a footnote anchor cannot be pointing at a previous location.
            if constexpr (FootnoteDebugging) {
                log_w("Potential Footnote at spineIdx %d is locate before current location %s. "
                      "Rejected!",
                      spineIdx, streamPlace.str().c_str());
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
    if ((it != idsInSpine.end()) && (it->second < streamPlace.getOffset())) {
        // Heuristic part 2: a footnote anchor cannot be pointing at a previous location.
        if constexpr (FootnoteDebugging) {
            log_w("Potential Footnote at offset %" PRIu32
                  " is locate before current location %s. Rejected!",
                  it->second, streamPlace.str().c_str());
        }
        return false;
    }

    footnotes_.push_back(SpiRamPtr<Footnote>(streamPlace, id, filename, SpiramString{}, false));

    return true;
}

auto FootnoteMgr::parseChapter(const std::shared_ptr<EPubFile> &stream, SpineIdx spineIdx) -> bool {

    clear();

    if (!stream->spineIdxIsChapterStart(spineIdx)) {
        spineIdx = stream->getChapterIdx(spineIdx);
    }

    currentSpineIdx_ = spineIdx;

    FOOTNOTE_PROFILE_START(parseChapter);

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

            auto for_each(pugi::xml_node &node) -> bool override {
                if (node.type() == pugi::node_element) {

                    bool result = true;

                    SpiramString id = node.attribute("id").value();
                    if (!id.empty()) {
                        idsInSpine[id] = node.offset_debug();
                    }
                    if (strcmp(node.name(), "sup") == 0) {
                        // case 1: <sup><a href ="xxx">yyy</a></sup>
                        const char *href = node.child("a").attribute("href").value();
                        if (href[0] != '\0') {
                            result = footnoteMgr->addEntry(
                                href, *stream,
                                {static_cast<uint32_t>(node.offset_debug()), currentSpineIdx},
                                idsInSpine);
                        }
                    } else if (strcmp(node.name(), "a") == 0) {
                        // case 2: <a href="xxx"><sup>yyy</sup></a>
                        // case 3: <a href="xxx" id="iii">yyy</a>
                        const char *href = node.attribute("href").value();
                        if ((href[0] != '0') && (strncmp(href, "http://", 7) != 0) &&
                            (strncmp(href, "https://", 8) != 0)) {
                            if (node.attribute("id") || node.child("sup")) {
                                result = footnoteMgr->addEntry(
                                    href, *stream,
                                    {static_cast<uint32_t>(node.offset_debug()), currentSpineIdx},
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
            // - the new current file is the beginning of a chapter
        }
    } while ((currentSpineIdx_ < stream->getSpineCount()) &&
             !stream->spineIdxIsChapterStart(currentSpineIdx_));

    FOOTNOTE_PROFILE_END(parseChapter);

    if constexpr (FootnoteDebugging) {
        if (!retrieveNotes(stream)) {
            log_e("Unable to retrieve all footnotes");
            show();
        }
    }
    return true;
}

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
                    while (pNode && strcmp(pNode.name(), "p") != 0) {
                        pNode = pNode.parent();
                    }
                    if (!pNode) {
                        pNode = node;
                    }
                    pNode.print(writer, "", pugi::format_raw);
                    (*footnotes)[it->second]->contentFound = true;
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

auto FootnoteMgr::getFootnoteHTML(int index) const -> std::shared_ptr<SpiramString> {
    auto result = std::make_shared<SpiramString>();

    result->assign("<?xml version='1.0' encoding='utf-8'?><!DOCTYPE html><html><head/><body>");

    if ((index < footnotes_.size()) && footnotes_[index]->contentFound) {
        result->append(footnotes_[index]->content);
    } else {
        result->append("<p>No Footnote</p>");
    }

    result->append("</body></html>");

    return result;
}

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

        auto stream = std::make_shared<EPubFile>(file, readable);

        auto spineCount = stream->getSpineCount();
        for (SpineIdx idx = 0; idx < spineCount; idx++) {
            if (stream->spineIdxIsChapterStart(idx)) {
                logStream << "----- Chapter at spineIdx " << idx << " -----" << std::endl;
                if (!parseChapter(stream, idx)) {
                    logStream << "Some errors parsing chapter." << std::endl;
                } else if (!retrieveNotes(stream)) {
                    logStream << "Some errors retrieving notes." << std::endl;
                }
                show(logStream);
            }
        }

        stream->close();
    }

    logStream.close();
    std::cout << "Testing FootnoteMgr completed." << std::endl;
}

#endif
