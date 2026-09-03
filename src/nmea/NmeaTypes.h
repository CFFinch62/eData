#pragma once

#include <cstddef>
#include <cstdint>

#include "NmeaLineReader.h"  // NMEA_MAX_SENTENCE_LEN

// Sentence types this project understands well enough to pull instrument
// fields out of. Add more here (and in NmeaParser.cpp) as needed - the
// checksum/framing layer (NmeaLineReader) already handles any sentence.
// eData is nav-instrument-only (no AIS, no engine/tank data - see eAIS and
// the planned eEngine for those).
enum class NmeaSentenceType : uint8_t {
  Unknown = 0,
  GGA,  // GPS fix: position, fix quality
  RMC,  // Recommended minimum: position, speed, course, UTC time
  VTG,  // Track made good and speed over ground
  HDT,  // Heading, true
  HDG,  // Heading, magnetic (+ deviation/variation)
  MTW,  // Water temperature
  DBT,  // Depth below transducer (feet/meters/fathoms)
  DPT,  // Depth (meters + transducer offset)
  MWV,  // Wind speed and angle (relative or true, per its reference field)
  MWD,  // Wind direction and speed, true
  RMB,  // Recommended minimum navigation info: bearing/distance/XTE to waypoint
};

// Latest parsed values. Each field's "has*" flag is set the first time a
// sentence supplies it and stays set (stale data is still shown, just
// visually marked stale by the formatter once *UpdateMs is too old).
struct NmeaData {
  bool hasPosition = false;
  double latDeg = 0.0;  // + = North
  double lonDeg = 0.0;  // + = East
  unsigned long positionUpdateMs = 0;

  bool hasSpeed = false;
  float speedKnots = 0.0f;
  unsigned long speedUpdateMs = 0;

  bool hasCourse = false;
  float courseDegTrue = 0.0f;
  unsigned long courseUpdateMs = 0;

  bool hasHeading = false;
  float headingDeg = 0.0f;
  bool headingIsTrue = false;  // false = magnetic (from HDG)
  unsigned long headingUpdateMs = 0;

  bool hasWaterTemp = false;
  float waterTempC = 0.0f;
  unsigned long waterTempUpdateMs = 0;

  bool hasDepth = false;
  float depthMeters = 0.0f;
  unsigned long depthUpdateMs = 0;

  bool hasWind = false;
  float windSpeedKnots = 0.0f;
  float windDirectionDeg = 0.0f;
  bool windDirectionIsTrue = false;  // false = relative to bow (MWV with reference 'R')
  unsigned long windUpdateMs = 0;

  // From RMB, only when its status field is 'A' (data valid). ETA and
  // Time-To-Go are deliberately not stored fields - no standard NMEA 0183
  // sentence reliably carries them, so DataCatalog computes both at display
  // time from distanceToWaypointNm, speedKnots and timeOfDaySeconds.
  bool hasWaypoint = false;
  float distanceToWaypointNm = 0.0f;
  float bearingToWaypointDegTrue = 0.0f;
  float crossTrackErrorNm = 0.0f;
  char crossTrackDirection = '\0';  // 'L' = steer left, 'R' = steer right
  char destWaypointId[9] = {0};
  unsigned long waypointUpdateMs = 0;

  // UTC time-of-day from RMC, used only to compute ETA (see hasWaypoint above).
  bool hasTimeOfDay = false;
  uint32_t timeOfDaySeconds = 0;  // seconds since UTC midnight, 0..86399
  unsigned long timeOfDayUpdateMs = 0;
};

// Per-sentence-ID tracking. Not surfaced in eData's UI (no sentence
// checklist - that's eNMEA's job), but NmeaSource still tallies into it, so
// a future diagnostics view is a display-only change, not a plumbing one.
struct SentenceStatus {
  char id[4] = {0};  // e.g. "GGA" (talker prefix stripped)
  uint32_t validCount = 0;
  uint32_t checksumFailCount = 0;
  unsigned long lastValidMs = 0;
};

// Not surfaced anywhere in eData's UI (no sentence checklist), but a NMEA
// 2000-to-0183 gateway on a healthy backbone can still emit dozens of
// distinct sentence IDs - keep the same headroom eNMEA's checklist needed,
// since NmeaSource tallies every ID regardless of whether anything displays
// the table.
constexpr int MAX_TRACKED_SENTENCE_IDS = 48;

struct SentenceTable {
  SentenceStatus entries[MAX_TRACKED_SENTENCE_IDS];
  int count = 0;
  // Set once a new sentence ID had to be turned away. The UI surfaces this, so
  // a full table is never mistaken for a quiet network.
  bool overflowed = false;

  // Finds or creates (if room) the entry for a 3-char sentence id.
  SentenceStatus* findOrAdd(const char* id3) {
    for (int i = 0; i < count; ++i) {
      if (entries[i].id[0] == id3[0] && entries[i].id[1] == id3[1] && entries[i].id[2] == id3[2]) {
        return &entries[i];
      }
    }
    if (count >= MAX_TRACKED_SENTENCE_IDS) {
      overflowed = true;
      return nullptr;
    }
    SentenceStatus* s = &entries[count++];
    s->id[0] = id3[0];
    s->id[1] = id3[1];
    s->id[2] = id3[2];
    s->id[3] = '\0';
    return s;
  }
};
