#include "ComicViewerScreen.hpp"

#include <algorithm>
#include "Displays/DisplaySystem.hpp"
#include "sindarin-debug.h"

#include <fstream>

#include "Misc/BMPImage.hpp"
#ifdef SIMULATOR
#include "Misc/JPGImageSTB.hpp"
#else
#if USE_JPG_DECODER
#include "Misc/JPGImage.hpp"
#endif
#endif


ComicViewerScreen::ComicViewerScreen(DisplaySystem &display, ComicPtr comic, ComicStore &comicStore, FileSystem &fileSystem)
    : display_(display), comic_(std::move(comic)), comicStore_(comicStore), fileSystem_(fileSystem),
      menuView_(display), popupMenuBackground_(display, 5), popupMenuForeground_(display, 5) {
    setupPopupMenu();
    loadCurrentPage();
}

ComicViewerScreen::~ComicViewerScreen() {
    saveProgress();
}

void ComicViewerScreen::render(DisplaySystem &display) {
    if (showMenu_) {
        renderPopupMenu(display);
        return;
    }
    
    // Clear screen
    display.fillScreen(display.bg);
    
    // Display current page
    if (comic_->getImageFiles().empty()) {
        // Show "No images" message - just draw a rectangle for now
        display.fillRect(50, 50, 200, 30, display.fg);
        display.updateDisplay();
        return;
    }
    
    if (comic_->getCurrentPage() >= comic_->getTotalPages()) {
        comic_->setCurrentPage(0);
    }
    
    const auto& currentImagePath = comic_->getImageFiles()[comic_->getCurrentPage()];
    
    // Load and display the actual image
    bool imageLoaded = false;
    std::string ext = currentImagePath.substr(currentImagePath.find_last_of('.'));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
#ifdef SIMULATOR
    // For simulator: Try different approaches based on file type
    if (ext == ".pbm") {
        try {
            display.drawPBM(0, 0, currentImagePath, fileSystem_);
            imageLoaded = true;
            log_d("Loaded PBM image: %s", currentImagePath.c_str());
        } catch (...) {
            log_w("Failed to load PBM: %s", currentImagePath.c_str());
        }
    } else if (ext == ".jpg" || ext == ".jpeg") {
        JPGImageSTB jpgImage;
        if (jpgImage.load(currentImagePath, fileSystem_)) {
            jpgImage.draw(display, 0, 0);
            imageLoaded = true;
            log_d("Successfully loaded JPG image: %s", currentImagePath.c_str());
        } else {
            log_w("Failed to load JPG: %s", currentImagePath.c_str());
        }
    } else if (ext == ".bmp") {
        BMPImage bmpImage;
        if (bmpImage.load(currentImagePath, fileSystem_)) {
            bmpImage.draw(display, 0, 0);
            imageLoaded = true;
            log_d("Loaded BMP image: %s", currentImagePath.c_str());
        }
    } else if (ext == ".png") {
        // PNG not implemented for simulator yet, show placeholder
        display.fillScreen(display.bg);
        display.drawSingleLineOfText("PNG format not yet supported", 50, 200);
        std::string filename = currentImagePath.substr(currentImagePath.find_last_of('/') + 1);
        display.drawSingleLineOfText("File: " + filename, 50, 230);
        imageLoaded = false;
    }
#else
    // For device: Use appropriate image loaders based on format
    if (ext == ".pbm") {
        // PBM loading - drawPBM will handle errors internally
        display.drawPBM(0, 0, currentImagePath, fileSystem_);
        imageLoaded = true;
        log_d("Loaded PBM image: %s", currentImagePath.c_str());
    } else if (ext == ".bmp") {
        BMPImage bmpImage;
        if (bmpImage.load(currentImagePath, fileSystem_)) {
            bmpImage.draw(display, 0, 0);
            imageLoaded = true;
            log_d("Loaded BMP image: %s", currentImagePath.c_str());
        }
    } else if (ext == ".jpg" || ext == ".jpeg") {
#if USE_JPG_DECODER
        JPGImage jpgImage;
        if (jpgImage.load(currentImagePath, fileSystem_)) {
            jpgImage.draw(display, 0, 0);
            imageLoaded = true;
            log_d("Loaded JPG image: %s", currentImagePath.c_str());
        } else {
            log_w("Failed to load JPG: %s", currentImagePath.c_str());
        }
#else
        log_w("JPG loading not enabled (USE_JPG_DECODER not defined): %s", currentImagePath.c_str());
#endif
    } else if (ext == ".png") {
        // TODO: Add PNG loading for device when PNG decoder is available  
        log_w("PNG loading not yet implemented for device: %s", currentImagePath.c_str());
    }
#endif
    
    if (!imageLoaded) {
        // Show error indicator with format info
        display.fillScreen(display.bg);
        display.fillRect(50, 50, 400, 150, display.fg);
        display.fillRect(55, 55, 390, 140, display.bg);
        
        display.drawSingleLineOfText("Failed to load image:", 70, 90);
        std::string filename = currentImagePath.substr(currentImagePath.find_last_of('/') + 1);
        display.drawSingleLineOfText(filename, 70, 120);
        display.drawSingleLineOfText("Format: " + ext, 70, 150);
        display.drawSingleLineOfText("Supported: PBM, BMP", 70, 180);
        
        log_e("Failed to load image: %s", currentImagePath.c_str());
    }
    
    // Page indicator at bottom  
    int pageWidth = comic_->getTotalPages() > 0 ? (200 / comic_->getTotalPages()) : 200;
    int pageX = 10 + (comic_->getCurrentPage() * pageWidth);
    display.fillRect(10, display.height() - 20, 200, 15, display.bg);
    display.fillRect(pageX, display.height() - 20, pageWidth, 15, display.fg);
    
    // Page text
    char pageText[32];
    snprintf(pageText, sizeof(pageText), "%d/%d", comic_->getCurrentPage() + 1, comic_->getTotalPages());
    display.fillRect(220, display.height() - 20, 80, 15, display.fg);
    
    display.updateDisplay();
}

void ComicViewerScreen::processCommand(RemoteCommand command) {
    if (showMenu_) {
        switch (command) {
            case RemoteCommand::UP:
                menuView_.moveSelection(-1);
                setNeedsRerender();
                break;
                
            case RemoteCommand::DOWN:
                menuView_.moveSelection(1);
                setNeedsRerender();
                break;
                
            case RemoteCommand::ENTER: {
                // Handle menu selection based on selected ID
                std::string selectedID = menuView_.getSelectedMenuItemID();
                if (selectedID == "Main Menu") {
                    back();
                } else if (selectedID == "Back to Comic") {
                    hidePopupMenu();
                }
                break;
            }
                
            case RemoteCommand::LEFT:
            case RemoteCommand::RIGHT:
                hidePopupMenu();
                break;
                
            default:
                break;
        }
        return;
    }
    
    // Normal comic viewing controls
    switch (command) {
        case RemoteCommand::RIGHT:
        case RemoteCommand::ENTER:
            nextPage();
            break;
            
        case RemoteCommand::LEFT:
            previousPage();
            break;
            
        case RemoteCommand::UP:
        case RemoteCommand::DOWN:
            showPopupMenu();
            break;
            
        default:
            break;
    }
}

void ComicViewerScreen::loadCurrentPage() {
    setNeedsRerender();
}

void ComicViewerScreen::nextPage() {
    if (comic_->getCurrentPage() < comic_->getTotalPages() - 1) {
        comic_->setCurrentPage(comic_->getCurrentPage() + 1);
        saveProgress();
        loadCurrentPage();
    }
}

void ComicViewerScreen::previousPage() {
    if (comic_->getCurrentPage() > 0) {
        comic_->setCurrentPage(comic_->getCurrentPage() - 1);
        saveProgress();
        loadCurrentPage();
    }
}

void ComicViewerScreen::showPopupMenu() {
    showMenu_ = true;
    menuView_.setSelectedIndex(0);
    setNeedsRerender();
}

void ComicViewerScreen::hidePopupMenu() {
    showMenu_ = false;
    setNeedsRerender();
}

void ComicViewerScreen::setupPopupMenu() {
    menuView_.resetMenuItems();
    menuView_.setIndicatorType(MenuView::IndicatorType::Dot);
    menuView_.padding = 0;
    menuView_.itemsPerPage = 8;
    menuView_.indicatorWidth = 8;
    menuView_.indicatorPadding = 10;
    
    // Add menu items with callbacks
    auto mainMenuItem = std::make_shared<MenuItemSingleLine>("Main Menu", []() {});
    auto backMenuItem = std::make_shared<MenuItemSingleLine>("Back to Comic", []() {});
    
    menuView_.addMenuItem(mainMenuItem);
    menuView_.addMenuItem(backMenuItem);
    
    menuView_.setSelectedIndex(0);
}

void ComicViewerScreen::renderPopupMenu(DisplaySystem &display) {
    display.fillScreen(display.bg);
    
    // Use the member display_ reference to prevent unused field warning
    (void)display_;
    
    // Simple manual layout for the popup menu
    int menuWidth = 200;
    int menuHeight = 100;
    int menuX = (display.width() - menuWidth) / 2;
    int menuY = (display.height() - menuHeight) / 2;
    
    // Draw background
    display.fillRect(menuX, menuY, menuWidth, menuHeight, display.fg);
    display.fillRect(menuX + 2, menuY + 2, menuWidth - 4, menuHeight - 4, display.bg);
    
    // Manually layout and draw menu items
    auto menuItems = {"Main Menu", "Back to Comic"};
    int itemHeight = 30;
    int selectedIndex = menuView_.getSelectedIndex();
    int index = 0;
    
    for (const auto& itemText : menuItems) {
        bool isSelected = (index == selectedIndex);
        int currentItemY = menuY + 15 + (index * itemHeight);  // Start closer to menu top
        
        if (isSelected) {
            // Draw selection indicator (dot)
            int dotX = menuX + 15;
            int dotY = currentItemY + 10;  // Center dot with text
            display.fillCircle(dotX, dotY, 4, display.fg);
        }
        
        // Draw menu text
        display.drawSingleLineOfText(itemText, menuX + 30, currentItemY + 15, false);
        
        index++;
    }
    
    display.updateDisplay();
}

void ComicViewerScreen::saveProgress() {
    comicStore_.saveComicProgress(comic_->getName(), comic_->getCurrentPage());
}