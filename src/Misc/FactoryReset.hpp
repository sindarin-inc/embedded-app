#pragma once

#include "Storage/FileSystem.hpp"

// Erase all files in the filesystem and erase NVS partition
void SoftFactoryReset(FileSystem &fs);

// Fully erase FS and NVS paritions (requires a 4min re-format at next boot)
void HardFactoryReset();
