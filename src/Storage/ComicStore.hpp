#pragma once

#include <dirent.h>
#include <string>
#include <vector>
#include <memory>
#include "Models/Comic.hpp"
#include "Storage/PreferencesStore.hpp"

class ComicStore {
public:
    ComicStore(PreferencesStore &prefs) : comicProgress_(prefs, "-cp") {}
    
    auto scanSDCardForComics() -> ComicsVector;
    
    void saveComicProgress(const std::string& comicName, int currentPage);
    auto loadComicProgress(const std::string& comicName) -> int;
    
private:
    PreferencesStore::KeyedPrefsItem<int> comicProgress_;
    
    auto isImageFile(const std::string& filename) -> bool;
    auto scanDirectoryForImages(const std::string& scanPath, const std::string& filePath) -> std::vector<std::string>;
    auto getComicProgressKey(const std::string& comicName) -> std::string;
};