#pragma once

#include "RemoteInput.hpp"

class SimulatorRemoteInput : public RemoteInput {

public:
    void startRemote() override;
    void startRemote(int data) override;
    auto checkPress() -> int override;
    auto checkRelease() -> int override;
};
