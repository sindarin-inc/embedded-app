#include "FootnoteMgr.hpp"

void FootnoteMgr::addEntry(const char *href, StreamPlace streamPlace) {

    if (href == nullptr) {
        log_e("href is null!!");
        return;
    }

    SpiramString id{""}, filename{""};

    const char *ptr = href;
    while (ptr && (*ptr != '#')) {
        filename += *ptr++;
    }

    if ((ptr == nullptr) || (*ptr != '#')) {
        log_e("Invalid href: %s", href);
        return;
    }

    ptr += 1; // pass the '#'
    if (ptr == nullptr) {
        log_e("Invalid Id after '#': %s", href);
        return;
    }

    while (*ptr != '\0') {
        id += *ptr++;
    }

    footnotes_.push_back(SpiRamPtr<Footnote>(streamPlace, id, filename, SpiramString{}, false));
}

#define HEXTONIBBLE(c) ((c) >= 'A' ? ((c) >= 'a' ? ((c) - 'a') : ((c) - 'A')) + 10 : ((c) - '0'))

auto FootnoteMgr::adjustFilename(const char *filename) -> SpiramString {
    auto adjFName = SpiRamArrayPtr<char>(256);
    int idx{0};
    const char *fn = filename;

    while ((idx < 254) && (*fn != '\0')) {
        if ((*fn == '%') && (fn[1] != '\0') && (fn[2] != '\0')) {
            adjFName[idx++] = (HEXTONIBBLE(fn[1]) << 4) + HEXTONIBBLE(fn[2]);
            fn += 3;
        } else if ((*fn == '/') && (fn[1] == '.') && (fn[2] == '.') && (fn[3] == '/')) {
            while (idx > 0) {
                idx -= 1;
                if (adjFName[idx] == '/') break;
            }
            fn += idx > 0 ? 3 : 4;
        } else if ((*fn == '.') && (fn[1] == '.') && (fn[2] == '/')) {
            while (idx > 0) {
                idx -= 1;
                if (adjFName[idx] == '/') break;
            }
            fn += idx > 0 ? 2 : 3;
        } else if ((*fn == '.') && (fn[1] == '/')) {
            fn += idx > 0 ? 1 : 2;
        } else {
            adjFName[idx++] = *fn++;
        }
    }

    adjFName[idx] = 0;

    return SpiramString(adjFName.get());
}

auto FootnoteMgr::parseChapter(std::shared_ptr<EPubFile> stream, SpineIdx spineIdx) -> bool {

    clear();

    if (!stream->spineIdxIsChapterStart(spineIdx)) {
        spineIdx = stream->getChapterIdx(spineIdx);
    }

    currentSpineIdx_ = spineIdx;

    REND_PROFILE_START(parseChapter);

    do {
        REND_PROFILE_START(loadXMLFile)
        const EPubOpf::SpineItem &spineItem = stream->getSpine(currentSpineIdx_);
        pugi::xml_document &doc =
            stream->getXHTMLFile(static_cast<std::string>(spineItem.item->href));
        REND_PROFILE_END(loadXMLFile)

        pugi::xml_node node = doc.child("html").child("body").first_child();

        struct Interpret : pugi::xml_tree_walker {
            SpineIdx currentSpineIdx;
            FootnoteMgr *footnoteMgr;

            auto for_each(pugi::xml_node &node) -> bool override {
                if (node.type() == pugi::node_element) {

                    if (strcmp(node.name(), "sup") == 0) {
                        const char *href = node.child("a").attribute("href").value();
                        if (href[0] != '\0') {
                            footnoteMgr->addEntry(href, {static_cast<uint32_t>(node.offset_debug()),
                                                         currentSpineIdx});
                        }
                    } else if (strcmp(node.name(), "a") == 0) {
                        const char *href = node.attribute("href").value();
                        if (node.child("sup") && (href[0] != '\0')) {
                            footnoteMgr->addEntry(href, {static_cast<uint32_t>(node.offset_debug()),
                                                         currentSpineIdx});
                        }
                    }
                }

                return true;
            }

            void setup(SpineIdx spineIdx, FootnoteMgr *mgr) {
                currentSpineIdx = spineIdx;
                footnoteMgr = mgr;
            }
        } interpret;

        interpret.setup(currentSpineIdx_, this);

        if (node) {
            do {
                node.traverse(interpret);
                node = node.next_sibling();
            } while (node);
        } else {
            log_w("HTML file %s is empty.", spineItem.item->href.c_str());
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

    REND_PROFILE_END(parseChapter);

    // This is for debugging

    if (!retrieveNotes(stream)) {
        log_e("Unable to retrieve all footnotes");
    }
    show();

    return true;
}

auto FootnoteMgr::retrieveNotes(std::shared_ptr<EPubFile> stream) -> bool {

    std::set<std::string> filenames;
    SpiramMap<SpiramString, int> idMap;

    int idx = 0;
    for (auto &entry : footnotes_) {
        if (entry->filename_.empty()) {
            // This happen when a href contains only a '#' followed by the id. This means that the
            // footnote his in the chapter's spine.
            entry->filename_ =
                stream->getSpine(entry->streamPlace_.getSpineIdx()).item->href.c_str();
        } else {
            entry->filename_ = adjustFilename(entry->filename_.c_str());
        }
        filenames.insert(entry->filename_.c_str());
        idMap[entry->id_.c_str()] = idx++;
    }

    std::cout << "----- Files to check for footnotes -----" << std::endl;
    for (auto entryFilename : filenames) {
        std::cout << "filename: " << entryFilename << std::endl;
    }
    std::cout << "----- [end] -----" << std::endl;

    struct Walker : pugi::xml_tree_walker {
        struct xml_string_writer : pugi::xml_writer {
            SpiramString *result;

            virtual void write(const void *data, size_t size) {
                result->append(static_cast<const char *>(data), size);
            }

            void setup(SpiramString *res) { result = res; }
        } writer;

        auto for_each(pugi::xml_node &node) -> bool override {
            pugi::xml_attribute id = node.attribute("id");
            if (id) {
                auto it = idMap_->find(id.value());
                if (it != idMap_->end()) {
                    pugi::xml_node pNode = node;
                    writer.setup(&((*footnotes_)[it->second]->content_));
                    while (pNode && strcmp(pNode.name(), "p") != 0) {
                        pNode = pNode.parent();
                    }
                    if (!pNode) {
                        pNode = node;
                    }
                    pNode.print(writer, "", pugi::format_raw);
                    (*footnotes_)[it->second]->contentFound_ = true;
                }
            }
            return true;
        }

        void setup(SpiramMap<SpiramString, int> *idMap,
                   SpiramVector<std::shared_ptr<Footnote>> *footnotes) {
            idMap_ = idMap;
            footnotes_ = footnotes;
        }

        SpiramMap<SpiramString, int> *idMap_;
        SpiramVector<std::shared_ptr<Footnote>> *footnotes_;
    } fileWalker;

    fileWalker.setup(&idMap, &footnotes_);

    for (auto entryFilename : filenames) {
        pugi::xml_document doc;
        auto [buffer, length] = stream->getFile(stream->getFullPath(entryFilename));
        if (buffer != nullptr) {
            log_i("File %s, length = %" PRIu32 " ...", entryFilename.c_str(), length);
            pugi::xml_parse_result result = doc.load_buffer_inplace(
                buffer.get(), length,
                (pugi::parse_default | pugi::parse_ws_pcdata) & ~pugi::parse_escapes);

            if (result) {
                pugi::xml_node body = doc.child("html").child("body");
                if (body) {

                    body.traverse(fileWalker);

                } else {
                    log_e("<body> tag not found in %s.", entryFilename.c_str());
                    return false;
                }
            } else {
                log_e("Unable to parse file %s with pugixml.",
                      stream->getFullPath(entryFilename).c_str());
                return false;
            }
        } else {
            log_e("Unable to retrieve file %s.", stream->getFullPath(entryFilename).c_str());
            return false;
        }
    }

    return true;
}
