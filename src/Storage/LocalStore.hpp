#pragma once

#include <vector>

#include "Misc/Dispatch.hpp"
#include "Models/Readable.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"

//! Interface for local storage of structured data
class LocalStore {
public:
    LocalStore(FileSystem &fileSystem, PreferencesStore &prefs)
        : fileSystem(fileSystem), prefs_(prefs){};

    auto begin() -> bool;

    // Convenience methods
    auto hasReadable(Readable &readable) -> bool;
    auto readables() -> std::vector<Readable> & { return readables_; };
    void addReadable(Readable &readable);
    void removeReadable(const Readable &readable);

    auto calculateReadablesSha() -> std::string;

    void
    syncFromRemote(const std::function<void(std::string message, int percentage)> &progressCallback,
                   const std::function<void(bool success)> &doneCallback);

    auto save() -> bool;

    FileSystem &fileSystem;

private:
    std::vector<Readable> readables_;

    PreferencesStore &prefs_;
};
