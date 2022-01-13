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
            strcpy(this->key, key);
        }

        virtual void loadValue() {
            if constexpr (std::is_same_v<T, int>) {
                this->value = this->prefs.getInt(this->key);
            } else if constexpr (std::is_same_v<T, String>) {
                // TODO: This behavior is a little inconsistent between types. For String types, the
                // std::optional will have `has_value()==false` if it's not stored in NVS. For
                // primitive types (e.g. int), `has_value()==true` and the value is just the
                // `Preferences` default value. It would be good to make this consistent but
                // unfortunately `Preferences.isKey` looks like it's relatively expensive.
                String val = this->prefs.getString(this->key);
                if (!val.isEmpty()) {
                    this->value = val;
                }
            } else {
                log_e("Unhandled prefs type");
                assert(0);
            }
        }

        virtual std::optional<T> get() {
            if (!this->loaded) {
                this->loadValue();
                this->loaded = true;
            }
            return this->value;
        }

        virtual void set(T value) {
            this->value = value;
            this->writeToStorage(this->key, value);
        }

        void remove() { this->deleteFromStorage(this->key); }

    protected:
        bool loaded;
        std::optional<T> value;
        char key[MAX_KEY_LENGTH + 1];
    };

    class BLEAddressPrefsItem : public PrefsItem<BLEAddress> {
    public:
        BLEAddressPrefsItem(Preferences &prefs, const char *key) : PrefsItem(prefs, key){};

        void loadValue() override {
            uint64_t addressValue = this->prefs.getULong64(this->key, -1);
            if (addressValue != -1) {
                this->value = BLEAddress(addressValue);
            }
        }

        void set(BLEAddress address) override {
            this->prefs.putULong64(this->key, uint64_t(address));
            this->value = address;
        }
    };

    template <typename T> class KeyedPrefsItem : PrefsItemBase<T> {
    public:
        KeyedPrefsItem(Preferences &prefs, const char *suffix)
            : PrefsItemBase<T>(prefs), suffix(suffix) {}

        void makeKey(char *key, const char *prefix) { sprintf(key, "%s%s", prefix, this->suffix); }

        T get(const char *prefix) {
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
        const char *suffix;
    };

    PreferencesStore()
        : localXOffset(this->prefs, "x-offset"), localYOffset(this->prefs, "y-offset"),
          pairedTurnTouchAddress(this->prefs, "paired-tt"),
          pocketApiToken(this->prefs, "pocket-token"), lastReadableId(this->prefs, "rui-readable"),
          readablePlace(this->prefs, "-p"), readablePreviousPlace(this->prefs, "-pr-p"){};
    void begin();
    void clear() { prefs.clear(); }

    PrefsItem<int> localXOffset;
    PrefsItem<int> localYOffset;
    PrefsItem<String> pocketApiToken;
    BLEAddressPrefsItem pairedTurnTouchAddress;
    PrefsItem<String> lastReadableId;
    KeyedPrefsItem<int> readablePlace;
    KeyedPrefsItem<int> readablePreviousPlace;

protected:
    Preferences prefs;
};
