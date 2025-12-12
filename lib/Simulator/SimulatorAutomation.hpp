#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

class SimulatorAutomation {
public:
    static SimulatorAutomation& getInstance() {
        static SimulatorAutomation instance;
        return instance;
    }

    void start();
    void stop();
    
    void sendKey(SDL_Keycode key);
    void sendKeySequence(const std::string& keys, int delayMs = 100);
    void waitMs(int ms);
    
    bool takeScreenshot(const std::string& filename);
    bool waitForScreen(const std::string& expectedImagePath, int timeoutMs = 5000);
    
private:
    SimulatorAutomation() = default;
    ~SimulatorAutomation() { stop(); }
    
    void processCommands();
    void listenForCommands();
    
    bool running_ = false;
    std::thread commandThread_;
    std::thread listenerThread_;
    
    std::queue<std::function<void()>> commandQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;
};