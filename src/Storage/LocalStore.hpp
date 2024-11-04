#pragma once

#include <algorithm>
#include <vector>

#include "Communications/SindarinAPI.hpp"
#include "Misc/Battery.hpp"
#include "Misc/Telemetry.hpp"
#include "Models/CheckSync.hpp"
#include "Models/Place.hpp"
#include "Models/Readable.hpp"
#include "Models/UserReadable.hpp"
#include "Models/UserReadablePartialChapterSummary.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"

struct SortCriteria {
    enum Type { DateModified, Alphabetical, LastRead, Random } type = DateModified;

    [[nodiscard]] auto toString() const -> std::string {
        switch (type) {
        case SortCriteria::DateModified:
            return "date modified";
        case SortCriteria::Alphabetical:
            return "alphabetical";
        case SortCriteria::LastRead:
            return "last read";
        case SortCriteria::Random:
            return "random";
        }
        return "date modified";
    }

    auto operator==(const SortCriteria &other) const -> bool { return type == other.type; }
};

struct SortDirection {
    enum Type { Descending, Ascending } type = Descending;

    [[nodiscard]] auto toString() const -> std::string {
        switch (type) {
        case SortDirection::Descending:
            return "descending";
        case SortDirection::Ascending:
            return "ascending";
        }
        return "descending";
    }

    auto operator==(const SortDirection &other) const -> bool { return type == other.type; }
};

struct ContentSyncState {
    enum State {
        Idle,
        ConnectingWifi,
        UploadTelemetry,
        CheckingSync,
        SyncingBooks,
        SyncingArticles,
        Synced,
        AlreadySynced,
        Failure,
    } state = Idle;

    int percentage{};
    int booksDownloaded{};
    int articlesDownloaded{};

    std::string message;

    [[nodiscard]] auto running() const -> bool {
        return state != Idle && state != Synced && state != AlreadySynced && state != Failure;
    }
};

using ReadablesVector = std::vector<std::shared_ptr<Readable>>;
using UserReadablesVector = SpiramVector<UserReadable>;

//! Interface for local storage of structured data
class LocalStore {
public:
    LocalStore(FileSystem &fileSystem, PreferencesStore &prefs, Battery &battery,
               Telemetry &telemetry)
        : fileSystem(fileSystem), prefs_(prefs), battery_(battery), telemetry_(telemetry){};
    LocalStore(LocalStore const &) = delete;

    auto begin() -> bool;
    auto loadReadablesFromFile(ReadablesVector &readables, const std::string &path) -> bool;

    // Convenience methods
    auto books() -> ReadablesVector & { return books_; };
    auto articles() -> ReadablesVector & { return articles_; };
    auto recents() -> ReadablesVector & { return recents_; };

    auto readablesModelType(const ReadablesVector &readables) -> ReadablesModelType;
    [[nodiscard]] auto isReadableUnchanged(const ReadablesVector &readables,
                                           const std::shared_ptr<Readable> &readable,
                                           bool ignoreFileSizeCheck = false) const -> bool;
    void addReadable(ReadablesVector &readables, const std::shared_ptr<Readable> &readable);
    void removeReadable(ReadablesVector &readables, const std::shared_ptr<Readable> &readable);
    auto readablesSince(ReadablesModelType modelType) -> std::string;

    void storedReadableUuids(const ReadablesVector &readables,
                             StoredUuidsVector &storedUuids) const;
    void changedReadablePlacemarkJson(const ReadablesVector &readables,
                                      ReadablePlacemarksVector &placemarks);
    void changedReadablesArchivedJson(const ReadablesVector &readables,
                                      ReadableArchivesVector &archives);
    void changedReadablesBookmarksJson(ReadableBookmarksVector &bookmarks);
    auto processReadablePlacemarksFromServer(const std::shared_ptr<Readable> &readable,
                                             const std::shared_ptr<TxtPlace> &txtPlace,
                                             const std::shared_ptr<EPubPlace> &ePubPlace) -> bool;
    auto processReadablePartialChapterSummary(
        const std::shared_ptr<Readable> &readable,
        const std::shared_ptr<UserReadablePartialChapterSummary> &partialChapterSummary) -> bool;
    auto processReadablePlacemarkHistory(
        const std::shared_ptr<Readable> &readable,
        const SpiramVector<UserReadablePlacemarkHistory> &placemarkHistory) -> bool;
    void resetReadablesPlacemarkChanged(const ReadablesVector &readables);
    void resetReadablesArchivedChanged(const ReadablesVector &readables);
    void resetReadablesBookmarksChanged(const ReadablesVector &readables);

    void syncFromRemote(SyncReason syncReason, const std::shared_ptr<CancelFlag> &cancel,
                        const std::function<void(ContentSyncState state)> &progressCallback);
    auto processReadables(const ReadablesVector &readables, UserReadablesVector &serverReadables)
        -> std::pair<UserReadablesVector, UserReadablesVector>;
    void processReadableDeletion(const UserReadablesVector &readablesToDelete,
                                 ReadablesVector &readables);
    auto processReadableDownload(const UserReadablesVector &readablesToDownload,
                                 ReadablesVector &readables,
                                 const std::shared_ptr<CancelFlag> &cancel, int &downloadIndex,
                                 int &successfullyDownloaded,
                                 const std::function<void(ContentSyncState)> &progressCallback,
                                 ReadablesModelType modelType, int totalCount, bool useBle) -> bool;
    void deleteReadables(bool restart = true);
    void deleteArticles(bool restart = true);
    void deleteReadableFiles(const ReadablesVector &readables, bool restart);
    auto removeStopWords(const SpiramString &title) -> std::string;
    void sortReadables(ReadablesVector &readables);
    void purgeReadableToc(const std::shared_ptr<Readable> &readable);
    auto saveReadables(ReadablesVector &readables) -> bool;

    // Methods for interacting with readable files on disk
    [[nodiscard]] auto readableFilePathForWriting(const std::shared_ptr<Readable> &readable,
                                                  bool gzip = true) const -> std::string;
    // When reading, we will take whatever readable file type that's available
    [[nodiscard]] static auto readableFilePathForReading(FileSystem &fileSystem,
                                                         const std::shared_ptr<Readable> &readable)
        -> std::pair<std::string, size_t>;
    [[nodiscard]] auto readableFilePathForReading(const std::shared_ptr<Readable> &readable) const
        -> std::pair<std::string, size_t>;
    [[nodiscard]] auto readableGetUncompressedSize(const std::shared_ptr<Readable> &readable,
                                                   const std::string &path, size_t size) const
        -> int;
    void readableFilePurge(const std::shared_ptr<Readable> &readable);

    // Save a firmware JSON for future use. Indicates that firmware has already been successfully
    // downloaded into the alternate firmware slot.
    void saveFirmware(const Firmware &firmware);
    // Returns true if a newer firmware has been loaded.
    auto getPendingFirmwareUpdate(Firmware &firmware) const -> bool;
    // Delete any existing firmware.json file.
    void resetFirmware();

    // Clean up old keys in NVS
    void garbageCollectPreferences();
    void migrateTxtPlacemarksToEpubPlacemarks();
    void deleteAllToCJson();
    void migrateIfNeeded();
    void computeRecentReadables();

    FileSystem &fileSystem;

private:
    const uint32_t latestMigrationVersion_ = 3;
    PreferencesStore &prefs_;
    Battery &battery_;
    Telemetry &telemetry_;

    ReadablesVector books_;
    ReadablesVector articles_;

    ReadablesVector recents_;
};
