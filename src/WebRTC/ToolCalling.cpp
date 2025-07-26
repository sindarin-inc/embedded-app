#include "ToolCalling.hpp"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_webrtc.h>
#include <string.h>

#define ELEMS(a) (sizeof(a) / sizeof(a[0]))

typedef struct attribute_t attribute_t;
typedef struct class_t class_t;
typedef enum {
    ATTRIBUTE_TYPE_NONE,
    ATTRIBUTE_TYPE_BOOL,
    ATTRIBUTE_TYPE_INT,
    ATTRIBUTE_TYPE_PARENT,
} attribute_type_t;

struct attribute_t {
    char *name;
    char *desc;
    attribute_type_t type;
    union {
        bool b_state;
        int i_value;
        attribute_t *attr_list;
    };
    int attr_num;
    bool required;
    int (*control)(attribute_t *attr);
};

struct class_t {
    char *name;
    char *desc;
    attribute_t *attr_list;
    int attr_num;
    class_t *next;
};

static class_t *classes = NULL;

static int set_light_on_off(attribute_t *attr) {
    printf("Light set to %s\n", attr->b_state ? "ON" : "OFF");
    return 0;
}

static int set_light_color_red(attribute_t *attr) {
    printf("Red set to %d\n", attr->i_value);
    return 0;
}

static int set_light_color_blue(attribute_t *attr) {
    printf("Blue set to %d\n", attr->i_value);
    return 0;
}

static int set_light_color_green(attribute_t *attr) {
    printf("Green set to %d\n", attr->i_value);
    return 0;
}

static int set_speaker_volume(attribute_t *attr) {
    printf("Volume set to %d\n", attr->i_value);
    return 0;
}

static int set_door_state(attribute_t *attr) {
    printf("Door is %s\n", attr->b_state ? "Opened" : "Closed");
    return 0;
}

static class_t *build_volume_class(void) {
    class_t *vol = (class_t *)calloc(1, sizeof(class_t));
    if (vol == NULL) {
        return NULL;
    }
    static attribute_t vol_attrs[] = {
        {
            .name = "volume",
            .desc = "Speaker volume range 0-100",
            .type = ATTRIBUTE_TYPE_INT,
            .required = true,
            .control = set_speaker_volume,
        },
    };
    vol->name = "SetVolume";
    vol->desc = "Changes speaker volume";
    vol->attr_list = vol_attrs;
    vol->attr_num = ELEMS(vol_attrs);
    return vol;
}

static class_t *build_door_class(void) {
    class_t *vol = (class_t *)calloc(1, sizeof(class_t));
    if (vol == NULL) {
        return NULL;
    }
    static attribute_t vol_attrs[] = {
        {
            .name = "open",
            .desc = "Open or close the door",
            .type = ATTRIBUTE_TYPE_BOOL,
            .required = true,
            .control = set_door_state,
        },
    };
    vol->name = "OpenDoor";
    vol->desc = "Toggle the door state to open or close";
    vol->attr_list = vol_attrs;
    vol->attr_num = ELEMS(vol_attrs);
    return vol;
}

static class_t *build_light_class(void) {
    class_t *light = (class_t *)calloc(1, sizeof(class_t));
    if (light == NULL) {
        return NULL;
    }
    static attribute_t light_color[] = {
        {
            .name = "red",
            .desc = "Red value in the range of 0-255",
            .type = ATTRIBUTE_TYPE_INT,
            .required = true,
            .control = set_light_color_red,
        },
        {
            .name = "green",
            .desc = "Green value in the range of 0-255",
            .type = ATTRIBUTE_TYPE_INT,
            .required = true,
            .control = set_light_color_green,
        },
        {
            .name = "blue",
            .desc = "Blue value in the range of 0-255",
            .type = ATTRIBUTE_TYPE_INT,
            .required = true,
            .control = set_light_color_blue,
        },
    };
    static attribute_t light_attrs[] = {
        {
            .name = "LightState",
            .desc = "New light state (true or false is expected)",
            .type = ATTRIBUTE_TYPE_BOOL,
            .required = true,
            .control = set_light_on_off,
        },
        {
            .name = "LightColor",
            .desc = "Set light color of red, green and blue",
            .type = ATTRIBUTE_TYPE_PARENT,
            .attr_list = light_color,
            .attr_num = ELEMS(light_color),
        },
    };
    light->name = "SetLightState";
    light->desc = "Changes the state of the light";
    light->attr_list = light_attrs;
    light->attr_num = ELEMS(light_attrs);
    return light;
}

static void add_class(class_t *cls) {
    if (classes == NULL) {
        classes = cls;
    } else {
        classes->next = cls;
    }
}

int build_toolcalls(void) {
    static bool build_once = false;
    if (build_once) {
        return 0;
    }
    add_class(build_light_class());
    add_class(build_volume_class());
    add_class(build_door_class());
    build_once = true;
    return 0;
}

static char *get_attr_type(attribute_type_t type) {
    if (type == ATTRIBUTE_TYPE_BOOL) {
        return "boolean";
    }
    if (type == ATTRIBUTE_TYPE_INT) {
        return "integer";
    }
    if (type == ATTRIBUTE_TYPE_PARENT) {
        return "object";
    }
    return "";
}

static int add_parent_attribute(cJSON *parent, attribute_t *attr) {
    cJSON *properties = cJSON_CreateObject();
    cJSON_AddItemToObject(parent, "properties", properties);
    int require_num = 0;
    for (int i = 0; i < attr->attr_num; i++) {
        attribute_t *sub_attr = &attr->attr_list[i];
        cJSON *prop = cJSON_CreateObject();
        cJSON_AddItemToObject(properties, sub_attr->name, prop);
        cJSON_AddStringToObject(prop, "type", get_attr_type(sub_attr->type));
        cJSON_AddStringToObject(prop, "description", sub_attr->desc);
        if (sub_attr->type == ATTRIBUTE_TYPE_PARENT) {
            add_parent_attribute(prop, sub_attr);
        }
        if (sub_attr->required) {
            require_num++;
        }
    }
    if (require_num) {
        cJSON *requiresArray = cJSON_CreateArray();
        for (int i = 0; i < attr->attr_num; i++) {
            attribute_t *sub_attr = &attr->attr_list[i];
            if (sub_attr->required) {
                cJSON_AddItemToArray(requiresArray, cJSON_CreateString(sub_attr->name));
            }
        }
        cJSON_AddItemToObject(parent, "required", requiresArray);
    }
    return 0;
}

int generate_toolcall_description_json(cJSON *root) {
    if (classes == NULL) {
        return 0;
    }
    cJSON_AddStringToObject(root, "type", "session.update");
    cJSON *session = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "session", session);

    cJSON *modalities = cJSON_CreateArray();
    cJSON_AddItemToArray(modalities, cJSON_CreateString("text"));
    cJSON_AddItemToArray(modalities, cJSON_CreateString("audio"));

    cJSON_AddItemToObject(session, "modalities", modalities);
    cJSON_AddNullToObject(session, "input_audio_transcription");
    cJSON *tools = cJSON_CreateArray();
    cJSON_AddItemToObject(session, "tools", tools);

    class_t *iter = classes;
    while (iter) {
        cJSON *tool = cJSON_CreateObject();
        cJSON_AddItemToArray(tools, tool);
        cJSON_AddStringToObject(tool, "type", "function");
        cJSON_AddStringToObject(tool, "name", iter->name);
        cJSON_AddStringToObject(tool, "description", iter->desc);
        cJSON *parameters = cJSON_CreateObject();
        cJSON_AddItemToObject(tool, "parameters", parameters);
        cJSON_AddStringToObject(parameters, "type", "object");
        cJSON *properties = cJSON_CreateObject();
        cJSON_AddItemToObject(parameters, "properties", properties);
        int require_num = 0;
        for (int i = 0; i < iter->attr_num; i++) {
            attribute_t *attr = &iter->attr_list[i];
            cJSON *prop = cJSON_CreateObject();
            cJSON_AddItemToObject(properties, attr->name, prop);
            cJSON_AddStringToObject(prop, "type", get_attr_type(attr->type));
            cJSON_AddStringToObject(prop, "description", attr->desc);
            if (attr->type == ATTRIBUTE_TYPE_PARENT) {
                add_parent_attribute(prop, attr);
            }
            if (attr->required) {
                require_num++;
            }
        }
        if (require_num) {
            cJSON *requiresArray = cJSON_CreateArray();
            for (int i = 0; i < iter->attr_num; i++) {
                attribute_t *attr = &iter->attr_list[i];
                if (attr->required) {
                    cJSON_AddItemToArray(requiresArray, cJSON_CreateString(attr->name));
                }
            }
            cJSON_AddItemToObject(parameters, "required", requiresArray);
        }
        iter = iter->next;
    }
    return 0;
}

static int match_and_execute(cJSON *cur, attribute_t *attr) {
    cJSON *attr_value = cJSON_GetObjectItemCaseSensitive(cur, attr->name);
    if (!attr_value) {
        if (attr->required) {
            printf("Missing required attribute: %s\n", attr->name);
        }
        return 0;
    }

    // Process based on attribute type
    if (attr->type == ATTRIBUTE_TYPE_BOOL && cJSON_IsBool(attr_value)) {
        attr->b_state = cJSON_IsTrue(attr_value);
        if (attr->control) {
            attr->control(attr);
        }
    } else if (attr->type == ATTRIBUTE_TYPE_INT && cJSON_IsNumber(attr_value)) {
        attr->i_value = attr_value->valueint;
        if (attr->control) {
            attr->control(attr);
        }
    } else if (attr->type == ATTRIBUTE_TYPE_PARENT && cJSON_IsObject(attr_value)) {
        // Process nested attributes
        for (int j = 0; j < attr->attr_num; j++) {
            attribute_t *sub_attr = &attr->attr_list[j];
            match_and_execute(attr_value, sub_attr);
        }
    } else {
        printf("Unhandled attribute type or invalid value for: %s\n", attr->name);
    }

    return 1; // Success
}

int handle_received_json(const char *json_data) {
    cJSON *root = cJSON_Parse(json_data);
    if (!root) {
        printf("Error parsing JSON data\n");
        return -1; // Parsing error
    }

    const cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type) ||
        strcmp(type->valuestring, "response.function_call_arguments.done") != 0) {
        cJSON_Delete(root);
        return 0;
    }
    char *payload = cJSON_PrintUnformatted(root);
    if (payload) {
        printf("Function Call %s\n", payload);
        free(payload);
    }

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    const cJSON *arguments = cJSON_GetObjectItemCaseSensitive(root, "arguments");
    if (!cJSON_IsString(name) || !name->valuestring || !cJSON_IsString(arguments) ||
        !arguments->valuestring) {
        printf("Invalid JSON format\n");
        cJSON_Delete(root);
        return -1; // Invalid format
    }

    cJSON *args_root = cJSON_Parse(arguments->valuestring);
    if (!args_root) {
        printf("Error parsing arguments JSON\n");
        cJSON_Delete(root);
        return -1; // Parsing error
    }

    // Find the corresponding class and attributes
    class_t *iter = classes;
    while (iter) {
        if (strcmp(iter->name, name->valuestring) == 0) {
            for (int i = 0; i < iter->attr_num; i++) {
                attribute_t *attr = &iter->attr_list[i];
                match_and_execute(args_root, attr);
            }
        }
        iter = iter->next;
    }

    cJSON_Delete(args_root);
    cJSON_Delete(root);
    return 0;
}
