#include <initializer_list>

#include "test_support.h"
#include "ui/PageLayout.h"

// PageLayout::buildPages is pure data handling (no Arduino/EinkCanvas
// dependency), so it's tested here the same way profile selection is tested
// in test_profiles.cpp. This is the logic that answers the project owner's
// own question about the design: how box-count changes avoid forcing extra
// scrolling through unconfigured slots - by skipping DataItem::None entirely
// before grouping, so page count is ceil(configuredCount / boxCount), not a
// fixed 8/boxCount.

namespace {
void fillSlots(DataItem (&slots)[MAX_DISPLAY_SLOTS], std::initializer_list<DataItem> items) {
  int i = 0;
  for (DataItem item : items) slots[i++] = item;
  for (; i < MAX_DISPLAY_SLOTS; ++i) slots[i] = DataItem::None;
}
}  // namespace

void runPageLayoutTests() {
  beginSection("PageLayout - all 8 slots configured");
  {
    DataItem slots[MAX_DISPLAY_SLOTS];
    fillSlots(slots, {DataItem::Position, DataItem::Sog, DataItem::Cog, DataItem::Heading, DataItem::Depth,
                       DataItem::WaterTemp, DataItem::WindSpeed, DataItem::WindDirection});

    const auto pages1 = buildPages(slots, 1);
    CHECK(pages1.size() == 8);
    CHECK(pages1[0].count == 1);
    CHECK(pages1[0].items[0] == DataItem::Position);
    CHECK(pages1[7].items[0] == DataItem::WindDirection);

    const auto pages2 = buildPages(slots, 2);
    CHECK(pages2.size() == 4);
    CHECK(pages2[0].count == 2);
    CHECK(pages2[0].items[0] == DataItem::Position);
    CHECK(pages2[0].items[1] == DataItem::Sog);

    const auto pages4 = buildPages(slots, 4);
    CHECK(pages4.size() == 2);
    CHECK(pages4[0].count == 4);
    CHECK(pages4[1].count == 4);
    CHECK(pages4[1].items[3] == DataItem::WindDirection);
  }

  beginSection("PageLayout - unused slots are skipped, not padded");
  {
    // Only 3 items configured, scattered across the 8 slots with gaps -
    // exactly the "does not have to scroll so much" case: page count must
    // track the configured count, not a fixed 8/boxCount.
    DataItem slots[MAX_DISPLAY_SLOTS] = {DataItem::None,    DataItem::Depth, DataItem::None,
                                          DataItem::None,    DataItem::Sog,   DataItem::None,
                                          DataItem::Heading, DataItem::None};

    const auto pages1 = buildPages(slots, 1);
    CHECK(pages1.size() == 3);  // not 8

    const auto pages2 = buildPages(slots, 2);
    CHECK(pages2.size() == 2);  // ceil(3/2), not 4
    CHECK(pages2[0].count == 2);
    CHECK(pages2[0].items[0] == DataItem::Depth);
    CHECK(pages2[0].items[1] == DataItem::Sog);
    CHECK(pages2[1].count == 1);  // uneven last page
    CHECK(pages2[1].items[0] == DataItem::Heading);

    const auto pages4 = buildPages(slots, 4);
    CHECK(pages4.size() == 1);  // ceil(3/4), not 2
    CHECK(pages4[0].count == 3);
  }

  beginSection("PageLayout - nothing configured still returns one (empty) page");
  {
    DataItem slots[MAX_DISPLAY_SLOTS];
    fillSlots(slots, {});
    const auto pages = buildPages(slots, 1);
    CHECK(pages.size() == 1);
    CHECK(pages[0].count == 0);
  }
}
