#pragma once
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <optional>

#define MAX_KEY_LENGTH 15

class PreferencesStore {
public:
    template <typename T> class PrefsItem {
    public:
        PrefsItem(Preferences &prefs, const char *key) : prefs(prefs) { strcpy(this->key, key); }

        virtual void loadValue() {
            if constexpr (std::is_same_v<T, int>) {
                this->value = prefs.getInt(this->key);
            } else if constexpr (std::is_same_v<T, String>) {
                String val = prefs.getString(this->key);
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
            if constexpr (std::is_same_v<T, int>) {
                this->prefs.putInt(this->key, value);
            } else if constexpr (std::is_same_v<T, String>) {
                this->prefs.putString(this->key, value);
            } else {
                log_e("Unhandled prefs type");
                assert(0);
            }
        }

        void remove() { this->prefs.remove(key); }

    protected:
        Preferences &prefs;
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

    PreferencesStore()
        : localXOffset(this->prefs, "x-offset"), localYOffset(this->prefs, "y-offset"),
          pairedTurnTouchAddress(this->prefs, "paired-tt"),
          pocketApiToken(this->prefs, "pocket-token"),
          lastReadableId(this->prefs, "rui-readable"){};
    void begin();
    void clear() { prefs.clear(); }

    void setReadablePlace(const char *readableId, uint32_t place) {
        char key[16];
        sprintf(key, "%s%s", readableId, "-p");
        prefs.putULong(key, place);
    }

    void setReadablePreviousPlace(const char *readableId, uint32_t place) {
        char key[16];
        sprintf(key, "%s%s", readableId, "-pr-p");
        prefs.putULong(key, place);
    }

    uint32_t getReadablePlace(const char *readableId) {
        char key[16];
        sprintf(key, "%s%s", readableId, "-p");
        return prefs.getULong(key);
    }

    uint32_t getReadablePreviousPlace(const char *readableId) {
        char key[16];
        sprintf(key, "%s%s", readableId, "-pr-p");
        return prefs.getULong(key);
    }

    PrefsItem<int> localXOffset;
    PrefsItem<int> localYOffset;
    PrefsItem<String> pocketApiToken;
    BLEAddressPrefsItem pairedTurnTouchAddress;
    PrefsItem<String> lastReadableId;

protected:
    Preferences prefs;
};
