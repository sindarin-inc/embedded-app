#pragma once

#include <cinttypes>
#include <cstring>
#include <nvs.h>
#include <optional>
#include <string>
#include <vector>

#include "config.h"
#include "sindarin-debug.h"

#if USE_TURN_TOUCH_REMOTE
#include <NimBLEDevice.h>
#elif SIMULATOR
typedef uint64_t BLEAddress;
#endif

#define MAX_KEY_LENGTH 15

enum FONT_CHOICES {
    PREFS_TAHOMA_TINY = 0,
    PREFS_LUCIDA_SKINNY = 1,
    PREFS_LUCIDA_FAT = 2,
    PREFS_LUCIDA_REGULAR = 3,
    PREFS_TAHOMA_LARGE = 4,
    PREFS_TAHOMA_REGULAR = 5,
    PREFS_TAHOMA_HUGE = 6,
};

enum class SyncReason {
    Manual = 0,
    OnInactivity = 1,
    OnClose = 2,
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
auto GetNVSType() -> nvs_type_t {
    static_assert(
        std::disjunction_v<std::is_same<T, bool>, std::is_same<T, int>, std::is_same<T, int32_t>,
                           std::is_same<T, uint32_t>, std::is_same<T, uint64_t>,
                           std::is_same<T, std::string>, std::is_same<T, BLEAddress>>,
        "Unsupported type for NVS storage");
    if constexpr (std::is_same_v<T, bool>) {
        return NVS_TYPE_U8;
    } else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int32_t>) {
        return NVS_TYPE_I32;
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        return NVS_TYPE_U32;
    } else if constexpr (std::is_same_v<T, uint64_t> || std::is_same_v<T, BLEAddress>) {
        return NVS_TYPE_U64;
    } else if constexpr (std::is_same_v<T, std::string>) {
        return NVS_TYPE_STR;
    } else {
        // Should never happen because of the static_assert
        log_e("Unhandled prefs type");
        assert(0);
    }
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

void DumpNVS();

class PreferencesStore {
public:
    template <typename T>
    class PrefsItemBase {
    public:
        PrefsItemBase(PreferencesStore &prefs) : prefs(prefs) {}

        void writeToStorage(const char *key, T value) {
            esp_err_t err = WriteToStorage(prefs.nvsHandle_, key, value);
            if (err != ESP_OK) {
                log_e("nvs_set_* fail: %s %s", key, esp_err_to_name(err));
                return;
            }
            prefs.commit();
        }

        void deleteFromStorage(const char *key) {
            esp_err_t err = nvs_erase_key(prefs.nvsHandle_, key);
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
            esp_err_t err = GetFromStorage(this->prefs.nvsHandle_, this->key_, value);
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
            esp_err_t err = GetFromStorage(this->prefs.nvsHandle_, key, value);
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

        auto getAllKeyPrefixes() -> std::vector<std::string> {
            std::vector<std::string> keys;
#if !SIMULATOR
            // Grab all keys
            nvs_iterator_t it = nullptr;
            esp_err_t result = nvs_entry_find("nvs", "sindarin-app", NVS_TYPE_ANY, &it);
            if (result == ESP_ERR_NVS_NOT_FOUND) {
                return keys;
            }

            if (result != ESP_OK) {
                log_e("nvs_entry_find error: %s", esp_err_to_name(result));
                return keys;
            }

            do {
                nvs_entry_info_t info;
                nvs_entry_info(it, &info);
                result = nvs_entry_next(&it);
                if (result != ESP_OK) {
                    break;
                }

                // Check if info.key has the same suffix as this item
                if (strncmp(info.key + strlen(info.key) - strlen(this->suffix_), this->suffix_,
                            strlen(this->suffix_)) == 0) {
                    // Key has the same suffix, so add it to the list
                    keys.push_back(std::string(info.key, strlen(info.key) - strlen(this->suffix_)));
                }
            } while (result == ESP_OK);

            if (result != ESP_ERR_NVS_NOT_FOUND) {
                log_e("NVS error %s at current iteration, stopping.", esp_err_to_name(result));
            }
#endif
            return keys;
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
          lastBooksSince(*this, "readables-since"), readablePlace(*this, "-p"),
          readablePlaceChanged(*this, "-pc"), readableArchivedChanged(*this, "-ar"),
          readableBookmarksChanged(*this, "-bm"), invertSetting(*this, "invert"),
          vcom(*this, "vcom"), font(*this, "font"), rectX(*this, "rectX"), rectY(*this, "rectY"),
          rectW(*this, "rectW"), rectH(*this, "rectH"), lineSpacing(*this, "line-space"),
          noSetupMode(*this, "no-setup"), wordCount(*this, "word-count"),
          totalReadingTimeSeconds(*this, "tot-rd-time"), bootCount(*this, "boot-count"),
          firstRead(*this, "first-read"), autoPilotSpeed(*this, "auto-speed"),
          firstAutopilotRead(*this, "fst-auto-rd"), cesMode(*this, "ces"), emcMode(*this, "emc"),
          wifiPower(*this, "wifi-power"), tzIana(*this, "tz-iana"), tzPosix(*this, "tz-posix"),
          firmwareLoaded(*this, "fw-loaded"), sleepReason(*this, "sleep-reason"),
          noBooks(*this, "no-books"), showNews(*this, "show-news"),
          noArticles(*this, "no-articles"), noBible(*this, "no-bible"),
          noDeliverOnClose(*this, "no-deliv-close"), noMorningDelivery(*this, "no-morn-sync"),
          eveningDelivery(*this, "eve-deliver"), darkMode(*this, "dark-mode"),
          autoPilot(*this, "auto-pilot"), fastTransitions(*this, "fast-tran"),
          noUploadCrashes(*this, "no-upload-crash"), showDebugMenu(*this, "show-debug"),
          fastBookmarkRight(*this, "fst-mark-right"), fastBookmarkCenter(*this, "fst-mark-cent"),
          pageTransitionArrows(*this, "page-arrows"),
          allowUsageWhileCharging(*this, "allow-use-chg"), wifiSettingsJson(*this, "wifi-settings"),
          localStoreMigrationVersion(*this, "ls-version"), fontsLibrary(*this, "fonts-lib"),
          fontType(*this, "font-type"), tutorialsDeleted(*this, "tutorials"),
          bleAdvertisingKey(*this, "ble-adv-key"), bleAuthKey(*this, "ble-auth-key"),
          preferencesStoreMigrationVersion_(*this, "ps-version"){};
    PreferencesStore(PreferencesStore &) = delete;

    void migrateIfNeeded() {
        bool ranMigration = false;
        auto migrationVersion = preferencesStoreMigrationVersion_.get().value_or(0);
        while (migrationVersion < latestMigrationVersion_) {
            log_i("Migrating PreferencesStore from version %" PRIu32 " to %" PRIu32,
                  migrationVersion, migrationVersion + 1);
            ranMigration = true;
            switch (migrationVersion) {
            case 0: {
                // This method exists to destroys keys we created before the migration system
                // was set up. I used this absolutely horrifying bash command to find these:
                // clang-format off
                /*
                    git log -S'PrefsItem<' --pickaxe-all -p --all -- src/Storage/PreferencesStore.hpp | perl -lne \
                    'print $1 if /PrefsItem<.*> (.*);/' | sort | uniq | xargs -I {} sh -c 'git log -S"{}(" \
                    --pickaxe-all -p --all -- src/Storage/PreferencesStore.hpp | perl -lne "print \$2 if \
                    /^-.*{}\((\*this|this->prefs_), ([a-zA-Z-\"]*)/"' | sort | uniq | xargs -I {} sh -c \
                    'grep -q "{}" src/Storage/PreferencesStore.hpp || echo \"{}\"'
                */
                // clang-format on
                std::vector<std::string> keys = {
                    "backlight",    "box-h-off-mm",   "box-w-off-mm",  "deliver-close",
                    "fw-size",      "gzip-readables", "last-batt",     "next-sync",
                    "pocket-token", "pt-battery",     "show-articles", "show-bible",
                    "show-books",   "upload-crashes", "x-offset",      "y-offset",
                };
                for (auto &key : keys) {
                    esp_err_t err = nvs_erase_key(nvsHandle_, key.c_str());
                    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
                        log_e("nvs_erase_key failed: %s", esp_err_to_name(err));
                    }
                }
                break;
            }
            case 1: {
                // Destroying time-zone, replaced by tz-iana and tz-posix
                esp_err_t err = nvs_erase_key(nvsHandle_, "time-zone");
                if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
                    log_e("nvs_erase_key failed: %s", esp_err_to_name(err));
                }
                break;
            }
            case 2: {
                // Removing the morning and evening newspaper time for now in favor of fixing it
                // to 6am and 8pm. May revive at some later date.
                esp_err_t err = nvs_erase_key(nvsHandle_, "morning-news");
                if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
                    log_e("nvs_erase_key failed: %s", esp_err_to_name(err));
                }

                err = nvs_erase_key(nvsHandle_, "evening-news");
                if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
                    log_e("nvs_erase_key failed: %s", esp_err_to_name(err));
                }
                break;
            }
            default:
                log_e("Unknown migration version: %" PRIu32, migrationVersion);
                return;
            }
            migrationVersion++;
            preferencesStoreMigrationVersion_.set(migrationVersion);
        }
        if (ranMigration) {
            logStats();
        }
    }

    auto begin() -> bool {
        if (started_) {
            return true;
        }
        esp_err_t err = nvs_open("sindarin-app", NVS_READWRITE, &nvsHandle_);
        if (err) {
            log_e("nvs_open failed: %s", esp_err_to_name(err));
            return false;
        }
        logStats();
        migrateIfNeeded();
        started_ = true;
        return true;
    }

    auto end() -> esp_err_t {
        if (!started_) {
            return ESP_OK;
        }
        esp_err_t err = nvs_commit(nvsHandle_);
        if (err) {
            log_e("nvs_commit on close failed: %s", esp_err_to_name(err));
        }
        nvs_close(nvsHandle_);
        started_ = false;
        return err;
    }

    void clear() {
        // Save and re-set the Turn Touch
#if USE_TURN_TOUCH_REMOTE
        auto turnTouchAddress = this->pairedTurnTouchAddress.get();
#endif
        esp_err_t err = nvs_erase_all(nvsHandle_);
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

    void logStats() {
#if !SIMULATOR
        nvs_stats_t nvsStats;
        esp_err_t err = nvs_get_stats(nullptr, &nvsStats);
        if (err != ESP_OK) {
            log_e("nvs_get_stats error: %s", esp_err_to_name(err));
            return;
        }
        size_t sindarinEntries = 0;
        err = nvs_get_used_entry_count(nvsHandle_, &sindarinEntries);
        if (err != ESP_OK) {
            log_e("nvs_get_used_entry_count error: %s", esp_err_to_name(err));
            return;
        }
        log_d("NVS stats (entries): used=%zu free=%zu total=%zu sindarin-app=%zu",
              nvsStats.used_entries, nvsStats.free_entries, nvsStats.total_entries,
              sindarinEntries);
#endif
    }

#if USE_TURN_TOUCH_REMOTE
    PrefsItem<BLEAddress> pairedTurnTouchAddress;
#endif
    PrefsItem<std::string> cloudToken;
    PrefsItem<std::string> lastReadableId;
    PrefsItem<std::string> lastPageTurnerBuild;
    PrefsItem<std::string> lastArticlesSince;
    PrefsItem<std::string> lastBooksSince;
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
    PrefsItem<bool> noSetupMode;
    PrefsItem<uint32_t> wordCount;
    PrefsItem<uint32_t> totalReadingTimeSeconds;
    PrefsItem<uint32_t> bootCount;
    PrefsItem<bool> firstRead;
    PrefsItem<uint32_t> autoPilotSpeed;
    PrefsItem<bool> firstAutopilotRead;
    PrefsItem<bool> cesMode;
    PrefsItem<bool> emcMode;
    PrefsItem<int> wifiPower;
    PrefsItem<std::string> tzIana;
    PrefsItem<std::string> tzPosix;
    // Firmware that's been loaded to the other flash slot but not yet installed
    PrefsItem<std::string> firmwareLoaded;
    PrefsItem<int> sleepReason;
    PrefsItem<bool> noBooks;
    PrefsItem<bool> showNews;
    PrefsItem<bool> noArticles;
    PrefsItem<bool> noBible;
    PrefsItem<bool> noDeliverOnClose;
    PrefsItem<bool> noMorningDelivery;
    PrefsItem<bool> eveningDelivery;
    PrefsItem<bool> darkMode;
    PrefsItem<bool> autoPilot;
    PrefsItem<bool> fastTransitions;
    PrefsItem<bool> noUploadCrashes;
    PrefsItem<bool> showDebugMenu;
    PrefsItem<bool> fastBookmarkRight;
    PrefsItem<bool> fastBookmarkCenter;
    PrefsItem<bool> pageTransitionArrows;
    PrefsItem<bool> allowUsageWhileCharging;
    PrefsItem<std::string> wifiSettingsJson;
    PrefsItem<uint32_t> localStoreMigrationVersion;
    PrefsItem<bool> fontsLibrary;
    PrefsItem<int> fontType;
    PrefsItem<uint32_t> tutorialsDeleted;
    // Value that's advertised in manufacturer data as a hint to mobile apps so that they can
    // connect to a device paired with the correct account.
    PrefsItem<std::string> bleAdvertisingKey;
    // Value that's used to authenticate the mobile app to the glasses
    PrefsItem<std::string> bleAuthKey;

    // These are RTC_SLOW_MEM variables that persist across deep sleep / wake cycles but will be
    // reset on power cycles
    static SyncReason syncReason;
    static uint64_t nextSyncTime;
    static uint8_t lastBatteryReport;
    static uint8_t lastPageTurnerBattery;
    static bool settingsOnRestart;

protected:
    const uint32_t latestMigrationVersion_ = 3;
    uint32_t nvsHandle_;
    bool started_;

    PrefsItem<uint32_t> preferencesStoreMigrationVersion_;

    void commit() {
        esp_err_t err = nvs_commit(nvsHandle_);
        if (err) {
            log_e("nvs_commit fail: %s", esp_err_to_name(err));
        }
    }
};
