#ifndef CRASH_REPORT_SCREEN_HPP
#define CRASH_REPORT_SCREEN_HPP

#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

#define CRASH_REPORT_URL http://192.168.0.178:8080/crash

class CrashReportScreen : public Screen {
public:
    CrashReportScreen(DisplaySystem &display, PreferencesStore &prefs);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;

    void drawMessage(std::string &message);

};

#endif