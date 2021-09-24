#pragma once

#include <vector>

#include "Contents/Readable.hpp"
#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Optics/OpticsSystem.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/ContainerScreen.hpp"

class RootUI : public ContainerScreen {
public:
    RootUI(PreferencesStore &prefs) : prefs(prefs) {}

    void setup(DisplaySystem *display, OpticsSystem *optics, std::vector<Readable> &readables);

protected:
    Renderer *renderer;
    PreferencesStore &prefs;
};
