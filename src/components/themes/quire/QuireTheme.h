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
  // Clock and battery on the same 22px line as the titles and the home cover.
  v.headerStatusExtraInset = 0;
  v.tabSpacing = 20;
  // The status digits' ink spans y 12..26 (SMALL font centered on the 5..33
  // strip); the cover starts the same 12px below it.
  v.homeTopPadding = 38;
  v.homeCoverHeight = 448;
  // The whole home body; only the cover inside it is snapshotted.
  v.homeCoverTileHeight = 680;
  v.homeRecentBooksCount = 1;
  v.homeContinueReadingInMenu = false;
  v.buttonHintsHeight = 64;
  // Status popups as in Lyra: white text on a black card, tight margins.
  v.popupTopOffsetRatio = 0.36f;
  v.popupMarginX = 16;
  v.popupMarginY = 12;
  v.popupCornerRadius = 6;
  v.popupFrameThickness = 2;
  v.popupTextBold = false;
  v.popupTextInverted = false;
  v.popupProgressBarHeight = 4;
  v.popupProgressDrawOutline = false;
  v.popupProgressClampPercent = true;
  v.popupProgressFillInverted = false;
  v.popupProgressOutlineInverted = false;
  v.optionPopupInnerPadding = 24;
  v.optionPopupItemSpacing = 12;
  v.optionPopupSelectionVPadding = 12;
  v.controlRadius = 6;
  v.sheetRadius = 6;
  v.capsuleRadius = 6;
  v.homeCompactMenu = true;
  v.tabsUnderline = true;
  v.tabsLeftAligned = true;
  v.libraryCoverGrid = true;
  v.libraryGridProgress = true;
  v.homeDirectButtons = true;
  v.settingsSectionHints = true;
  v.dialogFormal = true;
  v.optionPopupSmallLabels = true;
  v.settingsVersionLabel = false;
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
  void drawProgressBar(const GfxRenderer& renderer, Rect rect, size_t current, size_t total) const override;
  bool drawBookOpening(const GfxRenderer& renderer, const BookOpening& book) const override;
  bool usesReadingStatus() const override { return true; }

 private:
  // The home cover's slot inside the home tile (left column, 5:8).
  static Rect coverRect(const GfxRenderer& renderer, Rect tile);
  void drawHints(GfxRenderer& renderer, const char* const labels[4], int primary, bool always) const;
  void drawStats(const GfxRenderer& renderer, int x, int y, int width, int height) const;
};
