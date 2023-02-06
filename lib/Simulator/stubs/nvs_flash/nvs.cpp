#include "nvs.h"

#include <inicpp.h>
#include <optional>
#include <string>

#include "sindarin-debug.h"

static const char *PrefsKey = "prefs";
static const char *SimprefsIni = "simprefs.ini";

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
    IniFile.load(SimprefsIni);
    return ESP_OK;
}

esp_err_t nvs_set_i32(nvs_handle_t handle, const char *key, int32_t value) {
    IniFile[PrefsKey][key] = value;
    return ESP_OK;
}

// esp_err_t nvs_set_u64(nvs_handle_t handle, const char *key, uint64_t value) {
//     IniFile[PrefsKey][key] = value;
//     return ESP_OK;
// }

esp_err_t nvs_set_str(nvs_handle_t handle, const char *key, const char *value) {
    IniFile[PrefsKey][key] = value;
    return ESP_OK;
}

esp_err_t nvs_get_i32(nvs_handle_t handle, const char *key, int32_t *out_value) {
    auto opt = IniFile[PrefsKey][key].as<std::optional<int32_t>>();
    if (opt.has_value()) {
        *out_value = opt.value();
        return ESP_OK;
    } else {
        return ESP_ERR_NVS_NOT_FOUND;
    }
}

// esp_err_t nvs_get_u64(nvs_handle_t handle, const char *key, uint64_t *out_value) {
//     if (!IniFile[PrefsKey].has(key)) {
//         return ESP_ERR_NVS_NOT_FOUND;
//     }
//     return IniFile[PrefsKey][key].as<uint64_t>();
//     return ESP_OK;
// }

esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *out_value, size_t *length) {
    auto string = IniFile[PrefsKey][key].as<const char *>();
    if (strlen(string) > 0) {
        if (out_value != nullptr) {
            strncpy(out_value, string, strlen(string) + 1);
        }
        *length = strlen(string);
        return ESP_OK;
    } else {
        return ESP_ERR_NVS_NOT_FOUND;
    }
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
    IniFile.save(SimprefsIni);
    return ESP_OK;
}
