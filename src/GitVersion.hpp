#ifndef GIT_VERSION_H
#define GIT_VERSION_H

#if !SIMULATOR
extern const char *kGitHash;
#else
const char *kGitHash = "SIMULATOR";
#endif

#endif // GIT_VERSION_H
