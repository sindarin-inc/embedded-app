#include "EPubAdditions.hpp"

auto HasSummary(const std::shared_ptr<EPubFile> &epubFile, const std::string_view &spineHref)
    -> bool {
    return epubFile->hasEpubAddition(spineHref, "summary");
}

auto GetSummaryContent(
    const std::shared_ptr<EPubFile> &epubFile, const std::shared_ptr<EPubPlace> &placeInReadable,
    std::shared_ptr<TxtPlace> &placeInSummary,
    const std::shared_ptr<UserReadablePartialChapterSummary> &partialChapterSummary,
    bool includeUnread) -> SpiramString {

    auto placeHref = placeInReadable->getHRef();
    if (!HasSummary(epubFile, placeHref) && partialChapterSummary == nullptr) {
        log_e("This chapter does not have a summary.");
        return "This chapter does not have a summary.";
    }

    // Load the summary xhtml content
    auto chapterHref = epubFile->getChapterFirstHref(placeHref);
    auto chapterIdx = epubFile->getChapterIdx(chapterHref);
    if (chapterIdx == -1) {
        log_e("Unable to find chapter index for %s", chapterHref.c_str());
        return "This chapter does not have a summary.";
    }
    log_w("Chapter href: %s, chapter idx: %d, toc href: %s", chapterHref.c_str(), chapterIdx,
          std::string(epubFile->getToc()[chapterIdx].href).c_str());
    if (std::string(epubFile->getToc()[chapterIdx].href) != chapterHref) {
        chapterHref = std::string(epubFile->getToc()[chapterIdx].href);
    }

    std::string partialSummaryChapterHRef;
    if (partialChapterSummary != nullptr && !partialChapterSummary->text.empty() &&
        partialChapterSummary->text != "null") {
        auto summarySpineHref = partialChapterSummary->ePubPlace->getHRef();
        partialSummaryChapterHRef = epubFile->getChapterFirstHref(summarySpineHref);
    }

    log_d("Loading summary for chapter href: %s, chapter idx: %d", chapterHref.c_str(), chapterIdx);
    SpiramString summaryText;
    bool foundAnySummaries = false;
    for (const auto &chapter : epubFile->getToc()) {
        auto sameChapter = chapter.href == chapterHref;
        auto tocChapterIdx = epubFile->getChapterIdx(std::string(chapter.href));
        if (tocChapterIdx == -1) {
            log_e("Unable to find chapter index for %s", chapter.href.c_str());
            continue;
        }
        auto futureChapter = tocChapterIdx > chapterIdx;
        auto pastOrSameChapter = tocChapterIdx <= chapterIdx;

        if (pastOrSameChapter) {
            placeInSummary->setOffset(summaryText.length());
        }

        if (futureChapter && !includeUnread) {
            break;
        }

        if (sameChapter && !includeUnread && partialSummaryChapterHRef == chapter.href) {
            summaryText += "Summary: " + chapter.title;
            summaryText += StringFormat(" (%d%% read)\n\n", partialChapterSummary->readPct);
            summaryText += partialChapterSummary->text;
            summaryText += "\n\n";
            foundAnySummaries = true;
        } else {
            std::string currentChapterId =
                epubFile->getManifestIdByHref(static_cast<std::string>(chapter.href));
            auto currentSummaryPath = epubFile->getHrefByManifestId("summary-" + currentChapterId);
            if (currentSummaryPath.empty()) {
                log_w("No summary for %s", chapter.href.c_str());
                continue;
            }
            foundAnySummaries = true;
            pugi::xml_document &currentDoc = epubFile->getXHTMLFile(currentSummaryPath);

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

    if (!foundAnySummaries) {
        return "This chapter does not have a summary.";
    }

    return summaryText;
}

auto HasDictionary(const std::shared_ptr<EPubFile> &epubFile, const std::string_view &spineHref)
    -> bool {
    return epubFile->hasEpubAddition(spineHref, "dictionary");
}

auto GetDictionaryContent(const std::shared_ptr<EPubFile> &epubFile,
                          const std::shared_ptr<EPubPlace> &placeInReadable,
                          std::shared_ptr<TxtPlace> &placeInDictionary) -> SpiramString {

    auto spineHref = placeInReadable->getHRef();
    if (!HasDictionary(epubFile, spineHref)) {
        log_e("This chapter does not have any dictionary definitions.");
        return "This chapter does not have any dictionary definitions.";
    }

    // Load the dictionary xhtml content
    int fileOffset = placeInReadable->getOffset();
    auto chapterIdx = epubFile->getChapterIdx(spineHref);
    auto spineIdx = epubFile->getSpineIdx(spineHref);
    auto spineId = epubFile->getManifestIdByHref(spineHref);

    if (chapterIdx == -1 || spineIdx == -1) {
        log_e("Unable to find chapter index for %s", spineHref.c_str());
        return "This chapter does not have any dictionary definitions.";
    }

    auto previousSpineOffset = 0;
    if (epubFile->getToc()[chapterIdx].href != spineHref) {
        spineHref = epubFile->getToc()[chapterIdx].href;
        spineId = epubFile->getManifestIdByHref(spineHref);
        // If we're looking at a spine item that doesn't match the chapter, we need to calculate the
        // offset to match how the server is calculating word locations, which is cumulatively
        // adding the size of each spine item
        previousSpineOffset = epubFile->getOffsetPreviousSpineItemsInChapter(spineIdx);
        log_d("Calculated previousSpineOffset: %d", previousSpineOffset);
    }

    // Convert chapterId to dictionary file path
    std::string dictionaryId = "dictionary-" + spineId;
    auto dictionaryPath = epubFile->getHrefByManifestId(dictionaryId);
    // int currentOffset = findCharOffsetAtFileOffset(spineHref, fileOffset);
    int currentOffset = fileOffset + previousSpineOffset;
    pugi::xml_document &doc = epubFile->getXHTMLFile(dictionaryPath);
    pugi::xml_node closestNode;
    pugi::xml_node prevNode;
    int closestLocation = -1;

    log_d("Loading dictionary for chapter: %s. (href: %s, chapter idx: %d) Current offset: %d "
          "(file:%d+prev:%d)",
          spineId.c_str(), spineHref.c_str(), chapterIdx, currentOffset, fileOffset,
          previousSpineOffset);
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
        log_e("Location not found in dictionaryString chapter: %s : %d", spineId.c_str(),
              currentOffset);
    }

    return dictionaryText;
}