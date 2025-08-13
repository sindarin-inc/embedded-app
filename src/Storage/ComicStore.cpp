#include "ComicStore.hpp"

#include <algorithm>
#include <cctype>
#include <sys/stat.h>
#include "sindarin-debug.h"

#ifdef SIMULATOR
constexpr const char* COMICS_SD_PATH = "./data/comics";  // For opendir() calls
constexpr const char* COMICS_FILE_PREFIX = "/comics";    // For SFile paths (starts with /)
#else
constexpr const char* COMICS_SD_PATH = "/sdcard/comics"; // For opendir() calls (absolute path)
constexpr const char* COMICS_FILE_PREFIX = "/comics";    // For SFile paths (relative to /sdcard prefix)
#endif

auto ComicStore::scanSDCardForComics() -> ComicsVector {
    ComicsVector comics;
    
    DIR* dir = opendir(COMICS_SD_PATH);
    if (!dir) {
        log_d("Comics directory not found at %s", COMICS_SD_PATH);
        return comics;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            std::string dirName = entry->d_name;
            
            // Skip . and .. directories
            if (dirName == "." || dirName == "..") {
                continue;
            }
            
            std::string filePath = std::string(COMICS_FILE_PREFIX) + "/" + dirName;
            std::string scanPath = std::string(COMICS_SD_PATH) + "/" + dirName;
            
            // Scan this directory for images
            auto imageFiles = scanDirectoryForImages(scanPath, filePath);
            if (!imageFiles.empty()) {
                auto comic = std::make_shared<Comic>(dirName, filePath);
                comic->setImageFiles(imageFiles);
                
                // Load saved progress
                int savedPage = loadComicProgress(dirName);
                comic->setCurrentPage(savedPage);
                
                comics.push_back(comic);
                log_d("Found comic: %s with %zu images", dirName.c_str(), imageFiles.size());
            }
        }
    }
    closedir(dir);
    
    // Sort comics alphabetically
    std::sort(comics.begin(), comics.end(), [](const ComicPtr& a, const ComicPtr& b) {
        return a->getName() < b->getName();
    });
    
    log_i("Found %zu comics on SD card", comics.size());
    return comics;
}

auto ComicStore::scanDirectoryForImages(const std::string& scanPath, const std::string& filePath) -> std::vector<std::string> {
    std::vector<std::string> imageFiles;
    
    DIR* dir = opendir(scanPath.c_str());
    if (!dir) {
        log_w("Failed to open comic directory: %s", scanPath.c_str());
        return imageFiles;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) {
            std::string filename = entry->d_name;
            
            // Skip hidden files
            if (filename.empty() || filename[0] == '.') {
                continue;
            }
            
            if (isImageFile(filename)) {
                imageFiles.push_back(filePath + "/" + filename);
            }
        }
    }
    closedir(dir);
    
    // Sort images naturally (1.jpg, 2.jpg, 10.jpg, etc.)
    std::sort(imageFiles.begin(), imageFiles.end(), [](const std::string& a, const std::string& b) {
        // Extract filename from full path
        auto getFilename = [](const std::string& path) {
            size_t pos = path.find_last_of('/');
            return (pos != std::string::npos) ? path.substr(pos + 1) : path;
        };
        
        std::string filenameA = getFilename(a);
        std::string filenameB = getFilename(b);
        
        // Try to extract numbers from filenames for natural sorting
        auto extractNumber = [](const std::string& str) -> int {
            std::string number;
            for (char c : str) {
                if (std::isdigit(c)) {
                    number += c;
                } else if (!number.empty()) {
                    break; // Stop at first non-digit after finding digits
                }
            }
            return number.empty() ? 0 : std::stoi(number);
        };
        
        int numA = extractNumber(filenameA);
        int numB = extractNumber(filenameB);
        
        if (numA != numB) {
            return numA < numB;
        }
        
        // Fall back to alphabetical sorting
        return filenameA < filenameB;
    });
    
    return imageFiles;
}

auto ComicStore::isImageFile(const std::string& filename) -> bool {
    if (filename.length() < 4) {
        return false;
    }
    
    std::string ext = filename.substr(filename.length() - 4);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".jpg" || ext == ".png" || ext == ".bmp") {
        return true;
    }
    
    if (filename.length() >= 5) {
        std::string ext5 = filename.substr(filename.length() - 5);
        std::transform(ext5.begin(), ext5.end(), ext5.begin(), ::tolower);
        if (ext5 == ".jpeg") {
            return true;
        }
    }
    
    return false;
}

void ComicStore::saveComicProgress(const std::string& comicName, int currentPage) {
    comicProgress_.set(comicName.c_str(), currentPage);
    log_d("Saved comic progress: %s -> page %d", comicName.c_str(), currentPage);
}

auto ComicStore::loadComicProgress(const std::string& comicName) -> int {
    return comicProgress_.get(comicName.c_str());
}

auto ComicStore::getComicProgressKey(const std::string& comicName) -> std::string {
    return "comic_progress_" + comicName;
}