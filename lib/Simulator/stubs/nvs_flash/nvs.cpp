#include "nvs.h"

#include <cstring>
#include <inicpp.h>
#include <optional>
#include <string>
#include <vector>

#include "Base64.hpp"
#include "sindarin-debug.h"

static const char *PrefsKey = "prefs";
static std::string SimPrefsIniFilename;

ini::IniFile IniFile;

namespace ini {

template <typename T>
struct Convert<std::optional<T>> {
    void decode(const std::string &value, std::optional<T> &result) {
        if (value.length() == 0) {
            return;
        }
        Convert<T> conv;
        T resultTmp;
        conv.decode(value, resultTmp);
        result = resultTmp;
    }

    void encode(const std::optional<T> &value, std::string &result) {
        std::stringstream ss;
        ss << value.value_or("");
        result = ss.str();
    }
};

} // namespace ini

esp_err_t nvs_open(const char *name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle) {
    SimPrefsIniFilename = name;
    IniFile.load(name);
    return ESP_OK;
}

esp_err_t nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value) {
    IniFile[PrefsKey][key] = (int)value;
    return ESP_OK;
}

esp_err_t nvs_set_i16(nvs_handle_t handle, const char *key, int16_t value) {
    IniFile[PrefsKey][key] = value;
    return ESP_OK;
}

esp_err_t nvs_set_u16(nvs_handle_t handle, const char *key, uint16_t value) {
    IniFile[PrefsKey][key] = value;
    return ESP_OK;
}

esp_err_t nvs_set_i32(nvs_handle_t handle, const char *key, int32_t value) {
    IniFile[PrefsKey][key] = value;
    return ESP_OK;
}

esp_err_t nvs_set_u32(nvs_handle_t handle, const char *key, uint32_t value) {
    IniFile[PrefsKey][key] = value;
    return ESP_OK;
}

esp_err_t nvs_set_u64(nvs_handle_t handle, const char *key, uint64_t value) {
    IniFile[PrefsKey][key] = value;
    return ESP_OK;
}

esp_err_t nvs_set_str(nvs_handle_t handle, const char *key, const char *value) {
    IniFile[PrefsKey][key] = value;
    return ESP_OK;
}

esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length) {
    std::vector<unsigned char> data(static_cast<const unsigned char *>(value),
                                    static_cast<const unsigned char *>(value) + length);
    std::string encoded = base64_encode(data.data(), data.size());
    IniFile[PrefsKey][key] = encoded;
    return ESP_OK;
}

esp_err_t nvs_get_u8(nvs_handle_t handle, const char *key, uint8_t *out_value) {
    auto opt = IniFile[PrefsKey][key].as<std::optional<int>>();
    if (opt.has_value()) {
        *out_value = opt.value();
        return ESP_OK;
    } else {
        return (esp_err_t)ESP_ERR_NVS_NOT_FOUND;
    }
}

esp_err_t nvs_get_i16(nvs_handle_t handle, const char *key, int16_t *out_value) {
    auto opt = IniFile[PrefsKey][key].as<std::optional<int16_t>>();
    if (opt.has_value()) {
        *out_value = opt.value();
        return ESP_OK;
    } else {
        return (esp_err_t)ESP_ERR_NVS_NOT_FOUND;
    }
}

esp_err_t nvs_get_u16(nvs_handle_t handle, const char *key, uint16_t *out_value) {
    auto opt = IniFile[PrefsKey][key].as<std::optional<uint16_t>>();
    if (opt.has_value()) {
        *out_value = opt.value();
        return ESP_OK;
    } else {
        return (esp_err_t)ESP_ERR_NVS_NOT_FOUND;
    }
}

esp_err_t nvs_get_i32(nvs_handle_t handle, const char *key, int32_t *out_value) {
    auto opt = IniFile[PrefsKey][key].as<std::optional<int32_t>>();
    if (opt.has_value()) {
        *out_value = opt.value();
        return ESP_OK;
    } else {
        return (esp_err_t)ESP_ERR_NVS_NOT_FOUND;
    }
}

esp_err_t nvs_get_u32(nvs_handle_t handle, const char *key, uint32_t *out_value) {
    auto opt = IniFile[PrefsKey][key].as<std::optional<uint32_t>>();
    if (opt.has_value()) {
        *out_value = opt.value();
        return ESP_OK;
    } else {
        return (esp_err_t)ESP_ERR_NVS_NOT_FOUND;
    }
}

esp_err_t nvs_get_u64(nvs_handle_t handle, const char *key, uint64_t *out_value) {
    auto opt = IniFile[PrefsKey][key].as<std::optional<uint64_t>>();
    if (opt.has_value()) {
        *out_value = opt.value();
        return ESP_OK;
    } else {
        return (esp_err_t)ESP_ERR_NVS_NOT_FOUND;
    }
}

// NOTE: nvs_get_str's returned length includes the null terminator (it is a buffer length)
esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *out_value, size_t *length) {
    auto string = IniFile[PrefsKey][key].as<const char *>();
    if (strlen(string) > 0) {
        if (out_value != nullptr) {
            // Include the null terminator character
            strncpy(out_value, string, strlen(string) + 1);
        }
        *length = strlen(string) + 1;
        return ESP_OK;
    } else {
        return (esp_err_t)ESP_ERR_NVS_NOT_FOUND;
    }
}

esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length) {
    auto encoded = IniFile[PrefsKey][key].as<std::string>();
    if (encoded.empty()) {
        return ESP_ERR_NVS_NOT_FOUND;
    }

    size_t decoded_size = base64_decode(encoded, nullptr, 0);

    if (out_value == nullptr) {
        *length = decoded_size;
        return ESP_OK;
    }

    if (*length < decoded_size) {
        return ESP_ERR_NVS_INVALID_LENGTH;
    }

    base64_decode(encoded, out_value, *length);
    *length = decoded_size;
    return ESP_OK;
}

esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key) {
    IniFile[PrefsKey].erase(key);
    return ESP_OK;
}

esp_err_t nvs_erase_all(nvs_handle_t handle) {
    IniFile.clear();
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t handle) {
    IniFile.save(SimPrefsIniFilename);
    return ESP_OK;
}

void nvs_close(nvs_handle_t handle) { return; }
