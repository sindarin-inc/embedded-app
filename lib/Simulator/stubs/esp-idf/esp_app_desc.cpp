
#include "esp_app_desc.h"

#include <cstdio>

// Initialize a global esp_app_desc_t struct
esp_app_desc_t app_desc = {0xABCD1234,
                           1,
                           {0, 0},
                           "1.0.0",
                           "MyProject",
                           "12:34:56",
                           "2023-08-31",
                           "4.0",
                           "abcdabcd",
                           0};

const esp_app_desc_t *esp_app_get_description(void) { return &app_desc; }

int esp_app_get_elf_sha256(char *dst, size_t size) { return snprintf(dst, size, "fakesha"); }


