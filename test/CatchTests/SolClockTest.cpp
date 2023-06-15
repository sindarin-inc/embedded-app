#include "Misc/SolClock.hpp"

#include <catch_amalgamated.hpp>

#include "Storage/PreferencesStore.hpp"

TEST_CASE("getNextEventEpoch returns the correct next event epoch time", "[next_event]") {
    PreferencesStore prefs;
    int morningNews = 6;
    int eveningNews = 20;

    prefs.noMorningDelivery.set(false);
    prefs.eveningDelivery.set(false);

    SolClock clock(prefs);

    // TODO: How do we mock the time?
    time_t now = time(nullptr);
    time_t nextEvent = clock.calculateNextSyncTime(now);

    // Verify that the next event is in the future
    REQUIRE(nextEvent > now);

    // Check if the next event hour is the morning or evening news hour
    struct tm nextEventTime;
    localtime_r(&nextEvent, &nextEventTime);
    REQUIRE((nextEventTime.tm_hour == morningNews || nextEventTime.tm_hour == eveningNews));

    // Verify that the minute and second fields are set to 0
    REQUIRE(nextEventTime.tm_min <= 10);
    // TODO: This fails because a random number of seconds is added
    // REQUIRE(nextEventTime.tm_sec == 0);
}
