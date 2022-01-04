#ifndef RemoteInput_H
#define RemoteInput_H

enum RemoteCommand : int {
    VOL_DOWN = 0x0,
    PLAY = 0x1,
    VOL_UP = 0x2,
    SETUP = 0x4,
    PREV = 0x5,
    STOP = 0x6,
    CHANNEL_MINUS = 0x8,
    ENTER = 0x9,
    CHANNEL_PLUS = 0xA,
    NOP_BUTTON = 0xC,
    NEXT = 0xD,
    BACK = 0xE,
    FACTORY_DEFAULTS = 0xFF,

    NONE = -1,
};

const char *codeToString(RemoteCommand code);

class RemoteInput {

public:
    virtual void start_remote() = 0;
    virtual void start_remote(int data) = 0;
    virtual auto check_press() -> int = 0;
    virtual auto check_release() -> int = 0;
    auto get_code() -> RemoteCommand;

protected:
    RemoteCommand last_code = RemoteCommand::NONE;
};

#endif