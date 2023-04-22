#include "PreferencesStore.hpp"

#include <esp_attr.h>
#include <memory>

RTC_SLOW_ATTR SyncReason PreferencesStore::syncReason;
RTC_SLOW_ATTR uint64_t PreferencesStore::nextSyncTime;
RTC_SLOW_ATTR uint8_t PreferencesStore::lastBatteryReport;
RTC_SLOW_ATTR uint8_t PreferencesStore::lastPageTurnerBattery;
RTC_SLOW_ATTR bool PreferencesStore::settingsOnRestart;

void DumpNVS() {
#if !SIMULATOR
    nvs_iterator_t it = nullptr;
    esp_err_t result = nvs_entry_find("nvs", nullptr, NVS_TYPE_ANY, &it);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        log_e("No such entry was found");
        return;
    }

    if (result != ESP_OK) {
        log_e("nvs_entry_find error: %s", esp_err_to_name(result));
        return;
    }

    auto typeToStr = [](nvs_type_t type) {
        switch (type) {
        case NVS_TYPE_I8:
            return "i8";
        case NVS_TYPE_U8:
            return "u8";
        case NVS_TYPE_U16:
            return "u16";
        case NVS_TYPE_I16:
            return "i16";
        case NVS_TYPE_U32:
            return "u32";
        case NVS_TYPE_I32:
            return "i32";
        case NVS_TYPE_U64:
            return "u64";
        case NVS_TYPE_I64:
            return "i64";
        case NVS_TYPE_STR:
            return "str";
        case NVS_TYPE_BLOB:
            return "blob";
        case NVS_TYPE_ANY:
            return "any";
        default:
            return "Unknown";
        }
    };

    std::unordered_map<std::string, nvs_handle_t> handleMap;

    do {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        result = nvs_entry_next(&it);
        if (result != ESP_OK) {
            break;
        }

        // Check if the namespace has already been opened
        auto handleIter = handleMap.find(info.namespace_name);
        if (handleIter == handleMap.end()) {
            // Namespace has not been opened, so open it and add to the map
            nvs_handle_t nvsHandle;
            esp_err_t err = nvs_open(info.namespace_name, NVS_READONLY, &nvsHandle);
            if (err != ESP_OK) {
                return;
            }
            handleMap.emplace(info.namespace_name, nvsHandle);
            handleIter = handleMap.find(info.namespace_name);
        }
        nvs_handle_t nvsHandle = handleIter->second;

        switch (info.type) {
        case NVS_TYPE_I8: {
            int8_t value;
            result = nvs_get_i8(nvsHandle, info.key, &value);
            if (result != ESP_OK) {
                log_e("NVS error: %s", esp_err_to_name(result));
                continue;
            }
            log_i("%s:%s (%s) = %" PRIi8 "", info.namespace_name, info.key, typeToStr(info.type),
                  value);
            break;
        }
        case NVS_TYPE_U8: {
            uint8_t value;
            result = nvs_get_u8(nvsHandle, info.key, &value);
            if (result != ESP_OK) {
                log_e("NVS error: %s", esp_err_to_name(result));
                continue;
            }
            log_i("%s:%s (%s) = %" PRIu8 "", info.namespace_name, info.key, typeToStr(info.type),
                  value);
            break;
        }
        case NVS_TYPE_U16: {
            uint16_t value;
            result = nvs_get_u16(nvsHandle, info.key, &value);
            if (result != ESP_OK) {
                log_e("NVS error: %s", esp_err_to_name(result));
                continue;
            }
            log_i("%s:%s (%s) = %" PRIu16 "", info.namespace_name, info.key, typeToStr(info.type),
                  value);
            break;
        }
        case NVS_TYPE_I16: {
            int16_t value;
            result = nvs_get_i16(nvsHandle, info.key, &value);
            if (result != ESP_OK) {
                log_e("NVS error: %s", esp_err_to_name(result));
                continue;
            }
            log_i("%s:%s (%s) = %" PRIi16 "", info.namespace_name, info.key, typeToStr(info.type),
                  value);
            break;
        }
        case NVS_TYPE_U32: {
            uint32_t value;
            result = nvs_get_u32(nvsHandle, info.key, &value);
            if (result != ESP_OK) {
                log_e("NVS error: %s", esp_err_to_name(result));
                continue;
            }
            log_i("%s:%s (%s) = %" PRIu32 "", info.namespace_name, info.key, typeToStr(info.type),
                  value);
            break;
        }
        case NVS_TYPE_I32: {
            int32_t value;
            result = nvs_get_i32(nvsHandle, info.key, &value);
            if (result != ESP_OK) {
                log_e("NVS error: %s", esp_err_to_name(result));
                continue;
            }
            log_i("%s:%s (%s) = %" PRIi32 "", info.namespace_name, info.key, typeToStr(info.type),
                  value);
            break;
        }
        case NVS_TYPE_U64: {
            uint64_t value;
            result = nvs_get_u64(nvsHandle, info.key, &value);
            if (result != ESP_OK) {
                log_e("NVS error: %s", esp_err_to_name(result));
                continue;
            }
            log_i("%s:%s (%s) = %" PRIu64 "", info.namespace_name, info.key, typeToStr(info.type),
                  value);
            break;
        }
        case NVS_TYPE_I64: {
            int64_t value;
            result = nvs_get_i64(nvsHandle, info.key, &value);
            if (result != ESP_OK) {
                log_e("NVS error: %s", esp_err_to_name(result));
                continue;
            }
            log_i("%s:%s (%s) = %" PRIi64 "", info.namespace_name, info.key, typeToStr(info.type),
                  value);
            break;
        }
        case NVS_TYPE_STR: {
            size_t strSize = 0;
            result = nvs_get_str(nvsHandle, info.key, nullptr, &strSize);
            if (result != ESP_OK) {
                log_e("NVS error: %s", esp_err_to_name(result));
                continue;
            }
            std::unique_ptr<char[]> str(new char[strSize]);
            result = nvs_get_str(nvsHandle, info.key, str.get(), &strSize);
            if (result != ESP_OK) {
                log_e("NVS error: %s", esp_err_to_name(result));
                continue;
            }
            log_i("%s:%s (%s) = \"%s\" (%zu bytes)", info.namespace_name, info.key,
                  typeToStr(info.type), str.get(), strSize);
            break;
        }
        case NVS_TYPE_BLOB: {
            size_t blobSize = 0;
            result = nvs_get_blob(nvsHandle, info.key, nullptr, &blobSize);
            if (result != ESP_OK) {
                log_e("NVS error: %s", esp_err_to_name(result));
                continue;
            }
            auto *blob = (uint8_t *)malloc(blobSize);
            result = nvs_get_blob(nvsHandle, info.key, blob, &blobSize);
            if (result != ESP_OK) {
                log_e("NVS error: %s", esp_err_to_name(result));
                free(blob);
                continue;
            }
            log_i("%s:%s (%s) (%zu bytes)", info.namespace_name, info.key, typeToStr(info.type),
                  blobSize);
            free(blob);
            break;
        }
        default:
            log_i("%s:%s (%s)", info.namespace_name, info.key, typeToStr(info.type));
            break;
        }
    } while (result == ESP_OK);

    if (result != ESP_ERR_NVS_NOT_FOUND) { // the last iteration ran into an internal error
        log_e("NVS error %s at current iteration, stopping.", esp_err_to_name(result));
        return;
    }

    nvs_stats_t nvsStats;
    nvs_get_stats(nullptr, &nvsStats);
    log_i("NVS stats (entries): used=%zu free=%zu total=%zu", nvsStats.used_entries,
          nvsStats.free_entries, nvsStats.total_entries);

    // Iterate through the handleMap and call nvs_get_used_entry_count
    // for each namespace
    for (const auto &[name, handle] : handleMap) {
        size_t usedEntries = 0;
        result = nvs_get_used_entry_count(handle, &usedEntries);
        if (result != ESP_OK) {
            log_e("NVS error: %s", esp_err_to_name(result));
            continue;
        }
        log_i("-> \"%s\" (%zu entries)", name.c_str(), usedEntries);
    }
#endif
}
