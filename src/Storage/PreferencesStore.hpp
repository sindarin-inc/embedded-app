#pragma once
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <optional>

#define MAX_KEY_LENGTH 15

class PreferencesStore {
public:
    template <typename T> class PrefsItemBase {
    public:
        PrefsItemBase(Preferences &prefs) : prefs(prefs) {}

        void writeToStorage(const char *key, T value) {
            if constexpr (std::is_same_v<T, int>) {
                this->prefs.putInt(key, value);
            } else if constexpr (std::is_same_v<T, String>) {
                this->prefs.putString(key, value);
            } else {
                log_e("Unhandled prefs type");
                assert(0);
            }
        }

        void deleteFromStorage(const char *key) { this->prefs.remove(key); }

        Preferences &prefs;
    };

    template <typename T> class PrefsItem : public PrefsItemBase<T> {
    public:
        PrefsItem(Preferences &prefs, const char *key) : PrefsItemBase<T>(prefs) {
            strcpy(this->key_, key);
        }

        virtual void loadValue() {
            if constexpr (std::is_same_v<T, int>) {
                this->value_ = this->prefs.getInt(this->key_);
            } else if constexpr (std::is_same_v<T, String>) {
                // TODO: This behavior is a little inconsistent between types. For String types, the
                // std::optional will have `has_value()==false` if it's not stored in NVS. For
                // primitive types (e.g. int), `has_value()==true` and the value is just the
                // `Preferences` default value. It would be good to make this consistent but
                // unfortunately `Preferences.isKey` looks like it's relatively expensive.
                String val = this->prefs.getString(this->key_);
                if (!val.isEmpty()) {
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

    template <typename T> class KeyedPrefsItem : PrefsItemBase<T> {
    public:
        KeyedPrefsItem(Preferences &prefs, const char *suffix)
            : PrefsItemBase<T>(prefs), suffix_(suffix) {}

        void makeKey(char *key, const char *prefix) {
            snprintf(key, MAX_KEY_LENGTH + 1, "%s%s", prefix, this->suffix_);
        }

        auto get(const char *prefix) -> T {
            char key[MAX_KEY_LENGTH + 1];
            this->makeKey(key, prefix);
            if constexpr (std::is_same_v<T, int>) {
                return this->prefs.getInt(key);
            } else {
                log_e("Unhandled prefs type");
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

    PreferencesStore()
        : localXOffset(this->prefs_, "x-offset"), localYOffset(this->prefs_, "y-offset"),
          pairedTurnTouchAddress(this->prefs_, "paired-tt"),
          pocketApiToken(this->prefs_, "pocket-token"),
          lastReadableId(this->prefs_, "rui-readable"), readablePlace(this->prefs_, "-p"),
          readablePreviousPlace(this->prefs_, "-pr-p"), invertSetting(this->prefs_, "invert"){};
    void begin();
    void clear() { prefs_.clear(); }

    PrefsItem<int> localXOffset;
    PrefsItem<int> localYOffset;
    PrefsItem<String> pocketApiToken;
    BLEAddressPrefsItem pairedTurnTouchAddress;
    PrefsItem<String> lastReadableId;
    KeyedPrefsItem<int> readablePlace;
    KeyedPrefsItem<int> readablePreviousPlace;
    PrefsItem<int> invertSetting;

protected:
    Preferences prefs_;
};
