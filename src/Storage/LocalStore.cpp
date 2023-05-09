#include "LocalStore.hpp"

#include <esp_system.h>
#include <esp_timer.h>
#include <memory>
#include <set>
#include <unordered_map>

#include "Communications/HtmlCleaner.hpp"
#include "Communications/WiFi.hpp"
#include "GitVersion.hpp"
#include "Misc/Crypto.hpp"
#include "Misc/GUnzipStream.hpp"
#include "Misc/Platform.hpp"
#include "Misc/PowerManager.hpp"
#include "Misc/SemVer.hpp"
#include "Misc/StringUtil.hpp"
#include "Models/Json.hpp"
#include "Models/ReadableBookmark.hpp"
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
        Readable readable;

        if (!readable.fromJson(v.as<JsonObject>())) {
            log_e("Failed to parse Readable");
            continue;
        }
        auto path = readableFilePathForReading(readable);
        if (path.empty()) {
            log_w("Couldn't find readable with path %s", path.c_str());
            continue;
        }
        int fileSize = readableGetUncompressedSize(readable);

        if (fileSize >= 0 && readable.files.size() && fileSize != readable.file().size) {
            log_w("File size mismatch for %s (%d on disk vs %d in local store), ignoring",
                  path.c_str(), fileSize, readable.file().size);
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

auto LocalStore::hasReadable(const ReadablesVector &readables, const Readable &readable,
                             bool ignoreFileSizeCheck) -> bool {
    for (const auto &r : readables) {
        auto path = readableFilePathForReading(r);
        if (r.uuid == readable.uuid && !path.empty()) {
            int fileSize = readableGetUncompressedSize(readable);

            if (fileSize <= 0) {
                log_w("Couldn't get file size for %s", path.c_str());
                return false;
            }

            // If we have a file object, check that the size matches
            if (r.files.size() > 0 && !ignoreFileSizeCheck) {
                if (fileSize >= 0 && fileSize != readable.file().size) {
                    log_w("File size mismatch for %s (%d on disk vs %d in local store), ignoring",
                          path.c_str(), fileSize, readable.file().size);
                    return false;
                } else {
                    return true;
                }
            }
            return true;
        }
    }
    return false;
}

void LocalStore::addReadable(ReadablesVector &readables, const Readable &readable) {
    readables.emplace_back(readable);
    return;
}

void LocalStore::removeReadable(ReadablesVector &readables, const Readable &readable) {
    for (auto it = readables.begin(); it != readables.end(); ++it) {
        if (it->uuid == readable.uuid) {
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
    for (auto &readable : readables) {
        if (readable.localOnly) {
            continue;
        }
        storedUuids.emplace_back(readable.uuid);
    }
}

void LocalStore::changedReadablePlacemarkJson(const ReadablesVector &readables,
                                              ReadablePlacemarksVector &placemarks) {
    auto modelType = this->readablesModelType(const_cast<ReadablesVector &>(readables));

    for (auto &readable : readables) {
        if (readable.localOnly) {
            continue;
        }

        bool changed = prefs_.readablePlaceChanged.get(readable.shortenedUuid.c_str());
        if (changed) {
            uint32_t place = prefs_.readablePlace.get(readable.shortenedUuid.c_str());
            log_i("Placemark on %s %s %s has changed to %" PRIu32,
                  ReadableModelTypeToString(modelType).c_str(), readable.title.c_str(),
                  readable.uuid.c_str(), place);
            ReadablePlacemark placemark;
            placemark.uuid = readable.uuid;
            placemark.position = place;
            placemarks.emplace_back(placemark);
        }
    }
}

void LocalStore::changedReadablesArchivedJson(const ReadablesVector &readables,
                                              ReadableArchivesVector &archives) {
    auto modelType = this->readablesModelType(const_cast<ReadablesVector &>(readables));

    for (auto &readable : readables) {
        if (readable.localOnly) {
            continue;
        }

        bool changed = prefs_.readableArchivedChanged.get(readable.shortenedUuid.c_str());
        if (changed) {
            log_i("Archived %s %s %s", ReadableModelTypeToString(modelType).c_str(),
                  readable.title.c_str(), readable.uuid.c_str());
            ReadableArchive archive;
            archive.uuid = readable.uuid;
            archive.isArchived = true;
            archives.emplace_back(archive);
        }
    }
}

void LocalStore::changedReadablesBookmarksJson(ReadableBookmarksVector &bookmarks) {
    for (auto &readables : {books_, articles_}) {
        for (auto &readable : readables) {
            if (readable.localOnly) {
                continue;
            }

            bool changed = prefs_.readableBookmarksChanged.get(readable.shortenedUuid.c_str());
            if (!changed) {
                continue;
            }

            auto modelType = this->readablesModelType(const_cast<ReadablesVector &>(readables));
            log_i("Bookmarks changed on %s %s %s", ReadableModelTypeToString(modelType).c_str(),
                  readable.title.c_str(), readable.uuid.c_str());
            std::string bookmarkPath = readable.bookmarkFilePath();
            SFile bookmarkFile = fileSystem.open(bookmarkPath.c_str(), FILE_READ, false);
            if (!bookmarkFile) {
                log_i("Failed to open bookmark file: %s", bookmarkPath.c_str());
                continue;
            }

            // Store bookmarks in a hash table
            std::unordered_map<uint32_t, uint32_t> bookmarksToAdd;
            std::unordered_map<uint32_t, uint32_t> bookmarksToDelete;
            LocalBookmark bookmark;
            while (ReadOneBookmark(bookmarkFile, bookmark)) {
                log_d("Bookmark: %" PRIu32 " %" PRIu32 " %d", bookmark.firstPlace,
                      bookmark.lastPlace, bookmark.status);

                if (bookmark.status == BOOKMARK_ON && bookmark.firstPlace < bookmark.lastPlace) {
                    bookmarksToAdd[bookmark.firstPlace] = bookmark.lastPlace;
                    bookmarksToDelete.erase(bookmark.firstPlace);
                } else if (bookmark.status == BOOKMARK_OFF) {
                    bookmarksToAdd.erase(bookmark.firstPlace);
                    bookmarksToDelete[bookmark.firstPlace] = bookmark.lastPlace;
                }
            }
            bookmarkFile.close();

            // Add bookmarks to the JSON array
            for (const auto &[startingPosition, endingPosition] : bookmarksToAdd) {
                ReadableBookmark bookmark;
                bookmark.readable = readable.uuid;
                bookmark.startingPosition = startingPosition;
                bookmark.endingPosition = endingPosition;
                bookmarks.emplace_back(bookmark);
            }

            // Add bookmarks to delete to the JSON array
            for (const auto &[startingPosition, endingPosition] : bookmarksToDelete) {
                ReadableBookmark bookmark;
                bookmark.readable = readable.uuid;
                bookmark.startingPosition = startingPosition;
                bookmark.endingPosition = endingPosition;
                bookmark.willDelete = true;
                bookmarks.emplace_back(bookmark);
            }
        }
    }
}

auto LocalStore::processReadablePlacemarksFromServer(const Readable &readable,
                                                     const uint32_t placemark) -> bool {
    uint32_t onDevicePlace = prefs_.readablePlace.get(readable.shortenedUuid.c_str());
    bool changed = prefs_.readablePlaceChanged.get(readable.shortenedUuid.c_str());

    if (placemark == onDevicePlace) {
        // If the placemark on the server is the same as the one on the device, don't change it
        return false;
    } else if (placemark > onDevicePlace && !changed) {
        // Placemark on server is further than on the device, so update to the server value
        log_i("Updating placemark for \"%s\" from device value of %" PRIu32 " to server "
              "value of %" PRIu32,
              readable.title.c_str(), onDevicePlace, placemark);
        prefs_.readablePlace.set(readable.shortenedUuid.c_str(), placemark);
        return true;
    } else if (placemark != 0) {
        // Placemark on server is less than on the device, so don't update to the server value
        log_i("Not updating placemark for \"%s\" from device value of %" PRIu32 " to server "
              "value of %" PRIu32 ". Changed: %d",
              readable.title.c_str(), onDevicePlace, placemark, changed);
        return false;
    }

    return false;
}

void LocalStore::resetReadablesPlacemarkChanged(const ReadablesVector &readables) {
    for (auto &readable : readables) {
        bool changed = prefs_.readablePlaceChanged.get(readable.shortenedUuid.c_str());
        if (!readable.localOnly && changed) {
            log_d("Resetting placemark changed for \"%s\" (now synced to server)",
                  readable.title.c_str());
            prefs_.readablePlaceChanged.remove(readable.shortenedUuid.c_str());
        }
    }
}

void LocalStore::resetReadablesArchivedChanged(const ReadablesVector &readables) {
    for (auto &readable : readables) {
        bool changed = prefs_.readableArchivedChanged.get(readable.shortenedUuid.c_str());
        if (!readable.localOnly && changed) {
            log_d("Resetting article archive changed for \"%s\" (now synced to server)",
                  readable.title.c_str());
            prefs_.readableArchivedChanged.remove(readable.shortenedUuid.c_str());
        }
    }
}

void LocalStore::resetReadablesBookmarksChanged(const ReadablesVector &readables) {
    for (auto &readable : readables) {
        bool changed = prefs_.readableBookmarksChanged.get(readable.shortenedUuid.c_str());
        if (!readable.localOnly && changed) {
            log_d("Resetting bookmarks changed for \"%s\" (now synced to server)",
                  readable.title.c_str());
            prefs_.readableBookmarksChanged.remove(readable.shortenedUuid.c_str());
        }
    }
}

void LocalStore::syncFromRemote(
    const std::function<void(ContentSyncState state)> &progressCallback) {

    if (!WiFi::getInstance().hasWiFiCredentials()) {
        progressCallback(
            ContentSyncState{.state = ContentSyncState::State::Failure,
                             .message = "Go to Settings > WiFi Setup in the Sol app."});
        return;
    }

    if (!WiFi::getInstance().connectWiFiFromSettings()) {
        progressCallback(ContentSyncState{.state = ContentSyncState::State::Failure,
                                          .message = "Failed to connect to WiFi."});
        return;
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
    progressCallback(ContentSyncState{.state = ContentSyncState::State::CheckingSync});

    // Run this work on a separate thread, since it's long running
    auto queue = DispatchGetCore0BackgroundQueue();
    queue->dispatch([this, progressCallback] {
        auto syncReason = PreferencesStore::syncReason;
        CheckSyncRequest checkSyncRequest;
        CheckSyncResponse checkSyncResponse;
        int checkSyncFailures = 0;
        bool booksAlreadySynced = false;
        bool articlesAlreadySynced = false;
        bool booksNowSynced = false;
        bool articlesNowSynced = false;

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
            checkSyncRequest.ptBattery = PreferencesStore::lastPageTurnerBattery;
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
                nextPageCounter += 1;
                if (nextPageCounter > MAX_NUMBER_OF_PAGES_TO_BACKFILL) {
                    log_w("Too many pages to backfill, aborting");
                    break;
                }

                int httpCode = 0;
                if (nextPageCounter == 1) {
                    SHOW_HEAP();
                    httpCode =
                        SindarinAPI::getInstance()->checkSync(checkSyncRequest, &checkSyncResponse);
                    SHOW_HEAP();
                } else {
                    httpCode = SindarinAPI::getInstance()->checkSync(
                        nextPageUrl.c_str(), checkSyncRequest, &checkSyncResponse);
                }
                log_d("Server has %d %ss, next page: %s", checkSyncResponse.totalCount,
                      ReadableModelTypeToString(modelType).c_str(), nextPageUrl.c_str());
                if (httpCode != 200) {
                    log_e("Failed to check sync (HTTP %d)", httpCode);
                    progressCallback(ContentSyncState{.state = ContentSyncState::State::Failure,
                                                      .message = "Failed to check sync"});
                    return;
                }

                // Look through all the books and articles and download any that we don't have
                // already We also process readables before checking if we're synced before we need
                // to reset the local changed flags
                auto userReadables = modelType == ReadablesModelType::Book
                                         ? checkSyncResponse.userBooks
                                         : checkSyncResponse.userArticles;
                auto [readablesToDownload, readablesToDelete] =
                    this->processReadables(checkSyncResponse, *readables, userReadables);

                SHOW_HEAP();
                log_d("%ss to download: %zu", ReadableModelTypeToString(modelType).c_str(),
                      readablesToDownload.size());
                log_d("%ss to delete: %zu", ReadableModelTypeToString(modelType).c_str(),
                      readablesToDelete.size());
                nextPageUrl = checkSyncResponse.nextPageUrl;

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

                if (checkSyncResponse.synced) {
                    log_i("%ss are already in sync, no need to download anything",
                          ReadableModelTypeToString(modelType).c_str());
                    if (modelType == ReadablesModelType::Book) {
                        booksAlreadySynced = true;
                    } else if (modelType == ReadablesModelType::Article) {
                        articlesAlreadySynced = true;
                    }

                    break;
                }

                // Count books/articles to download and delete
                syncState.tasks.reserve(readablesToDownload.size() + readablesToDelete.size());
                auto tasksIndex = 0;

                if (modelType == ReadablesModelType::Book) {
                    for (const auto &readable : readablesToDelete) {
                        syncState.tasks.push_back(ContentSyncState::Task{
                            .message = StringFormat("Delete: %s", readable.title.c_str()),
                            .percentage = 0,
                        });
                    }

                    for (const auto &readable : readablesToDownload) {
                        syncState.tasks.push_back(ContentSyncState::Task{
                            .message = StringFormat("Get: %s", readable.title.c_str()),
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
                }
                progressCallback(syncState);
            }

            if (!this->saveReadables(*readables)) {
                log_e("Failed to save %s", ReadableModelTypeToString(modelType).c_str());
                failedDownload = true;
            }

            // Store since date values for next sync
            log_i("%sSetting last%sSince to %s/%s", failedDownload ? "Not " : "",
                  ReadableModelTypeToString(modelType).c_str(),
                  checkSyncResponse.booksSince.c_str(), checkSyncResponse.articlesSince.c_str());
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
            SindarinAPI::getInstance()->checkSync(checkSyncRequest, &checkSyncResponse);

            // TODO: Force reload of rootUI instead of rebooting
            if (checkSyncResponse.synced) {
                log_i("%ss are in sync", ReadableModelTypeToString(modelType).c_str());
                if (modelType == ReadablesModelType::Book) {
                    booksNowSynced = true;
                } else if (modelType == ReadablesModelType::Article) {
                    articlesNowSynced = true;
                }
            } else {
                log_w("After running sync, local %ss are still out of sync",
                      ReadableModelTypeToString(modelType).c_str());
                checkSyncFailures += 1;
            }
        }

        // Artificially delay so the user has the satisfaction of seeing the list done
        Delay(1000);

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
    });
}

auto LocalStore::processReadables(const CheckSyncResponse &checkSyncResponse,
                                  const ReadablesVector &readables,
                                  UserReadablesVector &userReadables)
    -> std::pair<ReadablesVector, ReadablesVector> {
    ReadablesVector readablesToDownload = {};
    ReadablesVector readablesToDelete = {};
    auto modelType = readablesModelType(const_cast<ReadablesVector &>(readables));

    auto serverReadables = modelType == ReadablesModelType::Book ? checkSyncResponse.userBooks
                                                                 : checkSyncResponse.userArticles;

    for (auto &userReadable : serverReadables) {
        log_d("User readable %s (%s)", userReadable.readable.title.c_str(),
              userReadable.readable.uuid.c_str());
        SHOW_HEAP();

        if (processReadablePlacemarksFromServer(userReadable.readable, userReadable.placemark)) {
            log_i("Placemark for %s updated to %" PRIu32, userReadable.readable.title.c_str(),
                  userReadable.placemark);
        }

        auto found = hasReadable(readables, userReadable.readable, true);
        if (userReadable.isArchived) {
            log_i("Readable \"%s\" is archived, will delete", userReadable.readable.title.c_str());
            // TODO: Store these articles in an accessible archive instead of just deleting them
            // and ignoring
            if (found) {
                readablesToDelete.push_back(userReadable.readable);
            } else {
                log_i("Already deleted \"%s\" %s", userReadable.readable.title.c_str(),
                      userReadable.readable.uuid.c_str());
            }
            continue;
        } else if (found) {
            // Should never get here as readablesSince should hide these articles
            log_w("Already have \"%s\" %s", userReadable.readable.title.c_str(),
                  userReadable.readable.uuid.c_str());
            continue;
        }
        readablesToDownload.push_back(userReadable.readable);
    }
    resetReadablesPlacemarkChanged(readables);
    resetReadablesArchivedChanged(readables);

    // Clear both sets of readables because all bookmarks are synced at once
    resetReadablesBookmarksChanged(books_);
    resetReadablesBookmarksChanged(articles_);

    return std::make_pair(readablesToDownload, readablesToDelete);
}

void LocalStore::processReadableDeletion(
    const ReadablesVector &readablesToDelete, ReadablesVector &readables, int &tasksIndex,
    ContentSyncState &syncState, const std::function<void(ContentSyncState)> &progressCallback) {
    for (int i = 0; i < readablesToDelete.size(); i++) {

        syncState.tasks[tasksIndex].percentage = (float(i + 1) / readablesToDelete.size()) * 100;

        log_i("Deleting %s", readablesToDelete[i].title.c_str());
        readableFilePurge(readablesToDelete[i]);
        this->removeReadable(readables, readablesToDelete[i]);
    }

    // All article deletions take a single task line item
    if (readablesToDelete.size() > 0) {
        tasksIndex += 1;
    }
    progressCallback(syncState);
}

auto LocalStore::processReadableDownload(
    const ReadablesVector &readablesToDownload, ReadablesVector &readables, int &tasksIndex,
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
        auto &readable = readablesToDownload[i];
        auto path = readableFilePathForWriting(readable);
        log_i("Downloading %s (file=%s, url=%s)", readable.title.c_str(), path.c_str(),
              readable.remoteUrl().c_str());
        SHOW_HEAP();

        for (int t = 0; t < NUMBER_OF_READABLE_DOWNLOAD_TRIES; t++) {
            auto success = SindarinAPI::getInstance()->downloadReadable(
                readable, this->fileSystem, path,
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
            if (success) {
                addReadable(readables, readable);
                log_d("File successfully downloaded.");
                if (!saveReadables(readables)) {
                    log_e("Failed to save %s", ReadableModelTypeToString(modelType).c_str());
                }

                break;
            } else {
                log_w("Failed to download file. Retrying...");
                if (t == NUMBER_OF_READABLE_DOWNLOAD_TRIES - 1) {
                    log_e("Failed to download file.");
                    failedDownload = true;
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
}

void LocalStore::deleteArticles(bool restart) {
    deleteReadableFiles(articles_, restart);
    articles_.clear();
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
    for (const Readable &readable : readables) {
        log_i("Removing %s \"%s\"", ReadableModelTypeToString(modelType).c_str(),
              readable.title.c_str());
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

auto LocalStore::saveReadables(const ReadablesVector &readables) -> bool {
    SHOW_HEAP();
    // Readables take up to 20KB of RAM each since the JSON can be large (includes ToC, cover
    // images, file metadata, etc.)
    auto doc = std::make_unique<SpiRamJsonDocument>(20000 * std::max(1, (int)readables.size()));
    auto modelType = readablesModelType(const_cast<ReadablesVector &>(readables));
    std::string path = modelType == ReadablesModelType::Book ? BOOKS_PATH : ARTICLES_PATH;
    SHOW_HEAP();
    // Create a key named readables of type array
    JsonArray readablesJson = doc->createNestedArray("readables");

    log_d("Saving %zu readables in %s", readables.size(), path.c_str());
    for (const auto &readable : readables) {
        readablesJson.add(readable);
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

    return true;
};

auto LocalStore::readableFilePathForWriting(const Readable &readable) const -> std::string {
    return StringFormat("/%s/%s.txt.gz", readable.readableType.c_str(), readable.uuid.c_str());
}

auto LocalStore::readableFilePathForReading(const Readable &readable) const -> std::string {
    // We look for the readable file in the following order:
    // We first try the most modern path style:
    // /[readableType]/[uuid].txt.gz
    // /[readableType]/[uuid].txt

    // Then if it's a book we try:
    // /[readableType]/[uuid].epub
    // /[file().name].txt.gz
    // /[file().name].txt

    // Then if it's not a book we will search:
    // /articles/[uuid].txt.gz
    // /articles/[uuid].txt

    auto type = readable.readableType;
    auto &uuid = readable.uuid;

    std::string path;

    path = StringFormat("/%s/%s.txt.gz", type.c_str(), uuid.c_str());
    if (fileSystem.exists(path)) {
        return path;
    }
    path = StringFormat("/%s/%s.txt", type.c_str(), uuid.c_str());
    if (fileSystem.exists(path)) {
        return path;
    }

    if (type == "book") {
        path = StringFormat("/%s/%s.epub", type.c_str(), uuid.c_str());
        if (fileSystem.exists(path)) {
            return path;
        }
        // Previously we also stored books in the root directory
        if (readable.files.size() > 0) {
            path = StringFormat("/%s.gz", readable.file().name.c_str());
            if (fileSystem.exists(path)) {
                return path;
            }
            path = StringFormat("/%s", readable.file().name.c_str());
            if (fileSystem.exists(path)) {
                return path;
            }
        }
    } else {
        path = StringFormat("/articles/%s.txt.gz", uuid.c_str());
        if (fileSystem.exists(path)) {
            return path;
        }
        path = StringFormat("/articles/%s.txt", uuid.c_str());
        if (fileSystem.exists(path)) {
            return path;
        }
    }
    log_w("Unable to find readable file for %s (%s)", readable.title.c_str(), uuid.c_str());
    return "";
}

auto LocalStore::readableGetUncompressedSize(const Readable &readable) const -> int {
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

void LocalStore::readableFilePurge(const Readable &readable) {
    // Purge all possible readble file paths:
    // /[readableType]/[uuid].txt.gz
    // /[readableType]/[uuid].txt
    // /[readableType]/[uuid].epub
    // /[file().name].txt.gz
    // /[file().name].txt
    // /articles/[uuid].txt.gz
    // /articles/[uuid].txt

    auto type = readable.readableType;
    auto &uuid = readable.uuid;

    log_d("Purging readable file %s (%s)", readable.title.c_str(), uuid.c_str());
    fileSystem.remove(StringFormat("/%s/%s.txt.gz", type.c_str(), uuid.c_str()));
    fileSystem.remove(StringFormat("/%s/%s.txt", type.c_str(), uuid.c_str()));
    fileSystem.remove(StringFormat("/%s/%s.epub", type.c_str(), uuid.c_str()));

    if (readable.files.size() > 0) {
        fileSystem.remove(StringFormat("/%s.gz", readable.file().name.c_str()));
        fileSystem.remove(StringFormat("/%s", readable.file().name.c_str()));
    }

    fileSystem.remove(StringFormat("/articles/%s.txt.gz", uuid.c_str()));
    fileSystem.remove(StringFormat("/articles/%s.txt", uuid.c_str()));

    fileSystem.remove(readable.bookmarkFilePath().c_str());

    // Also purge placemarks in NVS
    prefs_.readablePlace.remove(readable.shortenedUuid.c_str());
    prefs_.readablePlaceChanged.remove(readable.shortenedUuid.c_str());
    prefs_.readableArchivedChanged.remove(readable.shortenedUuid.c_str());
    prefs_.readableBookmarksChanged.remove(readable.shortenedUuid.c_str());
}

const char *firmwarePath = "/firmware.json";

void LocalStore::saveFirmware(const Firmware &firmware) {
    auto doc = std::make_unique<SpiRamJsonDocument>(
        std::min((size_t)firmware.releaseNotes.size() * 2, (size_t)2048));
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
    } else {
        auto doc = std::make_unique<SpiRamJsonDocument>(firmwareFile.size() * 4);

        DeserializationError error = deserializeJson(*doc, firmwareFile);
        if (error) {
            log_e("Failed to parse firmware file: %s", error.c_str());
            return false;
        }
        firmware.fromJson(doc->as<JsonObject>());
        firmwareFile.close();

        return SemVerCompareVersions(kVersionString, firmware.version) ==
               SemVerComparison::LessThan;
    }
}

void LocalStore::resetFirmware() { fileSystem.remove(firmwarePath); }

void LocalStore::garbageCollectPreferences() {
    log_i("Running NVS garbage collection");
    prefs_.logStats();

    // Remove any readablePlace and readablePlaceChanged keys for readables that no longer exist
    auto readablePlaceKeys = prefs_.readablePlace.getAllKeyPrefixes();
    auto readablePlaceChangedKeys = prefs_.readablePlaceChanged.getAllKeyPrefixes();
    auto readableArchivedChangedKeys = prefs_.readableArchivedChanged.getAllKeyPrefixes();
    auto readableBookmarksChangedKeys = prefs_.readableBookmarksChanged.getAllKeyPrefixes();

    // Combine and deduplicate all keys
    std::set<std::string> allKeys;
    allKeys.insert(readablePlaceKeys.begin(), readablePlaceKeys.end());
    allKeys.insert(readablePlaceChangedKeys.begin(), readablePlaceChangedKeys.end());
    allKeys.insert(readableArchivedChangedKeys.begin(), readableArchivedChangedKeys.end());
    allKeys.insert(readableBookmarksChangedKeys.begin(), readableBookmarksChangedKeys.end());

    for (auto &key : allKeys) {
        bool isStoredBook = std::any_of(books_.begin(), books_.end(), [&key](const auto &r) {
            return r.shortenedUuid == key.c_str();
        });
        bool isStoredArticle =
            std::any_of(articles_.begin(), articles_.end(),
                        [&key](const auto &r) { return r.shortenedUuid == key.c_str(); });
        if (!isStoredBook && !isStoredArticle) {
            log_i("Removing all KeyedPrefsItems for %s", key.c_str());
            prefs_.readablePlace.remove(key.c_str());
            prefs_.readablePlaceChanged.remove(key.c_str());
            prefs_.readableArchivedChanged.remove(key.c_str());
            prefs_.readableBookmarksChanged.remove(key.c_str());
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
            // For a long while we weren't actively cleaning up these old readable-specific keys in
            // NVS. This migration goes through all readable-keys and purges any keys that don't
            // correspond to a readable on the glasses.
            garbageCollectPreferences();
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
