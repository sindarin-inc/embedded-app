#include "LocalStore.hpp"

#include <algorithm>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <limits>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>

#include "Communications/HtmlCleaner.hpp"
#include "Communications/WiFi.hpp"
#include "GitVersion.hpp"
#include "Misc/Crypto.hpp"
#include "Misc/Defer.hpp"
#include "Misc/GUnzipStream.hpp"
#include "Misc/Platform.hpp"
#include "Misc/PowerManager.hpp"
#include "Misc/SemVer.hpp"
#include "Misc/StringUtil.hpp"
#include "Models/Json.hpp"
#include "Models/Place.hpp"
#include "Models/ReadableBookmark.hpp"
#include "Renderers/RendererStream.hpp"
#include "Version.hpp"
#include "sindarin-debug.h"

#define BOOKS_PATH "/readables.json"
#define ARTICLES_PATH "/articles.json"

enum { NUMBER_OF_READABLE_DOWNLOAD_TRIES = 3 };
enum { MAX_NUMBER_OF_PAGES_TO_BACKFILL = 10 };

auto LocalStore::begin() -> bool {
    auto loadedBooks = loadReadablesFromFile(books_, BOOKS_PATH);
    auto loadedArticles = loadReadablesFromFile(articles_, ARTICLES_PATH);

    migrateIfNeeded();

    return loadedBooks && loadedArticles;
}

auto LocalStore::loadReadablesFromFile(ReadablesVector &readables, const std::string &path)
    -> bool {
    log_d("Loading %ss from %s", ReadableModelTypeToString(readablesModelType(readables)).c_str(),
          path.c_str());
    SFile readablesFile = fileSystem.open(path);
    if (!readablesFile || (int)readablesFile.size() == 0) {
        log_w("Failed to open %s", path.c_str());
        // Fail positively since it's ok to not have any when you first use the glasses
        return true;
    }

    // Grab a minimum amount of ram, even if the readables file is empty
    auto docSize = (int)readablesFile.size() * 2;
    auto doc = std::make_unique<SpiRamJsonDocument>(docSize);
    auto error = deserializeJson(*doc, readablesFile);
    readablesFile.close();

    if (error.code() != DeserializationError::Ok) {
        log_e("Failed to deserialize readables file: %s", error.c_str());
        return false;
    }

    auto readablesJson = (*doc)["readables"].as<JsonArray>();
    if (readablesJson.size() == 0) {
        log_w("No readables found in %s", path.c_str());
        return true;
    }
    readables.reserve(readablesJson.size());

    for (JsonVariant v : readablesJson) {
        std::shared_ptr<Readable> readable = SpiRamPtr<Readable>();

        if (!readable->fromJson(v.as<JsonObject>())) {
            log_e("Failed to parse Readable");
            continue;
        }
        auto path = readableFilePathForReading(readable);
        if (path.empty()) {
            log_w("Couldn't find readable with path %s", path.c_str());
            continue;
        }
        int fileSize = readableGetUncompressedSize(readable);

        if (fileSize >= 0 && readable->files.size() && fileSize != readable->file(path).size) {
            log_w("File size mismatch for %s (%d on disk vs %d in local store), ignoring",
                  path.c_str(), fileSize, readable->file(path).size);
            continue;
        }
        readables.push_back(readable);
    }

    log_d("Loaded %zu readables from %s", readables.size(), path.c_str());
    return true;
}

auto LocalStore::readablesModelType(const ReadablesVector &readables) -> ReadablesModelType {
    return &readables == &articles_ ? ReadablesModelType::Article : ReadablesModelType::Book;
}

auto LocalStore::isReadableUnchanged(const ReadablesVector &readables,
                                     const std::shared_ptr<Readable> &readable,
                                     bool ignoreFileSizeCheck) -> bool {
    for (const auto &r : readables) {
        auto path = readableFilePathForReading(r);
        if (r->uuid == readable->uuid && !path.empty()) {
            // First check if file is different
            int fileSize = readableGetUncompressedSize(readable);

            if (fileSize <= 0) {
                log_w("Couldn't get file size for %s", path.c_str());
                return false;
            }

            // If we have a txt file and an epub exists among the readable->files(), download it
            if (RendererStream::parseDocType(path.c_str()) == DocType::TXT) {
                for (const auto &file : readable->files) {
                    if (file.filetype == "epub") {
                        log_i("Found epub file for %s, downloading", path.c_str());
                        return false;
                    }
                }
            }

            // If we have a file object, check that the size matches
            if (r->files.size() > 0 && !ignoreFileSizeCheck) {
                if (fileSize >= 0 && fileSize != readable->file().size) {
                    log_w("File size mismatch for %s (%d on disk vs %d in local store), ignoring",
                          path.c_str(), fileSize, readable->file().size);
                    return false;
                } else {
                    return true;
                }
            }

            // Next check if the readable is updated more recently than local copy
            if (r->dateModified != readable->dateModified) {
                log_i("Found newer version of %s on server", path.c_str());
                return false;
            }
            return true;
        }
    }
    return false;
}

void LocalStore::addReadable(ReadablesVector &readables,
                             const std::shared_ptr<Readable> &readable) {
    // Check if we already have a readable with the same uuid, and if so, replace it in readables,
    // otherwise, add it to the end
    for (auto &it : readables) {
        if (it.get()->uuid == readable->uuid) {
            log_i("Replacing readable %s", readable->uuid.c_str());
            it = readable;
            return;
        }
    }

    // If not found, add to the end
    log_i("Adding readable %s", readable->uuid.c_str());
    readables.emplace_back(readable);
    return;
}

void LocalStore::removeReadable(ReadablesVector &readables,
                                const std::shared_ptr<Readable> &readable) {
    for (auto it = readables.begin(); it != readables.end(); ++it) {
        if (it->get()->uuid == readable->uuid) {
            readables.erase(it);
            return;
        }
    }
}

auto LocalStore::readablesSince(const ReadablesModelType modelType) -> std::string {
    switch (modelType) {
    case ReadablesModelType::Article:
        return prefs_.lastArticlesSince.get().value_or("");
    case ReadablesModelType::Book:
        return prefs_.lastBooksSince.get().value_or("");
    default:
        return "";
    }
}

void LocalStore::storedReadableUuids(const ReadablesVector &readables,
                                     StoredUuidsVector &storedUuids) {
    for (const auto &readable : readables) {
        if (readable->localOnly) {
            continue;
        }

        // If we have a txt file and an epub exists among the readable->files(), don't add it to
        // storedUuids so it gets downloaded
        auto path = readableFilePathForReading(readable);
        if (RendererStream::parseDocType(path.c_str()) == DocType::TXT) {
            auto foundEpub = false;
            for (const auto &file : readable->files) {
                if (file.filetype == "epub") {
                    log_i("Found epub file for %s, forcing download", path.c_str());
                    foundEpub = true;
                    break;
                }
            }
            if (foundEpub) {
                continue;
            }
        }

        storedUuids.emplace_back(readable->uuid);
    }
}

void LocalStore::changedReadablePlacemarkJson(const ReadablesVector &readables,
                                              ReadablePlacemarksVector &placemarks) {
    auto modelType = this->readablesModelType(const_cast<ReadablesVector &>(readables));

    for (const auto &readable : readables) {
        if (readable->localOnly) {
            continue;
        }

        bool changed = prefs_.readablePlaceChanged.get(readable->shortenedUuid.c_str());
        if (changed) {
            this->telemetry_.logMetric(TelemetryField::METRIC_SYNC_STATUS,
                                       StringFormat("sync-%ss-placemark-upload",
                                                    ReadableModelTypeToString(modelType).c_str())
                                           .c_str(),
                                       {{"uuid", readable->shortenedUuid.c_str()}});
            std::string place = prefs_.readablePlace.get(readable->shortenedUuid.c_str());
            log_i("Placemark on %s %s %s has changed to %s",
                  ReadableModelTypeToString(modelType).c_str(), readable->title.c_str(),
                  readable->uuid.c_str(), place.c_str());
            ReadablePlacemark placemark;
            placemark.uuid = readable->uuid;
            placemark.place = Place::factory(place);
            placemarks.emplace_back(placemark);
        }
    }
}

void LocalStore::changedReadablesArchivedJson(const ReadablesVector &readables,
                                              ReadableArchivesVector &archives) {
    auto modelType = this->readablesModelType(const_cast<ReadablesVector &>(readables));

    for (const auto &readable : readables) {
        if (readable->localOnly) {
            continue;
        }

        bool changed = prefs_.readableArchivedChanged.get(readable->shortenedUuid.c_str());
        if (changed) {
            this->telemetry_.logMetric(TelemetryField::METRIC_SYNC_STATUS,
                                       StringFormat("sync-%ss-archive-upload",
                                                    ReadableModelTypeToString(modelType).c_str())
                                           .c_str(),
                                       {{"uuid", readable->shortenedUuid.c_str()}});
            log_i("Archived %s %s %s", ReadableModelTypeToString(modelType).c_str(),
                  readable->title.c_str(), readable->uuid.c_str());
            ReadableArchive archive;
            archive.uuid = readable->uuid;
            archive.isArchived = true;
            archives.emplace_back(archive);
        }
    }
}

void LocalStore::changedReadablesBookmarksJson(ReadableBookmarksVector &bookmarks) {
    for (auto &readables : {books_, articles_}) {
        auto modelType = this->readablesModelType(const_cast<ReadablesVector &>(readables));
        for (const auto &readable : readables) {
            if (readable->localOnly) {
                continue;
            }

            bool changed = prefs_.readableBookmarksChanged.get(readable->shortenedUuid.c_str());
            if (!changed) {
                continue;
            }

            log_i("Bookmarks changed on %s %s %s", ReadableModelTypeToString(modelType).c_str(),
                  readable->title.c_str(), readable->uuid.c_str());
            this->telemetry_.logMetric(TelemetryField::METRIC_SYNC_STATUS,
                                       StringFormat("sync-%ss-bookmark-upload",
                                                    ReadableModelTypeToString(modelType).c_str())
                                           .c_str(),
                                       {{"uuid", readable->shortenedUuid.c_str()}});
            std::string bookmarkPath = readable->bookmarkFilePath();
            SFile bookmarkFile = fileSystem.open(bookmarkPath.c_str(), FILE_READ, false);
            if (!bookmarkFile) {
                log_i("Failed to open bookmark file: %s", bookmarkPath.c_str());
                continue;
            }

            // Store bookmarks in a hash table
            std::unordered_map<std::string, std::string> bookmarksToAdd;
            std::unordered_map<std::string, std::string> bookmarksToDelete;
            LocalBookmark bookmark;
            while (ReadOneBookmark(bookmarkFile, bookmark)) {
                log_d("Bookmark: %s %s %d", bookmark.firstPlace->str().c_str(),
                      bookmark.lastPlace->str().c_str(), bookmark.status);

                if (bookmark.status == BOOKMARK_ON &&
                    (bookmark.firstPlace->getOffset() < bookmark.lastPlace->getOffset())) {
                    bookmarksToAdd[bookmark.firstPlace->str()] = bookmark.lastPlace->str();
                    bookmarksToDelete.erase(bookmark.firstPlace->str());
                } else if (bookmark.status == BOOKMARK_OFF) {
                    bookmarksToAdd.erase(bookmark.firstPlace->str());
                    bookmarksToDelete[bookmark.firstPlace->str()] = bookmark.lastPlace->str();
                }
            }
            bookmarkFile.close();

            // Add bookmarks to the JSON array
            for (const auto &[startingPosition, endingPosition] : bookmarksToAdd) {
                ReadableBookmark bookmark;
                bookmark.readable = readable->uuid;
                auto placeStart = Place::factory(startingPosition);
                auto placeEnd = Place::factory(endingPosition);
                if (placeStart->isFor(DocType::TXT)) {
                    bookmark.startingPosition = placeStart->getOffset();
                    bookmark.endingPosition = placeEnd->getOffset();
                } else if (placeStart->isFor(DocType::EPUB)) {
                    auto ePubPlace = std::dynamic_pointer_cast<EPubPlace>(placeStart);
                    bookmark.chapter = ePubPlace->getHRef();
                    bookmark.chapterOffset = ePubPlace->getOffset();
                }
                bookmarks.emplace_back(bookmark);
            }

            // Add bookmarks to delete to the JSON array
            for (const auto &[startingPosition, endingPosition] : bookmarksToDelete) {
                ReadableBookmark bookmark;
                bookmark.readable = readable->uuid;
                auto placeStart = Place::factory(startingPosition);
                auto placeEnd = Place::factory(endingPosition);
                if (placeStart->isFor(DocType::TXT)) {
                    bookmark.startingPosition = placeStart->getOffset();
                    bookmark.endingPosition = placeEnd->getOffset();
                } else if (placeStart->isFor(DocType::EPUB)) {
                    auto ePubPlace = std::dynamic_pointer_cast<EPubPlace>(placeStart);
                    bookmark.chapter = ePubPlace->getHRef();
                    bookmark.chapterOffset = ePubPlace->getOffset();
                }
                bookmark.willDelete = true;
                bookmarks.emplace_back(bookmark);
            }
        }
    }
}

auto LocalStore::processReadablePlacemarksFromServer(const std::shared_ptr<Readable> &readable,
                                                     const std::shared_ptr<TxtPlace> &txtPlace,
                                                     const std::shared_ptr<EPubPlace> &ePubPlace)
    -> bool {
    auto path = readableFilePathForReading(readable);
    auto docType = RendererStream::parseDocType(path.c_str());
    auto prefsPlaceString = prefs_.readablePlace.get(readable->shortenedUuid.c_str());

    if (docType == DocType::TXT) {
        // If we have a txt file (and not an epub file), and the server place is non-zero and
        // different, then save the txt place
        if (txtPlace != nullptr && !txtPlace->isZero() && prefsPlaceString != txtPlace->str()) {
            log_i("Updating TXT placemark for \"%s\" from device value of `%s` to server value of "
                  "`%s`",
                  readable->title.c_str(), prefsPlaceString.c_str(), txtPlace->str().c_str());
            prefs_.readablePlace.set(readable->shortenedUuid.c_str(), txtPlace->str());
            return true;
        }
    } else if (docType == DocType::EPUB || docType == DocType::NONE) {
        // If we have an epub file (or no file at all), and the server place is non-zero and
        // different, then save the epub place
        if (ePubPlace != nullptr && !ePubPlace->isZero() && prefsPlaceString != ePubPlace->str()) {
            log_i("Updating EPUB placemark for \"%s\" from device value of `%s` to server value of "
                  "`%s`",
                  readable->title.c_str(), prefsPlaceString.c_str(), ePubPlace->str().c_str());
            prefs_.readablePlace.set(readable->shortenedUuid.c_str(), ePubPlace->str());
            return true;
        }
    }
    // Otherwise do nothing
    auto placeStr = ePubPlace == nullptr ? "null" : ePubPlace->str();
    log_d("Not updating placemark for \"%s\" (path: %s) from device value of `%s` to server "
          "value of `%s`",
          readable->title.c_str(), path.c_str(), prefsPlaceString.c_str(),
          ePubPlace->str().c_str());
    return false;
}

auto LocalStore::processReadablePartialChapterSummary(
    const std::shared_ptr<Readable> &readable,
    const std::shared_ptr<UserReadablePartialChapterSummary> &partialChapterSummary) -> bool {

    // TODO: Delete any existing partial summary?
    if (partialChapterSummary == nullptr) {
        return false;
    }

    auto path = readableFilePathForWriting(readable);
    auto docType = RendererStream::parseDocType(path.c_str());
    // Only supported for epub
    if (docType != DocType::EPUB) {
        return false;
    }

    auto existingPartialSummary =
        UserReadablePartialChapterSummary::loadForReadable(fileSystem, readable);
    // Check if the existing summary is for the same place. If so, don't overwrite it if we dont
    // need to.
    if (existingPartialSummary != nullptr && existingPartialSummary == partialChapterSummary) {
        return false;
    }

    // Otherwise write the updated partial summary to the json file
    auto doc = std::make_unique<SpiRamJsonDocument>(
        std::min(2048, (int)partialChapterSummary->text.length() * 2));
    bool serialized = convertToJson(*partialChapterSummary, *doc);
    if (!serialized) {
        log_e("Failed to serialize partial summary");
        return false;
    }

    if (!fileSystem.createDir("/summary")) {
        log_e("could not create /summary directory");
        return false;
    }

    auto partialSummaryPath = readable->partialSummaryFilePath();
    fileSystem.remove(partialSummaryPath.c_str());
    auto file = fileSystem.open(partialSummaryPath.c_str(), FILE_WRITE, false);
    if (!file) {
        log_e("Failed to open partial summary file for writing");
        return false;
    }
    defer { file.close(); };

    // Write the partial summary to the json file
    auto len = serializeJson(*doc, file);
    if (len == 0) {
        log_e("Failed to write partial summary to json file");
        return false;
    }

    return true;
}

void LocalStore::resetReadablesPlacemarkChanged(const ReadablesVector &readables) {
    auto modelType = this->readablesModelType(const_cast<ReadablesVector &>(readables));

    for (const auto &readable : readables) {
        bool changed = prefs_.readablePlaceChanged.get(readable->shortenedUuid.c_str());
        if (!readable->localOnly && changed) {
            this->telemetry_.logMetric(TelemetryField::METRIC_SYNC_STATUS,
                                       StringFormat("sync-%ss-placemark-synced",
                                                    ReadableModelTypeToString(modelType).c_str())
                                           .c_str(),
                                       {{"uuid", readable->shortenedUuid.c_str()}});
            log_d("Resetting placemark changed for \"%s\" (now synced to server)",
                  readable->title.c_str());
            prefs_.readablePlaceChanged.remove(readable->shortenedUuid.c_str());
        }
    }
}

void LocalStore::resetReadablesArchivedChanged(const ReadablesVector &readables) {
    auto modelType = this->readablesModelType(const_cast<ReadablesVector &>(readables));

    for (const auto &readable : readables) {
        bool changed = prefs_.readableArchivedChanged.get(readable->shortenedUuid.c_str());
        if (!readable->localOnly && changed) {
            this->telemetry_.logMetric(TelemetryField::METRIC_SYNC_STATUS,
                                       StringFormat("sync-%ss-archive-synced",
                                                    ReadableModelTypeToString(modelType).c_str())
                                           .c_str(),
                                       {{"uuid", readable->shortenedUuid.c_str()}});
            log_d("Resetting article archive changed for \"%s\" (now synced to server)",
                  readable->title.c_str());
            prefs_.readableArchivedChanged.remove(readable->shortenedUuid.c_str());
        }
    }
}

void LocalStore::resetReadablesBookmarksChanged(const ReadablesVector &readables) {
    auto modelType = this->readablesModelType(const_cast<ReadablesVector &>(readables));

    for (const auto &readable : readables) {
        bool changed = prefs_.readableBookmarksChanged.get(readable->shortenedUuid.c_str());
        if (!readable->localOnly && changed) {
            this->telemetry_.logMetric(TelemetryField::METRIC_SYNC_STATUS,
                                       StringFormat("sync-%ss-bookmark-synced",
                                                    ReadableModelTypeToString(modelType).c_str())
                                           .c_str(),
                                       {{"uuid", readable->shortenedUuid.c_str()}});
            log_d("Resetting bookmarks changed for \"%s\" (now synced to server)",
                  readable->title.c_str());
            prefs_.readableBookmarksChanged.remove(readable->shortenedUuid.c_str());
        }
    }
}

void LocalStore::syncFromRemote(
    SyncReason syncReason, const std::function<void(ContentSyncState state)> &progressCallback) {
    if (!WiFi::getInstance().hasWiFiCredentials()) {
        this->telemetry_.logMetric(TelemetryField::METRIC_WIFI, "sync-wifi-no-credentials");
        progressCallback(
            ContentSyncState{.state = ContentSyncState::State::Failure,
                             .message = "Go to Settings > WiFi Setup in the Sol app."});
        return;
    }

    if (!WiFi::getInstance().connectWiFiFromSettings()) {
        this->telemetry_.logMetric(TelemetryField::METRIC_WIFI, "sync-wifi-failed");
        progressCallback(ContentSyncState{.state = ContentSyncState::State::Failure,
                                          .message = "Failed to connect to WiFi."});
        return;
    }

    // If we don't have a clock that's accurate, sync it
    if (SolClock::getInstance().getEpochTime() < 1000000000) {
        log_i("Clock not set, syncing from internet");
        // Sync the clock
        this->telemetry_.logMetric(TelemetryField::METRIC_CLOCK_SET, "pre-sync",
                                   {{"run-time", GetRunTimeMs()}});
        auto success = SolClock::getInstance().setClockTimeFromInternet();
        if (success) {
            this->telemetry_.logMetric(TelemetryField::METRIC_CLOCK_SET, "success",
                                       {{"run-time", GetRunTimeMs()}});
        } else {
            this->telemetry_.logMetric(TelemetryField::METRIC_CLOCK_SET, "failure",
                                       {{"run-time", GetRunTimeMs()}});
        }
    }

    // Uncomment below line to set your access token for local development
    // prefs._cloudToken.set("");

    // Grab the token here from prefs if set
    auto token = prefs_.cloudToken.get();
    if (!token.has_value()) {
        log_e("No access token set");
        // TODO: Not great separation-of-concerns to have user-facing strings here for typical
        // failure modes
        progressCallback(
            ContentSyncState{.state = ContentSyncState::State::Failure,
                             .message = "Go to Settings > Pair Glasses in the Sol app."});
        return;
    }

    // Uncomment below line to steal your access token for local development
    // log_w("Access token: %s", token.value().c_str());

    SindarinAPI::getInstance()->setAccessToken(token.value());

    progressCallback(ContentSyncState{.state = ContentSyncState::State::UploadTelemetry});

    // TODO: Thinking more about this, we should probably trust the caller of `syncFromRemote` to
    // call from whatever queue is most fitting based on the context. In tests, this might not be in
    // a queue.
    auto queue = DispatchGetCore0BackgroundQueue();
    queue->dispatch([this] {
        auto httpCode =
            this->telemetry_.uploadAndResetTelemetry(this->fileSystem, this->telemetry_);
        if (httpCode != 201) {
            this->telemetry_.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-telemetry-failed",
                                       {{"http-code", httpCode}});
        } else {
            this->telemetry_.logMetric(TelemetryField::METRIC_SYNC_STATUS,
                                       "sync-telemetry-success");
        }
    });

    // Run this work on a separate thread, since it's long running
    queue->dispatch([this, syncReason, progressCallback] {
        CheckSyncRequest checkSyncRequest;
        CheckSyncResponse checkSyncResponse = {};
        int checkSyncFailures = 0;
        bool booksAlreadySynced = false;
        bool articlesAlreadySynced = false;
        bool booksNowSynced = false;
        bool articlesNowSynced = false;

        progressCallback(ContentSyncState{.state = ContentSyncState::State::CheckingSync});

        std::vector<ReadablesVector *> readablesList = {&books_, &articles_};
        for (auto &readables : readablesList) {
            bool failedDownload = false;
            auto modelType = readablesModelType(const_cast<ReadablesVector &>(*readables));
            auto since = this->readablesSince(modelType);
            StoredUuidsVector storedUuids;
            ReadableArchivesVector archives;
            ReadableBookmarksVector bookmarks;
            ReadablePlacemarksVector placemarks;

            this->storedReadableUuids(*readables, storedUuids);
            this->changedReadablePlacemarkJson(*readables, placemarks);
            this->changedReadablesArchivedJson(*readables, archives);
            this->changedReadablesBookmarksJson(bookmarks);

            checkSyncRequest.syncReason = syncReason;
            checkSyncRequest.modelType = modelType;
            checkSyncRequest.since = since;
            checkSyncRequest.archives = archives;
            checkSyncRequest.placemarks = placemarks;
            checkSyncRequest.bookmarks = bookmarks;
            checkSyncRequest.storedUuids = storedUuids;
            checkSyncRequest.skipContent = true;
            checkSyncRequest.skipSummary = true;
            checkSyncRequest.skipToc = true;
            checkSyncRequest.ptBattery = PreferencesStore::lastPageTurnerBattery;
            checkSyncRequest.battery = (uint8_t)battery_.getBatteryPercentage();
            checkSyncRequest.ptBuild = this->prefs_.lastPageTurnerBuild.get().value_or("");
            checkSyncRequest.build = kVersionString;
            checkSyncRequest.gitHash = kGitHash;
            checkSyncRequest.sn = GetSerialNumber();

            auto nextPageCounter = 0;
            auto nextPageUrl = std::string();
            while (nextPageUrl.length() || nextPageCounter == 0) {
                if (nextPageCounter == 0) {
                    log_d("Fetching first page of %ss",
                          ReadableModelTypeToString(modelType).c_str());
                } else {
                    log_d("Fetching next page (#%d): %s", nextPageCounter + 1, nextPageUrl.c_str());
                }
                this->telemetry_.logMetric(
                    TelemetryField::METRIC_SYNC_STATUS,
                    StringFormat("sync-%ss-page", ReadableModelTypeToString(modelType).c_str())
                        .c_str(),
                    {{"page", nextPageCounter}});

                ContentSyncState syncState;
                if (modelType == ReadablesModelType::Book) {
                    progressCallback(
                        ContentSyncState{.state = ContentSyncState::State::SyncingBooks});
                    syncState = {.state = ContentSyncState::State::Syncing};
                } else if (modelType == ReadablesModelType::Article) {
                    progressCallback(
                        ContentSyncState{.state = ContentSyncState::State::SyncingArticles});
                    syncState = {.state = ContentSyncState::State::Syncing};
                }

                nextPageCounter += 1;
                if (nextPageCounter > MAX_NUMBER_OF_PAGES_TO_BACKFILL) {
                    log_w("Too many pages to backfill, aborting");
                    this->telemetry_.logMetric(
                        TelemetryField::METRIC_SYNC_STATUS,
                        StringFormat("sync-%ss-max-backfill",
                                     ReadableModelTypeToString(modelType).c_str())
                            .c_str());
                    break;
                }

                int httpCode = 0;
                if (nextPageCounter == 1) {
                    SHOW_HEAP();
                    httpCode = SindarinAPI::getInstance()->checkSync(
                        checkSyncRequest, &checkSyncResponse, this->telemetry_);
                    SHOW_HEAP();
                } else {
                    httpCode =
                        SindarinAPI::getInstance()->checkSync(nextPageUrl.c_str(), checkSyncRequest,
                                                              &checkSyncResponse, this->telemetry_);
                }
                log_d("Server has %d %ss, next page: %s", checkSyncResponse.totalCount,
                      ReadableModelTypeToString(modelType).c_str(), nextPageUrl.c_str());
                this->telemetry_.logMetric(
                    TelemetryField::METRIC_SYNC_STATUS,
                    StringFormat("sync-status-%ss", ReadableModelTypeToString(modelType).c_str())
                        .c_str(),
                    {{"http-code", httpCode}});
                auto pageUuids = std::string();
                auto userReadables = modelType == ReadablesModelType::Book
                                         ? checkSyncResponse.userBooks
                                         : checkSyncResponse.userArticles;
                // Log the uuids of the readables into the pageUuids string
                for (const auto &readable : userReadables) {
                    pageUuids += readable.readable->shortenedUuid + ",";
                }
                this->telemetry_.logMetric(
                    TelemetryField::METRIC_SYNC_STATUS,
                    StringFormat("sync-%ss-count", ReadableModelTypeToString(modelType).c_str())
                        .c_str(),
                    {{"total-count", checkSyncResponse.totalCount}, {"uuids", pageUuids}});
                if (httpCode != 200) {
                    log_e("Failed to check sync (HTTP %d)", httpCode);
                    progressCallback(ContentSyncState{.state = ContentSyncState::State::Failure,
                                                      .message = "Failed to check sync"});
                    this->telemetry_.logMetric(
                        TelemetryField::METRIC_SYNC_STATUS,
                        StringFormat("failed-sync-%ss",
                                     ReadableModelTypeToString(modelType).c_str())
                            .c_str(),
                        {{"http-code", httpCode}});
                    return;
                }

                // Look through all the books and articles and download any that we don't have
                // already We also process readables before checking if we're synced before we
                // need to reset the local changed flags
                auto [readablesToDownload, readablesToDelete] =
                    this->processReadables(*readables, userReadables);

                SHOW_HEAP();
                log_d("%ss to download: %zu", ReadableModelTypeToString(modelType).c_str(),
                      readablesToDownload.size());
                log_d("%ss to delete: %zu", ReadableModelTypeToString(modelType).c_str(),
                      readablesToDelete.size());
                nextPageUrl = checkSyncResponse.nextPageUrl;

                if (checkSyncResponse.timezone.size()) {
                    SolClock::getInstance().setTimezone(checkSyncResponse.timezone);
                }

                if (checkSyncResponse.synced) {
                    log_i("%ss are already in sync, no need to download anything",
                          ReadableModelTypeToString(modelType).c_str());
                    if (modelType == ReadablesModelType::Book) {
                        booksAlreadySynced = true;
                    } else if (modelType == ReadablesModelType::Article) {
                        articlesAlreadySynced = true;
                    }

                    this->telemetry_.logMetric(
                        TelemetryField::METRIC_SYNC_STATUS,
                        StringFormat("already-synced-%ss",
                                     ReadableModelTypeToString(modelType).c_str())
                            .c_str());
                    break;
                }

                // Count books/articles to download and delete
                syncState.tasks.reserve(readablesToDownload.size() + readablesToDelete.size());
                auto tasksIndex = 0;

                if (modelType == ReadablesModelType::Book) {
                    for (const auto &userReadable : readablesToDelete) {
                        syncState.tasks.push_back(ContentSyncState::Task{
                            .message =
                                StringFormat("Delete: %s", userReadable.readable->title.c_str()),
                            .percentage = 0,
                        });
                    }

                    for (const auto &userReadable : readablesToDownload) {
                        syncState.tasks.push_back(ContentSyncState::Task{
                            .message =
                                StringFormat("Get: %s", userReadable.readable->title.c_str()),
                            .percentage = 0,
                        });
                    }
                } else {
                    if (readablesToDelete.size() > 0) {
                        syncState.tasks.push_back(ContentSyncState::Task{
                            .message = "Delete: Archived articles",
                            .percentage = 0,
                        });
                    }
                    if (readablesToDownload.size() > 0) {
                        syncState.tasks.push_back(ContentSyncState::Task{
                            .message = "Get: Articles and newsletters",
                            .percentage = 0,
                        });
                    }
                }
                // Check if we have any readables to download/delete, and if not, continue paging
                if (syncState.tasks.size() == 0) {
                    log_i("No %ss to download or delete, continuing to next page",
                          ReadableModelTypeToString(modelType).c_str());
                    if (modelType == ReadablesModelType::Book) {
                        progressCallback(
                            ContentSyncState{.state = ContentSyncState::State::SyncingBooks});
                    } else if (modelType == ReadablesModelType::Article) {
                        progressCallback(
                            ContentSyncState{.state = ContentSyncState::State::SyncingArticles});
                    }
                    continue;
                }
                progressCallback(syncState);

                processReadableDeletion(readablesToDelete, *readables, tasksIndex, syncState,
                                        progressCallback);
                failedDownload =
                    processReadableDownload(readablesToDownload, *readables, tasksIndex, syncState,
                                            progressCallback, modelType) ||
                    failedDownload;
                if (failedDownload) {
                    log_d("Failed to process %ss, have %zu %ss",
                          ReadableModelTypeToString(modelType).c_str(), readables->size(),
                          ReadableModelTypeToString(modelType).c_str());
                    this->telemetry_.logMetric(
                        TelemetryField::METRIC_SYNC_STATUS,
                        StringFormat("failed-download-%ss",
                                     ReadableModelTypeToString(modelType).c_str())
                            .c_str());
                }

                progressCallback(syncState);
            }

            if (!this->saveReadables(*readables)) {
                log_e("Failed to save %s", ReadableModelTypeToString(modelType).c_str());
                failedDownload = true;
                this->telemetry_.logMetric(
                    TelemetryField::METRIC_SYNC_STATUS,
                    StringFormat("failed-save-%ss", ReadableModelTypeToString(modelType).c_str())
                        .c_str());
            }

            // Store since date values for next sync
            log_i("%sSetting %ssSince to %s", failedDownload ? "Not " : "",
                  ReadableModelTypeToString(modelType).c_str(),
                  modelType == ReadablesModelType::Book ? checkSyncResponse.booksSince.c_str()
                                                        : checkSyncResponse.articlesSince.c_str());
            std::string serverSince;
            if (!failedDownload) {
                if (checkSyncResponse.booksSince != "") {
                    this->prefs_.lastBooksSince.set(checkSyncResponse.booksSince.c_str());
                    serverSince = checkSyncResponse.booksSince;
                }
                if (checkSyncResponse.articlesSince != "") {
                    this->prefs_.lastArticlesSince.set(checkSyncResponse.articlesSince.c_str());
                    serverSince = checkSyncResponse.articlesSince;
                }
            }

            if ((modelType == ReadablesModelType::Book && booksAlreadySynced) ||
                (modelType == ReadablesModelType::Article && articlesAlreadySynced)) {
                continue;
            }

            // Update the since dates and try once more to sync to ensure we don't miss anything
            storedUuids.clear();
            this->storedReadableUuids(*readables, storedUuids);
            checkSyncRequest.storedUuids = storedUuids;
            checkSyncRequest.bookmarks = {};
            checkSyncRequest.archives = {};
            checkSyncRequest.placemarks = {};
            checkSyncRequest.since = serverSince;

            SHOW_HEAP();
            SindarinAPI::getInstance()->checkSync(checkSyncRequest, &checkSyncResponse,
                                                  this->telemetry_);

            // TODO: Force reload of rootUI instead of rebooting
            if (checkSyncResponse.synced) {
                log_i("%ss are in sync", ReadableModelTypeToString(modelType).c_str());
                if (modelType == ReadablesModelType::Book) {
                    booksNowSynced = true;
                } else if (modelType == ReadablesModelType::Article) {
                    articlesNowSynced = true;
                }
                this->telemetry_.logMetric(
                    TelemetryField::METRIC_SYNC_STATUS,
                    StringFormat("success-download-%ss",
                                 ReadableModelTypeToString(modelType).c_str()));
            } else {
                log_w("After running sync, local %ss are still out of sync",
                      ReadableModelTypeToString(modelType).c_str());
                checkSyncFailures += 1;
                this->telemetry_.logMetric(
                    TelemetryField::METRIC_SYNC_STATUS,
                    StringFormat("failed-finish-sync-%ss",
                                 ReadableModelTypeToString(modelType).c_str())
                        .c_str());
            }
        }

#if !TESTS
        // Artificially delay so the user has the satisfaction of seeing the list done
        Delay(1000);
#endif

        if (booksAlreadySynced && articlesAlreadySynced) {
            progressCallback(ContentSyncState{.state = ContentSyncState::State::AlreadySynced});
        } else if (booksNowSynced || articlesNowSynced) {
            progressCallback(ContentSyncState{.state = ContentSyncState::State::Synced});
        } else if (checkSyncFailures > 0) {
            progressCallback(ContentSyncState{.state = ContentSyncState::State::Failure,
                                              .message = "Failed to check sync"});
        } else {
            progressCallback(ContentSyncState{.state = ContentSyncState::State::AlreadySynced,
                                              .message = "Already synced"});
        }
        log_d("Min watermark for task %s: %u bytes (%s:%d)", pcTaskGetName(nullptr),
              (unsigned int)uxTaskGetStackHighWaterMark(NULL), __FILE__, __LINE__);
    });
}

auto LocalStore::processReadables(const ReadablesVector &readables,
                                  UserReadablesVector &serverReadables)
    -> std::pair<UserReadablesVector, UserReadablesVector> {
    UserReadablesVector readablesToDownload = {};
    UserReadablesVector readablesToDelete = {};
    auto modelType = readablesModelType(const_cast<ReadablesVector &>(readables));

    log_d("Processing %zu %ss from server", serverReadables.size(),
          ReadableModelTypeToString(modelType).c_str());

    for (auto &userReadable : serverReadables) {
        log_d("Processing user readable %s (%s)", userReadable.readable->title.c_str(),
              userReadable.readable->uuid.c_str());
        SHOW_HEAP();

        if (processReadablePlacemarksFromServer(userReadable.readable, userReadable.txtPlace,
                                                userReadable.ePubPlace)) {
            log_i("Placemark for %s updated to %s", userReadable.readable->title.c_str(),
                  prefs_.readablePlace.get(userReadable.readable->shortenedUuid.c_str()).c_str());
        }

        if (processReadablePartialChapterSummary(userReadable.readable,
                                                 userReadable.partialChapterSummary)) {
            log_i("Partial chapter summary for %s updated to %s",
                  userReadable.readable->title.c_str(),
                  userReadable.partialChapterSummary->ePubPlace->str().c_str());
        }

        auto unchanged = isReadableUnchanged(readables, userReadable.readable, true);
        if (userReadable.isArchived) {
            log_i("Readable \"%s\" is archived, will delete", userReadable.readable->title.c_str());
            // TODO: Store these articles in an accessible archive instead of just deleting them
            // and ignoring
            readablesToDelete.push_back(userReadable);
            continue;
        } else if (unchanged) {
            // Should never get here as readablesSince should hide these articles
            log_w("Already have \"%s\" %s", userReadable.readable->title.c_str(),
                  userReadable.readable->uuid.c_str());
            continue;
        }
        readablesToDownload.push_back(userReadable);
    }
    resetReadablesPlacemarkChanged(readables);
    resetReadablesArchivedChanged(readables);

    // Clear both sets of readables because all bookmarks are synced at once
    resetReadablesBookmarksChanged(books_);
    resetReadablesBookmarksChanged(articles_);

    return std::make_pair(readablesToDownload, readablesToDelete);
}

void LocalStore::processReadableDeletion(
    const UserReadablesVector &readablesToDelete, ReadablesVector &readables, int &tasksIndex,
    ContentSyncState &syncState, const std::function<void(ContentSyncState)> &progressCallback) {
    for (int i = 0; i < readablesToDelete.size(); i++) {

        syncState.tasks[tasksIndex].percentage = (float(i + 1) / readablesToDelete.size()) * 100;

        log_i("Deleting %s", readablesToDelete[i].readable->title.c_str());
        readableFilePurge(readablesToDelete[i].readable);
        removeReadable(readables, readablesToDelete[i].readable);
    }

    // All article deletions take a single task line item
    if (readablesToDelete.size() > 0) {
        tasksIndex += 1;
    }
    progressCallback(syncState);
}

auto LocalStore::processReadableDownload(
    const UserReadablesVector &readablesToDownload, ReadablesVector &readables, int &tasksIndex,
    ContentSyncState &syncState, const std::function<void(ContentSyncState)> &progressCallback,
    ReadablesModelType modelType) -> bool {
    bool failedDownload = false;

    if (readablesToDownload.size() == 0) {
        progressCallback(syncState);
        return failedDownload;
    }

    float pctPerReadable = 100.0 / readablesToDownload.size();
    for (int i = 0; i < readablesToDownload.size(); i++) {
        float currPercentage = (float)i * pctPerReadable;
        const auto &userReadable = readablesToDownload[i];
        const auto &readable = userReadable.readable;
        auto path = readableFilePathForWriting(readable);
        log_i("Downloading %s (file=%s, url=%s)", readable->title.c_str(), path.c_str(),
              readable->remoteUrl().c_str());
        SHOW_HEAP();

        for (int t = 0; t < NUMBER_OF_READABLE_DOWNLOAD_TRIES; t++) {
            auto httpCode = SindarinAPI::getInstance()->downloadReadable(
                readable, this->fileSystem, this->telemetry_, path,
                [progressCallback, &syncState, tasksIndex, currPercentage, pctPerReadable,
                 modelType](int percentage) {
                if (modelType == ReadablesModelType::Book) {
                    syncState.tasks[tasksIndex].percentage = percentage;
                } else if (modelType == ReadablesModelType::Article) {
                    float pctThisReadable = (float)percentage * pctPerReadable / 100.0;
                    syncState.tasks[tasksIndex].percentage = currPercentage + pctThisReadable;
                }
                progressCallback(syncState);
                });
            if (httpCode == 200) {
                addReadable(readables, readable);
                log_d("File successfully downloaded.");
                purgeReadableToc(readable);
                if (!saveReadables(readables)) {
                    log_e("Failed to save %s", ReadableModelTypeToString(modelType).c_str());

                    this->telemetry_.logMetric(
                        TelemetryField::METRIC_SYNC_STATUS,
                        StringFormat("failed-save-%s",
                                     ReadableModelTypeToString(modelType).c_str()),
                        {{"uuid", readable->uuid}, {"title", readable->title.c_str()}});
                } else {
                    // Readables that were previously TXT and are now EPUB may still need their
                    // placemark updated
                    processReadablePlacemarksFromServer(readable, userReadable.txtPlace,
                                                        userReadable.ePubPlace);
                }

                break;
            } else {
                log_w("Failed to download file. Retrying...");
                if (t == NUMBER_OF_READABLE_DOWNLOAD_TRIES - 1) {
                    log_e("Failed to download file.");
                    failedDownload = true;
                    this->telemetry_.logMetric(
                        TelemetryField::METRIC_SYNC_STATUS,
                        StringFormat("failed-download-%s",
                                     ReadableModelTypeToString(modelType).c_str())
                            .c_str(),
                        {{"uuid", readable->uuid},
                         {"title", readable->title.c_str()},
                         {"http-code", httpCode}});
                }
            }
        }

        if (modelType == ReadablesModelType::Book) {
            tasksIndex += 1;
        }
    }

    progressCallback(syncState);

    if (modelType == ReadablesModelType::Article) {
        tasksIndex += 1;
    }

    return failedDownload;
}

void LocalStore::deleteReadables(bool restart) {
    deleteReadableFiles(books_, restart);
    books_.clear();

    // Also delete localStoreMigrationVersion
    prefs_.localStoreMigrationVersion.remove();
}

void LocalStore::deleteArticles(bool restart) {
    deleteReadableFiles(articles_, restart);
    articles_.clear();

    // Also delete localStoreMigrationVersion
    prefs_.localStoreMigrationVersion.remove();
}

void LocalStore::deleteReadableFiles(const ReadablesVector &readables, bool restart) {
    auto modelType = readablesModelType(const_cast<ReadablesVector &>(readables));
    std::string path = modelType == ReadablesModelType::Book ? BOOKS_PATH : ARTICLES_PATH;
    log_i("Clearing %s", path.c_str());
    bool removed = fileSystem.remove(path);
    if (!removed) {
        log_e("Failed to remove %s", path.c_str());
    }

    if (modelType == ReadablesModelType::Article) {
        prefs_.lastArticlesSince.remove();
    } else if (modelType == ReadablesModelType::Book) {
        prefs_.lastBooksSince.remove();
    }

    int64_t start = esp_timer_get_time();
    for (const auto &readable : readables) {
        log_i("Removing %s \"%s\"", ReadableModelTypeToString(modelType).c_str(),
              readable->title.c_str());
        readableFilePurge(readable);
    }
    int64_t duration = esp_timer_get_time() - start;
    log_d("Purged %zu %ss in %" PRIi64 " ms", readables.size(),
          ReadableModelTypeToString(modelType).c_str(), duration / 1000);

    log_i("Resetting to guarantee %s are removed", ReadableModelTypeToString(modelType).c_str());

    if (restart) {
        PowerManager::getInstance().reboot();
    }
}

auto LocalStore::removeStopWords(const SpiramString &title) -> std::string {
    std::string tempTitle(title.begin(), title.end());
    std::set<std::string> stopWords = {"a ", "and ", "the "};

    // Case-insensitive comparison function
    auto compareIgnoreCase = [](const char c1, const char c2) {
        return std::tolower(c1) == std::tolower(c2);
    };

    // Remove stop words from the beginning of the title (case-insensitive)
    for (const auto &stopWord : stopWords) {
        if (std::equal(stopWord.begin(), stopWord.end(), tempTitle.begin(), compareIgnoreCase)) {
            tempTitle.erase(0, stopWord.size());
            break;
        }
    }

    return tempTitle;
}

void LocalStore::sortReadables(ReadablesVector &readables) {
    SortCriteria criteria;
    criteria.type = static_cast<SortCriteria::Type>(prefs_.sortCriteria.get().value_or(0));
    SortDirection direction;
    direction.type = static_cast<SortDirection::Type>(prefs_.sortDirection.get().value_or(0));

    auto modelType = readablesModelType(const_cast<ReadablesVector &>(readables));
    log_d("Sorting %zu %ss by %s/%s", readables.size(),
          ReadableModelTypeToString(modelType).c_str(), criteria.toString().c_str(),
          direction.toString().c_str());

    std::sort(readables.begin(), readables.end(),
              [this, &criteria, &direction](const std::shared_ptr<Readable> &a,
                                            const std::shared_ptr<Readable> &b) {
        if (criteria.type == SortCriteria::DateModified) {
            if (a->dateModified.empty()) {
                return direction.type == SortDirection::Descending;
            }
            if (b->dateModified.empty()) {
                return direction.type == SortDirection::Ascending;
            }
            return direction.type == SortDirection::Ascending ? a->dateModified < b->dateModified
                                                              : a->dateModified > b->dateModified;
        } else if (criteria.type == SortCriteria::Alphabetical) {
            std::string aTitle = removeStopWords(a->title);
            std::string bTitle = removeStopWords(b->title);
            if (aTitle.empty()) {
                return direction.type == SortDirection::Descending;
            }
            if (bTitle.empty()) {
                return direction.type == SortDirection::Ascending;
            }
            return direction.type == SortDirection::Ascending
                       ? CaseInsensitiveCompare(aTitle, bTitle)
                       : CaseInsensitiveCompare(bTitle, aTitle);
        } else if (criteria.type == SortCriteria::LastRead) {
            auto aLastRead = prefs_.lastReadTime.get(a->shortenedUuid.c_str());
            auto bLastRead = prefs_.lastReadTime.get(b->shortenedUuid.c_str());

            // Unread items are considered least valued
            if (aLastRead == 0) {
                aLastRead = std::numeric_limits<decltype(aLastRead)>::min();
            }
            if (bLastRead == 0) {
                bLastRead = std::numeric_limits<decltype(bLastRead)>::min();
            }

            return direction.type == SortDirection::Ascending ? aLastRead < bLastRead
                                                              : aLastRead > bLastRead;
        } else if (criteria.type == SortCriteria::Random) {
            return random() % 2 == 0;
        }

        return false;
    });

    log_d("Finished sorting");
}

void LocalStore::purgeReadableToc(const std::shared_ptr<Readable> &readable) {
    // Delete the TOC files so they can be re-saved with updated copies
    auto tocPath = readable->tocFilePath();
    if (tocPath.size()) {
        log_i("Removing TOC for %s: %s", readable->title.c_str(), tocPath.c_str());
        fileSystem.remove(tocPath.c_str());
    }
}

auto LocalStore::saveReadables(ReadablesVector &readables) -> bool {
    SHOW_HEAP();

    sortReadables(readables);

    // Readables take up to 20KB of RAM each since the JSON can be large (includes cover
    // images, file metadata, etc.)
    auto doc = std::make_unique<SpiRamJsonDocument>(20000 * std::max(1, (int)readables.size()));
    auto modelType = readablesModelType(const_cast<ReadablesVector &>(readables));
    std::string path = modelType == ReadablesModelType::Book ? BOOKS_PATH : ARTICLES_PATH;
    SHOW_HEAP();
    // Create a key named readables of type array
    JsonArray readablesJson = doc->createNestedArray("readables");

    log_d("Saving %zu %ss in %s", readables.size(), ReadableModelTypeToString(modelType).c_str(),
          path.c_str());
    for (const auto &readable : readables) {
        // Save the readable
        Readable readableCopy = *readable;
        readablesJson.add(readableCopy);
    }

    if (doc->overflowed()) {
        log_e("Out of space in SpiRamJsonDocument for %s", path.c_str());
        return false;
    }

    // Write doc to disk
    SFile file = fileSystem.open(path, FILE_WRITE, true);
    if (!file) {
        log_e("Failed to open file for writing");
        return false;
    }

    serializeJsonPretty(*doc, file);
    file.close();
    log_d("Saved %zu %ss in %s", readables.size(), ReadableModelTypeToString(modelType).c_str(),
          path.c_str());

    return true;
};

auto LocalStore::readableFilePathForWriting(const std::shared_ptr<Readable> &readable) const
    -> std::string {
    if (readable->remoteUrl().substr(readable->remoteUrl().find_last_of('.') + 1) == "epub") {
        return StringFormat("/%s/%s.epub", readable->readableType.c_str(), readable->uuid.c_str());
    }
    return StringFormat("/%s/%s.txt.gz", readable->readableType.c_str(), readable->uuid.c_str());
}

auto LocalStore::readableFilePathForReading(FileSystem &fileSystem,
                                            const std::shared_ptr<Readable> &readable)
    -> std::string {
    // We look for the readable file in the following order:
    // We first try the most modern path style:
    // /[readableType]/[uuid].epub
    // /[readableType]/[uuid].txt.gz
    // /[readableType]/[uuid].txt

    // Then if it's a book we try:
    // /[file().name].epub
    // /[file().name].txt.gz
    // /[file().name].txt

    // Then if it's not a book we will search:
    // /articles/[uuid].epub
    // /articles/[uuid].txt.gz
    // /articles/[uuid].txt

    auto type = readable->readableType;
    auto &uuid = readable->uuid;

    std::string path;

    path = StringFormat("/%s/%s.epub", type.c_str(), uuid.c_str());
    if (fileSystem.exists(path)) {
        return path;
    }
    path = StringFormat("/%s/%s.txt.gz", type.c_str(), uuid.c_str());
    if (fileSystem.exists(path)) {
        return path;
    }
    path = StringFormat("/%s/%s.txt", type.c_str(), uuid.c_str());
    if (fileSystem.exists(path)) {
        return path;
    }

    if (type == "book") {
        // Previously we also stored books in the root directory
        if (readable->files.size() > 0) {
            path = StringFormat("/%s.epub", readable->file().name.c_str());
            if (fileSystem.exists(path)) {
                return path;
            }
            path = StringFormat("/%s.gz", readable->file().name.c_str());
            if (fileSystem.exists(path)) {
                return path;
            }
            path = StringFormat("/%s", readable->file().name.c_str());
            if (fileSystem.exists(path)) {
                return path;
            }
        }
    } else {
        path = StringFormat("/articles/%s.epub", uuid.c_str());
        if (fileSystem.exists(path)) {
            return path;
        }
        path = StringFormat("/articles/%s.txt.gz", uuid.c_str());
        if (fileSystem.exists(path)) {
            return path;
        }
        path = StringFormat("/articles/%s.txt", uuid.c_str());
        if (fileSystem.exists(path)) {
            return path;
        }
    }
    // log_w("Unable to find readable file for %s (%s)", readable->title.c_str(), uuid.c_str());
    return "";
}

auto LocalStore::readableFilePathForReading(const std::shared_ptr<Readable> &readable) const
    -> std::string {
    return LocalStore::readableFilePathForReading(fileSystem, readable);
}

auto LocalStore::readableGetUncompressedSize(const std::shared_ptr<Readable> &readable) const
    -> int {
    auto path = readableFilePathForReading(readable);
    if (path.empty()) {
        return -1;
    }
    auto file = fileSystem.open(path);
    if (!file) {
        log_w("Failed to open readable file to check size %s", path.c_str());
        return -1;
    }

    int fileSize = -1;
    // If the path has a .gz suffix interpret it as a GZip file
    if (HasSuffix(path, ".gz")) {
        GUnzipStream gzStream(file);
        fileSize = gzStream.size();
    } else {
        fileSize = file.size();
    }
    file.close();

    return fileSize;
}

void LocalStore::readableFilePurge(const std::shared_ptr<Readable> &readable) {
    // Purge all possible readble file paths:
    // /[readableType]/[uuid].txt.gz
    // /[readableType]/[uuid].txt
    // /[readableType]/[uuid].epub
    // /toc/[uuid]-toc.json
    // /[file().name].epub
    // /[file().name].txt.gz
    // /[file().name].txt
    // /articles/[uuid].epub
    // /articles/[uuid].txt.gz
    // /articles/[uuid].txt
    // Bookmarks
    // Placemarks in NVS

    auto type = readable->readableType;
    auto &uuid = readable->uuid;

    log_d("Purging readable file %s (%s)", readable->title.c_str(), uuid.c_str());
    fileSystem.remove(StringFormat("/%s/%s.txt.gz", type.c_str(), uuid.c_str()));
    fileSystem.remove(StringFormat("/%s/%s.txt", type.c_str(), uuid.c_str()));
    fileSystem.remove(StringFormat("/%s/%s.epub", type.c_str(), uuid.c_str()));
    fileSystem.remove(StringFormat("/toc/%s-toc.json", uuid.c_str()));
    fileSystem.remove(StringFormat("/summary/%s-summary.json", uuid.c_str()));

    if (readable->files.size() > 0) {
        fileSystem.remove(StringFormat("/%s.epub", readable->file().name.c_str()));
        fileSystem.remove(StringFormat("/%s.gz", readable->file().name.c_str()));
        fileSystem.remove(StringFormat("/%s", readable->file().name.c_str()));
    }

    fileSystem.remove(StringFormat("/articles/%s.txt.gz", uuid.c_str()));
    fileSystem.remove(StringFormat("/articles/%s.txt", uuid.c_str()));
    fileSystem.remove(StringFormat("/articles/%s.epub", uuid.c_str()));

    fileSystem.remove(readable->bookmarkFilePath().c_str());

    // Also purge placemarks in NVS
    prefs_.readablePlace.remove(readable->shortenedUuid.c_str());
    prefs_.readablePlaceChanged.remove(readable->shortenedUuid.c_str());
    prefs_.readableArchivedChanged.remove(readable->shortenedUuid.c_str());
    prefs_.readableBookmarksChanged.remove(readable->shortenedUuid.c_str());
}

const char *firmwarePath = "/firmware.json";

void LocalStore::saveFirmware(const Firmware &firmware) {
    auto doc = std::make_unique<SpiRamJsonDocument>(
        std::max((size_t)firmware.releaseNotes.size() * 2, (size_t)2048));
    convertToJson(firmware, *doc);
    // Delete any existing firmware file if it exists
    fileSystem.remove(firmwarePath);
    auto firmwareFile = fileSystem.open(firmwarePath, "w");
    serializeJson(*doc, firmwareFile);
    firmwareFile.close();
    prefs_.firmwareLoaded.set(firmware.version);
}

auto LocalStore::getPendingFirmwareUpdate(Firmware &firmware) -> bool {
    auto firmwareFile = fileSystem.open(firmwarePath, "r");
    if (!firmwareFile) {
        return false;
    }
    defer { firmwareFile.close(); };
    auto doc = std::make_unique<SpiRamJsonDocument>(firmwareFile.size() * 4);

    DeserializationError error = deserializeJson(*doc, firmwareFile);
    if (error) {
        log_e("Failed to parse firmware file JSON: %s", error.c_str());
        return false;
    }
    auto parsed = firmware.fromJson(doc->as<JsonObject>());
    if (!parsed) {
        log_e("Failed to parse firmware file");
        return false;
    }

    return SemVerCompareVersions(kVersionString, firmware.version) == SemVerComparison::LessThan;
}

void LocalStore::resetFirmware() { fileSystem.remove(firmwarePath); }

void LocalStore::migrateTxtPlacemarksToEpubPlacemarks() {
    // If old txt place still exists, use it to overwrite the new place and then delete the old
    // txt place
    std::vector<ReadablesVector *> readablesList = {&books_, &articles_};
    for (auto &readables : readablesList) {
        for (auto &readable : *readables) {
            auto oldPlace = prefs_.oldReadablePlace.get(readable->shortenedUuid.c_str());
            if (oldPlace > 0) {
                log_d("Found old place for %s = %" PRIu32 ", migrating to new place format",
                      readable->shortenedUuid.c_str(), oldPlace);
                auto newPlace = Place::factory(oldPlace);
                prefs_.readablePlace.set(readable->shortenedUuid.c_str(), newPlace->str().c_str());
                prefs_.oldReadablePlace.remove(readable->shortenedUuid.c_str());
            }
        }
    }
}

void LocalStore::deleteAllToCJson() {
    // Remove the toc directory, logging each entry being deleted
    FileList tocFiles;
    fileSystem.listDir("/toc", tocFiles);
    for (auto &toc : tocFiles) {
        log_d("Deleting %s", toc.c_str());
        fileSystem.remove("/toc/" + toc);
    }
    fileSystem.remove("/toc");
}

void LocalStore::garbageCollectPreferences() {
    log_i("Running NVS garbage collection");
    prefs_.logStats();

    // Remove any readablePlace and readablePlaceChanged keys for readables that no longer exist
    auto readablePlaceKeys = prefs_.readablePlace.getAllKeyPrefixes();
    auto readablePlaceChangedKeys = prefs_.readablePlaceChanged.getAllKeyPrefixes();
    auto readableArchivedChangedKeys = prefs_.readableArchivedChanged.getAllKeyPrefixes();
    auto readableBookmarksChangedKeys = prefs_.readableBookmarksChanged.getAllKeyPrefixes();
    auto lastReadTime = prefs_.lastReadTime.getAllKeyPrefixes();

    // Combine and deduplicate all keys
    std::set<std::string> allKeys;
    allKeys.insert(readablePlaceKeys.begin(), readablePlaceKeys.end());
    allKeys.insert(readablePlaceChangedKeys.begin(), readablePlaceChangedKeys.end());
    allKeys.insert(readableArchivedChangedKeys.begin(), readableArchivedChangedKeys.end());
    allKeys.insert(readableBookmarksChangedKeys.begin(), readableBookmarksChangedKeys.end());
    allKeys.insert(lastReadTime.begin(), lastReadTime.end());

    for (auto &key : allKeys) {
        bool isStoredBook = std::any_of(books_.begin(), books_.end(),
                                        [&key](auto r) { return r->shortenedUuid == key.c_str(); });
        bool isStoredArticle = std::any_of(articles_.begin(), articles_.end(), [&key](auto r) {
            return r->shortenedUuid == key.c_str();
        });
        if (!isStoredBook && !isStoredArticle) {
            log_i("Removing all KeyedPrefsItems for %s", key.c_str());
            prefs_.readablePlace.remove(key.c_str());
            prefs_.readablePlaceChanged.remove(key.c_str());
            prefs_.readableArchivedChanged.remove(key.c_str());
            prefs_.readableBookmarksChanged.remove(key.c_str());
            prefs_.lastReadTime.remove(key.c_str());
        }
    }

    prefs_.logStats();
}

void LocalStore::migrateIfNeeded() {
    auto migrationVersion = prefs_.localStoreMigrationVersion.get().value_or(0);
    while (migrationVersion < latestMigrationVersion_) {
        log_i("Migrating LocalStore from version %" PRIu32 " to %" PRIu32 "", migrationVersion,
              migrationVersion + 1);
        switch (migrationVersion) {
        case 0: {
            // For a long while we weren't actively cleaning up these old readable-specific keys
            // in NVS. This migration goes through all readable-keys and purges any keys that
            // don't correspond to a readable on the glasses.
            garbageCollectPreferences();
            break;
        }
        case 1: {
            migrateTxtPlacemarksToEpubPlacemarks();
            break;
        }
        case 2: {
            deleteAllToCJson();
            break;
        }
        default:
            log_e("Unknown LocalStore migration version: %" PRIu32, migrationVersion);
            return;
        }
        migrationVersion++;
        prefs_.localStoreMigrationVersion.set(migrationVersion);
    }
}
