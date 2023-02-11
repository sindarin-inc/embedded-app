#include "LocalStore.hpp"

#include <esp_system.h>

#include "Communications/HtmlCleaner.hpp"
#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFi.hpp"
#include "Misc/Crypto.hpp"
#include "Misc/GUnzipStream.hpp"
#include "Misc/StringUtil.hpp"
#include "Models/Json.hpp"
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
        } else if (!fileSystem.exists(readable.localFilePath()) &&
                   !fileSystem.exists(readable.compressedLocalFilePath())) {
            log_w("Couldn't find readable with path %s", readable.localFilePath().c_str());
            continue;
        } else {
            int fileSize = -1;
            auto file = fileSystem.open(readable.localFilePath());
            if (file) {
                fileSize = file.size();
                file.close();
            } else {
                log_w("Failed to open readable %s", readable.localFilePath().c_str());
                // try opening the compressed file
                auto cf = fileSystem.open(readable.compressedLocalFilePath());
                if (!cf) {
                    log_w("Failed to open compressed readable %s",
                          readable.compressedLocalFilePath().c_str());
                    continue;
                }

                GUnzipStream gzStream(cf);
                fileSize = gzStream.size();
            }

            if (fileSize >= 0 && readable.files.size() && fileSize != readable.file().size) {
                log_w("File size mismatch for %s (%d on disk vs %d in local store), ignoring",
                      readable.localFilePath().c_str(), fileSize, readable.file().size);
                continue;
            }
            readables.push_back(readable);
        }
    }

    log_d("Loaded %zu readables from %s", readables.size(), path.c_str());
    return true;
}

auto LocalStore::readablesModelType(const ReadablesVector &readables) -> ReadablesModelType {
    return &readables == &articles_ ? ReadablesModelType::Article : ReadablesModelType::Readable;
}

auto LocalStore::hasReadable(const ReadablesVector &readables, const Readable &readable) -> bool {
    auto modelType = this->readablesModelType(const_cast<ReadablesVector &>(readables));
    for (auto r = readables.begin(); r != readables.end(); ++r) {
        if (r->uuid == readable.uuid && (fileSystem.exists(r->localFilePath()) ||
                                         fileSystem.exists(r->compressedLocalFilePath()))) {
            int fileSize = -1;
            auto file = fileSystem.open(r->localFilePath());
            if (file) {
                fileSize = file.size();
                file.close();
            } else {
                log_w("Checking for %s, but failed to open file: %s %s",
                      ToString(modelType).c_str(), r->localFilePath().c_str(), r->uuid.c_str());

                auto cf = fileSystem.open(r->compressedLocalFilePath());
                if (cf) {
                    GUnzipStream gs(cf);
                    fileSize = gs.size();
                } else {
                    log_w("failed to open compressed file %s",
                          r->compressedLocalFilePath().c_str());
                }
            }

            if (&readables == &readables_) {
                if (fileSize >= 0 && fileSize != readable.file().size) {
                    log_w("File size mismatch for %s (%d on disk vs %d in local store), ignoring",
                          readable.localFilePath().c_str(), fileSize, readable.file().size);
                    return false;
                } else {
                    return true;
                }
            } else if (&readables == &articles_) {
                return true;
            }
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
        CheckSyncResponse checkSyncResponse;
        CheckSyncRequest request =
            CheckSyncRequest{.readablesSince = readablesSince,
                             .articlesSince = articlesSince,
                             .readablePlacemarks = readablePlacemarks,
                             .articlePlacemarks = articlePlacemarks,
                             .readablesArchived = readablesArchived,
                             .articlesArchived = articlesArchived,
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

        if (readablesToDownload.size() == 0 && readablesToDelete.size() == 0 &&
            articlesToDelete.size() == 0 && articlesToSave.size() == 0) {
            log_i("No new content to download");
            progressCallback(ContentSyncState{.state = ContentSyncState::State::AlreadySynced});
            return;
        }

        // Clear the readablesSince cache since it will be set at the end of a successful run.
        // If we fail, then the next content sync will not have a cutoff date
        if (checkSyncResponse.readablesSince != "") {
            log_d("Clearing readablesSince cache since it will be set at the end of a "
                  "successful run");
            this->prefs_.lastReadablesSince.set("");
        }

        ContentSyncState syncState = {.state = ContentSyncState::State::Syncing};
        syncState.tasks.reserve(readablesToDownload.size());

        for (const auto &readable : readablesToDelete) {
            syncState.tasks.push_back(ContentSyncState::Task{
                .message = "Delete: " + readable.title,
                .percentage = 0,
            });
        }

        for (const auto &readable : readablesToDownload) {
            syncState.tasks.push_back(ContentSyncState::Task{
                .message = "Get: " + readable.title,
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

        for (int i = 0; i < readablesToDelete.size(); i++) {
            const auto &readable = readablesToDelete[i];
            log_i("Deleting %s", readable.title.c_str());
            this->fileSystem.remove(readable.localFilePath());
            this->fileSystem.remove(readable.compressedLocalFilePath());
            this->removeReadable(readables_, readable);
            syncState.tasks[i].percentage = 100;
        }

        progressCallback(syncState);

        for (int i = 0; i < readablesToDownload.size(); i++) {
            // Index into the progress tasks vector
            auto tasksIndex = readablesToDelete.size() + i;
            const auto &readable = readablesToDownload[i];
            log_i("Downloading %s (file=%s, url=%s)", readable.title.c_str(),
                  readable.localFilePath().c_str(), readable.remoteUrl().c_str());
            SHOW_HEAP();

            for (int t = 0; t < NUMBER_OF_READABLE_DOWNLOAD_TRIES; t++) {
                auto success = SindarinAPI::getInstance()->downloadReadable(
                    readable, this->fileSystem,
                    [progressCallback, &syncState, tasksIndex](int percentage) {
                        syncState.tasks[tasksIndex].percentage = percentage;
                        progressCallback(syncState);
                    },
                    prefs_.gzipReadables.get().value_or(false));
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
        }

        progressCallback(syncState);

        auto tasksIndex = readablesToDownload.size() + readablesToDelete.size();
        if (articlesToDelete.size() > 0) {
            for (int i = 0; i < articlesToDelete.size(); i++) {
                syncState.tasks[tasksIndex].percentage =
                    (float(i + 1) / articlesToDelete.size()) * 100;

                const auto &readable = articlesToDelete[i];
                log_i("Deleting %s", readable.title.c_str());
                this->fileSystem.remove(readable.localFilePath());
                this->fileSystem.remove(readable.compressedLocalFilePath());
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

            // create log directory in filesystem if it doesn't exist
            log_d("Creating directory /articles");
            if (!fileSystem.createDir("/articles")) {
                log_e("Could not create /articles directory");
            }

            float pctPerArticle = 100.0 / articlesToSave.size();
            for (int i = 0; i < articlesToSave.size(); i++) {
                float currPercentage = (float)i * pctPerArticle;
                auto &readable = articlesToSave[i];
                for (int t = 0; t < NUMBER_OF_READABLE_DOWNLOAD_TRIES; t++) {
                    auto success = SindarinAPI::getInstance()->downloadReadable(
                        readable, this->fileSystem,
                        [progressCallback, &syncState, tasksIndex, currPercentage,
                         pctPerArticle](int percentage) {
                            float pctThisReadable = (float)percentage * pctPerArticle / 100.0;
                            syncState.tasks[tasksIndex].percentage =
                                currPercentage + pctThisReadable;
                            progressCallback(syncState);
                        },
                        prefs_.gzipReadables.get().value_or(false));
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

auto LocalStore::saveArticleFile(const Readable &readable) -> bool {
    log_i("Storing article %s: %s", readable.title.c_str(), readable.localFilePath().c_str());

    // create log directory in filesystem if it doesn't exist
    log_d("Creating directory /articles");
    if (!fileSystem.createDir("/articles")) {
        log_e("Could not create /articles directory");
        return false;
    }

    // Sleep for 2 seconds
    delay(2000);

    log_d("Opening file for writing...");
    SHOW_HEAP();
    File file = fileSystem.open(readable.localFilePath(), FILE_WRITE, true);
    if (!file) {
        log_e("Failed to open %s", readable.localFilePath().c_str());
        return false;
    }
    SHOW_HEAP();

    const char *title = readable.title.c_str();
    file.write((const uint8_t *)title, strlen(title));
    file.write('\n');

    StripHtml(readable.content, file);

    file.flush();
    size_t docSize = file.size();
    file.close();

    log_d("Finished storing article %s, size %zu/%zu", readable.title.c_str(), docSize,
          (size_t)readable.content.size());

    return true;
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
        log_i("Removing %s \"%s\" (%s)", ToString(modelType).c_str(), readable.title.c_str(),
              readable.localFilePath().c_str());
        fileSystem.remove(readable.localFilePath());

        log_i("Removing %s \"%s\" (%s)", ToString(modelType).c_str(), readable.title.c_str(),
              readable.compressedLocalFilePath().c_str());
        fileSystem.remove(readable.compressedLocalFilePath());
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
