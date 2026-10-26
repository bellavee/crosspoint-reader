#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class Bitmap;
class GfxRenderer;
struct RecentBook;
namespace freeink {
namespace ui {
struct HeaderProps;
struct Rect;
struct BitmapRef;
struct ListItem;
}  // namespace ui
}  // namespace freeink

struct Rect {
  int x;
  int y;
  int width;
  int height;

  explicit Rect(int x = 0, int y = 0, int width = 0, int height = 0) : x(x), y(y), width(width), height(height) {}
};

struct ThemeMetrics {
  int batteryWidth;
  int batteryHeight;

  int topPadding;
  int batteryBarHeight;
  int headerHeight;
  int verticalSpacing;

  int previewPadding;
  int previewHeightPercent;

  int contentSidePadding;
  int listRowHeight;
  int listWithSubtitleRowHeight;
  // FreeInkUI list shape, consumed by uiThemeTokens() for screens rendered
  // through FreeInkApp: the theme supplies geometry and selection style, the
  // uiScale fonts supply the sizes. Plain data by design — the eventual
  // SD-card theme files will provide exactly these values.
  int listRowGap;          // vertical gap between rows
  int listRowRadius;       // row corner radius (RoundedRaff cards, Lyra pill)
  int listInset;           // horizontal inset of the whole list band
  int listSidePadding;     // text inset within a row
  int listSelectionStyle;  // 0=invert fill, 1=light pill, 2=underline, 3=triangle (fui::SelectionStyle order)
  int listScrollWidth;     // scroll indicator thickness
  int listScrollSide;      // 0 = right edge, 1 = left edge
  bool listTitleBold;      // bold row titles (RoundedRaff)
  // FreeInkUI header shape, same contract as the list fields above.
  int headerSidePadding;    // title text inset
  int headerUnderlineSize;  // bottom rule thickness (Lyra), 0 = none
  int headerTitleAlign;     // 0 = left, 1 = center, 2 = right (fui::TextAlign order)
  int headerBatterySide;    // 0 = right edge, 1 = left edge
  // Header clock opt-out for themes whose title layout can't spare the left
  // reserve (RoundedRaff); the user setting still governs the themes that can.
  bool headerShowsClock = true;
  // Clock slot: centered on the band, or on the left after the back arrow.
  bool headerClockCentered = true;
  // Corner clock and battery sit this much further in than the header title,
  // an optical bias for the digits' open sides.
  int headerStatusExtraInset = 4;
  int menuRowHeight;
  int menuSpacing;

  int tabSpacing;
  int tabBarHeight;
  int coverGridTabBarHeight = 72;
  // Selected-tab pill fills its equal-width slot (legacy RoundedRaff tabs)
  // instead of shrinking to hug the label (legacy Lyra tabs).
  bool tabPillFullSlot = false;

  int scrollBarWidth;
  int scrollBarRightOffset;

  int homeTopPadding;
  int homeCoverHeight;
  int homeCoverTileHeight;
  int homeRecentBooksCount;
  bool homeContinueReadingInMenu;
  int homeMenuTopOffset;

  int buttonHintsHeight;
  int sideButtonHintsWidth;

  int progressBarHeight;
  int progressBarMarginTop;
  int statusBarHorizontalMargin;
  int statusBarVerticalMargin;
  int keyboardKeyHeight;
  int keyboardKeySpacing;
  bool keyboardCenteredText;
  int keyboardVerticalOffset;
  int keyboardTextFieldWidthPercent;
  int keyboardWidthPercent;

  float popupTopOffsetRatio;
  int popupMarginX;
  int popupMarginY;
  int popupFrameThickness;
  int popupCornerRadius;
  bool popupTextBold;
  bool popupTextInverted;
  int popupTextBaselineOffsetY;
  int popupProgressBarHeight;
  bool popupProgressDrawOutline;
  bool popupProgressClampPercent;
  bool popupProgressFillInverted;
  bool popupProgressOutlineInverted;

  int optionPopupItemSpacing;
  int optionPopupInnerPadding;
  int optionPopupSelectionVPadding;
  int optionPopupDialogSideMargin;

  int textFieldHorizontalPadding;
  int textFieldNormalThickness;
  int textFieldCursorThickness;
  int textFieldLineEndOffset;

  // FreeInkUI control shape (the control center panel), same contract as the
  // list fields above: quick-setting tiles and slider step buttons, the
  // sheet's free-edge corners, and the capsule slider's corners (255 = full
  // stadium, i.e. radius = half the control height).
  int controlRadius;
  int sheetRadius;
  int capsuleRadius;

  // Home menu without OPDS Browser and File Transfer (Bindery covers both).
  bool homeCompactMenu = false;
  // Tab bars mark the active tab with an underline and no band wash; with
  // focus on the bar the active tab takes the list's selection fill.
  bool tabsUnderline = false;
  // Tabs hug their labels and line up from the left edge instead of sharing
  // the bar in equal slots; the active tab is underlined, with no rule under
  // the bar.
  bool tabsLeftAligned = false;
  // Library shows books as a cover grid, one column under each tab, instead
  // of a list. Cover size: UITheme::libraryCoverHeight().
  bool libraryCoverGrid = false;
  // Grid cells show the title and a reading progress bar ("New" for unopened
  // books) instead of title and author.
  bool libraryGridProgress = false;
  // Home has no cursor: the four front buttons run Resume, Library, Bindery
  // and Settings directly, the side buttons open the remaining entries.
  bool homeDirectButtons = false;
  // Settings section rows carry a one-line summary under their title.
  bool settingsSectionHints = false;
  // Two-option dialogs read like a printed form: caption, serif headline,
  // left-aligned text and the two choices side by side over a dimmed screen.
  bool dialogFormal = false;
  // Option popups set their choices in the small UI font.
  bool optionPopupSmallLabels = false;
  // The Settings header carries the firmware version on its right.
  bool settingsVersionLabel = true;
};

// What a theme's book-opening screen shows while a book is being indexed.
struct BookOpening {
  const char* title = nullptr;
  const char* author = nullptr;
  std::string coverPath;  // thumb BMP, may not exist
  const char* status = nullptr;
};

enum UIIcon {
  None = 0,
  Folder,
  Text,
  Image,
  Book,
  File,
  Recent,
  Settings,
  Transfer,
  Library,
  Plugins,
  Wifi,
  Hotspot,
  Bookmark,
  Usb,
  Blocks
};

// Default theme implementation (Classic Theme)
// Additional themes can inherit from this and override methods as needed

namespace BaseMetrics {
constexpr ThemeMetrics values = {.batteryWidth = 15,
                                 .batteryHeight = 12,
                                 .topPadding = 5,
                                 .batteryBarHeight = 20,
                                 .headerHeight = 84,
                                 .verticalSpacing = 10,
                                 .previewPadding = 12,
                                 .previewHeightPercent = 30,
                                 .contentSidePadding = 20,
                                 .listRowHeight = 30,
                                 .listWithSubtitleRowHeight = 50,
                                 .listRowGap = 0,
                                 .listRowRadius = 0,
                                 .listInset = 0,
                                 .listSidePadding = 20,
                                 .listSelectionStyle = 0,  // invert fill
                                 .listScrollWidth = 4,
                                 .listScrollSide = 0,
                                 .listTitleBold = false,
                                 .headerSidePadding = 18,
                                 .headerUnderlineSize = 0,
                                 .headerTitleAlign = 1,  // centered
                                 .headerBatterySide = 0,
                                 // Corner clock: a centered clock would collide with the centered title.
                                 .headerClockCentered = false,
                                 .menuRowHeight = 45,
                                 .menuSpacing = 8,
                                 .tabSpacing = 10,
                                 .tabBarHeight = 50,
                                 .scrollBarWidth = 4,
                                 .scrollBarRightOffset = 5,
                                 .homeTopPadding = 40,
                                 .homeCoverHeight = 400,
                                 .homeCoverTileHeight = 400,
                                 .homeRecentBooksCount = 1,
                                 .homeContinueReadingInMenu = false,
                                 .homeMenuTopOffset = 10,
                                 .buttonHintsHeight = 40,
                                 .sideButtonHintsWidth = 30,
                                 .progressBarHeight = 16,
                                 .progressBarMarginTop = 1,
                                 .statusBarHorizontalMargin = 5,
                                 .statusBarVerticalMargin = 19,
                                 .keyboardKeyHeight = 56,
                                 .keyboardKeySpacing = 0,
                                 .keyboardCenteredText = false,
                                 .keyboardVerticalOffset = -13,
                                 .keyboardTextFieldWidthPercent = 85,
                                 .keyboardWidthPercent = 94,
                                 .popupTopOffsetRatio = 0.075f,
                                 .popupMarginX = 15,
                                 .popupMarginY = 15,
                                 .popupFrameThickness = 2,
                                 .popupCornerRadius = 0,
                                 .popupTextBold = true,
                                 .popupTextInverted = true,
                                 .popupTextBaselineOffsetY = -2,
                                 .popupProgressBarHeight = 4,
                                 .popupProgressDrawOutline = false,
                                 .popupProgressClampPercent = false,
                                 .popupProgressFillInverted = true,
                                 .popupProgressOutlineInverted = true,
                                 .optionPopupItemSpacing = 6,
                                 .optionPopupInnerPadding = 16,
                                 .optionPopupSelectionVPadding = 4,
                                 .optionPopupDialogSideMargin = 20,
                                 .textFieldHorizontalPadding = 6,
                                 .textFieldNormalThickness = 1,
                                 .textFieldCursorThickness = 3,
                                 .textFieldLineEndOffset = 0,
                                 .controlRadius = 0,
                                 .sheetRadius = 0,
                                 .capsuleRadius = 0};
}

class BaseTheme {
 public:
  virtual ~BaseTheme() = default;

  // Component drawing methods
  static void setCheckboxRow(freeink::ui::ListItem& item, bool checked);
  static void drawCoverPlaceholder(const GfxRenderer& renderer, Rect rect);
  // Draws a pre-dithered cover thumb 1:1, centered and clipped to fill the
  // slot. Rescaling a dithered bitmap aliases badly, so overflow is cropped.
  static bool drawCoverThumbFill(const GfxRenderer& renderer, const Bitmap& bitmap, Rect slot, int xOffset = 0);
  // Whites out the pixels outside a radius-r arc at each corner, so a cover
  // drawn square reads as a rounded card. r = 0 leaves the corners square.
  static void roundCoverCorners(const GfxRenderer& renderer, Rect rect, int r);
  virtual void drawProgressBar(const GfxRenderer& renderer, Rect rect, size_t current, size_t total) const;
  void drawBatteryLeft(const GfxRenderer& renderer, Rect rect,
                       bool showPercentage = true) const;  // Left aligned (reader mode)
  virtual void fillBatteryIcon(const GfxRenderer& renderer, Rect rect, uint16_t percentage) const;
  virtual void drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                               const char* btn4) const;
  // As drawButtonHints, with `primary` (0-3, the slot in btn order) marked as
  // the main action. Themes without a primary style ignore it.
  virtual void drawButtonHintsPrimary(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                                      const char* btn4, int /*primary*/) const {
    drawButtonHints(renderer, btn1, btn2, btn3, btn4);
  }
  // Shared by every theme's drawButtonHints(): centres a hint label in its box,
  // wrapping to two lines rather than overflowing when it's too wide to fit.
  static void drawHintLabel(const GfxRenderer& renderer, int fontId, const char* label, int x, int boxWidth, int boxTop,
                            int boxHeight);
  virtual void drawSideButtonHints(const GfxRenderer& renderer, const char* topBtn, const char* bottomBtn) const;
  // Menu row height as DRAWN by drawButtonMenu. HomeActivity builds its touch
  // grid from this, so hit bands always match the visuals (RoundedRaff derives
  // its row height from the font, not the metrics table).
  virtual int getMenuRowHeight(const GfxRenderer& renderer) const;
  // Also draws the wall clock opposite the battery when the user enabled
  // SETTINGS.clockShowInHeader and an RTC is present. On touch boards a
  // tappable back button leads the band (see HeaderBackTapTarget); root
  // screens that own their stack bottom pass backButton = false.
  virtual void drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle = nullptr,
                          bool backButton = true) const;
  // Fill the battery/clock status chrome (settings + theme metrics) into
  // header props, so FUI-native screens drawing their own interactive header
  // carry the same band as drawHeader. Status text is styled with the
  // FONT_LABEL slot (bound to the fixed small font by makeUiTarget and
  // drawHeader). The label strings point at internal static buffers refreshed
  // per call (headers draw on the single render task).
  static void applyHeaderStatus(const GfxRenderer& renderer, freeink::ui::HeaderProps& props);
  // The header band every screen draws in: the caller's full-width rect inset
  // by the board's side insets, its top edge lowered by the board's
  // headerOffsetY (bottom edge fixed, so content below keeps its boundary).
  // Sets props.bleed so the band's fill and rule still span `rect`, and widens
  // the status line's edge inset to clear rounded corners. Call after
  // applyHeaderStatus.
  static freeink::ui::Rect headerBand(const GfxRenderer& renderer, freeink::ui::Rect rect,
                                      freeink::ui::HeaderProps& props);
  // Edge inset drawHeader uses for the clock/battery status line (detached
  // layouts hug the corner with a legacy 12px inset instead of the padding).
  static int headerStatusInset();
  virtual void drawSubHeader(const GfxRenderer& renderer, Rect rect, const char* label,
                             const char* rightLabel = nullptr) const;
  virtual void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                                   const int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                                   bool& bufferRestored, std::function<bool()> storeCoverBuffer) const;
  // rowValue (optional) is a short trailing label per row, e.g. a book count;
  // themes without a value slot ignore it.
  virtual void drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                              const std::function<std::string(int index)>& buttonLabel,
                              const std::function<UIIcon(int index)>& rowIcon,
                              const std::function<std::string(int index)>& rowValue = nullptr) const;
  static void drawSelectionHandle(const GfxRenderer& renderer, Rect rect, bool left);
  static void drawSelectionActions(const GfxRenderer& renderer, Rect rect, int count = 3);
  virtual Rect drawPopup(const GfxRenderer& renderer, const char* message) const;
  virtual void fillPopupProgress(const GfxRenderer& renderer, const Rect& layout, const int progress) const;
  static void drawStatusBar(GfxRenderer& renderer, const float bookProgress, const int currentPage, const int pageCount,
                            std::string title, const int paddingBottom = 0, const int textYOffset = 0,
                            const bool fillMargin = true, const bool isPageBookmarked = false,
                            const bool pageCountEstimated = false);
  static void drawHelpText(const GfxRenderer& renderer, Rect rect, const char* label);
  virtual void drawTextField(const GfxRenderer& renderer, Rect rect, const int textWidth, bool cursorMode = false,
                             int contentStartX = 0, int contentWidth = 0) const;
  virtual bool showsFileIcons() const { return false; }
  // The home card shows chapter, progress and sync status: HomeActivity loads
  // reading_status for the featured book on entry.
  virtual bool usesReadingStatus() const { return false; }
  // Draws a header's title row itself (below the status strip of `band`),
  // for screens that build their header through FreeInkUI with no title.
  // rightReserve keeps clear of trailing header buttons. False = the theme
  // has no custom title row; the caller keeps the FreeInkUI title.
  virtual bool drawHeaderTitleRow(const GfxRenderer&, Rect, const char*, const char*, int) const { return false; }
  virtual bool hasHeaderTitleRow() const { return false; }
  // Thumb generation height for home covers; 0 means use metrics.homeCoverHeight.
  // Themes with slots wider than 0.6 aspect override this so covers still fill.
  virtual int homeCoverThumbHeight(const GfxRenderer&) const { return 0; }
  // Part of the home tile kept as a snapshot between repaints (only the slow
  // cover read needs it). Default: the whole tile.
  virtual Rect homeCoverSnapshotRect(const GfxRenderer&, const Rect tile) const { return tile; }
  // Full-screen "opening this book" page in place of the indexing popup.
  // False = the theme has none; the caller shows drawPopup instead.
  virtual bool drawBookOpening(const GfxRenderer&, const BookOpening&) const { return false; }

  // Shared constants and helpers for battery drawing (used by all themes)
  static constexpr int batteryPercentSpacing = 4;
  static void drawBatteryOutline(const GfxRenderer& renderer, int x, int y, int battWidth, int rectHeight);
  static void drawBatteryLightningBolt(const GfxRenderer& renderer, int boltX, int boltY);
};
