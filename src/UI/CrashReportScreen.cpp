#include "UI/CrashReportScreen.hpp"

#include "UI/Fonts/Tahoma8pt7b.h"
#include "sindarin-debug.h"
#include "Communications/WiFi.hpp"
#include <esp_core_dump.h>
#include <esp_flash.h>

CrashReportScreen::CrashReportScreen(DisplaySystem &display, PreferencesStore &prefs)
    : display_(display), prefs_(prefs) {}

void CrashReportScreen::drawMessage(std::string &message) {
    display_.drawSingleLineOfCenteredText(message);
    display_.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
}

void CrashReportScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    std::string message = "no wifi";
    if (!NetworkConnected()) {
        drawMessage(message);
        return;
    }

    message = "Uploading crash report.";

    display.setFont(&TAHOMA_REGULAR_FONT8PT7B);
    display.drawSingleLineOfText(message, 0, 0);

    size_t out_addr, out_size;
    esp_core_dump_image_get(&out_addr, &out_size);

    uint8_t* buffer = (uint8_t*)malloc(out_size);
    //esp_flash_read(NULL, buffer, out_addr, out_size);
    esp_flash_read(NULL, NULL, (uint32_t)0, (uint32_t)0);

    printf("core dump size %d\n", (int)out_size);

    for (int i = 0; i < 16; i++)
    {
        printf("%d: %X %c\n", i, buffer[i], buffer[i]);
    }

    // send crash report


    free(buffer);
    buffer = NULL;

    display.updateDisplay();
    needsRerender = false;
    
}

void CrashReportScreen::processCommand(RemoteCommand command) { backCallback(); }