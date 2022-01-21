#include "Contents/Readable.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Renderers/FileRenderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class ReadableScreen : public Screen {
public:
    ReadableScreen(FileRenderer *fileRenderer, Readable &readable, PreferencesStore &prefs)
        : fileRenderer_(fileRenderer), readable_(readable), prefs_(prefs) {}
    ~ReadableScreen() override = default;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    FileRenderer *fileRenderer_;
    Readable &readable_;
    PreferencesStore &prefs_;
    unsigned long newPlace_ = 0;
};
