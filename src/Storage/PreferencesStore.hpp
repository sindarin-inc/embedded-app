#pragma once

#include <cstring>
#include <nvs.h>
#include <optional>
#include <string>

#include "config.h"
#include "sindarin-debug.h"

#if USE_TURN_TOUCH_REMOTE
#include <NimBLEDevice.h>
#elif SIMULATOR
typedef uint64_t BLEAddress;
#endif

#define MAX_KEY_LENGTH 15

enum FONT_CHOICES {
    PREFS_TAHOMA_REGULAR = 0,
    PREFS_LUCIDA_SKINNY = 1,
    PREFS_LUCIDA_FAT = 2,
    PREFS_LUCIDA_REGULAR = 3,
    PREFS_TAHOMA_LARGE = 4,
};

enum class SyncReason {
    Manual = 0,
    Inactivity = 1,
    TemplesClosed = 2,
    Periodic = 3,
};

template <typename T>
auto GetFromStorage(uint32_t nvsHandle, const char *key, T &value) -> esp_err_t {
    static_assert(
        std::disjunction_v<std::is_same<T, bool>, std::is_same<T, int>, std::is_same<T, int32_t>,
                           std::is_same<T, uint32_t>, std::is_same<T, uint64_t>,
                           std::is_same<T, std::string>, std::is_same<T, BLEAddress>>,
        "Unsupported type for NVS storage");
    esp_err_t err;
    if constexpr (std::is_same_v<T, bool>) {
        err = nvs_get_u8(nvsHandle, key, (uint8_t *)&value);
    } else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int32_t>) {
        err = nvs_get_i32(nvsHandle, key, (int32_t *)&value);
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        err = nvs_get_u32(nvsHandle, key, &value);
    } else if constexpr (std::is_same_v<T, uint64_t> || std::is_same_v<T, BLEAddress>) {
        err = nvs_get_u64(nvsHandle, key, (uint64_t *)&value);
    } else if constexpr (std::is_same_v<T, std::string>) {
        size_t len = 0;
        // NVS strings can be up to 4000 bytes long, so we need to get the length first
        err = nvs_get_str(nvsHandle, key, nullptr, &len);
        if (err == ESP_OK) {
            // Create a blank string of the correct length
            value.resize(len);

            // Now that we have the len write to a std::string
            err = nvs_get_str(nvsHandle, key, value.data(), &len);
            if (err == ESP_OK) {
                // nvs_get_str's len includes the null terminator (it is a buffer length)
                // We need to strip back off the null terminator since it was copied into the
                // string.
                value.resize(len - 1);
            }
        }
    } else {
        // Should never happen because of the static_assert
        log_e("Unhandled prefs type");
        assert(0);
    }
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        log_e("nvs_get_* fail: %s %s", key, esp_err_to_name(err));
    }
    return err;
}

template <typename T>
auto WriteToStorage(uint32_t nvsHandle, const char *key, T &value) -> esp_err_t {
    static_assert(
        std::disjunction_v<std::is_same<T, bool>, std::is_same<T, int>, std::is_same<T, int32_t>,
                           std::is_same<T, uint32_t>, std::is_same<T, uint64_t>,
                           std::is_same<T, std::string>, std::is_same<T, BLEAddress>>,
        "Unsupported type for NVS storage");
    esp_err_t err;
    if constexpr (std::is_same_v<T, bool>) {
        err = nvs_set_u8(nvsHandle, key, value);
    } else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int32_t>) {
        err = nvs_set_i32(nvsHandle, key, value);
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        err = nvs_set_u32(nvsHandle, key, value);
    } else if constexpr (std::is_same_v<T, uint64_t> || std::is_same_v<T, BLEAddress>) {
        err = nvs_set_u64(nvsHandle, key, value);
    } else if constexpr (std::is_same_v<T, std::string>) {
        err = nvs_set_str(nvsHandle, key, value.c_str());
    } else {
        log_e("Unhandled prefs type for key %s", key);
        assert(0);
    }
    if (err != ESP_OK) {
        log_e("nvs_set_* fail: %s %s", key, esp_err_to_name(err));
    }
    return err;
}

class PreferencesStore {
public:
    template <typename T>
    class PrefsItemBase {
    public:
        PrefsItemBase(PreferencesStore &prefs) : prefs(prefs) {}

        void writeToStorage(const char *key, T value) {
            esp_err_t err = WriteToStorage(prefs.nvsHandle, key, value);
            if (err != ESP_OK) {
                log_e("nvs_set_* fail: %s %s", key, esp_err_to_name(err));
                return;
            }
            prefs.commit();
        }

        void deleteFromStorage(const char *key) {
            esp_err_t err = nvs_erase_key(prefs.nvsHandle, key);
            if (err) {
                // Fail silently if the key doesn't exist
                if (err != ESP_ERR_NVS_NOT_FOUND) {
                    log_e("nvs_erase_key fail: %s %s", key, esp_err_to_name(err));
                }
                return;
            }
            prefs.commit();
        }

        PreferencesStore &prefs;
    };

    template <typename T>
    class PrefsItem : public PrefsItemBase<T> {
    public:
        PrefsItem(PreferencesStore &prefs, const char *key) : PrefsItemBase<T>(prefs) {
            strncpy(this->key_, key, sizeof(this->key_));
        }

        virtual void loadValue() {
            T value;
            esp_err_t err = GetFromStorage(this->prefs.nvsHandle, this->key_, value);
            if (err == ESP_OK) {
                this->value_ = value;
            }
        }

        virtual auto get() -> std::optional<T> {
            if (!this->loaded_) {
                this->loadValue();
                this->loaded_ = true;
            }
            return this->value_;
        }

        virtual void set(T value) {
            this->value_ = value;
            this->writeToStorage(this->key_, value);
        }

        void remove() {
            this->deleteFromStorage(this->key_);
            this->value_.reset();
        }

    protected:
        bool loaded_ = false;
        std::optional<T> value_;
        char key_[MAX_KEY_LENGTH + 1];
    };

    template <typename T>
    class KeyedPrefsItem : PrefsItemBase<T> {
    public:
        KeyedPrefsItem(PreferencesStore &prefs, const char *suffix)
            : PrefsItemBase<T>(prefs), suffix_(suffix) {}

        void makeKey(char *key, const char *prefix) {
            auto size = snprintf(key, MAX_KEY_LENGTH + 1, "%s%s", prefix, this->suffix_);
            if (size > MAX_KEY_LENGTH + 1) {
                log_e("Key too long: %s%s", prefix, this->suffix_);
            }
        }

        auto get(const char *prefix) -> T {
            char key[MAX_KEY_LENGTH + 1];
            this->makeKey(key, prefix);
            T value;
            esp_err_t err = GetFromStorage(this->prefs.nvsHandle, key, value);
            if (err != ESP_OK) {
                // Ignore not found errors
                if (err != ESP_ERR_NVS_NOT_FOUND) {
                    log_e("nvs_get_* fail: %s %s", key, esp_err_to_name(err));
                }
                value = T();
            }
            return value;
        }

        void set(const char *prefix, T value) {
            char key[MAX_KEY_LENGTH + 1];
            this->makeKey(key, prefix);
            this->writeToStorage(key, value);
        }

        void remove(const char *prefix) {
            char key[MAX_KEY_LENGTH + 1];
            this->makeKey(key, prefix);
            this->deleteFromStorage(key);
        }

    protected:
        const char *suffix_;
    };

    PreferencesStore() noexcept
        :
#if USE_TURN_TOUCH_REMOTE
          pairedTurnTouchAddress(*this, "paired-tt"),
#endif
          cloudToken(*this, "cloud-token"), lastReadableId(*this, "rui-readable"),
          lastPageTurnerBuild(*this, "pt-build"), lastArticlesSince(*this, "articles-since"),
          lastReadablesSince(*this, "readables-since"), readablePlace(*this, "-p"),
          readablePlaceChanged(*this, "-pc"), readableArchivedChanged(*this, "-ar"),
          readableBookmarksChanged(*this, "-bm"), invertSetting(*this, "invert"),
          vcom(*this, "vcom"), font(*this, "font"), rectX(*this, "rectX"), rectY(*this, "rectY"),
          rectW(*this, "rectW"), rectH(*this, "rectH"), lineSpacing(*this, "line-space"),
          demoMode(*this, "demo"), wordCount(*this, "word-count"),
          totalReadingTimeSeconds(*this, "tot-rd-time"), bootCount(*this, "boot-count"),
          firstRead(*this, "first-read"), autoPilotSpeed(*this, "auto-speed"),
          firstAutopilotRead(*this, "fst-auto-rd"), cesMode(*this, "ces"), emcMode(*this, "emc"),
          wifiPower(*this, "wifi-power"), morningNewspaperTime(*this, "morning-news"),
          eveningNewspaperTime(*this, "evening-news"), solTimeZone(*this, "time-zone"){};
    PreferencesStore(PreferencesStore &) = delete;

    auto begin() -> bool {
        if (started_) {
            return true;
        }
        esp_err_t err = nvs_open("sindarin-app", NVS_READWRITE, &nvsHandle);
        if (err) {
            log_e("nvs_open failed: %s", esp_err_to_name(err));
            return false;
        }
        started_ = true;
        return true;
    }

    auto end() -> esp_err_t {
        if (!started_) {
            return ESP_OK;
        }
        esp_err_t err = nvs_commit(nvsHandle);
        if (err) {
            log_e("nvs_commit on close failed: %s", esp_err_to_name(err));
        }
        nvs_close(nvsHandle);
        started_ = false;
        return err;
    }

    void clear() {
        // Save and re-set the Turn Touch
#if USE_TURN_TOUCH_REMOTE
        auto turnTouchAddress = this->pairedTurnTouchAddress.get();
#endif
        esp_err_t err = nvs_erase_all(nvsHandle);
        if (err) {
            log_e("nvs_erase_all fail: %s", esp_err_to_name(err));
        }
#if USE_TURN_TOUCH_REMOTE
        if (turnTouchAddress.has_value()) {
            this->pairedTurnTouchAddress.set(turnTouchAddress.value());
        }
#endif
        commit();
    }

    uint32_t nvsHandle;

#if USE_TURN_TOUCH_REMOTE
    PrefsItem<BLEAddress> pairedTurnTouchAddress;
#endif
    PrefsItem<std::string> cloudToken;
    PrefsItem<std::string> lastReadableId;
    PrefsItem<std::string> lastPageTurnerBuild;
    PrefsItem<std::string> lastArticlesSince;
    PrefsItem<std::string> lastReadablesSince;
    KeyedPrefsItem<uint32_t> readablePlace;
    KeyedPrefsItem<bool> readablePlaceChanged;
    KeyedPrefsItem<bool> readableArchivedChanged;
    KeyedPrefsItem<bool> readableBookmarksChanged;
    PrefsItem<bool> invertSetting;
    PrefsItem<int> vcom;
    PrefsItem<int> font;
    PrefsItem<int> rectX;
    PrefsItem<int> rectY;
    PrefsItem<int> rectW;
    PrefsItem<int> rectH;
    PrefsItem<int> lineSpacing;
    PrefsItem<bool> demoMode;
    PrefsItem<uint32_t> wordCount;
    PrefsItem<uint32_t> totalReadingTimeSeconds;
    PrefsItem<uint32_t> bootCount;
    PrefsItem<bool> firstRead;
    PrefsItem<uint32_t> autoPilotSpeed;
    PrefsItem<bool> firstAutopilotRead;
    PrefsItem<bool> cesMode;
    PrefsItem<bool> emcMode;
    PrefsItem<int> wifiPower;
    PrefsItem<int> morningNewspaperTime;
    PrefsItem<int> eveningNewspaperTime;
    PrefsItem<int> solTimeZone;

    // These are RTC_SLOW_MEM variables that persist across deep sleep / wake cycles but will be
    // reset on power cycles
    static SyncReason syncReason;
    static uint64_t nextSyncTime;
    static uint8_t lastBatteryReport;
    static uint8_t lastPageTurnerBattery;

protected:
    bool started_;

    void commit() {
        esp_err_t err = nvs_commit(nvsHandle);
        if (err) {
            log_e("nvs_commit fail: %s", esp_err_to_name(err));
        }
    }
};
