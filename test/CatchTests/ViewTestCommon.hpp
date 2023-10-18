#include <memory>
#include <string>

#include "Displays/DisplayEinkET013TT1.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Storage/PreferencesStore.hpp"

void StartFreeRtos(bool runAppMain = true, FONT_CHOICES font = FONT_CHOICES::PREFS_SOL_MEDIUM,
                   int x = 42, int y = 30, int lineSpacing = 0);

void StopFreeRtos();

void SetImagesFolder(std::string folder);

void SendRemoteCommand(RemoteCommand command);

auto WaitForDraw(long timeoutSeconds = 10) -> bool;

#define WAIT_FOR_DRAW() REQUIRE(WaitForDraw())

// TODO: This struct is probably overkill
struct ViewTestResult {
    bool passed;
    std::string acceptedImagePath;
    std::string renderedImagePath;
    std::string diffImagePath;
};

auto CheckViewUnchanged(std::string key) -> ViewTestResult;

#define ASSERT_VIEW_UNCHANGED(key) CHECK(CheckViewUnchanged(key).passed)

std::shared_ptr<DisplayEinkET013TT1> createDisplay();

void SetupFileSystemMock(const std::string &directory);

auto LoadJsonDoc(FileSystem &fileSystem, const std::string &path)
    -> std::unique_ptr<SpiRamJsonDocument>;