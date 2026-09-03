#include "InstrumentDisplay.h"

#include <algorithm>
#include <cstdio>

namespace {
constexpr int MARGIN = 4;
constexpr int GAP = 6;
constexpr int LABEL_H = 24;     // matches eNMEA Dashboard's label-strip height
constexpr int LABEL_SCALE = 2;  // 10x14px - readable at a glance, leaves room for the value below
constexpr int VALUE_SCALE_CAP = 16;  // upper bound for the "big as possible" search; no box is this large
}  // namespace

int InstrumentDisplay::footerReserve() const { return 22; }

InstrumentDisplay::BoxRect InstrumentDisplay::boxRect(int index, int boxCount) const {
  const int contentH = canvas_.height() - footerReserve();
  const int fullW = canvas_.width() - 2 * MARGIN;

  if (boxCount == 1) {
    return {MARGIN, MARGIN, fullW, contentH - 2 * MARGIN};
  }
  if (boxCount == 2) {
    const int rowH = (contentH - 2 * MARGIN - GAP) / 2;
    const int y = MARGIN + index * (rowH + GAP);
    return {MARGIN, y, fullW, rowH};
  }
  // boxCount == 4: 2x2 grid.
  const int colW = (fullW - GAP) / 2;
  const int rowH = (contentH - 2 * MARGIN - GAP) / 2;
  const int col = index % 2;
  const int row = index / 2;
  return {MARGIN + col * (colW + GAP), MARGIN + row * (rowH + GAP), colW, rowH};
}

void InstrumentDisplay::drawBox(const BoxRect& r, const char* label) {
  canvas_.drawRect(r.x, r.y, r.w, r.h, true);
  canvas_.drawText(r.x + 6, r.y + 5, label, LABEL_SCALE, true);
  canvas_.drawHLine(r.x + 1, r.y + LABEL_H, r.w - 2, true);
}

namespace {
// Largest integer scale at which `text` fits within maxW x maxH, searched
// downward from a generous cap - cheap (at most VALUE_SCALE_CAP iterations)
// and avoids the box math needing to know glyph metrics itself.
int fitScale(const EinkCanvas& canvas, const char* text, int maxW, int maxH) {
  for (int s = VALUE_SCALE_CAP; s >= 1; --s) {
    if (canvas.textWidth(text, s) <= maxW && canvas.textHeight(s) <= maxH) return s;
  }
  return 1;
}
}  // namespace

void InstrumentDisplay::drawBoxValue(const BoxRect& r, const FormattedValue& v) {
  const int valX = r.x + 2;
  const int valY = r.y + LABEL_H + 1;
  const int valW = r.w - 4;
  const int valH = r.h - LABEL_H - 3;
  if (valW <= 0 || valH <= 0) return;  // box too small at this panel size - nothing sane to draw

  canvas_.fillRect(valX, valY, valW, valH, false);  // erase the previous value - drawText only sets ink

  const bool twoLines = v.line2[0] != '\0';
  if (!twoLines) {
    const int scale = fitScale(canvas_, v.line1, valW, valH);
    const int tw = canvas_.textWidth(v.line1, scale);
    const int th = canvas_.textHeight(scale);
    canvas_.drawText(valX + (valW - tw) / 2, valY + (valH - th) / 2, v.line1, scale, true);
  } else {
    const int halfH = valH / 2;
    const int scale = std::min(fitScale(canvas_, v.line1, valW, halfH), fitScale(canvas_, v.line2, valW, halfH));
    const int th = canvas_.textHeight(scale);
    constexpr int lineGap = 4;
    const int startY = valY + (valH - (th * 2 + lineGap)) / 2;
    const int tw1 = canvas_.textWidth(v.line1, scale);
    canvas_.drawText(valX + (valW - tw1) / 2, startY, v.line1, scale, true);
    const int tw2 = canvas_.textWidth(v.line2, scale);
    canvas_.drawText(valX + (valW - tw2) / 2, startY + th + lineGap, v.line2, scale, true);
  }

  if (v.stale) {
    canvas_.drawText(r.x + r.w - canvas_.textWidth("STALE", 1) - 4, r.y + r.h - canvas_.textHeight(1) - 3, "STALE", 1,
                      true);
  }
}

void InstrumentDisplay::drawFooter(const char* sourceState, const char* netLine, const char* batteryText,
                                    int pageIndex, int pageCount) {
  const int y = canvas_.height() - footerReserve() + 4;
  canvas_.fillRect(0, y - 2, canvas_.width(), footerReserve() - 2, false);
  canvas_.drawHLine(0, y - 4, canvas_.width(), true);

  char left[40];
  std::snprintf(left, sizeof(left), "PAGE %d/%d  SOURCE: %s", pageIndex + 1, pageCount, sourceState);
  canvas_.drawText(4, y, left, 1, true);

  char right[64];
  std::snprintf(right, sizeof(right), "%s%s%s", netLine, batteryText[0] ? "  " : "", batteryText);
  const int rw = canvas_.textWidth(right, 1);
  canvas_.drawText(canvas_.width() - rw - 4, y, right, 1, true);
}

void InstrumentDisplay::drawChrome(const Page& page, int boxCount, int pageIndex, int pageCount) {
  canvas_.clear();
  for (int i = 0; i < MAX_BOXES_PER_PAGE && i < boxCount; ++i) {
    const BoxRect r = boxRect(i, boxCount);
    const char* label = (i < page.count) ? dataItemLabel(page.items[i]) : "UNUSED";
    drawBox(r, label);
  }
  drawFooter("", "", "", pageIndex, pageCount);
}

void InstrumentDisplay::drawValues(const Page& page, int boxCount, const NmeaData& data, const char* sourceState,
                                    const char* netLine, const char* batteryText, int pageIndex, int pageCount,
                                    unsigned long nowMs) {
  for (int i = 0; i < page.count; ++i) {
    const BoxRect r = boxRect(i, boxCount);
    const FormattedValue v = formatDataItem(page.items[i], data, nowMs);
    drawBoxValue(r, v);
  }
  drawFooter(sourceState, netLine, batteryText, pageIndex, pageCount);
}

void InstrumentDisplay::drawStatusMessage(const char* message) {
  const int y = MARGIN;
  const int h = canvas_.textHeight(2) + 8;
  canvas_.fillRect(MARGIN, y, canvas_.width() - 2 * MARGIN, h, false);
  canvas_.drawRect(MARGIN, y, canvas_.width() - 2 * MARGIN, h, true);
  canvas_.drawText(MARGIN + 8, y + 4, message, 2, true);
}
