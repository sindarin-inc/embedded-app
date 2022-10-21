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
        if (r.id == readable.id && fileSystem.exists(r.localFilePath())) {
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

void LocalStore::addReadable(Readable &readable) {
    readables_.emplace_back(readable);
    return;
}

void LocalStore::removeReadable(const Readable &readable) {
    for (auto it = readables_.begin(); it != readables_.end(); ++it) {
        if (it->id == readable.id) {
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

void LocalStore::syncFromRemote(
    const std::function<void(std::string message, int percentage)> &progressCallback,
    const std::function<void(bool success)> &doneCallback) {

    // Grab the token here from prefs if set
    auto token = prefs_.cloudToken.get();
    if (!token.has_value()) {
        log_e("No access token set");
        doneCallback(false);
    }
    SindarinAPI::getInstance()->setAccessToken(token.value());

    // Run this work on a separate thread, since it's long running
    auto queue = DispatchGetCore0BackgroundQueue();
    queue->dispatch([this, progressCallback, doneCallback] {
        auto readablesSha = this->calculateReadablesSha();

        progressCallback("Checking Sync", 0);
        CheckSyncResponse checkSyncResponse;
        auto httpCode = SindarinAPI::getInstance()->checkSync(readablesSha, &checkSyncResponse);
        if (httpCode != HTTP_CODE_OK) {
            log_e("Failed to check sync (HTTP %d)", httpCode);
            doneCallback(false);
            return;
        }
        if (checkSyncResponse.synced) {
            log_i("Client is in sync");
            doneCallback(true);
            return;
        }

        progressCallback("Loading list", 1);
        log_i("Not synced, downloading readables");
        std::vector<UserReadable> userReadables;
        httpCode = SindarinAPI::getInstance()->getReadablesUsersAll(&userReadables);
        if (httpCode != HTTP_CODE_OK) {
            log_e("Failed to get readables (HTTP %d)", httpCode);
            doneCallback(false);
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
                                                        return userReadable.readable.id ==
                                                               readable.id;
                                                    }) == userReadables.end()) {
                log_i("Will delete %s", readable.title.c_str());
                readablesToDelete.push_back(readable);
            }
        }

        for (auto readable : readablesToDownload) {
            log_i("Downloading %s (file=%s, url=%s)", readable.title.c_str(),
                  readable.localFilePath().c_str(), readable.remoteUrl().c_str());

            // TODO: Is it weird for local store to call sindarin api and sindarin api to call local
            // store? Probably
            auto success = SindarinAPI::getInstance()->downloadReadable(
                readable, this->fileSystem, [progressCallback, readable](int percentage) {
                    progressCallback(StringFormat("Download %s", readable.title.c_str()),
                                     percentage);
                });
            if (success) {
                addReadable(readable);
                log_d("File successfully downloaded.");
            }
        }

        for (const auto &readable : readablesToDelete) {
            log_i("Deleting %s", readable.title.c_str());
            this->fileSystem.remove(readable.localFilePath());
            this->removeReadable(readable);
        }

        // Update the SHA and try again
        readablesSha = this->calculateReadablesSha();
        SindarinAPI::getInstance()->checkSync(readablesSha, &checkSyncResponse);

        if (!this->save()) {
            log_e("Failed to save readables.json");
        }

        // TODO: Force reload of rootUI instead of rebooting
        if (checkSyncResponse.synced) {
            log_i("Synced, rebooting");
            esp_restart();
        }

        log_w("After running sync, local readables are still out of sync");
        doneCallback(false);
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
