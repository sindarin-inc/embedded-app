#pragma once

#include <Preferences.h>
#include <optional>
#include <string>

#include "sindarin-debug.h"

#if USE_TURN_TOUCH_REMOTE
#include <NimBLEDevice.h>
#endif

#define MAX_KEY_LENGTH 15

enum FONT_CHOICES {
    PREFS_DEFAULT_FONT = 0,
    PREFS_LUCIDA_SKINNY = 1,
    PREFS_LUCIDA_FAT = 2,
    PREFS_TAHOMA_REGULAR = 3,
    PREFS_TAHOMA_LARGE = 4,
};

class PreferencesStore {
public:
    template <typename T>
    class PrefsItemBase {
    public:
        PrefsItemBase(Preferences &prefs) : prefs(prefs) {}

        void writeToStorage(const char *key, T value) {
            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, uint32_t>) {
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
            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, uint32_t>) {
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
        bool loaded_;
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
            snprintf(key, MAX_KEY_LENGTH + 1, "%s%s", prefix, this->suffix_);
        }

        auto get(const char *prefix) -> T {
            char key[MAX_KEY_LENGTH + 1];
            this->makeKey(key, prefix);
            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, uint32_t>) {
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
        : localXOffset(this->prefs_, "x-offset"), localYOffset(this->prefs_, "y-offset"),
          displayBoxWidthOffsetMm(this->prefs_, "box-w-off-mm"),
          displayBoxHeightOffsetMm(this->prefs_, "box-h-off-mm"),
#if USE_TURN_TOUCH_REMOTE
          pairedTurnTouchAddress(this->prefs_, "paired-tt"),
#endif
          pocketApiToken(this->prefs_, "pocket-token"),
          lastReadableId(this->prefs_, "rui-readable"), readablePlace(this->prefs_, "-p"),
          invertSetting(this->prefs_, "invert"), vcom(this->prefs_, "vcom"),
          backlight(this->prefs_, "backlight"), font(this->prefs_, "font"){};

    void begin();
    void clear() { prefs_.clear(); }

    PrefsItem<int> localXOffset;
    PrefsItem<int> localYOffset;
    PrefsItem<int> displayBoxWidthOffsetMm;
    PrefsItem<int> displayBoxHeightOffsetMm;
    PrefsItem<std::string> pocketApiToken;
#if USE_TURN_TOUCH_REMOTE
    BLEAddressPrefsItem pairedTurnTouchAddress;
#endif
    PrefsItem<std::string> lastReadableId;
    KeyedPrefsItem<uint32_t> readablePlace;
    PrefsItem<int> invertSetting;
    PrefsItem<int> vcom;
    PrefsItem<int> backlight;
    PrefsItem<int> font;

protected:
    Preferences prefs_;
};
