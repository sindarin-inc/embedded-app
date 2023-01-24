#include "LocalStore.hpp"

#include <esp_system.h>

#include "Communications/HtmlCleaner.hpp"
#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFi.hpp"
#include "Misc/Crypto.hpp"
#include "Misc/StringUtil.hpp"
#include "Models/Json.hpp"
#include "sindarin-debug.h"

#define READABLES_PATH "/readables.json"
#define ARTICLES_PATH "/articles.json"
enum { NUMBER_OF_READABLE_DOWNLOAD_TRIES = 3 };

auto LocalStore::begin() -> bool {
    auto loadedReadables = loadReadablesFromFile(readables_, std::string(READABLES_PATH));
    auto loadedArticles = loadReadablesFromFile(articles_, std::string(ARTICLES_PATH));

    return loadedReadables && loadedArticles;
}

auto LocalStore::loadReadablesFromFile(std::vector<Readable> &readables, const std::string &path)
    -> bool {
    log_d("Loading %ss from %s", readablesModelType(readables).c_str(), path.c_str());
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
        } else if (!fileSystem.exists(readable.localFilePath())) {
            log_w("Couldn't find readable with path %s", readable.localFilePath().c_str());
            continue;
        } else {
            auto file = fileSystem.open(readable.localFilePath());
            if (!file) {
                log_w("Failed to open readable %s", readable.localFilePath().c_str());
                continue;
            }
            // If the filesize is wrong, ignore it so we redownload it
            auto fileSize = file.size();
            file.close();
            if (readable.files.size() && fileSize != readable.file().size) {
                log_w("File size mismatch for %s (%zu on disk vs %d in local store), ignoring",
                      readable.localFilePath().c_str(), fileSize, readable.file().size);
                continue;
            }
            readables.push_back(readable);
        }
    }

    log_d("Loaded %zu readables from %s", readables.size(), path.c_str());
    return true;
}

auto LocalStore::readablesModelType(const std::vector<Readable> &readables) -> std::string {
    return &readables == &articles_ ? "article" : "readable";
}

auto LocalStore::hasReadable(const std::vector<Readable> &readables, const Readable &readable)
    -> bool {
    for (auto r = readables.begin(); r != readables.end(); ++r) {
        if (r->uuid == readable.uuid && fileSystem.exists(r->localFilePath())) {
            auto file = fileSystem.open(r->localFilePath());
            auto fileSize = file.size();
            file.close();

            if (&readables == &readables_) {
                if (fileSize != readable.file().size) {
                    log_w("File size mismatch for %s (%zu on disk vs %d in local store), ignoring",
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

void LocalStore::addReadable(std::vector<Readable> &readables, const Readable &readable) {
    readables.emplace_back(readable);
    return;
}

void LocalStore::removeReadable(std::vector<Readable> &readables, const Readable &readable) {
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

auto LocalStore::changedReadablePlacemarkQueryParams(const std::vector<Readable> &readables)
    -> std::vector<std::string> {
    std::vector<std::string> placemarks;
    std::string modelType =
        this->readablesModelType(const_cast<std::vector<Readable> &>(readables));

    for (auto &readable : readables) {
        if (readable.localOnly) {
            continue;
        }

        bool changed = prefs_.readablePlaceChanged.get(readable.shortenedUuid.c_str());
        if (changed) {
            uint32_t place = prefs_.readablePlace.get(readable.shortenedUuid.c_str());
            log_i("Placemark on readable %s %s has changed to %d", readable.title.c_str(),
                  readable.uuid.c_str(), place);
            placemarks.push_back(StringFormat("&%s_placemark_%s=%d", modelType.c_str(),
                                              readable.uuid.c_str(), place));
        }
    }

    return placemarks;
}

auto LocalStore::changedReadablesArchivedQueryParams(const std::vector<Readable> &readables)
    -> std::vector<std::string> {
    std::vector<std::string> archived;
    std::string modelType =
        this->readablesModelType(const_cast<std::vector<Readable> &>(readables));

    for (auto &readable : readables) {
        if (readable.localOnly) {
            continue;
        }

        bool changed = prefs_.readableArchivedChanged.get(readable.shortenedUuid.c_str());
        if (changed) {
            log_i("Archived %s %s %s", modelType.c_str(), readable.title.c_str(),
                  readable.uuid.c_str());
            archived.push_back(
                StringFormat("&%s_archive_%s=1", modelType.c_str(), readable.uuid.c_str()));
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

void LocalStore::resetReadablesPlacemarkChanged(const std::vector<Readable> &readables) {
    for (auto &readable : readables) {
        bool changed = prefs_.readablePlaceChanged.get(readable.shortenedUuid.c_str());
        if (!readable.localOnly && changed) {
            log_d("Resetting placemark changed for \"%s\" (now synced to server)",
                  readable.title.c_str());
            prefs_.readablePlaceChanged.set(readable.shortenedUuid.c_str(), false);
        }
    }
}

void LocalStore::resetReadablesArchivedChanged(const std::vector<Readable> &readables) {
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
        CheckSyncRequest request = CheckSyncRequest{
            .readablesSince = readablesSince,
            .articlesSince = articlesSince,
            .readablePlacemarks = readablePlacemarks,
            .articlePlacemarks = articlePlacemarks,
            .readablesArchived = readablesArchived,
            .articlesArchived = articlesArchived,
            .ptBattery = this->prefs_.lastPageTurnerBattery.get().value_or(0),
            .ptBuild = this->prefs_.lastPageTurnerBuild.get().value_or(""),
        };

        auto httpCode = SindarinAPI::getInstance()->checkSync(request, &checkSyncResponse);
        if (httpCode != HTTP_CODE_OK) {
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

        // Clear the readablesSince cache since it will be set at the end of a successful run.
        // If we fail, then the next content sync will not have a cutoff date
        if (checkSyncResponse.readablesSince != "") {
            this->prefs_.lastReadablesSince.set("");
        }

        progressCallback(ContentSyncState{.state = ContentSyncState::State::LoadingTasks});

        // If out of sync, download any new readables.
        std::vector<Readable> readablesToDownload = {};
        std::vector<Readable> readablesToDelete = {};
        std::vector<Readable> articlesToDelete = {};
        std::vector<Readable> articlesToSave = {};

        // Now look through all the readables here, and download any that we don't have
        // already
        for (auto &userReadable : checkSyncResponse.userReadables) {
            log_d("User readable %s (%s)", userReadable.readable.title.c_str(),
                  userReadable.readable.file().shaHash.c_str());
            if (this->processReadablePlacemarksFromServer(userReadable.readable,
                                                          userReadable.placemark)) {
                log_i("Placemark for %s updated to %d", userReadable.readable.title.c_str(),
                      userReadable.placemark);
            }
            if (userReadable.isArchived) {
                log_w("Readable %s is archived, will delete", userReadable.readable.title.c_str());
                // TODO: Store these articles in an accessible archive instead of just deleting them
                // and ignoring
                readablesToDelete.push_back(userReadable.readable);
                continue;
            }
            if (this->hasReadable(readables_, userReadable.readable)) {
                log_w("Already have %s", userReadable.readable.title.c_str());
                continue;
            }

            readablesToDownload.push_back(userReadable.readable);
        }
        this->resetReadablesPlacemarkChanged(readables_);
        this->resetReadablesArchivedChanged(readables_);

        // Look through all the articles, deleting archived ones and downloading new ones
        for (auto &userReadable : checkSyncResponse.userArticles) {
            log_d("Fetched user article: %s (%zu)", userReadable.readable.title.c_str(),
                  userReadable.readable.content.size());
            if (this->processReadablePlacemarksFromServer(userReadable.readable,
                                                          userReadable.placemark)) {
                log_i("Placemark for %s updated to %d", userReadable.readable.title.c_str(),
                      userReadable.placemark);
            }
            if (userReadable.isArchived) {
                log_i("Article %s is archived", userReadable.readable.title.c_str());
                // TODO: Store these articles in an accessible archive instead of just deleting them
                // and ignoring
                articlesToDelete.push_back(userReadable.readable);
                continue;
            }
            if (this->hasReadable(articles_, userReadable.readable)) {
                // Should never get here as articlesSince should hide these articles
                log_w("Already have %s", userReadable.readable.title.c_str());
                continue;
            }

            articlesToSave.push_back(userReadable.readable);
        }
        this->resetReadablesPlacemarkChanged(articles_);
        this->resetReadablesArchivedChanged(articles_);

        // TODO: Probably these should be set operations
        log_d("Readables to download: %zu", readablesToDownload.size());
        log_d("Readables to delete: %zu", readablesToDelete.size());
        log_d("Articles to delete: %zu", articlesToDelete.size());

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

            for (int t = 0; t < NUMBER_OF_READABLE_DOWNLOAD_TRIES; t++) {
                auto success = SindarinAPI::getInstance()->downloadReadable(
                    readable, this->fileSystem,
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
                this->removeReadable(articles_, readable);
            }

            // All article deletions take a single task line item
            tasksIndex += 1;
        }

        progressCallback(syncState);

        // Clear the articlesSince cache since it will be set at the end of a successful run.
        // If we fail, then the next content sync will not have a cutoff date
        if (checkSyncResponse.articlesSince != "") {
            this->prefs_.lastArticlesSince.set("");
        }

        // Save the articles that are new
        if (articlesToSave.size() > 0) {
            for (int i = 0; i < articlesToSave.size(); i++) {
                syncState.tasks[tasksIndex].percentage =
                    (float(i + 1) / articlesToSave.size()) * 100;
                progressCallback(syncState);

                auto &readable = articlesToSave[i];

                if (this->saveArticleFile(readable)) {
                    this->addReadable(articles_, readable);
                } else {
                    failedArticleDownload = true;
                    log_e("Failed to save article \"%s\" %s", readable.title.c_str(),
                          readable.uuid.c_str());
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
                                              .message = "Not in sync"});
        }
    });
}

auto LocalStore::saveArticleFile(const Readable &readable) -> bool {
    log_i("Storing article %s: %s", readable.title.c_str(), readable.localFilePath().c_str());
    // create log directory in filesystem if it doesn't exist
    if (!fileSystem.createDir("/articles")) {
        log_e("Could not create /articles directory");
        return false;
    }

    File file = fileSystem.open(readable.localFilePath(), FILE_WRITE, true);
    if (!file) {
        log_e("Failed to open %s", readable.localFilePath().c_str());
        return false;
    }

    const char *title = readable.title.c_str();
    file.write((const uint8_t *)title, strlen(title));
    file.write('\n');

    StripHtml(readable.content, file);

    file.flush();
    size_t docSize = file.size();
    file.close();

    log_d("Finished storing article %s, size %zu/%zu", readable.title.c_str(), docSize,
          readable.content.size());
    return true;
}

void LocalStore::clearArticlesFile(bool restart) {
    log_i("Clearing %s", ARTICLES_PATH);
    bool removed = fileSystem.remove(ARTICLES_PATH);
    if (!removed) {
        log_e("Failed to remove %s", ARTICLES_PATH);
    }

    prefs_.lastArticlesSince.set("");

    log_i("Resetting to guarantee articles are removed");

    if (restart) {
        esp_restart();
    }
}

void LocalStore::clearReadablesFile(bool restart) {
    log_i("Clearing %s", READABLES_PATH);
    bool removed = fileSystem.remove(READABLES_PATH);
    if (!removed) {
        log_e("Failed to remove %s", READABLES_PATH);
    }

    prefs_.lastReadablesSince.set("");

    log_i("Resetting to guarantee readables are removed");
    if (restart) {
        esp_restart();
    }
}

auto LocalStore::saveReadables(const std::vector<Readable> &readables, const std::string &path)
    -> bool {
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
