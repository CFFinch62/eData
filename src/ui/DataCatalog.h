#pragma once

#include <cstdint>

#include "nmea/NmeaTypes.h"

// The full set of nav instrument items eData can show in a box, plus `None`
// for an unused config slot. Deliberately nav-only - no engine/tank/AIS
// items (see eEngine and eAIS for those). Pure C++, no Arduino dependency,
// so DataCatalog.cpp's formatting/ETA-TTG logic is host-testable the same
// way src/nmea/*.cpp is.
enum class DataItem : uint8_t {
  None = 0,
  Position,
  Sog,
  Cog,
  Heading,
  Depth,
  WaterTemp,
  WindSpeed,
  WindDirection,
  DistanceToWaypoint,
  BearingToWaypoint,
  CrossTrackError,
  Eta,
  TimeToGo,
};

constexpr int DATA_ITEM_COUNT = static_cast<int>(DataItem::TimeToGo) + 1;

// Short label for a box header/web-form option. Never null, always <= 12
// chars (fits the panel's box-label strip at the smallest scale used).
const char* dataItemLabel(DataItem item);

// A page box's fully-formatted content for one instant in time.
struct FormattedValue {
  char line1[24] = {0};
  char line2[24] = {0};  // may be empty - not every item needs a second line
  bool available = false;  // false -> line1 is "NO DATA AVAILABLE", line2 empty
  bool stale = false;      // available, but source data is older than STALE_AFTER_MS
};

// Formats `item` from the latest `data`, as of `nowMs` (for staleness).
// Every branch is independent of the others - an unconfigured/never-seen
// item always reports `available = false`, never garbage.
FormattedValue formatDataItem(DataItem item, const NmeaData& data, unsigned long nowMs);
