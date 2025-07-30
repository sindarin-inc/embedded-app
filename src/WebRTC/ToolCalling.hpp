#pragma once

#include <cJSON.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// Keep the exact same C structures so the generated JSON remains unchanged
// ----------------------------------------------------------------------

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
    int attr_num = 0;
    bool required = false;
    int (*control)(attribute_t *attr) = nullptr;
};

struct class_t {
    char *name;
    char *desc;
    attribute_t *attr_list;
    int attr_num = 0;
    class_t *next = nullptr;
};

// ----------------------------------------------------------------------
// C++ wrapper that allows registering tool calls with lambdas
// ----------------------------------------------------------------------

class ToolCalling {
public:
    using ControlLambda = std::function<int(attribute_t *, const char *)>;

    ToolCalling() = default;
    ~ToolCalling();

    // Disable copy – the class owns raw pointers in linked-lists.
    ToolCalling(const ToolCalling &) = delete;
    ToolCalling &operator=(const ToolCalling &) = delete;

    // Attribute helpers -------------------------------------------------
    attribute_t *createBoolAttribute(const char *name, const char *desc, bool required,
                                     ControlLambda cb);
    attribute_t *createIntAttribute(const char *name, const char *desc, bool required,
                                    ControlLambda cb);
    attribute_t *createParentAttribute(const char *name, const char *desc,
                                       const std::vector<attribute_t *> &children,
                                       bool required = false);

    // Register a full tool (function) composed of attributes
    void registerTool(const char *name, const char *desc,
                      const std::vector<attribute_t *> &attributes);
    void registerTool(const char *name, const char *desc,
                      std::initializer_list<attribute_t *> attributes) {
        registerTool(name, desc, std::vector<attribute_t *>{attributes});
    }

    // Runtime helpers ---------------------------------------------------
    cJSON *generateDescriptionJson() const;
    int handleReceivedJson(const char *json_data) const;

private:
    static std::unordered_map<attribute_t *, ControlLambda> lambdaMap_;
    friend int matchAndExecute(cJSON *, attribute_t *, const char *);

    class_t *classesHead_ = nullptr;
    class_t *classesTail_ = nullptr;

    // Utility -----------------------------------------------------------
    static char *strdupSafe(const char *str);
    static char *getAttrType(attribute_type_t type);
};
