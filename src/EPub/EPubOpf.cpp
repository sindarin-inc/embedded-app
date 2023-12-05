#include "EPubOpf.hpp"

#include <cstring>
#include <limits>
#include <pugixml.hpp>

#include "EPubDefs.hpp"

void EPubOpf::parse(const std::shared_ptr<uint8_t[]> &buffer, uint32_t length,
                    const std::shared_ptr<Readable> &readable) {

    basePath_ = "";
    extractPath(path_, basePath_);
    // log_i("Base Path: %s", basePath_.c_str());

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer_inplace(buffer.get(), length);

    if (result) {

        pugi::xml_node package = doc.child("package");
        pugi::xml_node manifest = package.child("manifest");
        pugi::xml_node metadata = package.child("metadata");
        pugi::xml_node spine = package.child("spine");

        title_ = metadata.child("dc:title").value();
        creator_ = metadata.child("dc:creator").value();

        for (auto item : manifest.children("item")) {
            manifest_[item.attribute("id").value()] =
                ManifestItem{.href = item.attribute("href").value(),
                             .mediaType = item.attribute("media-type").value()};
        }

        for (auto itemref : spine.children("itemref")) {
            std::string idref = itemref.attribute("idref").value();

            auto it = manifest_.find(static_cast<SpiramString>(idref));
            if (it != manifest_.end()) {
                spine_.push_back(SpineItem{.item = &it->second});
            } else {
                log_e("Spine idref not found in manifest: %s", idref.c_str());
            }
        }
    } else {
        log_e("Failed to parse EPub opf %s: %s", path_, result.description());
    }
}

void EPubOpf::loadSpine(const ReadableTableOfContentsChapterVector &toc) {
    int lastChapterIdx = -1;
    SpineVector spineWithChapters;

    for (auto spineItem : spine_) {
        auto chapterIdx = findChapterIdxInToc(toc, spineItem);
        bool chapterStart = false;
        if (chapterIdx != -1) {
            lastChapterIdx = chapterIdx;
            chapterStart = true;
        }
        // log_w("Spine item %s chapter %d chapterStart %d", spineItem.item->href.c_str(),
        // chapterIdx,
        //       chapterStart);
        spineWithChapters.push_back(SpineItem{
            .item = spineItem.item, .chapterIdx = lastChapterIdx, .chapterStart = chapterStart});
    }

    spine_ = spineWithChapters;
}

auto EPubOpf::findChapterIdxInToc(const ReadableTableOfContentsChapterVector &toc,
                                  SpineItem &spineItem) -> ChapterIdx {
    ChapterIdx idx = 0;
    // Walk the spine and compare the href to the toc. If the href isn't found in the toc, walk the
    // spine backwards until the href is found
    for (auto &chapter : toc) {
        // log_w("%d: Comparing %s to %s", idx, chapter.href.c_str(), spineItem.item->href.c_str());
        if (chapter.href == spineItem.item->href) {
            return idx;
        }
        idx++;
    }
    return -1;
}

void EPubOpf::extractPath(const char *fname, std::string &path) {
    path.clear();
    int i = strlen(fname) - 1;
    while ((i > 0) && (fname[i] != '/')) {
        i--;
    }
    if (i > 0) {
        path.assign(fname, ++i);
    }
}

auto EPubOpf::getSpineIdx(const std::string &href) const -> SpineIdx {
    for (SpineIdx spineIdx = 0;
         spineIdx <
         std::min(spine_.size(), static_cast<size_t>(std::numeric_limits<SpineIdx>::max()));
         spineIdx++) {
        if (spine_[spineIdx].item->href == href) {
            return spineIdx;
        }
    }
    return -1;
}

auto EPubOpf::getChapterIdx(const std::string &href) const -> ChapterIdx {
    SpineIdx spineIdx = getSpineIdx(href);
    if (spineIdx == -1) {
        return -1;
    }
    return spine_[spineIdx].chapterIdx;
}

auto EPubOpf::getChapterIdx(SpineIdx spineIdx) const -> ChapterIdx {
    return spine_[spineIdx].chapterIdx;
}

auto EPubOpf::spineIdxIsChapterStart(SpineIdx spineIdx) const -> bool {
    return spine_[spineIdx].chapterStart;
}

auto EPubOpf::getChapterFirstSpineIdx(SpineIdx spineIdx) const -> SpineIdx {
    SpineIdx chapterSpineIdx = spineIdx;
    while (!spine_[chapterSpineIdx].chapterStart && (chapterSpineIdx > 0)) {
        chapterSpineIdx--;
    }

    return chapterSpineIdx;
}

auto EPubOpf::getChapterFirstHref(const std::string &spineHRef) const -> std::string {
    auto spineIdx = getSpineIdx(spineHRef);
    auto chapterSpineIdx = getChapterFirstSpineIdx(spineIdx);
    return static_cast<std::string>(getSpine(chapterSpineIdx).item->href);
}

auto EPubOpf::exists(const std::string &id) const -> bool {
    log_d("Checking if %s exists in manifest: %d", id.c_str(),
          manifest_.find(static_cast<SpiramString>(id)) != manifest_.end());
    return manifest_.find(static_cast<SpiramString>(id)) != manifest_.end();
}

auto EPubOpf::getHrefByManifestId(const std::string &manifestId) const -> std::string {
    auto it = manifest_.find(static_cast<SpiramString>(manifestId));
    if (it != manifest_.end()) {
        return static_cast<std::string>(it->second.href);
    }
    return "";
}

auto EPubOpf::getManifestIdByHref(const std::string &href) const -> std::string {
    for (auto &item : manifest_) {
        if (item.second.href == href) {
            return static_cast<std::string>(item.first);
        }
    }
    return "";
}
