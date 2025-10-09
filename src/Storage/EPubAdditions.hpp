#include <EPubFile/EPubFile.hpp>
#include <Renderer/Place.hpp>
#include <Spiram/String.hpp>
#include <StringUtil/StringUtil.hpp>
#include <string>

#include "Models/UserReadablePartialChapterSummary.hpp"

auto HasSummary(const std::shared_ptr<EPubFile> &epubFile, const std::string_view &spineHref)
    -> bool;

auto GetSummaryContent(
    const std::shared_ptr<EPubFile> &epubFile, const std::shared_ptr<EPubPlace> &placeInReadable,
    std::shared_ptr<TxtPlace> &placeInSummary,
    const std::shared_ptr<UserReadablePartialChapterSummary> &partialChapterSummary,
    bool includeUnread) -> SpiramString;

auto HasDictionary(const std::shared_ptr<EPubFile> &epubFile, const std::string_view &spineHref)
    -> bool;

auto GetDictionaryContent(const std::shared_ptr<EPubFile> &epubFile,
                          const std::shared_ptr<EPubPlace> &placeInReadable,
                          std::shared_ptr<TxtPlace> &placeInDictionary) -> SpiramString;
