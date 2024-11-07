#include <catch_amalgamated.hpp>

#define protected public
#define private public
#include "Storage/SolDrm.hpp"
#undef protected
#undef private

TEST_CASE("Test generateKeypair", "[SolDrm]") {
    PreferencesStore prefs;
    prefs.begin();
    prefs.clear();
    SolDrm solDrm(prefs);
    auto pubKey = solDrm.publicKeyPem();
    REQUIRE(pubKey.size() > 0);
    auto privateKeyOptional = prefs.privateKey.get();
    REQUIRE(privateKeyOptional.has_value());
    auto privateKey = privateKeyOptional.value();
    REQUIRE(privateKey.size() > 0);
    log_i("Private Key (%zu bytes):\n%s", privateKey.size(),
          BytesToHex(privateKey.data(), privateKey.size()).c_str());
    log_i("Public Key (%zu bytes):\n%s", pubKey.size(), pubKey.c_str());
}
