#include "Contents/Readable.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Renderers/FileRenderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class ReadableScreen : public Screen {
public:
    ReadableScreen(FileRenderer *file_renderer, Readable &readable, PreferencesStore &prefs)
        : file_renderer(file_renderer), readable(readable), prefs(prefs) {}
    ~ReadableScreen() = default;

    void render();
    void process_command(RemoteCommand command);

protected:
    FileRenderer *file_renderer;
    Readable &readable;
    PreferencesStore &prefs;
    unsigned long new_place = 0;
};
