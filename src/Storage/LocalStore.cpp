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
        } else if (fileSystem.exists(readable.localFilePath())) {
            log_d("Found local readable %s (%s)", readable.title.c_str(),
                  readable.localFilePath().c_str());
            readables_.push_back(readable);
        } else {
            log_w("Couldn't find readable with path %s", readable.localFilePath().c_str());
        }
    }

    return true;
}

auto LocalStore::hasReadable(Readable &readable) -> bool {
    for (auto &r : readables_) {
        if (r.id == readable.id && fileSystem.exists(r.files[0].name)) {
            return true;
        }
    }
    return false;
}

void LocalStore::addReadable(Readable &readable) {
    readables_.emplace_back(readable);
    return;
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

auto LocalStore::syncFromRemote() -> bool {
    auto readablesSha = this->calculateReadablesSha();

    // Grab the token here from prefs if set
    auto token = prefs_.cloudToken.get();
    if (!token.has_value()) {
        log_e("No access token set");
        return false;
    }
    SindarinAPI::getInstance()->setAccessToken(token.value());

    CheckSyncResponse checkSyncResponse;
    SindarinAPI::getInstance()->checkSync(readablesSha, &checkSyncResponse);
    // TODO: We should bail if this fails to connect to the server
    if (checkSyncResponse.synced) {
        return true;
    }

    log_i("Not synced, downloading readables");
    std::vector<UserReadable> userReadables;
    SindarinAPI::getInstance()->getReadablesUsersAll(&userReadables);

    // If out of sync, download any new readables.

    // Now look through all the readables here, and download any that we don't have
    // already
    for (auto userReadable : userReadables) {
        log_i("User readable %s (%s)", userReadable.readable.title.c_str(),
              userReadable.readable.file().shaHash.c_str());
        if (this->hasReadable(userReadable.readable)) {
            log_i("Already have %s", userReadable.readable.title.c_str());
            continue;
        }

        log_i("Downloading %s (file=%s, url=%s)", userReadable.readable.title.c_str(),
              userReadable.readable.localFilePath().c_str(),
              userReadable.readable.remoteUrl().c_str());

        // TODO: Is it weird for local store to call sindarin api and sindarin api to call local
        // store? Probably
        SindarinAPI::getInstance()->downloadReadable(userReadable.readable, *this);
    }

    // TODO: Delete local readables that are no longer on the server

    // Update the SHA and try again
    readablesSha = this->calculateReadablesSha();
    SindarinAPI::getInstance()->checkSync(readablesSha, &checkSyncResponse);
    if (!checkSyncResponse.synced) {
        log_w("After running sync, local readables are still out of sync");
    }

    if (!this->save()) {
        log_e("Failed to save readables.json");
    }

    // TODO: Force reload of rootUI instead of rebooting
    esp_restart();

    return true;
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
    file.close();

    return true;
};
