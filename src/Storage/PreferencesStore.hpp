#pragma once

#include <nvs.h>
#include <optional>
#include <string>

#include "config.h"
#include "sindarin-debug.h"

#if USE_TURN_TOUCH_REMOTE
#include <NimBLEDevice.h>
#endif

#define MAX_KEY_LENGTH 15

enum FONT_CHOICES {
    PREFS_TAHOMA_REGULAR = 0,
    PREFS_LUCIDA_SKINNY = 1,
    PREFS_LUCIDA_FAT = 2,
    PREFS_LUCIDA_REGULAR = 3,
    PREFS_TAHOMA_LARGE = 4,
};

enum SLEEP_REASON {
    SLEEP_REASON_NONE = 0,
    SLEEP_REASON_INACTIVITY = 1,
    SLEEP_REASON_TEMPLES_CLOSED = 2,
};

class PreferencesStore {
public:
    template <typename T>
    class PrefsItemBase {
    public:
        PrefsItemBase(PreferencesStore &prefs) : prefs(prefs) {}

        void writeToStorage(const char *key, T value) {
            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, bool> ||
                          std::is_same_v<T, uint32_t>) {
                esp_err_t err = nvs_set_i32(prefs.nvsHandle, key, value);
                if (err != ESP_OK) {
                    log_e("nvs_set_i32 fail: %s %s", key, esp_err_to_name(err));
                }
            } else if constexpr (std::is_same_v<T, std::string>) {
                esp_err_t err = nvs_set_str(prefs.nvsHandle, key, value.c_str());
                if (err != ESP_OK) {
                    log_e("nvs_set_str fail: %s %s", key, esp_err_to_name(err));
                }
            } else {
                log_e("Unhandled prefs type for key %s", key);
                assert(0);
            }
            prefs.commit();
        }

        void deleteFromStorage(const char *key) {
            esp_err_t err = nvs_erase_key(prefs.nvsHandle, key);
            if (err) {
                log_e("nvs_erase_key fail: %s %s", key, esp_err_to_name(err));
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
            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, bool> ||
                          std::is_same_v<T, uint32_t>) {
                int32_t value;
                esp_err_t err = nvs_get_i32(this->prefs.nvsHandle, this->key_, &value);
                if (err == ESP_OK) {
                    this->value_ = value;
                }
            } else if constexpr (std::is_same_v<T, std::string>) {
                size_t len = 0;
                // NVS strings can be up to 4000 bytes long, so we need to get the length first
                esp_err_t err = nvs_get_str(this->prefs.nvsHandle, this->key_, nullptr, &len);
                if (err != ESP_OK) {
                    return;
                }

                // Create a blank string of the correct length
                std::string str;
                str.resize(len);

                // Now that we have the len write to a std::string
                err = nvs_get_str(this->prefs.nvsHandle, this->key_, str.data(), &len);
                if (err != ESP_OK) {
                    log_e("nvs_get_str fail: %s %s", this->key_, esp_err_to_name(err));
                    return;
                }

                this->value_ = str;
            } else {
                log_e("Unhandled prefs type");
                assert(0);
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

#if USE_TURN_TOUCH_REMOTE
    class BLEAddressPrefsItem : public PrefsItem<BLEAddress> {
    public:
        BLEAddressPrefsItem(PreferencesStore &prefs, const char *key) : PrefsItem(prefs, key){};

        void loadValue() override {

            uint64_t addressValue;
            esp_err_t err = nvs_get_u64(prefs.nvsHandle, this->key_, &addressValue);
            if (err != ESP_OK) {
                // Fail silently if the key doesn't exist
                return;
            }
            this->value_ = BLEAddress(addressValue);
        }

        void set(BLEAddress address) override {
            esp_err_t err = nvs_set_u64(prefs.nvsHandle, this->key_, uint64_t(address));
            if (err != ESP_OK) {
                log_e("nvs_set_i64 fail: %s %s", this->key_, esp_err_to_name(err));
            }
            prefs.commit();
            this->value_ = address;
        }
    };
#endif

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
            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, bool> ||
                          std::is_same_v<T, uint32_t>) {
                int32_t value = 0;
                nvs_get_i32(this->prefs.nvsHandle, key, &value);
                // Fail silently if the key doesn't exist
                return value;
            } else {
                log_e("Unhandled prefs type for key %s", key);
                assert(0);
            }
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
          readableBookmarksChanged(*this, "-bm"), lastPageTurnerBattery(*this, "pt-battery"),
          invertSetting(*this, "invert"), vcom(*this, "vcom"), font(*this, "font"),
          rectX(*this, "rectX"), rectY(*this, "rectY"), rectW(*this, "rectW"),
          rectH(*this, "rectH"), lineSpacing(*this, "line-space"), demoMode(*this, "demo"),
          wordCount(*this, "word-count"), totalReadingTimeSeconds(*this, "tot-rd-time"),
          bootCount(*this, "boot-count"), firstRead(*this, "first-read"),
          autoPilotSpeed(*this, "auto-speed"), firstAutopilotRead(*this, "fst-auto-rd"),
          cesMode(*this, "ces"), emcMode(*this, "emc"), lastBatteryReport(*this, "last-batt"),
          wifiPower(*this, "wifi-power"), gzipReadables(*this, "gzip-readables"),
          morningNewspaperTime(*this, "morning-news"), eveningNewspaperTime(*this, "evening-news"),
          solTimeZone(*this, "time-zone"), sleepReason(*this, "sleep-reason"){};

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
    BLEAddressPrefsItem pairedTurnTouchAddress;
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
    PrefsItem<int> lastPageTurnerBattery;
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
    PrefsItem<int> lastBatteryReport;
    PrefsItem<int> wifiPower;
    PrefsItem<bool> gzipReadables;
    PrefsItem<int> morningNewspaperTime;
    PrefsItem<int> eveningNewspaperTime;
    PrefsItem<int> solTimeZone;
    PrefsItem<int> sleepReason;

protected:
    bool started_;

    void commit() {
        esp_err_t err = nvs_commit(nvsHandle);
        if (err) {
            log_e("nvs_commit fail: %s", esp_err_to_name(err));
        }
    }
};
