#include "LocalStore.hpp"

#include "Communications/SindarinAPI.hpp"
#include "Misc/Crypto.hpp"
#include "Misc/StringUtil.hpp"
#include "Models/Json.hpp"
#include "sindarin-debug.h"

auto LocalStore::begin() -> bool {
    log_d("Loading readables.json");
    File readablesFile = fileSystem.open("/readables.json");
    if (!readablesFile) {
        log_w("Failed to open readables.json");
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
        log_w("No readables found in readables.json");
        return true;
    }
    readables_.reserve(readablesJson.size());

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
            readables_.push_back(readable);
        }
    }

    delete doc;
    return true;
}

auto LocalStore::hasReadable(Readable &readable) -> bool {
    for (auto &r : readables_) {
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

void LocalStore::addReadable(const Readable &readable) {
    readables_.emplace_back(readable);
    return;
}

void LocalStore::removeReadable(const Readable &readable) {
    for (auto it = readables_.begin(); it != readables_.end(); ++it) {
        if (it->uuid == readable.uuid) {
            readables_.erase(it);
            return;
        }
    }
}

auto LocalStore::calculateReadablesSha() -> std::string {
    if (readables_.size() == 0) {
        log_w("No readables");
        return "";
    }

    std::vector<std::string> shas;
    for (auto &readable : readables_) {
        // Skip local-only readables for the SHA calculation
        if (!readable.localOnly) {
            shas.push_back(readable.files[0].shaHash);
        }
    }

    // Sort all SHA strings alphabetically
    std::sort(shas.begin(), shas.end());

    log_d("SHA strings:");
    for (auto &sha : shas) {
        log_d("%s", sha.c_str());
    }

    // Join all SHA strings together
    // C++20 Version
    // std::string shasOfShas = std::accumulate(shas.begin(), shas.end(), std::string(""));
    std::string allShas;
    for (auto &sha : shas) {
        allShas += sha;
    }

    auto shaOfShas = CalculateStringHash(allShas);

    log_d("SHA of SHA strings: %s", shaOfShas.c_str());

    return shaOfShas;
}

auto LocalStore::readablePlacemarksChanged() -> std::vector<std::string> {
    std::vector<std::string> placemarks;
    for (auto &readable : readables_) {
        if (!readable.localOnly) {
            bool changed = prefs_.readablePlaceChanged.get(readable.placemarkUuid.c_str());
            if (changed) {
                uint32_t place = prefs_.readablePlace.get(readable.placemarkUuid.c_str());
                log_i("Placemark on readable %s has changed to %d", readable.uuid.c_str(), place);
                placemarks.push_back(
                    StringFormat("&placemark_%s=%d", readable.uuid.c_str(), place));
            }
        }
    }

    return placemarks;
}

void LocalStore::mergePlacemarksFromServer(const std::vector<Placemark> &placemarks) {
    for (auto &placemark : placemarks) {
        const auto readable = std::find_if(readables_.begin(), readables_.end(),
                                           [&placemark](const Readable &readable) {
                                               return placemark.readableUuid == readable.uuid;
                                           });

        if (readable == readables_.end()) {
            log_e("Could not find readable with UUID %s", placemark.readableUuid.c_str());
            return;
        }

        uint32_t onDevicePlace = prefs_.readablePlace.get(readable->placemarkUuid.c_str());
        bool changed = prefs_.readablePlaceChanged.get(readable->placemarkUuid.c_str());

        if (placemark.placemark == onDevicePlace) {
            // If the placemark on the server is the same as the one on the device, don't change it
        } else if (placemark.placemark > onDevicePlace && !changed) {
            log_i("Updating placemark for \"%s\" from device value of %d to server "
                  "value of %d",
                  readable->title.c_str(), onDevicePlace, placemark.placemark);
            prefs_.readablePlace.set(readable->placemarkUuid.c_str(), placemark.placemark);
        } else if (placemark.placemark != 0) {
            log_i("Not updating placemark for \"%s\" from device value of %d to server "
                  "value of %d. Changed: %d",
                  readable->title.c_str(), onDevicePlace, placemark.placemark, changed);
        }
    }

    this->resetReadablePlacemarkChanged();
}

void LocalStore::resetReadablePlacemarkChanged() {
    for (auto &readable : readables_) {
        if (!readable.localOnly &&
            prefs_.readablePlaceChanged.get(readable.placemarkUuid.c_str())) {
            log_d("Resetting placemark changed for \"%s\" (synced to server)",
                  readable.title.c_str());
            prefs_.readablePlaceChanged.set(readable.placemarkUuid.c_str(), false);
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
    SindarinAPI::getInstance()->setAccessToken(token.value());
    progressCallback(ContentSyncState{.state = ContentSyncState::State::CheckingSync});

    // Run this work on a separate thread, since it's long running
    auto queue = DispatchGetCore0BackgroundQueue();
    queue->dispatch([this, progressCallback] {
        auto readablesSha = this->calculateReadablesSha();
        auto placemarks = this->readablePlacemarksChanged();
        CheckSyncResponse checkSyncResponse;
        auto httpCode =
            SindarinAPI::getInstance()->checkSync(readablesSha, placemarks, &checkSyncResponse);
        if (httpCode != HTTP_CODE_OK) {
            log_e("Failed to check sync (HTTP %d)", httpCode);
            progressCallback(ContentSyncState{.state = ContentSyncState::State::Failure,
                                              .message = "Failed to check sync"});
            return;
        }
        if (checkSyncResponse.synced) {
            this->mergePlacemarksFromServer(checkSyncResponse.placemarks);
            log_i("Client is in sync");
            progressCallback(ContentSyncState{.state = ContentSyncState::State::Synced});
            return;
        }

        progressCallback(ContentSyncState{.state = ContentSyncState::State::LoadingTasks});

        log_i("Not synced, downloading readables");
        std::vector<UserReadable> userReadables;
        httpCode = SindarinAPI::getInstance()->getUserReadables(&userReadables);
        if (httpCode != HTTP_CODE_OK) {
            log_e("Failed to get readables (HTTP %d)", httpCode);
            progressCallback(ContentSyncState{.state = ContentSyncState::State::Failure,
                                              .message = "Failed to load list"});
            return;
        }

        // If out of sync, download any new readables.

        std::vector<Readable> readablesToDownload;

        // Now look through all the readables here, and download any that we don't have
        // already
        for (auto userReadable : userReadables) {
            log_i("User readable %s (%s)", userReadable.readable.title.c_str(),
                  userReadable.readable.file().shaHash.c_str());
            if (this->hasReadable(userReadable.readable)) {
                log_i("Already have %s", userReadable.readable.title.c_str());
                continue;
            }

            readablesToDownload.push_back(userReadable.readable);
        }

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
            this->removeReadable(readable);
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
                addReadable(readable);
                log_d("File successfully downloaded.");
            }
        }

        progressCallback(syncState);

        // Artificially delay so the user has the satisfaction of seeing the list done
        delay(1000);

        // Update the SHA and try again
        readablesSha = this->calculateReadablesSha();
        SindarinAPI::getInstance()->checkSync(readablesSha, placemarks, &checkSyncResponse);

        if (!this->save()) {
            log_e("Failed to save readables.json");
        }

        this->mergePlacemarksFromServer(checkSyncResponse.placemarks);

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

auto LocalStore::save() -> bool {
    // 2048 is probably a safe-ish upper bound per readable. I'm seeing about half that in my
    // testing so far
    auto *doc = new SpiRamJsonDocument(2048 * std::max(1, (int)readables_.size()));

    // Create a key named readables of type array
    JsonArray readablesJson = doc->createNestedArray("readables");

    log_d("Saving %zu readables in readables.json", readables_.size());
    for (const auto &readable : readables_) {
        readablesJson.add(readable);
    }

    if (doc->overflowed()) {
        log_e("Out of space in SpiRamJsonDocument for readables.json");
        return false;
    }

    // Write doc to disk
    File file = fileSystem.open("/readables.json", FILE_WRITE);
    if (!file) {
        log_e("Failed to open file for writing");
        return false;
    }

    serializeJsonPretty(*doc, file);
    delete doc;
    file.close();

    return true;
};
