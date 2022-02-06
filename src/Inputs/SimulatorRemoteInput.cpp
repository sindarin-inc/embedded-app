#if SIMULATOR

#include "SimulatorRemoteInput.hpp"

// This gets extern'd from native.cpp so the simulator can pass remote commands
int SimulatorRemoteCode;

void SimulatorRemoteInput::startRemote(){};
void SimulatorRemoteInput::startRemote(int data){};

auto SimulatorRemoteInput::checkPress() -> int { return 0; };

auto SimulatorRemoteInput::checkRelease() -> int {
    if (SimulatorRemoteCode == NONE) {
        lastCode_ = NONE;
        return 0;
    } else {
        lastCode_ = (RemoteCommand)SimulatorRemoteCode;
        SimulatorRemoteCode = NONE;
        return 1;
    }
};

#endif
