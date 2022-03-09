#ifndef RemoteInput_H
#define RemoteInput_H

enum RemoteCommand : int {
    VOL_DOWN = 0x0,
    PLAY = 0x1,
    VOL_UP = 0x2,
    SETUP = 0x4,
    // Up
    PREV = 0x5,
    STOP = 0x6,
    // Left
    CHANNEL_MINUS = 0x8,
    ENTER = 0x9,
    // Right
    CHANNEL_PLUS = 0xA,
    NOP_BUTTON = 0xC,
    // Down
    NEXT = 0xD,
    BACK = 0xE,
    FACTORY_DEFAULTS = 0xFF,

    NONE = -1,
};

auto CodeToString(RemoteCommand code) -> const char *;

class RemoteInput {

public:
    virtual void startRemote() = 0;
    virtual void startRemote(int data) = 0;
    virtual auto checkPress() -> int = 0;
    virtual auto checkRelease() -> int = 0;
    auto getCode() -> RemoteCommand;

protected:
    RemoteCommand lastCode_ = RemoteCommand::NONE;
};

#endif