#include "DataCatalog.h"

#include <cmath>
#include <cstdio>

namespace {

// Matches eNMEA's Dashboard threshold: long enough that a multiplexer
// sending one sentence every couple of seconds never flickers stale, short
// enough to notice a dead feed inside a few redraw ticks.
constexpr unsigned long STALE_AFTER_MS = 10000;

// Below this, "distance / speed" produces a technically-real but useless
// number (hours-long ETAs that swing wildly with GPS speed noise) - treat it
// the same as no speed at all rather than showing a misleading value.
constexpr float MIN_SOG_FOR_ETA_KNOTS = 0.2f;

bool isStale(unsigned long nowMs, unsigned long updateMs) { return (nowMs - updateMs) > STALE_AFTER_MS; }

void setUnavailable(FormattedValue& v) {
  v.available = false;
  v.stale = false;
  std::snprintf(v.line1, sizeof(v.line1), "NO DATA AVAILABLE");
  v.line2[0] = '\0';
}

// Seconds since UTC midnight -> "HH:MM", wrapping past 24h for a duration
// (TTG) or past the day boundary for a clock time (ETA).
void formatHhMm(uint32_t totalSeconds, char* out, size_t outLen) {
  const uint32_t hh = (totalSeconds / 3600) % 100;  // caps a runaway TTG at 99h rather than overflowing the field
  const uint32_t mm = (totalSeconds / 60) % 60;
  std::snprintf(out, outLen, "%02u:%02u", static_cast<unsigned>(hh), static_cast<unsigned>(mm));
}

// Shared precondition for both ETA and TTG: a valid route (RMB) and a
// meaningful speed, neither stale.
bool waypointEtaInputsReady(const NmeaData& data, unsigned long nowMs, bool& anyStale) {
  anyStale = false;
  if (!data.hasWaypoint || !data.hasSpeed || !data.hasTimeOfDay) return false;
  if (data.speedKnots < MIN_SOG_FOR_ETA_KNOTS) return false;
  anyStale = isStale(nowMs, data.waypointUpdateMs) || isStale(nowMs, data.speedUpdateMs) ||
             isStale(nowMs, data.timeOfDayUpdateMs);
  return true;
}

uint32_t computeTtgSeconds(const NmeaData& data) {
  const float hours = data.distanceToWaypointNm / data.speedKnots;
  return static_cast<uint32_t>(hours * 3600.0f);
}

}  // namespace

const char* dataItemLabel(DataItem item) {
  switch (item) {
    case DataItem::Position:
      return "POSITION";
    case DataItem::Sog:
      return "SPEED";
    case DataItem::Cog:
      return "COURSE";
    case DataItem::Heading:
      return "HEADING";
    case DataItem::Depth:
      return "DEPTH";
    case DataItem::WaterTemp:
      return "WATER TEMP";
    case DataItem::WindSpeed:
      return "WIND SPEED";
    case DataItem::WindDirection:
      return "WIND DIR";
    case DataItem::DistanceToWaypoint:
      return "DIST TO WPT";
    case DataItem::BearingToWaypoint:
      return "BRG TO WPT";
    case DataItem::CrossTrackError:
      return "XTE";
    case DataItem::Eta:
      return "ETA";
    case DataItem::TimeToGo:
      return "TIME TO GO";
    case DataItem::None:
    default:
      return "UNUSED";
  }
}

FormattedValue formatDataItem(DataItem item, const NmeaData& data, unsigned long nowMs) {
  FormattedValue v;

  switch (item) {
    case DataItem::Position: {
      if (!data.hasPosition) {
        setUnavailable(v);
        break;
      }
      v.available = true;
      v.stale = isStale(nowMs, data.positionUpdateMs);
      const double lat = std::fabs(data.latDeg);
      const double lon = std::fabs(data.lonDeg);
      std::snprintf(v.line1, sizeof(v.line1), "%.4f %c", lat, data.latDeg < 0 ? 'S' : 'N');
      std::snprintf(v.line2, sizeof(v.line2), "%.4f %c", lon, data.lonDeg < 0 ? 'W' : 'E');
      break;
    }
    case DataItem::Sog: {
      if (!data.hasSpeed) {
        setUnavailable(v);
        break;
      }
      v.available = true;
      v.stale = isStale(nowMs, data.speedUpdateMs);
      std::snprintf(v.line1, sizeof(v.line1), "%.1f KT", data.speedKnots);
      break;
    }
    case DataItem::Cog: {
      if (!data.hasCourse) {
        setUnavailable(v);
        break;
      }
      v.available = true;
      v.stale = isStale(nowMs, data.courseUpdateMs);
      std::snprintf(v.line1, sizeof(v.line1), "%.0f T", data.courseDegTrue);
      break;
    }
    case DataItem::Heading: {
      if (!data.hasHeading) {
        setUnavailable(v);
        break;
      }
      v.available = true;
      v.stale = isStale(nowMs, data.headingUpdateMs);
      std::snprintf(v.line1, sizeof(v.line1), "%.0f %c", data.headingDeg, data.headingIsTrue ? 'T' : 'M');
      break;
    }
    case DataItem::Depth: {
      if (!data.hasDepth) {
        setUnavailable(v);
        break;
      }
      v.available = true;
      v.stale = isStale(nowMs, data.depthUpdateMs);
      std::snprintf(v.line1, sizeof(v.line1), "%.1f M", data.depthMeters);
      break;
    }
    case DataItem::WaterTemp: {
      if (!data.hasWaterTemp) {
        setUnavailable(v);
        break;
      }
      v.available = true;
      v.stale = isStale(nowMs, data.waterTempUpdateMs);
      std::snprintf(v.line1, sizeof(v.line1), "%.1f C", data.waterTempC);
      break;
    }
    case DataItem::WindSpeed: {
      if (!data.hasWind) {
        setUnavailable(v);
        break;
      }
      v.available = true;
      v.stale = isStale(nowMs, data.windUpdateMs);
      std::snprintf(v.line1, sizeof(v.line1), "%.1f KT", data.windSpeedKnots);
      break;
    }
    case DataItem::WindDirection: {
      if (!data.hasWind) {
        setUnavailable(v);
        break;
      }
      v.available = true;
      v.stale = isStale(nowMs, data.windUpdateMs);
      std::snprintf(v.line1, sizeof(v.line1), "%.0f %c", data.windDirectionDeg, data.windDirectionIsTrue ? 'T' : 'R');
      break;
    }
    case DataItem::DistanceToWaypoint: {
      if (!data.hasWaypoint) {
        setUnavailable(v);
        break;
      }
      v.available = true;
      v.stale = isStale(nowMs, data.waypointUpdateMs);
      std::snprintf(v.line1, sizeof(v.line1), "%.1f NM", data.distanceToWaypointNm);
      if (data.destWaypointId[0] != '\0') {
        std::snprintf(v.line2, sizeof(v.line2), "TO %s", data.destWaypointId);
      }
      break;
    }
    case DataItem::BearingToWaypoint: {
      if (!data.hasWaypoint) {
        setUnavailable(v);
        break;
      }
      v.available = true;
      v.stale = isStale(nowMs, data.waypointUpdateMs);
      std::snprintf(v.line1, sizeof(v.line1), "%.0f T", data.bearingToWaypointDegTrue);
      break;
    }
    case DataItem::CrossTrackError: {
      if (!data.hasWaypoint || data.crossTrackDirection == '\0') {
        setUnavailable(v);
        break;
      }
      v.available = true;
      v.stale = isStale(nowMs, data.waypointUpdateMs);
      std::snprintf(v.line1, sizeof(v.line1), "%.2f NM", std::fabs(data.crossTrackErrorNm));
      std::snprintf(v.line2, sizeof(v.line2), "STEER %c", data.crossTrackDirection);
      break;
    }
    case DataItem::Eta: {
      bool anyStale = false;
      if (!waypointEtaInputsReady(data, nowMs, anyStale)) {
        setUnavailable(v);
        break;
      }
      v.available = true;
      v.stale = anyStale;
      const uint32_t ttg = computeTtgSeconds(data);
      const uint32_t eta = (data.timeOfDaySeconds + ttg) % 86400u;
      formatHhMm(eta, v.line1, sizeof(v.line1));
      std::snprintf(v.line2, sizeof(v.line2), "UTC");
      break;
    }
    case DataItem::TimeToGo: {
      bool anyStale = false;
      if (!waypointEtaInputsReady(data, nowMs, anyStale)) {
        setUnavailable(v);
        break;
      }
      v.available = true;
      v.stale = anyStale;
      formatHhMm(computeTtgSeconds(data), v.line1, sizeof(v.line1));
      break;
    }
    case DataItem::None:
    default:
      setUnavailable(v);
      break;
  }

  return v;
}
