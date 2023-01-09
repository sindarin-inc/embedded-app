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
        AlreadySynced,
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
    auto readablesModelType(const std::vector<Readable> &readables) -> std::string;
    auto hasReadable(const std::vector<Readable> &readables, const Readable &readable) -> bool;
    auto readables() -> std::vector<Readable> & { return readables_; };
    auto articles() -> std::vector<Readable> & { return articles_; };
    void addReadable(std::vector<Readable> &readables, const Readable &readable);
    void removeReadable(std::vector<Readable> &readables, const Readable &readable);
    auto readablesSince() -> std::string;
    auto articlesSince() -> std::string;

    auto changedReadablePlacemarkQueryParams(const std::vector<Readable> &readables)
        -> std::vector<std::string>;
    auto changedReadablesArchivedQueryParams(const std::vector<Readable> &readables)
        -> std::vector<std::string>;
    auto processReadablePlacemarksFromServer(const Readable &readable, const uint32_t placemark)
        -> bool;
    void resetReadablesPlacemarkChanged(const std::vector<Readable> &readables);
    void resetReadablesArchivedChanged(const std::vector<Readable> &readables);

    void syncFromRemote(const std::function<void(ContentSyncState state)> &progressCallback);
    void saveArticleFile(const Readable &readable);
    void clearArticlesFile(bool restart = true);
    void clearReadablesFile(bool restart = true);
    auto saveReadables(const std::vector<Readable> &readables, const std::string &path) -> bool;

    FileSystem &fileSystem;

private:
    std::vector<Readable> readables_;
    std::vector<Readable> articles_;

    PreferencesStore &prefs_;
};
