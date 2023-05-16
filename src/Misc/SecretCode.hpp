#pragma once

#include <utility>

#include "Inputs/RemoteInput.hpp"
#include "sindarin-debug.h"

class SecretCode {
public:
    SecretCode() {
        commands_[0] = RemoteCommand::PREV;
        commands_[1] = RemoteCommand::PREV;
        commands_[2] = RemoteCommand::NEXT;
        commands_[3] = RemoteCommand::NEXT;
        commands_[4] = RemoteCommand::CHANNEL_MINUS;
        commands_[5] = RemoteCommand::CHANNEL_PLUS;
        commands_[6] = RemoteCommand::CHANNEL_MINUS;
        commands_[7] = RemoteCommand::CHANNEL_PLUS;
    };
    ~SecretCode() = default;

    void setCallback(std::function<void()> callback) { callback_ = std::move(callback); }

    void processCommand(RemoteCommand command) {
        if (command == commands_[index_]) {
            log_d("Got the right code %d time(s)!", index_ + 1);
            index_++;
            if (index_ == commands_.size()) {
                log_d("Code entered!");
                if (callback_ != nullptr) {
                    callback_();
                }
                index_ = 0;
            }
        } else {
            index_ = 0;
        }
    }

protected:
    // std::function to store the action when code is entered
    std::function<void()> callback_;
    std::array<RemoteCommand, 8> commands_;
    uint8_t index_ = 0;
};
