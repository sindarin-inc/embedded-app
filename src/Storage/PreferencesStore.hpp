#pragma once

#include <Preferences.h>
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

class PreferencesStore {
public:
    template <typename T>
    class PrefsItemBase {
    public:
        PrefsItemBase(Preferences &prefs) : prefs(prefs) {}

        void writeToStorage(const char *key, T value) {
            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, bool> ||
                          std::is_same_v<T, uint32_t>) {
                this->prefs.putInt(key, value);
            } else if constexpr (std::is_same_v<T, std::string>) {
                this->prefs.putString(key, value.c_str());
            } else {
                log_e("Unhandled prefs type for key %s", key);
                assert(0);
            }
        }

        void deleteFromStorage(const char *key) { this->prefs.remove(key); }

        Preferences &prefs;
    };

    template <typename T>
    class PrefsItem : public PrefsItemBase<T> {
    public:
        PrefsItem(Preferences &prefs, const char *key) : PrefsItemBase<T>(prefs) {
            strncpy(this->key_, key, sizeof(this->key_));
        }

        virtual void loadValue() {
            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, bool> ||
                          std::is_same_v<T, uint32_t>) {
                this->value_ = this->prefs.getInt(this->key_);
            } else if constexpr (std::is_same_v<T, std::string>) {
                // TODO: This behavior is a little inconsistent between types. For std::string
                // types, the std::optional will have `has_value()==false` if it's not stored in
                // NVS. For primitive types (e.g. int), `has_value()==true` and the value is just
                // the `Preferences` default value. It would be good to make this consistent but
                // unfortunately `Preferences.isKey` looks like it's relatively expensive.
                std::string val = this->prefs.getString(this->key_).c_str();
                if (!val.empty()) {
                    this->value_ = val;
                }
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

        void remove() { this->deleteFromStorage(this->key_); }

    protected:
        bool loaded_ = false;
        std::optional<T> value_;
        char key_[MAX_KEY_LENGTH + 1];
    };

#if USE_TURN_TOUCH_REMOTE
    class BLEAddressPrefsItem : public PrefsItem<BLEAddress> {
    public:
        BLEAddressPrefsItem(Preferences &prefs, const char *key) : PrefsItem(prefs, key){};

        void loadValue() override {
            uint64_t addressValue = this->prefs.getULong64(this->key_, -1);
            if (addressValue != -1) {
                this->value_ = BLEAddress(addressValue);
            }
        }

        void set(BLEAddress address) override {
            this->prefs.putULong64(this->key_, uint64_t(address));
            this->value_ = address;
        }
    };
#endif

    template <typename T>
    class KeyedPrefsItem : PrefsItemBase<T> {
    public:
        KeyedPrefsItem(Preferences &prefs, const char *suffix)
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
                return this->prefs.getInt(key);
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

    protected:
        const char *suffix_;
    };

    PreferencesStore() noexcept
        :
#if USE_TURN_TOUCH_REMOTE
          pairedTurnTouchAddress(this->prefs_, "paired-tt"),
#endif
          pocketApiToken(this->prefs_, "pocket-token"), cloudToken(this->prefs_, "cloud-token"),
          lastReadableId(this->prefs_, "rui-readable"),
          lastPageTurnerBuild(this->prefs_, "pt-build"),
          lastArticlesSince(this->prefs_, "articles-since"),
          lastReadablesSince(this->prefs_, "readables-since"), readablePlace(this->prefs_, "-p"),
          readablePlaceChanged(this->prefs_, "-pc"), articleArchivedChanged(this->prefs_, "-ar"),
          lastPageTurnerBattery(this->prefs_, "pt-battery"), invertSetting(this->prefs_, "invert"),
          vcom(this->prefs_, "vcom"), font(this->prefs_, "font"), rectX(this->prefs_, "rectX"),
          rectY(this->prefs_, "rectY"), rectW(this->prefs_, "rectW"), rectH(this->prefs_, "rectH"),
          lineSpacing(this->prefs_, "line-space"), demoMode(this->prefs_, "demo"),
          wordCount(this->prefs_, "word-count"),
          totalReadingTimeSeconds(this->prefs_, "tot-rd-time"),
          bootCount(this->prefs_, "boot-count"), firstRead(this->prefs_, "first-read"),
          autoPilotSpeed(this->prefs_, "auto-speed"),
          firstAutopilotRead(this->prefs_, "fst-auto-rd"){};

    auto begin() -> bool;
    void clear() {
        // Save and re-set the Turn Touch
#if USE_TURN_TOUCH_REMOTE
        auto turnTouchAddress = this->pairedTurnTouchAddress.get();
#endif
        prefs_.clear();
#if USE_TURN_TOUCH_REMOTE
        if (turnTouchAddress.has_value()) {
            this->pairedTurnTouchAddress.set(turnTouchAddress.value());
        }
#endif
    }

#if USE_TURN_TOUCH_REMOTE
    BLEAddressPrefsItem pairedTurnTouchAddress;
#endif
    PrefsItem<std::string> pocketApiToken;
    PrefsItem<std::string> cloudToken;
    PrefsItem<std::string> lastReadableId;
    PrefsItem<std::string> lastPageTurnerBuild;
    PrefsItem<std::string> lastArticlesSince;
    PrefsItem<std::string> lastReadablesSince;
    KeyedPrefsItem<uint32_t> readablePlace;
    KeyedPrefsItem<int> readablePlaceChanged;
    KeyedPrefsItem<int> articleArchivedChanged;
    PrefsItem<int> lastPageTurnerBattery;
    PrefsItem<int> invertSetting;
    PrefsItem<int> vcom;
    PrefsItem<int> font;
    PrefsItem<int> rectX;
    PrefsItem<int> rectY;
    PrefsItem<int> rectW;
    PrefsItem<int> rectH;
    PrefsItem<int> lineSpacing;
    PrefsItem<int> demoMode;
    PrefsItem<uint32_t> wordCount;
    PrefsItem<uint32_t> totalReadingTimeSeconds;
    PrefsItem<uint32_t> bootCount;
    PrefsItem<uint32_t> firstRead;
    PrefsItem<uint32_t> autoPilotSpeed;
    PrefsItem<uint32_t> firstAutopilotRead;

protected:
    Preferences prefs_;
};
