#include "esp_app_desc.h"

#include <cstdio>

// Initialize a global esp_app_desc_t struct
esp_app_desc_t app_desc = {.magic_word = 0xABCD1234,
                           .secure_version = 1,
                           .reserv1 = {0, 0},
#ifdef GIT_VERSION
                           .version = GIT_VERSION,
#else
                           .version = "release/v0.9.0-9-g1348e563",
#endif
                           .project_name = "MyProject",
                           .time = "12:34:56",
                           .date = "2023-08-31",
                           .idf_ver = "4.0",
                           .app_elf_sha256 = "abcdabcd",
                           .reserv2 = {0}};

const esp_app_desc_t *esp_app_get_description(void) { return &app_desc; }

int esp_app_get_elf_sha256(char *dst, size_t size) { return snprintf(dst, size, "fakesha"); }