#pragma once

#include "DataCatalog.h"
#include "EinkCanvas.h"
#include "PageLayout.h"
#include "nmea/NmeaTypes.h"

// Draws one page of the configurable 1/2/4-box instrument grid. Replaces
// eNMEA's Dashboard entirely - eData has no sentence checklist, the whole
// screen is the instrument display.
//
// Box geometry is computed from canvas_.width()/height() at runtime (not
// hardcoded per-panel), so the same code fits the X3 (792x528) and X4
// (800x480) - see Dashboard.cpp's grid math in eNMEA for the precedent.
class InstrumentDisplay {
 public:
  explicit InstrumentDisplay(EinkCanvas& canvas) : canvas_(canvas) {}

  // Box borders + labels + footer chrome for `page`, once per page change
  // (a LEFT/RIGHT scroll, a display-layout save, or first boot). Must be
  // followed by a FULL_REFRESH present() - see EinkCanvas's double-buffering
  // note for why chrome has to be redrawn whenever what's under it changed.
  //
  // `boxCount` is the configured mode (1/2/4 - AppSettings::DisplayConfig),
  // not `page.count`: the grid shape must stay the mode's shape even on a
  // short last page where fewer than boxCount slots are filled.
  void drawChrome(const Page& page, int boxCount, int pageIndex, int pageCount);

  // Value text + footer status, called every redraw tick. Boxes beyond
  // `page.count` (the last page, when the configured slot count doesn't
  // divide evenly by box count) are left as drawChrome left them - blank
  // with an "UNUSED" label, not "NO DATA AVAILABLE" (that's reserved for a
  // configured item with no live data).
  void drawValues(const Page& page, int boxCount, const NmeaData& data, const char* sourceState, const char* netLine,
                   const char* batteryText, int pageIndex, int pageCount, unsigned long nowMs);

  // Transient overlay banner (hold-to-confirm gestures) - same pattern as
  // eNMEA's Dashboard::drawStatusMessage. Caller still owns present().
  void drawStatusMessage(const char* message);

 private:
  struct BoxRect {
    int x, y, w, h;
  };

  BoxRect boxRect(int index, int boxCount) const;
  void drawBox(const BoxRect& r, const char* label);
  void drawBoxValue(const BoxRect& r, const FormattedValue& v);
  void drawFooter(const char* sourceState, const char* netLine, const char* batteryText, int pageIndex,
                   int pageCount);
  int footerReserve() const;

  EinkCanvas& canvas_;
};
