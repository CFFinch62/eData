#include "test_support.h"

// RMB (bearing/distance/XTE to waypoint) and RMC's time-of-day field - both
// new for eData, no prior coverage to build on (see IMPLEMENTATION_PLAN.md).
//
// Checksums independently computed (XOR of bytes between '$' and '*'), not
// read back from this parser - the same discipline test_parser.cpp uses, for
// the same reason: a test that only confirms the code agrees with itself
// can't catch the parser being confidently wrong.

namespace {
// fields: 1=status(A/V) 2=xte 3=dirToSteer 4=originId 5=destId 6-9=destLatLon
//         10=range(nm) 11=bearingT 12=closingVelocity 13=arrivalStatus
constexpr const char* RMB_VALID = "$GPRMB,A,1.00,R,ORIG,DEST,4807.038,N,01131.000,E,5.6,047,10.0,V*11";
constexpr const char* RMB_STATUS_V = "$GPRMB,V,1.00,R,ORIG,DEST,4807.038,N,01131.000,E,5.6,047,10.0,V*06";
constexpr const char* RMB_NO_XTE = "$GPRMB,A,,,ORIG,DEST,4807.038,N,01131.000,E,12.3,270,,V*76";
constexpr const char* RMC_WITH_TIME = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A";
}  // namespace

void runRmbTests() {
  beginSection("RMB - distance/bearing/XTE to waypoint");
  {
    NmeaData data;
    const NmeaParser::Result r = parseSentence(RMB_VALID, data);
    CHECK(r.checksumValid);
    CHECK(data.hasWaypoint);
    CHECK_NEAR(data.distanceToWaypointNm, 5.6, 1e-6);
    CHECK_NEAR(data.bearingToWaypointDegTrue, 47.0, 1e-6);
    CHECK_NEAR(data.crossTrackErrorNm, 1.00, 1e-6);
    CHECK(data.crossTrackDirection == 'R');
    CHECK_STR(data.destWaypointId, "DEST");
    CHECK(data.waypointUpdateMs == g_fakeMillis);
  }
  beginSection("RMB - status V (warning) does not populate waypoint data");
  {
    NmeaData data;
    const NmeaParser::Result r = parseSentence(RMB_STATUS_V, data);
    CHECK(r.checksumValid);  // sentence itself is well-formed - only its content is "not ready"
    CHECK(!data.hasWaypoint);
  }
  beginSection("RMB - distance/bearing valid without XTE fields");
  {
    NmeaData data;
    const NmeaParser::Result r = parseSentence(RMB_NO_XTE, data);
    CHECK(r.checksumValid);
    CHECK(data.hasWaypoint);
    CHECK_NEAR(data.distanceToWaypointNm, 12.3, 1e-6);
    CHECK_NEAR(data.bearingToWaypointDegTrue, 270.0, 1e-6);
    // Empty XTE/direction fields must not be mistaken for a real "0.0 nm, no
    // direction" reading - crossTrackDirection stays unset ('\0').
    CHECK(data.crossTrackDirection == '\0');
  }
  beginSection("RMC - time-of-day (used only for ETA, not shown on its own)");
  {
    NmeaData data;
    const NmeaParser::Result r = parseSentence(RMC_WITH_TIME, data);
    CHECK(r.checksumValid);
    CHECK(data.hasTimeOfDay);
    CHECK(data.timeOfDaySeconds == 12u * 3600u + 35u * 60u + 19u);
    CHECK(data.timeOfDayUpdateMs == g_fakeMillis);
  }
}
