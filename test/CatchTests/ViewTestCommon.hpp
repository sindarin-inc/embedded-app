#include <memory>
#include <string>
#include <thread>

#include "Displays/DisplayEinkET013TT1.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Storage/PreferencesStore.hpp"

#define WAIT_FOR_DRAW(this) REQUIRE(this->WaitForDraw())
#define ASSERT_VIEW_UNCHANGED(this, key) CHECK(this->CheckViewUnchanged(key).passed)

extern bool useSimulatorTestTime;
extern int simulatorTestHours;
extern int simulatorTestMinutes;

extern "C" void app_main(void); // NOLINT(readability-identifier-naming)

class ViewTestCommon {
private:
    // std::string directory_;
    std::string testRunPrefix_{"Images/TestRun/"};
    std::string acceptedPathPrefix_{"Images/Accepted/"};

    std::shared_ptr<std::thread> schedulerThread_;

    // TODO: This struct is probably overkill
    struct ViewTestResult {
        bool passed;
        std::string acceptedImagePath;
        std::string renderedImagePath;
        std::string diffImagePath;
    };

    static void appMainTask(void *parameters) {
        useSimulatorTestTime = true;
        simulatorTestHours = 12;
        simulatorTestMinutes = 21;
        app_main();
        vTaskDelete(NULL);
    }

public:
    void StartFreeRtos(bool runAppMain = true, FONT_CHOICES font = FONT_CHOICES::PREFS_SOL_MEDIUM,
                       int x = 42, int y = 30, int lineSpacing = 0);

    void StopFreeRtos();

    void SetImagesFolder(std::string folder);

    void SendRemoteCommand(RemoteCommand command);

    auto WaitForDraw(long timeoutSeconds = 10) -> bool;

    auto CheckViewUnchanged(std::string key) -> ViewTestResult;

    auto createDisplay() -> std::shared_ptr<DisplayEinkET013TT1>;

    void SetupFileSystemMock(const std::string &directory);

    auto LoadJsonDoc(FileSystem &fileSystem, const std::string &path)
        -> std::unique_ptr<SpiRamJsonDocument>;

    auto GetDirectory(std::string filename = "") -> std::string;
};