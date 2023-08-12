#include "EPubTokenizer.hpp"

// clang-format off

// ToDo: Some cleanup required... there is more tags than required here

EPubTokenizer::Tags EPubTokenizer::tags
  = {{"p",           Tag::P}, {"div",               Tag::DIV}, {"span", Tag::SPAN}, {"br",   Tag::BREAK}, {"h1",                 Tag::H1},  
     {"h2",         Tag::H2}, {"h3",                 Tag::H3}, {"h4",     Tag::H4}, {"h5",      Tag::H5}, {"h6",                 Tag::H6}, 
     {"b",           Tag::B}, {"i",                   Tag::I}, {"em",     Tag::EM}, {"body",  Tag::BODY}, {"a",                   Tag::A},
     {"img",       Tag::IMG}, {"image",           Tag::IMAGE}, {"li",     Tag::LI}, {"pre",    Tag::PRE}, {"blockquote", Tag::BLOCKQUOTE},
     {"strong", Tag::STRONG}, {"sub",               Tag::SUB}, {"sup",   Tag::SUP}, {"none",  Tag::NONE}, {"*",                 Tag::ANY}, 
     {"@page",    Tag::PAGE}, {"@font-face",  Tag::FONT_FACE},
    };

// clang-format on

//
// This loads from the stream character by character and populates
// the token buffer based on the TOKEN enum.
// After this is done, then we can do the heavy lifting of walking
// through the tokens, checking size, and rendering the text on
// a line by line basis.
//
// We track backwards because we need to double the amount read in a backwards
// read (2 pages instead of 1)
//
// Computes the number of word in the page.
//
// Returns the computed streamPlace where the page start really.
//
auto EPubTokenizer::loadAndTokenizePage(StreamPlace fromPlace, Tokens &tokens, Pager &pager,
                                        bool backwards, StreamPlace *toPlace) -> StreamPlace {

    log_w("Opening file at streamPlace %s", fromPlace.str().c_str());

    const EPubOpf::SpineItem &spineItem = stream_->getSpine(fromPlace.getFileNumber());
    pugi::xml_document &doc = stream_->getXHTMLFile(spineItem.item->href);

    if (doc.child("html")) {
        pugi::xml_node *previousNode = nullptr;
        pugi::xml_node *theNode = nullptr;

        // Find the first xml_node that is containing the fromPlace offset
        for (pugi::xml_node &node : doc.child("html").child("body").children()) {

            theNode = &node;
            log_w("Node: %s (%zu)", node.name(), stream_->getOffset(node.name()));

            if (stream_->getOffset(node.name()) > fromPlace.getOffset()) {
                break;
            }

            Tags::iterator tagIt = tags.find(node.name());
            if (tagIt != tags.end()) {
                const Tag &tag = tagIt->second;
            }

            previousNode = &node;
        }

        if (previousNode != nullptr) {
            theNode = previousNode;
        }

        // Start the page construction task

        if (theNode != nullptr) {
        }

    } else {
        log_e("File is of the wrong format");
    }

    return fromPlace;
}
