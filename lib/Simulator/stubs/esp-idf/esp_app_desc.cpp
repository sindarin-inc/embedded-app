#include "esp_app_desc.h"

#include <cstdio>
#include <cstring>

// Initialize version string safely
static char version_buffer[32] = {0};
static void init_version() {
    const char* git_version =
#ifdef GIT_VERSION
        GIT_VERSION;
#else
        "release/v0.9.0-9-g1348e563";
#endif
    // Safely copy and truncate if necessary
    strncpy(version_buffer, git_version, sizeof(version_buffer) - 1);
    version_buffer[sizeof(version_buffer) - 1] = '\0';
}

// Initialize at startup
static struct VersionInitializer {
    VersionInitializer() { init_version(); }
} version_init;

// Initialize a global esp_app_desc_t struct
esp_app_desc_t app_desc = {.magic_word = 0xABCD1234,
                           .secure_version = 1,
                           .reserv1 = {0, 0},
                           .version = "",  // Will be set by init_version
                           .project_name = "MyProject",
                           .time = "12:34:56",
                           .date = "2023-08-31",
                           .idf_ver = "4.0",
                           .app_elf_sha256 = "abcdabcd",
                           .reserv2 = {0}};

const esp_app_desc_t *esp_app_get_description(void) {
    // Ensure version is set
    if (app_desc.version[0] == '\0') {
        strncpy(app_desc.version, version_buffer, sizeof(app_desc.version) - 1);
        app_desc.version[sizeof(app_desc.version) - 1] = '\0';
    }
    return &app_desc;
}

int esp_app_get_elf_sha256(char *dst, size_t size) { return snprintf(dst, size, "fakesha"); }