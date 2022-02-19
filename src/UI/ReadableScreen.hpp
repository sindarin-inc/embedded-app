#include "Contents/Readable.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Power.hpp"
#include "Renderers/FileRenderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/CalibrationScreen.hpp"
#include "UI/ContainerScreen.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/PagedMenuScreen.hpp"
#include "UI/PopupMenuScreen.hpp"
#include "UI/Screen.hpp"

class ReadableScreen : public ContainerScreen {
public:
    ReadableScreen(FileRenderer *fileRenderer, Readable &readable, PreferencesStore &prefs,
                   Power &power)
        : fileRenderer_(fileRenderer), readable_(readable), prefs_(prefs), power_(power) {
        popup_ = new PopupMenuScreen(power, fileRenderer);
        currentScreen_ = this;
        if (readable.id.rfind("pkt-", 0) == 0) {
            popup_->setLeftButtonTitle("Delete");
            leftButtonCallback_ = [this, readable, fileRenderer]() {
                log_i("Deleting Pocket readable %s", readable.path.c_str());
                if (!fileRenderer->fileSystem.remove(readable.path)) {
                    log_e("Failed to delete Pocket readable %s", readable.path.c_str());
                }
                log_d("Rebooting to reload readables");
                // TODO: Dynamically reload readables instead of rebooting
                esp_restart();
            };
        }
    }

    ~ReadableScreen() override = default;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    FileRenderer *fileRenderer_;
    Readable &readable_;
    PreferencesStore &prefs_;
    Power &power_;

    PopupMenuScreen *popup_;

    bool inPopup_ = false;

    unsigned long newPlace_ = 0;

    std::function<void(void)> leftButtonCallback_;
};
