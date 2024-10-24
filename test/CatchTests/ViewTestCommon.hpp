#include <memory>
#include <string>

#include "DisplayUtil.hpp"
#include "Displays/DisplayEinkET013TT1.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Storage/PreferencesStore.hpp"

// Mock the time in SolClock
void MockClock();

void UnMockClock();

// NOTE: If not running app_main from here, you need to have at least one task already created
// before calling this API.
void StartFreeRtos(bool runAppMain = false);

void StopFreeRtos();

void StartAppMain(FONT_CHOICES font = FONT_CHOICES::PREFS_SOL_SMALL, int x = 42, int y = 30,
                  int lineSpacing = 0);

void SetImagesFolder(std::string folder);

void SendRemoteCommand(RemoteCommand command);

auto WaitForDraw(DisplayEinkET013TT1 &display, long timeoutSeconds = 10) -> bool;

#define WAIT_FOR_DRAW(display) REQUIRE(WaitForDraw(display))

// TODO: This struct is probably overkill
struct ViewTestResult {
    bool passed;
    std::string acceptedImagePath;
    std::string renderedImagePath;
    std::string diffImagePath;
};

auto CheckViewUnchanged(DisplayEinkET013TT1 &display, std::string key) -> ViewTestResult;

auto CheckViewUnchanged(std::shared_ptr<DisplayEinkET013TT1> display, std::string key)
    -> ViewTestResult;

#define ASSERT_VIEW_UNCHANGED(display, key) CHECK(CheckViewUnchanged(display, key).passed)

auto GetRootDisplay() -> DisplayEinkET013TT1 &;

void SetupFileSystemMock(const std::string &directory);

auto LoadJsonDoc(FileSystem &fileSystem, const std::string &path)
    -> std::unique_ptr<SpiRamJsonDocument>;
