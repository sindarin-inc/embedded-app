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

enum class ReadablesModelType { Article, Readable };

typedef std::vector<Readable, SpiramAllocator<Readable>> ReadablesVector;

//! Interface for local storage of structured data
class LocalStore {
public:
    LocalStore(FileSystem &fileSystem, PreferencesStore &prefs)
        : fileSystem(fileSystem), prefs_(prefs){};

    auto begin() -> bool;
    auto loadReadablesFromFile(ReadablesVector &readables, const std::string &path) -> bool;

    // Convenience methods
    auto readablesModelType(const ReadablesVector &readables) -> ReadablesModelType;
    auto hasReadable(const ReadablesVector &readables, const Readable &readable) -> bool;
    auto readables() -> ReadablesVector & { return readables_; };
    auto articles() -> ReadablesVector & { return articles_; };
    void addReadable(ReadablesVector &readables, const Readable &readable);
    void removeReadable(ReadablesVector &readables, const Readable &readable);
    auto readablesSince() -> std::string;
    auto articlesSince() -> std::string;

    auto changedReadablePlacemarkQueryParams(const ReadablesVector &readables)
        -> std::vector<std::string>;
    auto changedReadablesArchivedQueryParams(const ReadablesVector &readables)
        -> std::vector<std::string>;
    auto changedReadablesBookmarksJson() -> std::pair<char *, int>;
    auto processReadablePlacemarksFromServer(const Readable &readable, const uint32_t placemark)
        -> bool;
    auto processReadableBookmarksFromServer(const Readable &readable,
                                            const std::string bookmarkRange) -> bool;
    void resetReadablesPlacemarkChanged(const ReadablesVector &readables);
    void resetReadablesArchivedChanged(const ReadablesVector &readables);
    void resetReadablesBookmarksChanged(const ReadablesVector &readables);

    void syncFromRemote(const std::function<void(ContentSyncState state)> &progressCallback);
    auto saveArticleFile(const Readable &readable) -> bool;
    void deleteReadables(bool restart = true);
    void deleteArticles(bool restart = true);
    void deleteReadableFiles(const ReadablesVector &readables, bool restart);
    auto saveReadables(const ReadablesVector &readables, const std::string &path) -> bool;

    FileSystem &fileSystem;

private:
    ReadablesVector readables_;
    ReadablesVector articles_;

    PreferencesStore &prefs_;
};
