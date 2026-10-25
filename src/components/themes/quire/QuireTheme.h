#pragma once

#include "components/themes/BaseTheme.h"

class GfxRenderer;

// Quire: a printed-page look built straight on BaseTheme. White ground, black
// ink, 6px radii, bold left-aligned titles over a thin status line, Noto Serif
// for book titles, and text-only button hints where the main action is a
// black pill. The home has no cursor: a big cover beside the reading stats,
// the book's place under it, and the four front buttons as the menu
// (metrics.homeDirectButtons). Sizes follow the X3's 528x792 panel and scale
// with the screen width elsewhere.
namespace QuireMetrics {
constexpr ThemeMetrics values = [] {
  ThemeMetrics v = BaseMetrics::values;
  v.topPadding = 5;
  v.batteryBarHeight = 28;
  v.headerHeight = 80;
  v.verticalSpacing = 5;
  v.contentSidePadding = 22;
  v.listRowHeight = 52;
  v.listWithSubtitleRowHeight = 76;
  v.listRowGap = 0;
  v.listRowRadius = 6;
  v.listInset = 12;
  v.listSidePadding = 12;
  v.listSelectionStyle = 0;  // invert fill
  v.headerSidePadding = 22;
  v.headerUnderlineSize = 0;
  v.headerTitleAlign = 0;  // left
  v.headerClockCentered = false;
  v.tabSpacing = 20;
  v.homeTopPadding = 44;
  v.homeCoverHeight = 420;
  // The whole home body; only the cover inside it is snapshotted.
  v.homeCoverTileHeight = 680;
  v.homeRecentBooksCount = 1;
  v.homeContinueReadingInMenu = false;
  v.buttonHintsHeight = 64;
  v.popupCornerRadius = 6;
  v.popupFrameThickness = 2;
  v.popupTextBold = false;
  v.popupTextInverted = false;
  v.popupProgressBarHeight = 8;
  v.popupProgressDrawOutline = true;
  v.popupProgressClampPercent = true;
  v.popupProgressFillInverted = false;
  v.popupProgressOutlineInverted = false;
  v.optionPopupInnerPadding = 24;
  v.optionPopupItemSpacing = 12;
  v.controlRadius = 6;
  v.sheetRadius = 6;
  v.capsuleRadius = 6;
  v.homeCompactMenu = true;
  v.tabsUnderline = true;
  v.libraryCoverGrid = true;
  v.libraryGridProgress = true;
  v.homeDirectButtons = true;
  v.settingsSectionHints = true;
  v.dialogFormal = true;
  return v;
}();
}  // namespace QuireMetrics

class QuireTheme : public BaseTheme {
 public:
  void drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle = nullptr,
                  bool backButton = true) const override;
  bool drawHeaderTitleRow(const GfxRenderer& renderer, Rect band, const char* title, const char* label,
                          int rightReserve) const override;
  bool hasHeaderTitleRow() const override { return true; }
  void drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                       const char* btn4) const override;
  void drawButtonHintsPrimary(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                              const char* btn4, int primary) const override;
  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           const int selectorIndex, bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                           std::function<bool()> storeCoverBuffer) const override;
  int homeCoverThumbHeight(const GfxRenderer& renderer) const override;
  Rect homeCoverSnapshotRect(const GfxRenderer& renderer, Rect tile) const override;
  Rect drawPopup(const GfxRenderer& renderer, const char* message) const override;
  void fillPopupProgress(const GfxRenderer& renderer, const Rect& layout, int progress) const override;
  bool drawBookOpening(const GfxRenderer& renderer, const BookOpening& book) const override;
  bool usesReadingStatus() const override { return true; }

 private:
  // The home cover's slot inside the home tile (left column, 2:3).
  static Rect coverRect(const GfxRenderer& renderer, Rect tile);
  void drawHints(GfxRenderer& renderer, const char* const labels[4], int primary, bool always) const;
  void drawStats(const GfxRenderer& renderer, int x, int y, int width, int height) const;
};
