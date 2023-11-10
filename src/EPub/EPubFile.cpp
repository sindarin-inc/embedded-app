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

auto EPubFile::calculateFileSize(const std::string &href) -> uint32_t {
    auto filePath = opf_->getFullPath(href);
    auto [buffer, size] = getFile(filePath);

    return size;
}

auto EPubFile::getChapterIdx(const std::shared_ptr<EPubPlace> &ePubPlace) const -> ChapterIdx {
    return opf_->getChapterIdx(ePubPlace->getHRef());
}

auto EPubFile::getChapterIdx(const std::string &href) const -> ChapterIdx {
    return opf_->getChapterIdx(href);
}

auto EPubFile::getChapterIdx(SpineIdx spineIdx) const -> ChapterIdx {
    return opf_->getChapterIdx(spineIdx);
}

auto EPubFile::spineIdxIsChapterStart(SpineIdx spineIdx) const -> bool {
    return opf_->spineIdxIsChapterStart(spineIdx);
}

auto EPubFile::getChapterFirstSpineIdx(SpineIdx spineIdx) const -> SpineIdx {
    return opf_->getChapterFirstSpineIdx(spineIdx);
}

auto EPubFile::hasToc() -> bool {
    std::string tocId = "nav";
    return opf_->exists(tocId);
}

auto EPubFile::getToc() -> ReadableTableOfContentsChapterVector {
    if (toc_) {
        return *toc_;
    }

    std::string tocId = "nav";
    std::string tocPath = opf_->getHrefByManifestId(tocId);
    pugi::xml_document &tocDoc = getXHTMLFile(tocPath);

    ReadableTableOfContentsChapterVector toc;
    uint32_t cumulativeOffset = 0;

    std::function<void(pugi::xml_node, ReadableTableOfContentsChapterVector &)> processNavPoints;
    processNavPoints = [&](pugi::xml_node navPoint, ReadableTableOfContentsChapterVector &toc) {
        for (auto navPointChild : navPoint.children("li")) {
            ReadableTableOfContentsChapter chapter;
            chapter.title = navPointChild.child("a").child_value();
            chapter.href = navPointChild.child("a").attribute("href").value();
            pugi::xml_attribute epubOffsetAttr =
                navPointChild.child("a").attribute("data-epub-offset");
            if (epubOffsetAttr) {
                chapter.epubOffset =
                    static_cast<uint32_t>(strtoul(epubOffsetAttr.value(), nullptr, 10));
            } else {
                chapter.epubOffset = cumulativeOffset;
            }
            pugi::xml_attribute endEpubOffsetAttr =
                navPointChild.child("a").attribute("data-end-epub-offset");
            if (endEpubOffsetAttr) {
                chapter.endEpubOffset =
                    static_cast<uint32_t>(strtoul(endEpubOffsetAttr.value(), nullptr, 10));
            } else {
                auto fileSize = calculateFileSize(std::string(chapter.href));
                cumulativeOffset += fileSize;
                chapter.endEpubOffset = cumulativeOffset;
            }

            // log_w("Chapter %s: %s (%" PRIu32 " - %" PRIu32 ")", chapter.title.c_str(),
            //       chapter.href.c_str(), chapter.epubOffset, chapter.endEpubOffset);
            toc.push_back(chapter);

            // Recurse into child <ol> elements, if any
            for (auto nestedList : navPointChild.children("ol")) {
                processNavPoints(nestedList, toc);
            }
        }
    };

    // Process the top-level <ol> elements in the TOC
    for (auto navPoint : tocDoc.child("html").child("body").child("nav").children("ol")) {
        processNavPoints(navPoint, toc);
    }

    toc_ = std::make_shared<ReadableTableOfContentsChapterVector>(toc);
    return toc;
}

auto EPubFile::hasSummary(const std::string &chapterId) -> bool {
    std::string summaryId = "summary-" + chapterId;

    auto hasDirectChapterSummary = opf_->exists(summaryId);
    auto hasToc = opf_->exists("nav");
    if (hasDirectChapterSummary && hasToc) {
        return true;
    }

    // Walk the chapters backwards in the spine to see if the chapter id matches
    auto summaryChapterHref = opf_->getHrefByManifestId(summaryId);
    auto summaryChapterIdx = opf_->getChapterIdx(summaryChapterHref);

    return summaryChapterIdx != -1 && hasToc;
}

auto EPubFile::getSummaryContent(
    std::string &chapterId, const std::shared_ptr<EPubPlace> &placeInReadable,
    std::shared_ptr<TxtPlace> &placeInSummary,
    const std::shared_ptr<UserReadablePartialChapterSummary> &partialChapterSummary,
    bool includeUnread) -> SpiramString {

    if (!hasSummary(chapterId)) {
        log_e("This chapter does not have a summary.");
        return "This chapter does not have a summary.";
    }

    // Load the summary xhtml content
    auto chapterHref = opf_->getHrefByManifestId(chapterId);
    auto chapterIdx = opf_->getChapterIdx(chapterHref);
    log_w("Chapter href: %s, chapter idx: %d, toc href: %s", chapterHref.c_str(), chapterIdx,
          std::string(getToc()[chapterIdx].href).c_str());
    if (std::string(getToc()[chapterIdx].href) != chapterHref) {
        chapterHref = std::string(getToc()[chapterIdx].href);
        chapterId = opf_->getManifestIdByHref(chapterHref);
    }

    log_d("Loading summary for chapter: %s (href: %s, chapter idx: %d)", chapterId.c_str(),
          chapterHref.c_str(), chapterIdx);
    SpiramString summaryText;

    for (const auto &chapter : getToc()) {
        auto sameChapter = chapter.href == chapterHref;
        auto futureChapter = opf_->getChapterIdx(std::string(chapter.href)) > chapterIdx;
        auto pastOrSameChapter = opf_->getChapterIdx(std::string(chapter.href)) <= chapterIdx;

        if (pastOrSameChapter) {
            placeInSummary->setOffset(summaryText.length());
        }

        if (futureChapter && !includeUnread) {
            break;
        }

        if (sameChapter && !includeUnread && partialChapterSummary != nullptr &&
            !partialChapterSummary->text.empty() &&
            partialChapterSummary->ePubPlace->getHRef() == chapter.href) {
            summaryText += chapter.title;
            summaryText += StringFormat(" (%d%% read)\n\n", partialChapterSummary->readPct);
            summaryText += partialChapterSummary->text;
            summaryText += "\n\n";
        } else {
            std::string currentChapterId = opf_->getManifestIdByHref(std::string(chapter.href));
            std::string currentSummaryPath =
                opf_->getHrefByManifestId("summary-" + currentChapterId);
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

auto EPubFile::hasDictionary(const std::string &chapterId) -> bool {
    std::string dictionaryId = "dictionary-" + chapterId;

    auto hasDirectChapterDictionary = opf_->exists(dictionaryId);
    auto hasToc = opf_->exists("nav");
    if (hasDirectChapterDictionary && hasToc) {
        return true;
    }

    // Walk the chapters backwards in the spine to see if the chapter id matches
    auto dictionaryChapterHref = opf_->getHrefByManifestId(dictionaryId);
    auto dictionaryChapterIdx = opf_->getChapterIdx(dictionaryChapterHref);

    return dictionaryChapterIdx != -1 && hasToc;
}

auto EPubFile::getDictionaryContent(std::string &chapterId,
                                    const std::shared_ptr<EPubPlace> &placeInReadable,
                                    std::shared_ptr<TxtPlace> &placeInDictionary) -> SpiramString {

    if (!hasDictionary(chapterId)) {
        log_e("This chapter does not have any dictionary definitions.");
        return "This chapter does not have any dictionary definitions.";
    }

    // Load the dictionary xhtml content
    int fileOffset = placeInReadable->getOffset();
    auto chapterHref = opf_->getHrefByManifestId(chapterId);
    auto chapterIdx = opf_->getChapterIdx(chapterHref);
    if (std::string(getToc()[chapterIdx].href) != chapterHref) {
        chapterHref = std::string(getToc()[chapterIdx].href);
        chapterId = opf_->getManifestIdByHref(chapterHref);
    }

    // Convert chapterId to dictionary file path
    std::string dictionaryId = "dictionary-" + chapterId;
    std::string dictionaryPath = opf_->getHrefByManifestId(dictionaryId);
    // int currentOffset = findCharOffsetAtFileOffset(chapterHref, fileOffset);
    int currentOffset = fileOffset;
    pugi::xml_document &doc = getXHTMLFile(dictionaryPath);
    pugi::xml_node closestNode;
    pugi::xml_node prevNode;
    int closestLocation = -1;

    log_d("Loading dictionary for chapter: %s. (href: %s, chapter idx: %d) Current offset: %d",
          chapterId.c_str(), chapterHref.c_str(), chapterIdx, currentOffset);
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
        log_e("Location not found in dictionaryString chapter: %s : %d", chapterId.c_str(),
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
        auto toc = getToc();
        opf_->loadSpine(toc);
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
