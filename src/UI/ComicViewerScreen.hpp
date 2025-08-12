#pragma once

#include <functional>
#include <memory>
#include "Models/Comic.hpp"
#include "Storage/ComicStore.hpp"
#include "Storage/FileSystem.hpp"
#include "UI/Screen.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/Menu/MenuItemSingleLine.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/SingleLineTextView.hpp"

class ComicViewerScreen : public Screen {
public:
    ComicViewerScreen(DisplaySystem &display, ComicPtr comic, ComicStore &comicStore, FileSystem &fileSystem);
    ~ComicViewerScreen() override;
    
protected:
    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

private:
    DisplaySystem &display_;
    ComicPtr comic_;
    ComicStore &comicStore_;
    FileSystem &fileSystem_;
    
    bool showMenu_ = false;
    MenuView menuView_;
    BoxView popupMenuBackground_;
    BoxView popupMenuForeground_;
    
    void loadCurrentPage();
    void nextPage();
    void previousPage();
    void showPopupMenu();
    void hidePopupMenu();
    void renderPopupMenu(DisplaySystem &display);
    void setupPopupMenu();
    void saveProgress();
};

using ComicViewerScreenPtr = std::shared_ptr<ComicViewerScreen>;