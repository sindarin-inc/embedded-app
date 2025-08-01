#include <pugixml.hpp>

#include "EPub/EPubFile.hpp"
#include "Models/Place.hpp"
#include "UI/ReadableScreen.hpp"

// Retrieve text from the beginning of the current chapter up to the current reading position
std::string ReadableScreen::getCurrentChapterText() const {
    // Validate that our renderer has a stream attached
    auto stream = renderer_.getStream();
    if (!stream) {
        return getCurrentPageText();
    }

    // Currently we only support EPUB streams. Fallback to current page text otherwise.
    if (!stream->isFor(DocType::EPUB)) {
        return getCurrentPageText();
    }

    auto epub = std::dynamic_pointer_cast<EPubFile>(stream);
    if (!epub) {
        return getCurrentPageText();
    }

    // Current position in the book
    auto place = renderer_.getCurrentPlace();
    auto ePubPlace = std::dynamic_pointer_cast<EPubPlace>(place);
    if (!ePubPlace) {
        return getCurrentPageText();
    }

    // Determine the spine index of the current position and the start of the chapter
    auto streamPlace = renderer_.getStreamPlace(place);
    auto spineIdx = streamPlace.getSpineIdx();
    auto chapterStartSpineIdx = epub->getChapterFirstSpineIdx(spineIdx);

    std::string chapterText;

    // Iterate through each spine item from the start of the chapter up to and including the
    // current spine item. Convert each to plain text and append.
    for (int idx = chapterStartSpineIdx; idx <= spineIdx; ++idx) {
        const auto &spineItem = epub->getSpine(idx);
        std::string href = static_cast<std::string>(spineItem.item->href);

        // Load the XHTML document
        pugi::xml_document &doc = epub->getXHTMLFile(href);
        std::string fileText;
        auto body = doc.child("html").child("body");
        if (body) {
            fileText = body.text().get();
        }

        // If we're on the current spine item, truncate to the current reading offset
        if (idx == spineIdx) {
            uint32_t charOffset = epub->findCharOffsetAtFileOffset(href, ePubPlace->getOffset());
            if (charOffset < fileText.size()) {
                fileText = fileText.substr(0, charOffset);
            }
        }

        chapterText += fileText;
        if (idx != spineIdx) {
            chapterText += "\n"; // Separate spine items with a newline for readability
        }
    }

    return chapterText;
}
