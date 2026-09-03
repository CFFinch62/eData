#pragma once

#include <vector>

#include "DataCatalog.h"

constexpr int MAX_DISPLAY_SLOTS = 8;
constexpr int MAX_BOXES_PER_PAGE = 4;

// One screen's worth of boxes. `count` is normally `boxCount`, except on the
// last page when the configured slot count doesn't divide evenly - the
// remaining box positions are simply not drawn (rendered blank), which is
// different from a configured item having no live data ("NO DATA
// AVAILABLE" - see DataCatalog).
struct Page {
  DataItem items[MAX_BOXES_PER_PAGE] = {DataItem::None, DataItem::None, DataItem::None, DataItem::None};
  int count = 0;
};

// Builds the page list for the current box-count mode from the user's
// ordered slot configuration. `DataItem::None` slots are skipped entirely
// before grouping, so page count is ceil(configuredCount / boxCount), not a
// fixed 8/boxCount - a user who configures 3 items scrolls through 3 pages
// at boxCount=1, not 8. Always returns at least one page (possibly with
// count == 0) so callers never need a special case for "nothing configured".
std::vector<Page> buildPages(const DataItem slots[MAX_DISPLAY_SLOTS], int boxCount);
