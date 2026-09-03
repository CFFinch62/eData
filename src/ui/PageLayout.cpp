#include "PageLayout.h"

std::vector<Page> buildPages(const DataItem slots[MAX_DISPLAY_SLOTS], int boxCount) {
  DataItem used[MAX_DISPLAY_SLOTS];
  int usedCount = 0;
  for (int i = 0; i < MAX_DISPLAY_SLOTS; ++i) {
    if (slots[i] != DataItem::None) used[usedCount++] = slots[i];
  }

  std::vector<Page> pages;
  if (usedCount == 0) {
    pages.emplace_back();  // one empty page - callers never see a zero-page list
    return pages;
  }

  for (int i = 0; i < usedCount; i += boxCount) {
    Page page;
    for (int j = 0; j < boxCount && (i + j) < usedCount; ++j) {
      page.items[j] = used[i + j];
      ++page.count;
    }
    pages.push_back(page);
  }
  return pages;
}
