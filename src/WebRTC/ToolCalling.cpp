#include "ToolCalling.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

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

// JSON helpers --------------------------------------------------------------
int ToolCalling::addParentAttributeToJson(cJSON *parent, attribute_t *attr) {
    cJSON *properties = cJSON_CreateObject();
    cJSON_AddItemToObject(parent, "properties", properties);
    int require_num = 0;
    for (int i = 0; i < attr->attr_num; i++) {
        attribute_t *sub_attr = &attr->attr_list[i];
        cJSON *prop = cJSON_CreateObject();
        cJSON_AddItemToObject(properties, sub_attr->name, prop);
        cJSON_AddStringToObject(prop, "type", getAttrType(sub_attr->type));
        cJSON_AddStringToObject(prop, "description", sub_attr->desc);
        if (sub_attr->type == ATTRIBUTE_TYPE_PARENT) {
            addParentAttributeToJson(prop, sub_attr);
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

int ToolCalling::generateDescriptionJson(cJSON *root) const {
    if (classesHead_ == nullptr) {
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

    for (class_t *iter = classesHead_; iter; iter = iter->next) {
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
            cJSON_AddStringToObject(prop, "type", getAttrType(attr->type));
            cJSON_AddStringToObject(prop, "description", attr->desc);
            if (attr->type == ATTRIBUTE_TYPE_PARENT) {
                addParentAttributeToJson(prop, attr);
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
    }
    return 0;
}

// Execution helpers ---------------------------------------------------------
int matchAndExecute(cJSON *cur, attribute_t *attr, const char *callId, const char *json_data) {
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
            ToolCalling::lambdaMap_[attr](attr, callId, json_data);
        }
    } else if (attr->type == ATTRIBUTE_TYPE_INT && cJSON_IsNumber(attr_value)) {
        attr->i_value = attr_value->valueint;
        if (ToolCalling::lambdaMap_.count(attr) > 0) {
            ToolCalling::lambdaMap_[attr](attr, callId, json_data);
        }
    } else if (attr->type == ATTRIBUTE_TYPE_PARENT && cJSON_IsObject(attr_value)) {
        // Process nested attributes
        for (int j = 0; j < attr->attr_num; j++) {
            attribute_t *sub_attr = &attr->attr_list[j];
            matchAndExecute(attr_value, sub_attr, callId, json_data);
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
    char *payload = cJSON_PrintUnformatted(root);
    if (payload) {
        printf("Function Call %s\n", payload);
        free(payload);
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
                matchAndExecute(args_root, attr, callIdStr.c_str(), json_data);
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
