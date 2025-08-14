#include "SimulatorAutomation.hpp"
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <chrono>
#include "sindarin-debug.h"

#define AUTOMATION_SOCKET_PATH "/tmp/sol_simulator_automation.sock"

void SimulatorAutomation::start() {
    if (running_) return;
    
    running_ = true;
    
    commandThread_ = std::thread([this]() { processCommands(); });
    listenerThread_ = std::thread([this]() { listenForCommands(); });
    
    log_i("Simulator automation started");
}

void SimulatorAutomation::stop() {
    if (!running_) return;
    
    running_ = false;
    queueCV_.notify_all();
    
    if (commandThread_.joinable()) {
        commandThread_.join();
    }
    if (listenerThread_.joinable()) {
        listenerThread_.join();
    }
    
    unlink(AUTOMATION_SOCKET_PATH);
    log_i("Simulator automation stopped");
}

void SimulatorAutomation::sendKey(SDL_Keycode key) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    commandQueue_.push([key]() {
        SDL_Event event;
        SDL_zero(event);
        
        event.type = SDL_KEYDOWN;
        event.key.keysym.sym = key;
        SDL_PushEvent(&event);
        
        SDL_Delay(10);
        
        event.type = SDL_KEYUP;
        event.key.keysym.sym = key;
        SDL_PushEvent(&event);
    });
    queueCV_.notify_one();
}

void SimulatorAutomation::sendKeySequence(const std::string& keys, int delayMs) {
    for (char c : keys) {
        SDL_Keycode key = SDLK_UNKNOWN;
        
        switch (c) {
            case 'u': key = SDLK_UP; break;
            case 'd': key = SDLK_DOWN; break;
            case 'l': key = SDLK_LEFT; break;
            case 'r': key = SDLK_RIGHT; break;
            case '\n': key = SDLK_RETURN; break;
            case '\e': key = SDLK_ESCAPE; break;
            default: key = static_cast<SDL_Keycode>(c); break;
        }
        
        if (key != SDLK_UNKNOWN) {
            sendKey(key);
            waitMs(delayMs);
        }
    }
}

void SimulatorAutomation::waitMs(int ms) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    commandQueue_.push([ms]() {
        SDL_Delay(ms);
    });
    queueCV_.notify_one();
}

bool SimulatorAutomation::takeScreenshot(const std::string& filename) {
    sendKey(static_cast<SDL_Keycode>('p'));
    return true;
}

bool SimulatorAutomation::waitForScreen(const std::string& expectedImagePath, int timeoutMs) {
    auto startTime = std::chrono::steady_clock::now();
    
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - startTime).count() < timeoutMs) {
        
        takeScreenshot("/tmp/sim_temp_screenshot.png");
        waitMs(100);
        
        SDL_Delay(100);
    }
    
    return false;
}

void SimulatorAutomation::processCommands() {
    while (running_) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        queueCV_.wait(lock, [this]() { return !commandQueue_.empty() || !running_; });
        
        while (!commandQueue_.empty()) {
            auto command = commandQueue_.front();
            commandQueue_.pop();
            lock.unlock();
            
            command();
            
            lock.lock();
        }
    }
}

void SimulatorAutomation::listenForCommands() {
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        log_e("Failed to create socket");
        return;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, AUTOMATION_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    unlink(AUTOMATION_SOCKET_PATH);
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_e("Failed to bind socket");
        close(sockfd);
        return;
    }
    
    if (listen(sockfd, 5) < 0) {
        log_e("Failed to listen on socket");
        close(sockfd);
        return;
    }
    
    log_i("Automation listening on %s", AUTOMATION_SOCKET_PATH);
    
    while (running_) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int result = select(sockfd + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (result > 0 && FD_ISSET(sockfd, &readfds)) {
            int client = accept(sockfd, nullptr, nullptr);
            if (client >= 0) {
                char buffer[256];
                int n = read(client, buffer, sizeof(buffer) - 1);
                if (n > 0) {
                    buffer[n] = '\0';
                    std::string command(buffer);
                    
                    if (command.substr(0, 4) == "KEY:") {
                        char key = command[4];
                        sendKey(static_cast<SDL_Keycode>(key));
                        write(client, "OK\n", 3);
                    } else if (command.substr(0, 8) == "KEYSEQ:") {
                        sendKeySequence(command.substr(8));
                        write(client, "OK\n", 3);
                    } else if (command == "SCREENSHOT") {
                        takeScreenshot("/tmp/simulator_screenshot.png");
                        write(client, "OK\n", 3);
                    } else if (command.substr(0, 5) == "WAIT:") {
                        int ms = std::stoi(command.substr(5));
                        waitMs(ms);
                        write(client, "OK\n", 3);
                    } else {
                        write(client, "ERROR: Unknown command\n", 23);
                    }
                }
                close(client);
            }
        }
    }
    
    close(sockfd);
    unlink(AUTOMATION_SOCKET_PATH);
}