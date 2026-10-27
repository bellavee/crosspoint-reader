#pragma once

#include <EpdFontFamily.h>

#include <functional>
#include <memory>

#include "CrossPointSettings.h"
#include "components/themes/BaseTheme.h"

class CoverGridHomeUi;

class UITheme {
  // Static instance
  static UITheme instance;

 public:
  enum class TextVerticalAlignment { TOP, CENTER, BOTTOM };

  UITheme();
  static UITheme& getInstance() { return instance; }

  const ThemeMetrics& getMetrics() const;
  const BaseTheme& getTheme() const { return currentTheme ? *currentTheme : fallbackTheme; }
  // The drawable content area: full screen minus the board's viewable insets
  // (bezel / rounded-corner clearance from BoardConfig::ACTIVE.viewableInsets,
  // oriented to the current rotation). THE single place activities should get
  // their content bounds from so every screen clears the bezel identically.
  // Zero-inset on rectangular panels, so no visible change there.
  static Rect getContentArea(const GfxRenderer& renderer);
  Rect getScreenSafeArea(const GfxRenderer& renderer, bool hasFrontButtonHints = false,
                         bool hasSideButtonHints = false);
  // Header rect for GUI.drawHeader() on a screen laid out in getScreenSafeArea():
  // clear of the button hints but NOT inset for the bezel, since the header band
  // applies the board's viewable insets itself.
  Rect getHeaderRect(const GfxRenderer& renderer, bool hasFrontButtonHints = false, bool hasSideButtonHints = false);
  static void drawCenteredText(const GfxRenderer& renderer, Rect screen, int fontId, int y, const char* text,
                               bool black = true, EpdFontFamily::Style style = EpdFontFamily::REGULAR);
  // Wraps only overflowing text, then aligns the complete line block within bounds.
  static void drawCenteredWrappedText(const GfxRenderer& renderer, Rect bounds, int fontId, const char* text,
                                      int maxLines, bool black = true,
                                      EpdFontFamily::Style style = EpdFontFamily::REGULAR,
                                      TextVerticalAlignment verticalAlignment = TextVerticalAlignment::CENTER);
  static bool supportsCoverGrid();
  static bool hasCoverGridHome();
  static void drawCoverGridHome(CoverGridHomeUi& home);
  void reload();
  void setTheme(CrossPointSettings::UI_THEME type);
  static std::string getCoverThumbPath(std::string coverBmpPath, int coverHeight);
  // Cover slots are 5:8 (1.6:1), the shape the Bindery covers are made in.
  static constexpr int COVER_ASPECT_WIDTH = 5;
  static constexpr int COVER_ASPECT_HEIGHT = 8;
  // Cover height of the library cover grid (5:8 covers, as wide as a tab slot
  // allows), or 0 when the theme shows a list. Thumbs are drawn 1:1, so this is
  // also the height library thumbs are generated at; it depends only on the
  // portrait screen width, so home and library agree on it.
  int libraryCoverHeight(const GfxRenderer& renderer) const;
  static UIIcon getFileIcon(const std::string& filename);
  static int getStatusBarHeight();
  static int getProgressBarHeight();

 private:
  BaseTheme fallbackTheme;
  const ThemeMetrics* currentMetrics = &BaseMetrics::values;
  std::unique_ptr<BaseTheme> currentTheme;
  mutable ThemeMetrics adjustedMetrics;
  mutable bool metricsValid = false;
  mutable bool metricsForTouch = false;
};

// Helper macro to access current theme
#define GUI UITheme::getInstance().getTheme()
