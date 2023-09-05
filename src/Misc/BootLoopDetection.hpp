#pragma once

#include "Misc/PowerManager.hpp"
#include "Storage/FileSystem.hpp"

void BootLoopCheck(FileSystem &fs, PowerManager &pm);
void GoodBootTimer();
auto GetBadBootCount() -> uint8_t;
