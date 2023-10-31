#include "EPubFile.hpp"

#include "EPubDefs.hpp"
#include "EPubMetaFile.hpp"

auto EPubFile::findFileOffsetAtCharOffset(const std::string &path, uint32_t charOffset)
    -> uint32_t {

    pugi::xml_document &doc = getXHTMLFile(path);

    struct Walker : pugi::xml_tree_walker {
        auto for_each(pugi::xml_node &node) -> bool override {
            if (node.type() == pugi::xml_node_type::node_pcdata) {
                uint32_t length = strlen(node.value());
                if ((chOffset + length) > charOffset) {
                    offset = epub->getOffset(node.value()) + (charOffset - chOffset);
                    return false;
                }
                chOffset += length;
            }
            return true;
        }

        uint32_t offset;
        uint32_t chOffset;
        uint32_t charOffset;
        EPubFile *epub;
    } walker;

    walker.offset = walker.chOffset = 0;
    walker.charOffset = charOffset;
    walker.epub = this;

    doc.child("html").child("body").traverse(walker);
    // if (readable_->isArticle()) log_w("article");

    return walker.offset;
}

auto EPubFile::findCharOffsetAtFileOffset(const std::string &path, uint32_t fileOffset)
    -> uint32_t {

    pugi::xml_document &doc = getXHTMLFile(path);

    struct Walker : pugi::xml_tree_walker {
        auto for_each(pugi::xml_node &node) -> bool override {
            if (node.type() == pugi::xml_node_type::node_pcdata) {
                uint32_t length = strlen(node.value());
                uint32_t nodeOffset = node.offset_debug();
                if ((nodeOffset + length) >= fileOffset) {
                    uint32_t offset = (fileOffset > nodeOffset) ? fileOffset - nodeOffset : 0;
                    charOffset += offset;
                    return false;
                }
                charOffset += length;
            }
            return true;
        }

        uint32_t charOffset;
        uint32_t fileOffset;
    } walker;

    walker.charOffset = 0;
    walker.fileOffset = fileOffset;

    doc.child("html").child("body").traverse(walker);
    // if (readable_->isArticle()) log_w("article");

    return walker.charOffset;
}

// Returns a pointer to the buffer containing the extracted file from the EPub and it's length in a
// tuple
auto EPubFile::getFile(const std::string &completeFilePath)
    -> std::pair<std::shared_ptr<uint8_t[]>, uint32_t> {

    std::shared_ptr<uint8_t[]> buffer = nullptr;

    EPUB_PROFILE_START(UnzipperOpenFile);
    auto res = epubUnzipper_.openFile(completeFilePath);
    EPUB_PROFILE_END(UnzipperOpenFile);

    if (!res) {
        log_e("Unzipper Failed to open file %s", completeFilePath.c_str());
    } else {
        EPUB_PROFILE_START(UnzipperGetFileSize);
        auto size = epubUnzipper_.getFileSize();
        EPUB_PROFILE_END(UnzipperGetFileSize);

        if (size > 0) {
            buffer = SpiRamArrayPtr<uint8_t>(size);

            if (buffer != nullptr) {
                EPUB_PROFILE_START(UnzipperReadFromFile);
                uint32_t length = epubUnzipper_.readFile(buffer.get(), size);
                epubUnzipper_.closeFile();
                EPUB_PROFILE_END(UnzipperReadFromFile);
                if (length == size) {
                    // if (readable_->isArticle()) log_w("article");

                    return {std::move(buffer), length};
                }
            } else {
                epubUnzipper_.closeFile();
                log_e("Unable to allocate space for file %s, space required: %" PRIu32,
                      completeFilePath.c_str(), static_cast<uint32_t>(size));
            }
        }
    }

    // if (readable_->isArticle()) log_w("article");

    return {nullptr, 0};
}

auto EPubFile::getXHTMLFile(const std::string &path) -> pugi::xml_document & {
    std::string filePath = opf_->getFullPath(path);
    // log_w("Loading file %s", filePath.c_str());
    uint32_t length;

    if (currentFilePath_ != filePath) {

        // log_w("currentFilePath_: %s, filePath : %s", currentFilePath_.c_str(), filePath.c_str());

        std::tie(currentFileBuffer_, length) = getFile(filePath);

        if (currentFileBuffer_ != nullptr) {
            // Using load_buffer_inplace() is mandatory here as it permits the
            // computation of a node offset location, as string elements are taken
            // directly in the currentFileBuffer instead of being copied.
            // (see the EPubFile::getOffset() method).

            EPUB_PROFILE_START(ParseXHTML);
            pugi::xml_parse_result result = currentFileDoc_.load_buffer_inplace(
                currentFileBuffer_.get(), length,
                (pugi::parse_default | pugi::parse_ws_pcdata) & ~pugi::parse_escapes);
            EPUB_PROFILE_END(ParseXHTML);

            if (result) {
                currentFilePath_ = filePath;
            } else {
                log_e("Failed to parse XHTML file %s: %s", filePath.c_str(), result.description());
            }
        } else {
            log_e("Unable to unzip file %s", filePath.c_str());
        }
    }

    // if (readable_->isArticle()) log_w("article");

    return currentFileDoc_;
}

auto EPubFile::getReadableToCChapterIndex(const std::shared_ptr<EPubPlace> &ePubPlace) const
    -> int {
    return opf_->getReadableToCChapterIndex(ePubPlace->getHRef());
}

auto EPubFile::hasSummary(const std::string &chapterID) -> bool {
    std::string summaryID = "summary-" + chapterID;

    auto hasDirectChapterSummary = opf_->exists(summaryID);
    if (hasDirectChapterSummary) {
        return hasDirectChapterSummary;
    }

    // Walk the chapters backwards in the spine to see if the chapter id matches
    auto summaryChapterHref = opf_->getHrefByID(summaryID);
    auto summaryChapterIdx = opf_->getReadableToCChapterIndex(summaryChapterHref);

    return summaryChapterIdx != -1;
}

auto EPubFile::getSummaryContent(std::string &chapterID,
                                 const std::shared_ptr<EPubPlace> &placeInReadable,
                                 std::shared_ptr<TxtPlace> &placeInSummary, bool includeUnread)
    -> SpiramString {

    if (!hasSummary(chapterID)) {
        log_e("This chapter does not have a summary.");
        return "This chapter does not have a summary.";
    }

    // Load the summary xhtml content
    auto chapterHref = opf_->getHrefByID(chapterID);
    auto chapterIdx = readable_->findEPubHRefChapterIdx(chapterHref);
    if (chapterIdx == -1) {
        chapterIdx = opf_->getReadableToCChapterIndex(chapterHref);
        chapterHref = getToc()[chapterIdx].href;
        chapterID = opf_->getIdByHref(chapterHref);
    }

    log_d("Loading summary for chapter: %s (href: %s, chapter idx: %d)", chapterID.c_str(),
          chapterHref.c_str(), chapterIdx);
    SpiramString summaryText;

    for (const auto &chapter : getToc()) {
        auto sameChapter = (placeInReadable->isFor(DocType::EPUB) && chapter.href == chapterHref) ||
                           (placeInReadable->isFor(DocType::TXT) &&
                            chapter.txtOffset < placeInReadable->getOffset() &&
                            placeInReadable->getOffset() < chapter.endTxtOffset);
        auto futureChapter = (placeInReadable->isFor(DocType::EPUB) &&
                              readable_->findEPubHRefChapterIdx(chapter.href) > chapterIdx) ||
                             (placeInReadable->isFor(DocType::TXT) &&
                              placeInReadable->getOffset() < chapter.txtOffset);
        auto pastOrSameChapter = (placeInReadable->isFor(DocType::EPUB) &&
                                  readable_->findEPubHRefChapterIdx(chapter.href) <= chapterIdx) ||
                                 (placeInReadable->isFor(DocType::TXT) &&
                                  placeInReadable->getOffset() >= chapter.txtOffset);

        if (pastOrSameChapter) {
            placeInSummary->setOffset(summaryText.length());
        }

        if (futureChapter && !includeUnread) {
            break;
        }

        if (sameChapter && !includeUnread && chapter.partialChapterSummary != nullptr &&
            !chapter.partialChapterSummary->text.empty()) {
            summaryText += chapter.title;
            summaryText += StringFormat(" (%d%% read)\n\n", chapter.partialChapterSummary->readPct);
            summaryText += chapter.partialChapterSummary->text;
            summaryText += "\n\n";
        } else {
            std::string currentChapterId = opf_->getIdByHref(chapter.href);
            std::string currentSummaryPath = opf_->getHrefByID("summary-" + currentChapterId);
            pugi::xml_document &currentDoc = getXHTMLFile(currentSummaryPath);

            for (auto node : currentDoc.child("html").child("body").children()) {
                // If the node is p or other nodes with child nodes
                if (std::string(node.name()) == "p") {
                    for (auto child : node.children()) {
                        if (child.type() == pugi::node_pcdata) { // If child is plain character data
                            summaryText += child.value();
                        }
                        if (std::string(child.name()) == "br") {
                            summaryText += "\n";
                        }
                    }
                    summaryText += "\n\n";
                } else if (std::string(node.name()) == "h1") {
                    summaryText += node.text().get();
                    summaryText += "\n\n";
                } else if (std::string(node.name()) == "br") {
                    summaryText += "\n";
                } else {
                    summaryText += node.text().get();
                }
            }
        }
    }

    return summaryText;
}

auto EPubFile::hasDictionary(const std::string &chapterID) -> bool {
    std::string dictionaryID = "dictionary-" + chapterID;

    auto hasDirectChapterDictionary = opf_->exists(dictionaryID);
    if (hasDirectChapterDictionary) {
        return hasDirectChapterDictionary;
    }

    // Walk the chapters backwards in the spine to see if the chapter id matches
    auto dictionaryChapterHref = opf_->getHrefByID(dictionaryID);
    auto dictionaryChapterIdx = opf_->getReadableToCChapterIndex(dictionaryChapterHref);

    return dictionaryChapterIdx != -1;
}

auto EPubFile::getDictionaryContent(std::string &chapterID,
                                    const std::shared_ptr<EPubPlace> &placeInReadable,
                                    std::shared_ptr<TxtPlace> &placeInDictionary) -> SpiramString {

    if (!hasDictionary(chapterID)) {
        log_e("This chapter does not have any dictionary definitions.");
        return "This chapter does not have any dictionary definitions.";
    }

    // Load the dictionary xhtml content
    int fileOffset = placeInReadable->getOffset();
    auto chapterHref = opf_->getHrefByID(chapterID);
    auto chapterIdx = readable_->findEPubHRefChapterIdx(chapterHref);
    if (chapterIdx == -1) {
        chapterIdx = opf_->getReadableToCChapterIndex(chapterHref);
        chapterHref = getToc()[chapterIdx].href;
        chapterID = opf_->getIdByHref(chapterHref);
    }

    // Convert chapterID to dictionary file path
    std::string dictionaryID = "dictionary-" + chapterID;
    std::string dictionaryPath = opf_->getHrefByID(dictionaryID);
    // int currentOffset = findCharOffsetAtFileOffset(chapterHref, fileOffset);
    int currentOffset = fileOffset;
    pugi::xml_document &doc = getXHTMLFile(dictionaryPath);
    pugi::xml_node closestNode;
    pugi::xml_node prevNode;
    int closestLocation = -1;

    log_d("Loading dictionary for chapter: %s. (href: %s, chapter idx: %d) Current offset: %d",
          chapterID.c_str(), chapterHref.c_str(), chapterIdx, currentOffset);
    // Search for the closest data-locations attribute closest to currentOffset without going over
    for (auto p : doc.child("html").child("body").children("p")) {
        int location = p.attribute("data-locations").as_int();
        log_d("Found dictionary location: %d", location);

        // If currentOffset is 0, set the closestNode as the first node
        if (currentOffset == 0 || closestNode == nullptr) {
            closestNode = p;
            closestLocation = location;
        }

        if (location <= currentOffset) {
            // Still finding nodes behind the user's placemark
            closestLocation = location;
            prevNode = closestNode;
            closestNode = p;
        } else {
            // If we've gone over and the location is greater than the offset, we're good with
            // what's left. Clearing prevNode means we haven't exhausted all definitions and will
            // still have some to show.
            prevNode = pugi::xml_node();
            break;
        }
    }

    // If closestNode refers to the last entry and there was no break, backtrack one entry so we
    // always show one dictionary definition
    if (prevNode) {
        closestNode = prevNode;
        closestLocation = closestNode.attribute("data-locations").as_int();
    }

    SpiramString dictionaryText;
    SpiramString beforeClosestNodeText;
    if (closestLocation != -1) {
        // Construct the text with the XML before the closest node
        for (auto p : doc.child("html").child("body").children("p")) {
            if (p == closestNode) {
                if (closestLocation < currentOffset) {
                    beforeClosestNodeText += StringFormat("%s\n\n", p.child_value());
                } else {
                    dictionaryText += StringFormat("%s\n\n", p.child_value());
                }
                break;
            }
            beforeClosestNodeText += StringFormat("%s\n\n", p.child_value());
        }

        // Construct the text with the XML after the closest node
        for (auto p = closestNode.next_sibling(); p; p = p.next_sibling()) {
            dictionaryText += StringFormat("%s\n\n", p.child_value());
        }

        // Set the place in the dictionary based on the length of the text before the closest node
        uint32_t charOffset = beforeClosestNodeText.length();
        placeInDictionary = std::dynamic_pointer_cast<TxtPlace>(Place::factory(charOffset));

        // Concatenate to form the full dictionaryText
        dictionaryText = beforeClosestNodeText + dictionaryText;

        log_i("Found closest node at char offset: %" PRIu32 " for location: %d/%d", charOffset,
              closestLocation, currentOffset);
    } else {
        log_e("Location not found in dictionaryString chapter: %s : %d", chapterID.c_str(),
              currentOffset);
    }

    return dictionaryText;
}

auto EPubFile::open() -> bool {

    PROFILE_START(openEPub);

    EPUB_PROFILE_START(UnzipperOpen);
    if (!epubUnzipper_.open()) {
        log_e("Failed to open ePub file");
        return false;
    }
    EPUB_PROFILE_END(UnzipperOpen);

    EPUB_PROFILE_START(EPubManifest);
    EPubMetaFile manifest("META-INF/container.xml");
    auto [manifestBuffer, manifestLength] = getFile("META-INF/container.xml");
    if (manifestBuffer == nullptr) {
        log_e("Failed to open ePub's manifest %s", "META-INF/container.xml");
        return false;
    } else {
        manifest.parse(manifestBuffer, manifestLength);
    }
    EPUB_PROFILE_END(EPubManifest);

    // for (auto &path : manifest.rootfilePaths) {
    //     log_i("Rootfile path: %s", path.c_str());
    // }

    if (manifest.rootfilePaths.size() == 0) {
        log_e("No rootfile paths found");
        return false;
    } else if (manifest.rootfilePaths.size() > 1) {
        log_w("Multiple rootfile paths found");
    }

    EPUB_PROFILE_START(EPubOpf)
    auto [opfBuffer, length] = getFile(manifest.rootfilePaths[0].c_str());
    if (opfBuffer == nullptr) {
        log_e("Failed to open ePub's opf %s", manifest.rootfilePaths[0].c_str());
        return false;
    } else {
        opf_ = std::make_shared<EPubOpf>(manifest.rootfilePaths[0].c_str());
        opf_->parse(opfBuffer, length, readable_);
    }
    EPUB_PROFILE_END(EPubOpf)

    PROFILE_END_LOG_MS(openEPub);

    // log_d("Total size of all ePub text is %d", opf_->getTotalSize());
    fileOpen_ = true;
    return true;
}

void EPubFile::close() {
    epubUnzipper_.close();
    fileOpen_ = false;
}
