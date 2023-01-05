#include <memory>
#include <string>

#include "Displays/DisplayEinkET013TT1.hpp"
#include "Inputs/RemoteInput.hpp"

void StartFreeRtos(bool runAppMain = true);

void StopFreeRtos();

void SendRemoteCommand(RemoteCommand command);

void WaitForDraw(long timeoutSeconds = 10);

// TODO: This struct is probably overkill
struct ViewTestResult {
    bool passed;
    std::string acceptedImagePath;
    std::string renderedImagePath;
    std::string diffImagePath;
};

auto AssertViewUnchanged(std::string key) -> ViewTestResult;

std::shared_ptr<DisplayEinkET013TT1> createDisplay();
