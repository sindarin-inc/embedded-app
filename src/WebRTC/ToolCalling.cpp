#include "ToolCalling.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <esp_log.h>
#include <unordered_map>
#include <vector>

#include "WebRTC/OpenAIAPI.hpp"

constexpr const char *TAG = "OpenAI";

#define ELEMS(a) (sizeof(a) / sizeof(a[0]))

// Static member definition --------------------------------------------------
std::unordered_map<attribute_t *, ToolCalling::ControlLambda> ToolCalling::lambdaMap_;

// Utility helpers -----------------------------------------------------------
char *ToolCalling::strdupSafe(const char *str) {
    if (!str) {
        return nullptr;
    }
    size_t len = strlen(str) + 1;
    char *dup = static_cast<char *>(malloc(len));
    if (dup) {
        memcpy(dup, str, len);
    }
    return dup;
}

char *ToolCalling::getAttrType(attribute_type_t type) {
    switch (type) {
    case ATTRIBUTE_TYPE_BOOL:
        return (char *)"boolean";
    case ATTRIBUTE_TYPE_INT:
        return (char *)"integer";
    case ATTRIBUTE_TYPE_PARENT:
        return (char *)"object";
    default:
        return (char *)"";
    }
}

// Attribute creation --------------------------------------------------------
attribute_t *ToolCalling::createBoolAttribute(const char *name, const char *desc, bool required,
                                              ControlLambda cb) {
    attribute_t *attr = new attribute_t{};
    attr->name = strdupSafe(name);
    attr->desc = strdupSafe(desc);
    attr->type = ATTRIBUTE_TYPE_BOOL;
    attr->required = required;
    lambdaMap_[attr] = std::move(cb);
    return attr;
}

attribute_t *ToolCalling::createIntAttribute(const char *name, const char *desc, bool required,
                                             ControlLambda cb) {
    attribute_t *attr = new attribute_t{};
    attr->name = strdupSafe(name);
    attr->desc = strdupSafe(desc);
    attr->type = ATTRIBUTE_TYPE_INT;
    attr->required = required;
    lambdaMap_[attr] = std::move(cb);
    return attr;
}

attribute_t *ToolCalling::createParentAttribute(const char *name, const char *desc,
                                                const std::vector<attribute_t *> &children,
                                                bool required) {
    attribute_t *attr = new attribute_t{};
    attr->name = strdupSafe(name);
    attr->desc = strdupSafe(desc);
    attr->type = ATTRIBUTE_TYPE_PARENT;
    attr->required = required;
    attr->attr_num = static_cast<int>(children.size());
    if (attr->attr_num > 0) {
        attribute_t *arr = new attribute_t[attr->attr_num];
        for (int i = 0; i < attr->attr_num; ++i) {
            arr[i] = *children[i]; // copy
            // Propagate lambda mapping to the copied instance if present
            auto it = lambdaMap_.find(children[i]);
            if (it != lambdaMap_.end()) {
                lambdaMap_[&arr[i]] = it->second;
            }
        }
        attr->attr_list = arr;
    } else {
        attr->attr_list = nullptr;
    }
    // Parent attributes don't get a control callback directly.
    return attr;
}

// Tool registration ---------------------------------------------------------
void ToolCalling::registerTool(const char *name, const char *desc,
                               const std::vector<attribute_t *> &attributes) {
    class_t *cls = new class_t{};
    cls->name = strdupSafe(name);
    cls->desc = strdupSafe(desc);
    cls->attr_num = static_cast<int>(attributes.size());
    if (cls->attr_num > 0) {
        attribute_t *arr = new attribute_t[cls->attr_num];
        for (int i = 0; i < cls->attr_num; ++i) {
            arr[i] = *attributes[i]; // copy value
            auto it = lambdaMap_.find(attributes[i]);
            if (it != lambdaMap_.end()) {
                lambdaMap_[&arr[i]] = it->second;
            }
        }
        cls->attr_list = arr;
    } else {
        cls->attr_list = nullptr;
    }

    // Append to linked list
    if (classesHead_ == nullptr) {
        classesHead_ = classesTail_ = cls;
    } else {
        classesTail_->next = cls;
        classesTail_ = cls;
    }
}

// ----------------------------------------------------------------------
//  OpenAIAPI-based JSON generation
// ----------------------------------------------------------------------

cJSON *ToolCalling::generateDescriptionJson() const {
    using namespace OpenAIAPI;

    // Convert internal tool definitions to OpenAIAPI structures --------
    std::vector<Tool> tools;

    for (class_t *cls = classesHead_; cls; cls = cls->next) {
        // Build property list for this tool ----------------------------
        std::vector<Tool::ToolParameter::ToolParameterProperties::ToolParameterProperty>
            propertyList;
        std::vector<char *> requiredNames;

        for (int i = 0; i < cls->attr_num; ++i) {
            attribute_t *attr = &cls->attr_list[i];

            propertyList.push_back({attr->name, getAttrType(attr->type), attr->desc});

            if (attr->required) {
                requiredNames.push_back(attr->name);
            }
        }

        // Compose parameter object ------------------------------------

        // Compose tool -------------------------------------------------
        Tool tool = {
            .name = cls->name,
            .description = cls->desc,
            .parameters = {{
                .type = "object",
                .properties = propertyList,
                .required = requiredNames,
            }},
        };

        tools.push_back(tool);
    }

    SessionUpdateEvent event = {
        .session =
            {
                .modalities = {"text", "audio"},
                .inputAudioTranscription = nullptr,
                .tools = tools,
                .toolChoice = "auto",
            },
    };

    // Caller takes ownership of returned JSON object.
    return event.toJson();
}

// Execution helpers ---------------------------------------------------------
int matchAndExecute(cJSON *cur, attribute_t *attr, const char *callId) {
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
        if (ToolCalling::lambdaMap_.count(attr) > 0) {
            ESP_LOGI(TAG, "Calling lambda for %s", attr->name);
            ToolCalling::lambdaMap_[attr](attr, callId);
        }
    } else if (attr->type == ATTRIBUTE_TYPE_INT && cJSON_IsNumber(attr_value)) {
        attr->i_value = attr_value->valueint;
        if (ToolCalling::lambdaMap_.count(attr) > 0) {
            ESP_LOGI(TAG, "Calling lambda for %s", attr->name);
            ToolCalling::lambdaMap_[attr](attr, callId);
        }
    } else if (attr->type == ATTRIBUTE_TYPE_PARENT && cJSON_IsObject(attr_value)) {
        // Process nested attributes
        for (int j = 0; j < attr->attr_num; j++) {
            attribute_t *sub_attr = &attr->attr_list[j];
            matchAndExecute(attr_value, sub_attr, callId);
        }
    } else {
        printf("Unhandled attribute type or invalid value for: %s\n", attr->name);
    }

    return 1; // Success
}

int ToolCalling::handleReceivedJson(const char *json_data) const {
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
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    const cJSON *arguments = cJSON_GetObjectItemCaseSensitive(root, "arguments");
    const cJSON *tool_call_id = cJSON_GetObjectItemCaseSensitive(root, "call_id");
    std::string callIdStr;
    if (cJSON_IsString(tool_call_id) && tool_call_id->valuestring) {
        callIdStr = tool_call_id->valuestring;
    }
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
    for (class_t *iter = classesHead_; iter; iter = iter->next) {
        if (strcmp(iter->name, name->valuestring) == 0) {
            for (int i = 0; i < iter->attr_num; i++) {
                attribute_t *attr = &iter->attr_list[i];
                matchAndExecute(args_root, attr, callIdStr.c_str());
            }
        }
    }

    cJSON_Delete(args_root);
    cJSON_Delete(root);
    return 0;
}

// Destructor ----------------------------------------------------------------
ToolCalling::~ToolCalling() {
    // Free all allocated memory for classes and attributes
    for (class_t *cls = classesHead_; cls != nullptr;) {
        for (int i = 0; i < cls->attr_num; ++i) {
            attribute_t *attr = &cls->attr_list[i];
            free(attr->name);
            free(attr->desc);
            if (attr->type == ATTRIBUTE_TYPE_PARENT && attr->attr_list) {
                delete[] attr->attr_list; // Children were deep copied
            }
        }
        delete[] cls->attr_list;
        free(cls->name);
        free(cls->desc);
        class_t *next = cls->next;
        delete cls;
        cls = next;
    }
}
