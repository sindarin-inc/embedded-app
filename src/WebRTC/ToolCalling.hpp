#pragma once

#include <cJSON.h>

int build_toolcalls(void);

int generate_toolcall_description_json(cJSON *root);

int handle_received_json(const char *json_data);
