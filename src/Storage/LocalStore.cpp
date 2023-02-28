#include "LocalStore.hpp"

#include <esp_system.h>
#include <unordered_map>

#include "Communications/HtmlCleaner.hpp"
#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFi.hpp"
#include "Misc/Crypto.hpp"
#include "Misc/GUnzipStream.hpp"
#include "Misc/StringUtil.hpp"
#include "Models/Json.hpp"
#include "Models/ReadableBookmark.hpp"
#include "sindarin-debug.h"

#define READABLES_PATH "/readables.json"
#define ARTICLES_PATH "/articles.json"

enum { NUMBER_OF_READABLE_DOWNLOAD_TRIES = 3 };

auto ToString(ReadablesModelType type) -> std::string {
    switch (type) {
    case ReadablesModelType::Article:
        return "article";
    case ReadablesModelType::Readable:
        return "readable";
    default:
        return "unknown";
    }
}

auto LocalStore::begin() -> bool {
    auto loadedReadables = loadReadablesFromFile(readables_, READABLES_PATH);
    auto loadedArticles = loadReadablesFromFile(articles_, ARTICLES_PATH);

    return loadedReadables && loadedArticles;
}

auto LocalStore::loadReadablesFromFile(ReadablesVector &readables, const std::string &path)
    -> bool {
    log_d("Loading %ss from %s", ToString(readablesModelType(readables)).c_str(), path.c_str());
    File readablesFile = fileSystem.open(path);
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
    return &readables == &articles_ ? ReadablesModelType::Article : ReadablesModelType::Readable;
}

auto LocalStore::hasReadable(const ReadablesVector &readables, const Readable &readable) -> bool {
    for (const auto &r : readables) {
        auto path = readableFilePathForReading(r);
        if (r.uuid == readable.uuid && !path.empty()) {
            int fileSize = readableGetUncompressedSize(readable);

            if (fileSize <= 0) {
                log_w("Couldn't get file size for %s", path.c_str());
                return false;
            }

            // If we have a file object, check that the size matches
            if (r.files.size() > 0) {
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

auto LocalStore::readablesSince() -> std::string {
    return prefs_.lastReadablesSince.get().value_or("");
}

auto LocalStore::articlesSince() -> std::string {
    return prefs_.lastArticlesSince.get().value_or("");
}

auto LocalStore::changedReadablePlacemarkQueryParams(const ReadablesVector &readables)
    -> std::vector<std::string> {
    std::vector<std::string> placemarks;
    auto modelType = this->readablesModelType(const_cast<ReadablesVector &>(readables));

    for (auto &readable : readables) {
        if (readable.localOnly) {
            continue;
        }

        bool changed = prefs_.readablePlaceChanged.get(readable.shortenedUuid.c_str());
        if (changed) {
            uint32_t place = prefs_.readablePlace.get(readable.shortenedUuid.c_str());
            log_i("Placemark on readable %s %s has changed to %d", readable.title.c_str(),
                  readable.uuid.c_str(), place);
            placemarks.push_back(StringFormat("&%s_placemark_%s=%d", ToString(modelType).c_str(),
                                              readable.uuid.c_str(), place));
        }
    }

    return placemarks;
}

auto LocalStore::changedReadablesArchivedQueryParams(const ReadablesVector &readables)
    -> std::vector<std::string> {
    std::vector<std::string> archived;
    auto modelType = this->readablesModelType(const_cast<ReadablesVector &>(readables));

    for (auto &readable : readables) {
        if (readable.localOnly) {
            continue;
        }

        bool changed = prefs_.readableArchivedChanged.get(readable.shortenedUuid.c_str());
        if (changed) {
            log_i("Archived %s %s %s", ToString(modelType).c_str(), readable.title.c_str(),
                  readable.uuid.c_str());
            archived.push_back(StringFormat("&%s_archive_%s=1", ToString(modelType).c_str(),
                                            readable.uuid.c_str()));
        }
    }

    return archived;
}

auto LocalStore::changedReadablesBookmarksJson() -> std::pair<char *, int> {
    auto doc = std::make_unique<SpiRamJsonDocument>(
        2048 * std::max(1, (int)readables_.size() + (int)articles_.size()));
    JsonArray bookmarksJsonArray = doc->to<JsonArray>();
    int changedCount = 0;

    for (auto &readables : {readables_, articles_}) {
        for (auto &readable : readables) {
            if (readable.localOnly) {
                continue;
            }

            bool changed = prefs_.readableBookmarksChanged.get(readable.shortenedUuid.c_str());
            if (changed) {
                auto modelType = this->readablesModelType(const_cast<ReadablesVector &>(readables));
                log_i("Bookmarks changed on %s %s %s", ToString(modelType).c_str(),
                      readable.title.c_str(), readable.uuid.c_str());
                std::string bookmarkPath =
                    StringFormat("/bookmarks/%s-bookmark.txt", readable.uuid.c_str());
                File bookmarkFile = fileSystem.open(bookmarkPath.c_str(), FILE_READ, false);
                if (!bookmarkFile) {
                    log_i("Failed to open bookmark file: %s", bookmarkPath.c_str());
                    continue;
                }
                changedCount += 1;

                // Store bookmarks in a hash table
                std::unordered_map<uint32_t, uint32_t> bookmarks;
                std::unordered_map<uint32_t, uint32_t> bookmarksToDelete;
                LocalBookmark bookmark;
                while (ReadOneBookmark(bookmarkFile, bookmark)) {
                    log_d("Bookmark: %d %d %d", bookmark.firstPlace, bookmark.lastPlace,
                          bookmark.status);

                    if (bookmark.status == BOOKMARK_ON &&
                        bookmark.firstPlace < bookmark.lastPlace) {
                        bookmarks[bookmark.firstPlace] = bookmark.lastPlace;
                        bookmarksToDelete.erase(bookmark.firstPlace);
                    } else if (bookmark.status == BOOKMARK_OFF) {
                        bookmarks.erase(bookmark.firstPlace);
                        bookmarksToDelete[bookmark.firstPlace] = bookmark.lastPlace;
                    }
                }
                bookmarkFile.close();

                // Add bookmarks to the JSON array
                for (const auto &[starting_position, ending_position] : bookmarks) {
                    JsonObject bookmark = bookmarksJsonArray.createNestedObject();
                    bookmark["starting_position"] = starting_position;
                    bookmark["ending_position"] = ending_position;
                    bookmark["readable"] = readable.uuid;
                }

                // Add bookmarks to delete to the JSON array
                for (const auto &[starting_position, ending_position] : bookmarksToDelete) {
                    JsonObject bookmark = bookmarksJsonArray.createNestedObject();
                    bookmark["starting_position"] = starting_position;
                    bookmark["ending_position"] = ending_position;
                    bookmark["status"] = 0;
                    bookmark["readable"] = readable.uuid;
                }

                if (doc->overflowed()) {
                    log_e("Out of space in SpiRamJsonDocument for %s", bookmarkPath.c_str());
                    return std::make_pair(nullptr, 0);
                }
            }
        }
    }

    if (!changedCount) {
        return std::make_pair(nullptr, 0);
    }

    size_t serializedMaxSize = (*doc).memoryUsage() * 2;
    char *bookmarksJson = (char *)heap_caps_malloc(serializedMaxSize, MALLOC_CAP_SPIRAM);
    size_t serializedLength = serializeJson(*doc, bookmarksJson, serializedMaxSize);

    return std::make_pair(bookmarksJson, serializedLength);
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
        log_i("Updating placemark for \"%s\" from device value of %d to server "
              "value of %d",
              readable.title.c_str(), onDevicePlace, placemark);
        prefs_.readablePlace.set(readable.shortenedUuid.c_str(), placemark);
        return true;
    } else if (placemark != 0) {
        // Placemark on server is less than on the device, so don't update to the server value
        log_i("Not updating placemark for \"%s\" from device value of %d to server "
              "value of %d. Changed: %d",
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
            prefs_.readablePlaceChanged.set(readable.shortenedUuid.c_str(), false);
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

    if (!HasWiFiCredentials()) {
        progressCallback(
            ContentSyncState{.state = ContentSyncState::State::Failure,
                             .message = "Go to Settings > WiFi Setup in the Sol app."});
        return;
    }

    if (!ConnectWifi()) {
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
        // Uncomment below to force a full sync
        // this->prefs_.lastReadablesSince.set("");
        // this->prefs_.lastArticlesSince.set("");
        bool failedReadableDownload = false;
        bool failedArticleDownload = false;
        auto readablesSince = this->readablesSince();
        auto articlesSince = this->articlesSince();
        auto readablePlacemarks = this->changedReadablePlacemarkQueryParams(readables_);
        auto articlePlacemarks = this->changedReadablePlacemarkQueryParams(articles_);
        auto readablesArchived = this->changedReadablesArchivedQueryParams(readables_);
        auto articlesArchived = this->changedReadablesArchivedQueryParams(articles_);
        auto sleepReason = this->prefs_.sleepReason.get().value_or(SLEEP_REASON_NONE);
        auto [readablesBookmarked, readablesBookmarkedSize] = this->changedReadablesBookmarksJson();
        log_i("Bookmarked readables (%d bytes)", readablesBookmarkedSize);
        CheckSyncResponse checkSyncResponse;
        CheckSyncRequest request =
            CheckSyncRequest{.readablesSince = readablesSince,
                             .articlesSince = articlesSince,
                             .readablePlacemarks = readablePlacemarks,
                             .articlePlacemarks = articlePlacemarks,
                             .readablesArchived = readablesArchived,
                             .articlesArchived = articlesArchived,
                             .sleepReason = sleepReason,
                             .ptBattery = this->prefs_.lastPageTurnerBattery.get().value_or(0),
                             .ptBuild = this->prefs_.lastPageTurnerBuild.get().value_or(""),
                             .skipContent = true};

        SHOW_HEAP();
        auto httpCode = SindarinAPI::getInstance()->checkSync(request, &checkSyncResponse);
        SHOW_HEAP();

        if (httpCode != 200) {
            log_e("Failed to check sync (HTTP %d)", httpCode);
            progressCallback(ContentSyncState{.state = ContentSyncState::State::Failure,
                                              .message = "Failed to check sync"});
            return;
        }
        if (checkSyncResponse.synced) {
            log_i("Client is in sync");
            progressCallback(ContentSyncState{.state = ContentSyncState::State::AlreadySynced});
            return;
        }

        progressCallback(ContentSyncState{.state = ContentSyncState::State::LoadingTasks});

        // If out of sync, download any new readables.
        ReadablesVector readablesToDownload = {};
        ReadablesVector readablesToDelete = {};
        ReadablesVector articlesToDelete = {};
        ReadablesVector articlesToSave = {};

        // Now look through all the readables here, and download any that we don't have
        // already
        for (auto &userReadable : checkSyncResponse.userReadables) {
            log_d("User readable %s (%s)", userReadable.readable.title.c_str(),
                  userReadable.readable.file().shaHash.c_str());
            SHOW_HEAP();

            if (this->processReadablePlacemarksFromServer(userReadable.readable,
                                                          userReadable.placemark)) {
                log_i("Placemark for %s updated to %d", userReadable.readable.title.c_str(),
                      userReadable.placemark);
            }

            auto hasReadable = this->hasReadable(readables_, userReadable.readable);
            if (userReadable.isArchived) {
                log_i("Readable \"%s\" is archived, will delete",
                      userReadable.readable.title.c_str());
                // TODO: Store these articles in an accessible archive instead of just deleting them
                // and ignoring
                if (hasReadable) {
                    readablesToDelete.push_back(userReadable.readable);
                } else {
                    log_i("Already deleted \"%s\" %s", userReadable.readable.title.c_str(),
                          userReadable.readable.uuid.c_str());
                }
                continue;
            } else if (hasReadable) {
                // Should never get here as readablesSince should hide these articles
                log_w("Already have \"%s\" %s", userReadable.readable.title.c_str(),
                      userReadable.readable.uuid.c_str());
                continue;
            }

            readablesToDownload.push_back(userReadable.readable);
        }
        this->resetReadablesPlacemarkChanged(readables_);
        this->resetReadablesArchivedChanged(readables_);

        // Look through all the articles, deleting archived ones and downloading new ones
        for (auto &userReadable : checkSyncResponse.userArticles) {
            log_d("Fetched user article: %s (%zu bytes, placemark: %d)",
                  userReadable.readable.title.c_str(), (size_t)userReadable.readable.content.size(),
                  userReadable.placemark);
            SHOW_HEAP();

            if (this->processReadablePlacemarksFromServer(userReadable.readable,
                                                          userReadable.placemark)) {
                log_i("Placemark for %s updated to %d", userReadable.readable.title.c_str(),
                      userReadable.placemark);
            }

            auto hasReadable = this->hasReadable(articles_, userReadable.readable);
            if (userReadable.isArchived) {
                log_i("Article \"%s\" is archived, will delete",
                      userReadable.readable.title.c_str());
                // TODO: Store these articles in an accessible archive instead of just deleting them
                // and ignoring
                if (hasReadable) {
                    articlesToDelete.push_back(userReadable.readable);
                } else {
                    log_i("Already deleted \"%s\" %s", userReadable.readable.title.c_str(),
                          userReadable.readable.uuid.c_str());
                }
                continue;
            } else if (hasReadable) {
                // Should never get here as articlesSince should hide these articles
                log_w("Already have \"%s\" %s", userReadable.readable.title.c_str(),
                      userReadable.readable.uuid.c_str());
                continue;
            }
            articlesToSave.push_back(userReadable.readable);
        }
        this->resetReadablesPlacemarkChanged(articles_);
        this->resetReadablesArchivedChanged(articles_);

        // TODO: Probably these should be set operations
        SHOW_HEAP();
        log_d("Readables to download: %zu", readablesToDownload.size());
        log_d("Readables to delete: %zu", readablesToDelete.size());
        log_d("Articles to delete: %zu", articlesToDelete.size());

        // syncState is used to track the progress of the sync
        ContentSyncState syncState = {.state = ContentSyncState::State::Syncing};
        syncState.tasks.reserve(readablesToDownload.size() + readablesToDelete.size() +
                                articlesToDelete.size() + articlesToSave.size() +
                                (readablesBookmarkedSize > 0 ? 1 : 0));
        auto tasksIndex = 0;

        if (readablesBookmarkedSize == 0 && readablesToDownload.size() == 0 &&
            readablesToDelete.size() == 0 && articlesToDelete.size() == 0 &&
            articlesToSave.size() == 0) {
            log_i("No new content to download");
            progressCallback(ContentSyncState{.state = ContentSyncState::State::AlreadySynced});
            return;
        }

        if (readablesBookmarkedSize > 0) {
            syncState.tasks.push_back(ContentSyncState::Task{
                .message = "Storing bookmarks",
                .percentage = 0,
            });
        }

        // Clear the readablesSince cache since it will be set at the end of a successful run.
        // If we fail, then the next content sync will not have a cutoff date
        if (checkSyncResponse.readablesSince != "") {
            log_d("Clearing readablesSince cache since it will be set at the end of a "
                  "successful run");
            this->prefs_.lastReadablesSince.set("");
        }

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
        if (articlesToDelete.size() > 0) {
            syncState.tasks.push_back(ContentSyncState::Task{
                .message = "Delete: Archived articles",
                .percentage = 0,
            });
        }
        if (articlesToSave.size() > 0) {
            syncState.tasks.push_back(ContentSyncState::Task{
                .message = "Get: Articles from Pocket",
                .percentage = 0,
            });
        }

        progressCallback(syncState);

        // Sync bookmarks before we download any new readables
        if (readablesBookmarkedSize > 0) {
            log_i("Syncing bookmarks");
            auto success = SindarinAPI::getInstance()->syncBookmarks(readablesBookmarked,
                                                                     readablesBookmarkedSize);
            if (success) {
                log_i("Bookmarks synced");
                this->resetReadablesBookmarksChanged(readables_);
                this->resetReadablesBookmarksChanged(articles_);
            } else {
                log_e("Failed to sync bookmarks");
                progressCallback(ContentSyncState{.state = ContentSyncState::State::Failure,
                                                  .message = "Failed to sync bookmarks"});
                delay(1000);
            }

            syncState.tasks[tasksIndex].percentage = 100;
            tasksIndex += 1;
            progressCallback(syncState);
        }

        for (auto &readable : readablesToDelete) {
            log_i("Deleting %s", readable.title.c_str());
            readableFilePurge(readable);
            this->removeReadable(readables_, readable);
            syncState.tasks[tasksIndex].percentage = 100;
            tasksIndex += 1;
        }

        progressCallback(syncState);

        for (auto &readable : readablesToDownload) {
            // Index into the progress tasks vector

            auto path = readableFilePathForWriting(readable);
            log_i("Downloading %s (file=%s, url=%s)", readable.title.c_str(), path.c_str(),
                  readable.remoteUrl().c_str());
            SHOW_HEAP();

            for (int t = 0; t < NUMBER_OF_READABLE_DOWNLOAD_TRIES; t++) {
                auto success = SindarinAPI::getInstance()->downloadReadable(
                    readable, this->fileSystem, path,
                    [progressCallback, &syncState, tasksIndex](int percentage) {
                        syncState.tasks[tasksIndex].percentage = percentage;
                        progressCallback(syncState);
                    });
                if (success) {
                    addReadable(readables_, readable);
                    log_d("File successfully downloaded.");
                    break;
                } else {
                    log_w("Failed to download file. Retrying...");
                    if (t == NUMBER_OF_READABLE_DOWNLOAD_TRIES - 1) {
                        log_e("Failed to download file.");
                        failedReadableDownload = true;
                    }
                }
            }

            tasksIndex += 1;
        }

        progressCallback(syncState);

        if (articlesToDelete.size() > 0) {
            for (int i = 0; i < articlesToDelete.size(); i++) {
                syncState.tasks[tasksIndex].percentage =
                    (float(i + 1) / articlesToDelete.size()) * 100;

                const auto &readable = articlesToDelete[i];
                log_i("Deleting %s", readable.title.c_str());
                readableFilePurge(readable);
                this->removeReadable(articles_, readable);
            }

            // All article deletions take a single task line item
            tasksIndex += 1;
        }

        progressCallback(syncState);

        // Clear the articlesSince cache since it will be set at the end of a successful run.
        // If we fail, then the next content sync will not have a cutoff date
        if (checkSyncResponse.articlesSince != "") {
            log_d("Clearing articlesSince cache since it will be set at the end of a successful "
                  "run.");
            this->prefs_.lastArticlesSince.set("");
        }

        // Save the articles that are new
        if (articlesToSave.size() > 0) {
            float pctPerArticle = 100.0 / articlesToSave.size();
            for (int i = 0; i < articlesToSave.size(); i++) {
                float currPercentage = (float)i * pctPerArticle;
                auto &readable = articlesToSave[i];
                for (int t = 0; t < NUMBER_OF_READABLE_DOWNLOAD_TRIES; t++) {
                    auto path = readableFilePathForWriting(readable);
                    auto success = SindarinAPI::getInstance()->downloadReadable(
                        readable, this->fileSystem, path,
                        [progressCallback, &syncState, tasksIndex, currPercentage,
                         pctPerArticle](int percentage) {
                            float pctThisReadable = (float)percentage * pctPerArticle / 100.0;
                            syncState.tasks[tasksIndex].percentage =
                                currPercentage + pctThisReadable;
                            progressCallback(syncState);
                        });
                    if (success) {
                        this->addReadable(articles_, readable);
                        log_d("File successfully downloaded.");
                        break;
                    } else {
                        log_w("Failed to download file. Retrying...");
                        if (t == NUMBER_OF_READABLE_DOWNLOAD_TRIES - 1) {
                            log_e("Failed to download file.");
                            failedArticleDownload = true;
                        }
                    }
                }
            }
            tasksIndex += 1;
        }

        progressCallback(syncState);

        // Store the metadata for both readables and articles
        if (!this->saveReadables(readables_, READABLES_PATH)) {
            log_e("Failed to save %s", READABLES_PATH);
            failedReadableDownload = true;
        }
        if (!this->saveReadables(articles_, ARTICLES_PATH)) {
            log_e("Failed to save %s", ARTICLES_PATH);
            failedArticleDownload = true;
        }

        // Store since date values for next sync
        if (failedArticleDownload || failedReadableDownload) {
            log_w("Not setting lastReadablesSince to %s and lastArticlesSince to %s due to failed "
                  "download",
                  checkSyncResponse.readablesSince.c_str(),
                  checkSyncResponse.articlesSince.c_str());
        } else {
            log_i("Setting lastReadablesSince to %s and lastArticlesSince to %s",
                  checkSyncResponse.readablesSince.c_str(),
                  checkSyncResponse.articlesSince.c_str());
        }
        if (checkSyncResponse.readablesSince != "" && !failedReadableDownload) {
            this->prefs_.lastReadablesSince.set(checkSyncResponse.readablesSince.c_str());
        }
        if (checkSyncResponse.articlesSince != "" && !failedArticleDownload) {
            this->prefs_.lastArticlesSince.set(checkSyncResponse.articlesSince.c_str());
        }

        // Update the since dates and try again
        readablesSince = this->readablesSince();
        articlesSince = this->articlesSince();
        request = CheckSyncRequest{
            .readablesSince = readablesSince,
            .articlesSince = articlesSince,
            .readablePlacemarks = {},
            .articlePlacemarks = {},
            .readablesArchived = {},
            .articlesArchived = {},
            .ptBattery = this->prefs_.lastPageTurnerBattery.get().value_or(0),
            .ptBuild = this->prefs_.lastPageTurnerBuild.get().value_or(""),
        };

        // Artificially delay so the user has the satisfaction of seeing the list done
        delay(1000);

        SindarinAPI::getInstance()->checkSync(request, &checkSyncResponse);

        // TODO: Force reload of rootUI instead of rebooting
        if (checkSyncResponse.synced) {
            log_i("Glasses are synced!");
            progressCallback(ContentSyncState{.state = ContentSyncState::State::Synced});
        } else {
            log_w("After running sync, local readables are still out of sync");
            // TODO: this isn't 100% right
            progressCallback(ContentSyncState{.state = ContentSyncState::State::Failure,
                                              .message = "Not in sync...restarting"});
        }
    });
}

void LocalStore::deleteReadables(bool restart) {
    deleteReadableFiles(readables_, restart);
    readables_.clear();
}

void LocalStore::deleteArticles(bool restart) {
    deleteReadableFiles(articles_, restart);
    articles_.clear();
}

void LocalStore::deleteReadableFiles(const ReadablesVector &readables, bool restart) {
    auto modelType = readablesModelType(const_cast<ReadablesVector &>(readables));
    std::string path = modelType == ReadablesModelType::Readable ? READABLES_PATH : ARTICLES_PATH;
    log_i("Clearing %s", path.c_str());
    bool removed = fileSystem.remove(path);
    if (!removed) {
        log_e("Failed to remove %s", path.c_str());
    }

    if (modelType == ReadablesModelType::Article) {
        prefs_.lastArticlesSince.set("");
    } else if (modelType == ReadablesModelType::Readable) {
        prefs_.lastReadablesSince.set("");
    }

    for (const Readable &readable : readables) {
        log_i("Removing %s \"%s\"", ToString(modelType).c_str(), readable.title.c_str());
        readableFilePurge(readable);
    }

    log_i("Resetting to guarantee %s are removed", ToString(modelType).c_str());

    if (restart) {
        esp_restart();
    }
}

auto LocalStore::saveReadables(const ReadablesVector &readables, const std::string &path) -> bool {
    // 2048 is probably a safe-ish upper bound per readable. I'm seeing about half that in my
    // testing so far
    auto doc = std::make_unique<SpiRamJsonDocument>(2048 * std::max(1, (int)readables.size()));

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
    File file = fileSystem.open(path, FILE_WRITE, true);
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

    std::string path;
    path = StringFormat("/%s/%s.txt.gz", type.c_str(), uuid.c_str());
    fileSystem.remove(path);

    path = StringFormat("/%s/%s.txt", type.c_str(), uuid.c_str());
    fileSystem.remove(path);

    path = StringFormat("/%s/%s.epub", type.c_str(), uuid.c_str());
    fileSystem.remove(path);

    if (readable.files.size() > 0) {
        path = StringFormat("/%s.gz", readable.file().name.c_str());
        fileSystem.remove(path);

        path = StringFormat("/%s", readable.file().name.c_str());
        fileSystem.remove(path);
    }

    path = StringFormat("/articles/%s.txt.gz", uuid.c_str());
    fileSystem.remove(path);

    path = StringFormat("/articles/%s.txt", uuid.c_str());
    fileSystem.remove(path);
}
