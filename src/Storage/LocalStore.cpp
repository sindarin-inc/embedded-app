#include "LocalStore.hpp"

#include "Communications/HtmlCleaner.hpp"
#include "Communications/SindarinAPI.hpp"
#include "Misc/Crypto.hpp"
#include "Misc/StringUtil.hpp"
#include "Models/Json.hpp"
#include "sindarin-debug.h"

#define READABLES_PATH "/readables.json"
#define ARTICLES_PATH "/articles.json"

auto LocalStore::begin() -> bool {
    auto loadedReadables = loadReadablesFromFile(readables_, std::string(READABLES_PATH));
    auto loadedArticles = loadReadablesFromFile(articles_, std::string(ARTICLES_PATH));

    return loadedReadables && loadedArticles;
}

auto LocalStore::loadReadablesFromFile(std::vector<Readable> &readables, const std::string &path)
    -> bool {
    log_d("Loading %s", path.c_str());
    File readablesFile = fileSystem.open(path);
    if (!readablesFile) {
        log_w("Failed to open %s", path.c_str());
        // Fail positively since it's ok to not have any when you first use the glasses
        return true;
    }

    // Grab a minimum amount of ram, even if the readables file is empty
    auto docSize = std::max(100, (int)readablesFile.size() * 2);
    auto *doc = new SpiRamJsonDocument(docSize);
    auto error = deserializeJson(*doc, readablesFile);
    readablesFile.close();

    if (error.code() != DeserializationError::Ok) {
        log_e("Failed to deserialize readables file: %s", error.c_str());
        delete doc;
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
            // If the filesize is wrong, ignore it so we redownload it
            auto fileSize = file.size();
            file.close();
            if (fileSize != readable.file().size) {
                log_w("File size mismatch for %s (%zu on disk vs %d in local store), ignoring",
                      readable.localFilePath().c_str(), fileSize, readable.file().size);
                continue;
            }
            log_d("Found local readable %s (%s)", readable.title.c_str(),
                  readable.localFilePath().c_str());
            readables.push_back(readable);
        }
    }

    delete doc;
    return true;
}

auto LocalStore::hasReadable(const std::vector<Readable> &readables, Readable &readable) -> bool {
    for (auto &r : readables) {
        if (r.uuid == readable.uuid && fileSystem.exists(r.localFilePath())) {
            auto file = fileSystem.open(r.localFilePath());
            auto fileSize = file.size();
            file.close();
            if (fileSize != readable.file().size) {
                log_w("File size mismatch for %s (%zu on disk vs %d in local store), ignoring",
                      readable.localFilePath().c_str(), fileSize, readable.file().size);
                return false;
            } else {
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
    for (auto &readable : readables) {
        if (readable.localOnly) {
            continue;
        }
        bool changed = prefs_.readablePlaceChanged.get(readable.shortenedUuid.c_str());
        if (changed) {
            uint32_t place = prefs_.readablePlace.get(readable.shortenedUuid.c_str());
            log_i("Placemark on readable %s has changed to %d", readable.uuid.c_str(), place);
            std::string readableGroup =
                std::addressof(readables) == std::addressof(articles_) ? "articles" : "readables";
            placemarks.push_back(StringFormat("&%s_placemark_%s=%d", readableGroup.c_str(),
                                              readable.uuid.c_str(), place));
        }
    }

    return placemarks;
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
            log_d("Resetting placemark changed for \"%s\" (synced to server)",
                  readable.title.c_str());
            prefs_.readablePlaceChanged.set(readable.shortenedUuid.c_str(), 0);
        }
    }
}

void LocalStore::syncFromRemote(
    const std::function<void(ContentSyncState state)> &progressCallback) {

    // Grab the token here from prefs if set
    auto token = prefs_.cloudToken.get();
    if (!token.has_value()) {
        log_e("No access token set");
        progressCallback(ContentSyncState{.state = ContentSyncState::State::Failure,
                                          .message = "No access token set"});
        return;
    }

    // Uncomment below line to steal your access token for local development
    // log_w("Access token: %s", token.value().c_str());

    SindarinAPI::getInstance()->setAccessToken(token.value());
    progressCallback(ContentSyncState{.state = ContentSyncState::State::CheckingSync});

    // Run this work on a separate thread, since it's long running
    auto queue = DispatchGetCore0BackgroundQueue();
    queue->dispatch([this, progressCallback] {
        auto readablesSince = this->readablesSince();
        auto articlesSince = this->articlesSince();
        auto readablePlacemarks = this->changedReadablePlacemarkQueryParams(readables_);
        auto articlePlacemarks = this->changedReadablePlacemarkQueryParams(articles_);
        CheckSyncResponse checkSyncResponse;
        CheckSyncRequest request = CheckSyncRequest{
            .readablesSince = readablesSince,
            .articlesSince = articlesSince,
            .readablePlacemarks = readablePlacemarks,
            .articlePlacemarks = articlePlacemarks,
            .ptBattery = this->prefs_.lastPageTurnerBattery.get().value_or(0),
            .ptBuild = this->prefs_.lastPageTurnerBuild.get().value_or(""),
        };

        std::vector<UserReadable> userReadables;
        std::vector<UserReadable> userArticles;
        auto httpCode = SindarinAPI::getInstance()->checkSync(request, &checkSyncResponse);
        if (httpCode != HTTP_CODE_OK) {
            log_e("Failed to check sync (HTTP %d)", httpCode);
            progressCallback(ContentSyncState{.state = ContentSyncState::State::Failure,
                                              .message = "Failed to check sync"});
            return;
        }
        if (checkSyncResponse.synced) {
            log_i("Client is in sync");
            progressCallback(ContentSyncState{.state = ContentSyncState::State::Synced});
            return;
        }

        progressCallback(ContentSyncState{.state = ContentSyncState::State::LoadingTasks});

        // Store since date values for next sync
        this->prefs_.lastReadablesSince.set(checkSyncResponse.readablesSince);
        this->prefs_.lastArticlesSince.set(checkSyncResponse.articlesSince);

        // log_i("Not synced, downloading readables");
        // std::vector<UserReadable> userReadables;
        // httpCode = SindarinAPI::getInstance()->getUserReadables(&userReadables);
        // if (httpCode != HTTP_CODE_OK) {
        //     log_e("Failed to get readables (HTTP %d)", httpCode);
        //     progressCallback(ContentSyncState{.state = ContentSyncState::State::Failure,
        //                                       .message = "Failed to load list"});
        //     return;
        // }

        // If out of sync, download any new readables.
        std::vector<Readable> readablesToDownload;

        // Now look through all the readables here, and download any that we don't have
        // already
        for (auto &userReadable : checkSyncResponse.userReadables) {
            log_i("User readable %s (%s)", userReadable.readable.title.c_str(),
                  userReadable.readable.file().shaHash.c_str());
            if (this->processReadablePlacemarksFromServer(userReadable.readable,
                                                          userReadable.placemark)) {
                log_i("Placemark for %s updated to %d", userReadable.readable.title.c_str(),
                      userReadable.placemark);
            }
            if (this->hasReadable(readables_, userReadable.readable)) {
                log_i("Already have %s", userReadable.readable.title.c_str());
                continue;
            }

            readablesToDownload.push_back(userReadable.readable);
        }
        this->resetReadablesPlacemarkChanged(readables_);

        // TODO: Probably these should be set operations
        std::vector<Readable> readablesToDelete;
        for (auto &readable : readables_) {
            if (!readable.localOnly && std::find_if(userReadables.begin(), userReadables.end(),
                                                    [&readable](const UserReadable &userReadable) {
                                                        return userReadable.readable.uuid ==
                                                               readable.uuid;
                                                    }) == userReadables.end()) {
                log_i("Will delete %s", readable.title.c_str());
                readablesToDelete.push_back(readable);
            }
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

            auto success = SindarinAPI::getInstance()->downloadReadable(
                readable, this->fileSystem,
                [progressCallback, &syncState, tasksIndex](int percentage) {
                    syncState.tasks[tasksIndex].percentage = percentage;
                    progressCallback(syncState);
                });
            if (success) {
                addReadable(readables_, readable);
                log_d("File successfully downloaded.");
            }
        }

        progressCallback(syncState);

        // Save the articles that are new
        for (auto &userReadable : checkSyncResponse.userArticles) {
            log_i("User article %s (%s)", userReadable.readable.title.c_str(),
                  userReadable.readable.file().shaHash.c_str());
            if (this->processReadablePlacemarksFromServer(userReadable.readable,
                                                          userReadable.placemark)) {
                log_i("Placemark for %s updated to %d", userReadable.readable.title.c_str(),
                      userReadable.placemark);
            }
            if (this->hasReadable(articles_, userReadable.readable)) {
                log_i("Already have %s", userReadable.readable.title.c_str());
                continue;
            }

            this->saveArticleFile(userReadable.readable);
            this->addReadable(articles_, userReadable.readable);
        }
        this->resetReadablesPlacemarkChanged(articles_);

        // Artificially delay so the user has the satisfaction of seeing the list done
        delay(1000);

        // Update the since dates and try again
        readablesSince = this->readablesSince();
        articlesSince = this->articlesSince();
        request.readablesSince = readablesSince;
        request.articlesSince = articlesSince;
        SindarinAPI::getInstance()->checkSync(request, &checkSyncResponse);

        if (!this->saveReadables(readables_, READABLES_PATH)) {
            log_e("Failed to save %s", READABLES_PATH);
        }
        if (!this->saveReadables(articles_, ARTICLES_PATH)) {
            log_e("Failed to save %s", READABLES_PATH);
        }

        // TODO: Force reload of rootUI instead of rebooting
        if (checkSyncResponse.synced) {
            log_i("Synced, rebooting");
            esp_restart();
            // Only for the sim
            progressCallback(ContentSyncState{.state = ContentSyncState::State::Synced});
        } else {
            log_w("After running sync, local readables are still out of sync");
            // TODO: this isn't 100% right
            progressCallback(ContentSyncState{.state = ContentSyncState::State::Failure,
                                              .message = "Not in sync"});
        }
    });
}

void LocalStore::saveArticleFile(const Readable &readable) {
    log_i("Storing article %s", readable.title.c_str());

    auto path = StringFormat("/articles/%s.txt", readable.uuid.c_str());
    File file = fileSystem.open(path.c_str(), FILE_WRITE);
    const char *title = readable.title.c_str();
    file.write((const uint8_t *)title, strlen(title));
    file.write('\n');

    StripHtml(readable.content, file);

    file.flush();
    file.close();
}

auto LocalStore::saveReadables(const std::vector<Readable> &readables, const std::string &path)
    -> bool {
    // 2048 is probably a safe-ish upper bound per readable. I'm seeing about half that in my
    // testing so far
    auto *doc = new SpiRamJsonDocument(2048 * std::max(1, (int)readables.size()));

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
    File file = fileSystem.open(path, FILE_WRITE);
    if (!file) {
        log_e("Failed to open file for writing");
        return false;
    }

    serializeJsonPretty(*doc, file);
    delete doc;
    file.close();

    return true;
};
