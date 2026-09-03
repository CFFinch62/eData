#include "test_support.h"
#include "ui/DataCatalog.h"

// DataCatalog::formatDataItem is pure C++ (no Arduino dependency - `nowMs`
// and every NmeaData field come from the caller), so it's host-testable the
// same way the parser is. This is the layer the project owner's requirement
// ("say NO DATA AVAILABLE") and the ETA/Time-To-Go design decision (computed
// on-device from RMB distance + RMC/VTG speed, since no standard NMEA 0183
// sentence carries either) both live in - both are worth pinning down with
// tests, not just eyeballing on hardware.

namespace {
constexpr unsigned long STALE_AFTER_MS = 10000;  // must track DataCatalog.cpp's own constant
}  // namespace

void runDataCatalogTests() {
  beginSection("DataCatalog - unconfigured/never-seen data says NO DATA AVAILABLE");
  {
    NmeaData data;  // every hasX flag defaults false
    const FormattedValue v = formatDataItem(DataItem::Sog, data, g_fakeMillis);
    CHECK(!v.available);
    CHECK_STR(v.line1, "NO DATA AVAILABLE");
    CHECK_STR(v.line2, "");
  }

  beginSection("DataCatalog - a normal reading formats with units and is not stale");
  {
    NmeaData data;
    data.hasSpeed = true;
    data.speedKnots = 6.2f;
    data.speedUpdateMs = g_fakeMillis;
    const FormattedValue v = formatDataItem(DataItem::Sog, data, g_fakeMillis);
    CHECK(v.available);
    CHECK(!v.stale);
    CHECK_STR(v.line1, "6.2 KT");
  }

  beginSection("DataCatalog - staleness is independent per item");
  {
    NmeaData data;
    data.hasDepth = true;
    data.depthMeters = 12.0f;
    data.depthUpdateMs = g_fakeMillis;
    const FormattedValue fresh = formatDataItem(DataItem::Depth, data, g_fakeMillis + STALE_AFTER_MS);
    CHECK(fresh.available);
    CHECK(!fresh.stale);  // exactly at the threshold, not past it
    const FormattedValue stale = formatDataItem(DataItem::Depth, data, g_fakeMillis + STALE_AFTER_MS + 1);
    CHECK(stale.available);
    CHECK(stale.stale);
  }

  beginSection("DataCatalog - ETA/TTG need waypoint + speed + time-of-day together");
  {
    NmeaData data;
    data.hasWaypoint = true;
    data.distanceToWaypointNm = 10.0f;
    data.waypointUpdateMs = g_fakeMillis;
    // Speed missing - TTG can't be computed even though distance is known.
    CHECK(!formatDataItem(DataItem::TimeToGo, data, g_fakeMillis).available);
    CHECK(!formatDataItem(DataItem::Eta, data, g_fakeMillis).available);
  }

  beginSection("DataCatalog - near-zero SOG is treated as no speed, not a 100-hour ETA");
  {
    NmeaData data;
    data.hasWaypoint = true;
    data.distanceToWaypointNm = 10.0f;
    data.waypointUpdateMs = g_fakeMillis;
    data.hasSpeed = true;
    data.speedKnots = 0.05f;  // below MIN_SOG_FOR_ETA_KNOTS
    data.speedUpdateMs = g_fakeMillis;
    data.hasTimeOfDay = true;
    data.timeOfDaySeconds = 0;
    data.timeOfDayUpdateMs = g_fakeMillis;
    CHECK(!formatDataItem(DataItem::TimeToGo, data, g_fakeMillis).available);
  }

  beginSection("DataCatalog - TTG and ETA arithmetic, including midnight wraparound");
  {
    NmeaData data;
    data.hasWaypoint = true;
    data.distanceToWaypointNm = 10.0f;  // at 5kn -> 2h exactly
    data.waypointUpdateMs = g_fakeMillis;
    data.hasSpeed = true;
    data.speedKnots = 5.0f;
    data.speedUpdateMs = g_fakeMillis;
    data.hasTimeOfDay = true;
    data.timeOfDaySeconds = 23 * 3600u;  // 23:00 UTC
    data.timeOfDayUpdateMs = g_fakeMillis;

    const FormattedValue ttg = formatDataItem(DataItem::TimeToGo, data, g_fakeMillis);
    CHECK(ttg.available);
    CHECK(!ttg.stale);
    CHECK_STR(ttg.line1, "02:00");

    // 23:00 + 2h wraps past midnight to 01:00, not 25:00.
    const FormattedValue eta = formatDataItem(DataItem::Eta, data, g_fakeMillis);
    CHECK(eta.available);
    CHECK_STR(eta.line1, "01:00");
  }

  beginSection("DataCatalog - Position formats both hemispheres correctly");
  {
    NmeaData data;
    data.hasPosition = true;
    data.latDeg = -33.5;  // South
    data.lonDeg = 151.25;  // East
    data.positionUpdateMs = g_fakeMillis;
    const FormattedValue v = formatDataItem(DataItem::Position, data, g_fakeMillis);
    CHECK(v.available);
    CHECK_STR(v.line1, "33.5000 S");
    CHECK_STR(v.line2, "151.2500 E");
  }
}
