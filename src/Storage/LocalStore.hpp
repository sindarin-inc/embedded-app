#pragma once

#include <algorithm>
#include <vector>

#include "Misc/Dispatch.hpp"
#include "Models/Readable.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"

struct ContentSyncState {
    enum State {
        Idle,
        ConnectingWifi,
        CheckingSync,
        LoadingTasks,
        Syncing,
        Synced,
        Failure,
    } state = Idle;

    struct Task {
        std::string message;
        int percentage;
    };

    std::vector<Task> tasks;
    std::string message;
};

//! Interface for local storage of structured data
class LocalStore {
public:
    LocalStore(FileSystem &fileSystem, PreferencesStore &prefs)
        : fileSystem(fileSystem), prefs_(prefs){};

    auto begin() -> bool;
    auto loadReadablesFromFile(std::vector<Readable> &readables, const std::string &path) -> bool;

    // Convenience methods
    auto hasReadable(const std::vector<Readable> &readables, Readable &readable) -> bool;
    auto readables() -> std::vector<Readable> & { return readables_; };
    auto articles() -> std::vector<Readable> & { return articles_; };
    void addReadable(std::vector<Readable> &readables, const Readable &readable);
    void removeReadable(std::vector<Readable> &readables, const Readable &readable);
    auto readablesSince() -> std::string;
    auto articlesSince() -> std::string;

    auto changedReadablePlacemarkQueryParams(const std::vector<Readable> &readables)
        -> std::vector<std::string>;
    auto comparePlacemark(const std::vector<Readable> &readables, const Readable &readable,
                          const uint32_t placemark) -> bool;
    void resetReadablePlacemarkChanged(const std::vector<Readable> &readables);

    void syncFromRemote(const std::function<void(ContentSyncState state)> &progressCallback);

    auto saveReadables(const std::vector<Readable> &readables, const std::string &path) -> bool;

    FileSystem &fileSystem;

private:
    std::vector<Readable> readables_;
    std::vector<Readable> articles_;

    PreferencesStore &prefs_;
};
