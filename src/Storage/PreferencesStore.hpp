#pragma once
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <optional>

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
        char key[16];
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
          pocketApiToken(this->prefs, "pocket-token"){};

    Preferences prefs;

    PrefsItem<int> localXOffset;
    PrefsItem<int> localYOffset;
    PrefsItem<String> pocketApiToken;
    BLEAddressPrefsItem pairedTurnTouchAddress;

    void begin();
};
