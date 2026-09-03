#pragma once

#include <cstdint>

#include "ui/DataCatalog.h"  // DataItem

// Up to eight complete source configurations, switchable from the device's own
// buttons.
//
// This exists because the tool is used on a bench, not installed once on a
// boat: every multiplexer and gateway ships with its own access point, IP
// address and port, and checking a shelf of them means changing all of that
// repeatedly. Retyping an SSID and host on a phone for each one is where the
// time and the mistakes go. Store them once, then step through with UP/DOWN
// and apply with CONFIRM - no phone involved.
struct NmeaProfile {
  enum class Protocol : uint8_t { UDP = 0, TCP = 1 };

  char name[17] = {0};      // short label, shown on the panel
  char ssid[33] = {0};      // 802.11 SSID max is 32 chars + NUL
  char password[65] = {0};  // WPA2 PSK max is 63 chars + NUL
  char host[64] = {0};      // TCP target only; ignored in UDP mode
  uint16_t port = 10110;    // 10110 is the common NMEA-over-IP port
  Protocol protocol = Protocol::UDP;
  bool used = false;        // false = empty slot

  bool hasWifiCredentials() const { return ssid[0] != '\0'; }
};

constexpr int MAX_PROFILES = 8;

// How many instrument boxes are shown per screen, and which nav item fills
// each of the 8 configurable slots. Box count decides how those 8 slots are
// grouped into pages (see PageLayout::buildPages) - it is not three separate
// saved layouts, so switching box count never discards a slot's assignment.
struct DisplayConfig {
  uint8_t boxCount = 1;  // 1, 2, or 4
  DataItem slots[8] = {DataItem::Position,  DataItem::Sog,        DataItem::Cog,       DataItem::Heading,
                        DataItem::Depth,    DataItem::WaterTemp,  DataItem::WindSpeed, DataItem::WindDirection};

  bool boxCountValid() const { return boxCount == 1 || boxCount == 2 || boxCount == 4; }
};

struct AppSettings {
  NmeaProfile profiles[MAX_PROFILES];
  // Index of the profile in use, or -1 for none. Treated as none if it points
  // at a slot that has since been emptied.
  int8_t activeIndex = -1;
  DisplayConfig display;

  bool indexValid(int i) const { return i >= 0 && i < MAX_PROFILES && profiles[i].used; }
  bool hasActive() const { return indexValid(activeIndex); }
  const NmeaProfile* active() const { return hasActive() ? &profiles[activeIndex] : nullptr; }

  int usedCount() const {
    int n = 0;
    for (const NmeaProfile& p : profiles) {
      if (p.used) ++n;
    }
    return n;
  }

  // Next occupied slot from `from` in direction `dir` (+1/-1), wrapping.
  // Returns `from` when it is the only one, or -1 when none are occupied.
  // Empty slots are skipped so button cycling never lands on a blank.
  int nextUsed(int from, int dir) const {
    if (usedCount() == 0) return -1;
    int i = (from < 0 || from >= MAX_PROFILES) ? 0 : from;
    for (int step = 0; step < MAX_PROFILES; ++step) {
      i += dir;
      if (i >= MAX_PROFILES) i = 0;
      if (i < 0) i = MAX_PROFILES - 1;
      if (profiles[i].used) return i;
    }
    return from;
  }

  // First occupied slot, or -1. Used after clearing the active one.
  int firstUsed() const {
    for (int i = 0; i < MAX_PROFILES; ++i) {
      if (profiles[i].used) return i;
    }
    return -1;
  }
};

// Returns false (with `out` left default) when nothing usable is stored - no
// saved blob, or one written by an incompatible build.
bool loadAppSettings(AppSettings& out);
bool saveAppSettings(const AppSettings& in);
void clearAppSettings();

// Set by ProvisioningPortal's display-layout save handler, consumed by
// main.cpp's loop() to rebuild the page list and reset the on-screen page
// index. Unlike Wi-Fi/profile changes, a layout change never reboots (it has
// no effect on the network connection), so main.cpp needs a way to notice a
// save that happened via a web request instead of its own code path.
void markDisplayConfigChanged();
bool takeDisplayConfigChanged();
