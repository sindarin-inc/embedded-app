#ifdef SIMULATOR

#include "PosixFileSystem.hpp"

void PosixFileSystem::begin() {}

auto PosixFileSystem::open(const String &path) -> File { return File(); }

bool PosixFileSystem::exists(const String &path) { return false; }

#endif
