#pragma once

#include "components/themes/lyra/LyraTheme.h"

class GfxRenderer;

// CrossBind: the bellasekai / Bindery look. Hairlines and 3px radii, a home
// card with the current book's cover, title, chapter, progress and sync
// status, a flat four-row menu with no separators, italic serif titles, and a
// solid black selection fill.
namespace CrossBindMetrics {
constexpr ThemeMetrics values = [] {
  ThemeMetrics v = LyraMetrics::values;
  v.headerHeight = 80;
  v.contentSidePadding = 22;
  v.listRowHeight = 52;
  v.listRowGap = 4;
  v.listRowRadius = 3;
  v.listInset = 22;
  v.listSidePadding = 14;
  v.listSelectionStyle = 0;  // invert fill
  v.headerSidePadding = 22;
  v.headerUnderlineSize = 0;  // drawHeader draws its own hairline
  v.menuRowHeight = 68;
  v.menuSpacing = 6;
  v.homeTopPadding = 46;
  v.homeCoverHeight = 210;
  v.homeCoverTileHeight = 322;
  v.homeMenuTopOffset = 16;
  v.buttonHintsHeight = 42;
  v.popupCornerRadius = 3;
  v.controlRadius = 3;
  v.sheetRadius = 3;
  v.capsuleRadius = 3;
  v.homeCompactMenu = true;
  v.tabsUnderline = true;
  v.libraryCoverGrid = true;
  return v;
}();
}  // namespace CrossBindMetrics

class CrossBindTheme : public LyraTheme {
 public:
  void drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle = nullptr,
                  bool backButton = true) const override;
  void drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                       const char* btn4) const override;
  void drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                      const std::function<std::string(int index)>& buttonLabel,
                      const std::function<UIIcon(int index)>& rowIcon,
                      const std::function<std::string(int index)>& rowValue = nullptr) const override;
  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           const int selectorIndex, bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                           std::function<bool()> storeCoverBuffer) const override;
  bool usesReadingStatus() const override { return true; }
  bool drawHeaderTitleRow(const GfxRenderer& renderer, Rect band, const char* title, const char* label,
                          int rightReserve) const override;
  bool hasHeaderTitleRow() const override { return true; }

 private:
  void drawStatusRow(const GfxRenderer& renderer, int x, int y, int width, int rowHeight, bool filled,
                     const char* label, const std::string& text, const std::string& when) const;
};
