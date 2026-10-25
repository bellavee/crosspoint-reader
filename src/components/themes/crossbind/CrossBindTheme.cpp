#include "CrossBindTheme.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "components/icons/blocks.h"
#include "components/icons/book.h"
#include "components/icons/folder.h"
#include "components/icons/library.h"
#include "components/icons/settings2.h"
#include "components/icons/transfer.h"
#include "components/icons/wifi.h"
#include "fontIds.h"
#include "util/ReadingStatus.h"

namespace {
constexpr int radius = 3;
constexpr int menuIconSize = 32;
constexpr int labelTracking = 2;  // letter spacing of the small caps labels

// Home card geometry, inside the card's border.
constexpr int cardPadTop = 14;
constexpr int cardPadLeft = 20;
constexpr int cardPadRight = 16;
constexpr int coverWidth = 140;
constexpr int coverTextGap = 18;
constexpr int statusLabelWidth = 92;

const uint8_t* iconForName(UIIcon icon) {
  switch (icon) {
    case UIIcon::Folder:
      return FolderIcon;
    case UIIcon::Book:
      return BookIcon;
    case UIIcon::Settings:
      return Settings2Icon;
    case UIIcon::Transfer:
      return TransferIcon;
    case UIIcon::Library:
      return LibraryIcon;
    case UIIcon::Wifi:
      return WifiIcon;
    case UIIcon::Blocks:
    case UIIcon::Plugins:
      return BlocksIcon;
    default:
      return nullptr;
  }
}

// Top of a line box whose capitals sit centered on centerY: drawText's y is
// the line top and the baseline is y + ascender. Centering on the line box
// instead puts fonts with different leading at different heights, which is
// what made a small label float off a large title's midline.
int textTopForCenter(const GfxRenderer& renderer, const int fontId, const int centerY) {
  return centerY - renderer.getFontAscenderSize(fontId) * 3 / 5;
}

// UI frames are 1-bit, so a "lighter" line is made by knocking a sparse
// ordered pattern out of its ink: one pixel in four turns white.
void lightenInk(const GfxRenderer& renderer, const int x, const int y, const int width, const int height) {
  for (int py = y; py < y + height; ++py) {
    for (int px = x + ((py * 2) & 3); px < x + width; px += 4) renderer.drawPixel(px, py, false);
  }
}

std::string formatted(const StrId id, const std::string& arg) {
  char text[96];
  snprintf(text, sizeof(text), I18N.get(id), arg.c_str());
  return text;
}
}  // namespace

void CrossBindTheme::drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle,
                                const bool backButton) const {
  const ThemeMetrics& metrics = UITheme::getInstance().getMetrics();
  // Touch boards keep the FreeInkUI header for its tappable back button; short
  // bands (home's status strip) have no room for a title row.
  if (title == nullptr || gpio.hasTouch() || rect.height < metrics.headerHeight) {
    BaseTheme::drawHeader(renderer, rect, title, subtitle, backButton);
    return;
  }

  // Battery and clock strip only, then the title row below it.
  BaseTheme::drawHeader(renderer, rect, nullptr, nullptr, false);
  drawHeaderTitleRow(renderer, rect, title, subtitle, 0);
}

// Italic serif title with a hairline running out to the right label: the
// web's .block-head.
bool CrossBindTheme::drawHeaderTitleRow(const GfxRenderer& renderer, Rect band, const char* title, const char* label,
                                        const int rightReserve) const {
  if (title == nullptr || gpio.hasTouch()) return false;
  const ThemeMetrics& metrics = UITheme::getInstance().getMetrics();
  const int left = band.x + metrics.headerSidePadding;
  const int right = band.x + band.width - metrics.headerSidePadding - rightReserve;
  const int rowTop = band.y + metrics.batteryBarHeight;
  const int centerY = rowTop + (band.y + band.height - rowTop) / 2;
  constexpr int titleFont = SERIF_18_FONT_ID;  // italic-only face
  constexpr int gap = 14;

  int labelX = right;
  std::string upper;
  if (label != nullptr && label[0] != '\0') {
    upper = label;
    for (auto& c : upper) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    labelX = right - renderer.getTextAdvanceX(SMALL_FONT_ID, upper.c_str(), EpdFontFamily::BOLD, labelTracking);
  }

  const int titleMax = std::max(0, labelX - left - 2 * gap - 24);
  const std::string shownTitle = renderer.truncatedText(titleFont, title, titleMax, EpdFontFamily::ITALIC);
  renderer.drawText(titleFont, left, textTopForCenter(renderer, titleFont, centerY), shownTitle.c_str(), true,
                    EpdFontFamily::ITALIC);
  const int ruleStart = left + renderer.getTextWidth(titleFont, shownTitle.c_str(), EpdFontFamily::ITALIC) + gap;
  const int ruleEnd = upper.empty() ? right : labelX - gap;
  if (ruleEnd > ruleStart) renderer.drawLine(ruleStart, centerY, ruleEnd, centerY, true);
  if (!upper.empty()) {
    renderer.drawText(SMALL_FONT_ID, labelX, textTopForCenter(renderer, SMALL_FONT_ID, centerY), upper.c_str(), true,
                      EpdFontFamily::BOLD, BidiUtils::BidiBaseDir::AUTO, labelTracking);
  }
  return true;
}

void CrossBindTheme::drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                                     const char* btn4) const {
  if (gpio.hasTouch()) {
    return;
  }

  const GfxRenderer::Orientation origOrientation = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  // Full-width strip; labels keep the classic slots so each sits over its
  // physical button.
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  constexpr int slotWidth = 106;
  constexpr int stripHeight = CrossBindMetrics::values.buttonHintsHeight;
  constexpr int narrowSlots[] = {25, 130, 245, 350};
  constexpr int wideSlots[] = {38, 154, 268, 384};
  const int* slots = pageWidth >= 528 ? wideSlots : narrowSlots;
  const char* labels[] = {btn1, btn2, btn3, btn4};
  const bool grayscale = renderer.getRenderMode() != GfxRenderer::BW && !renderer.grayPlanesAreAbsolute();
  const int top = pageHeight - stripHeight;

  // Solid black strip with white labels. The gray pass repeats the same fill,
  // which leaves the monochrome strip from the base pass intact.
  renderer.fillRect(0, top, pageWidth, stripHeight, true);
  if (!grayscale) {
    constexpr int padding = 4;
    const int centerY = top + stripHeight / 2;
    for (int i = 0; i < 4; i++) {
      if (labels[i] == nullptr || labels[i][0] == '\0') continue;
      const std::string label = renderer.truncatedText(UI_10_FONT_ID, labels[i], slotWidth - 2 * padding);
      const int width = renderer.getTextWidth(UI_10_FONT_ID, label.c_str());
      renderer.drawText(UI_10_FONT_ID, slots[i] + (slotWidth - width) / 2,
                        textTopForCenter(renderer, UI_10_FONT_ID, centerY), label.c_str(), false);
    }
  }

  renderer.setOrientation(origOrientation);
}

void CrossBindTheme::drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                                    const std::function<std::string(int index)>& buttonLabel,
                                    const std::function<UIIcon(int index)>& rowIcon,
                                    const std::function<std::string(int index)>& rowValue) const {
  const ThemeMetrics& metrics = UITheme::getInstance().getMetrics();
  const int x = rect.x + metrics.contentSidePadding;
  const int width = rect.width - 2 * metrics.contentSidePadding;
  const int rowHeight = metrics.menuRowHeight;

  for (int i = 0; i < buttonCount; ++i) {
    const int y = rect.y + i * (rowHeight + metrics.menuSpacing);
    const bool selected = i == selectedIndex;
    // No separators: only the selected row gets a fill, drawn in white on it.
    if (selected) {
      renderer.fillRoundedRect(x, y, width, rowHeight, radius, Color::Black);
    }
    const bool ink = !selected;

    int textX = x + 16;
    if (rowIcon != nullptr) {
      if (const uint8_t* icon = iconForName(rowIcon(i))) {
        renderer.drawIcon(icon, textX, y + (rowHeight - menuIconSize) / 2, menuIconSize, ink);
        textX += menuIconSize + 16;
      }
    }

    int textRight = x + width - 16;
    if (rowValue != nullptr) {
      const std::string value = rowValue(i);
      if (!value.empty()) {
        const int valueWidth =
            renderer.getTextAdvanceX(SMALL_FONT_ID, value.c_str(), EpdFontFamily::BOLD, labelTracking);
        renderer.drawText(SMALL_FONT_ID, textRight - valueWidth,
                          y + (rowHeight - renderer.getLineHeight(SMALL_FONT_ID)) / 2, value.c_str(), ink,
                          EpdFontFamily::BOLD, BidiUtils::BidiBaseDir::AUTO, labelTracking);
        textRight -= valueWidth + 12;
      }
    }

    const std::string label = renderer.truncatedText(UI_12_FONT_ID, buttonLabel(i).c_str(), textRight - textX);
    renderer.drawText(UI_12_FONT_ID, textX, y + (rowHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2, label.c_str(),
                      ink);
  }
}

void CrossBindTheme::drawStatusRow(const GfxRenderer& renderer, const int x, const int y, const int width,
                                   const int rowHeight, const bool filled, const char* label, const std::string& text,
                                   const std::string& when) const {
  constexpr int dot = 9;
  const int centerY = y + rowHeight / 2;
  if (filled) {
    renderer.fillRoundedRect(x, centerY - dot / 2, dot, dot, dot / 2, Color::Black);
  } else {
    renderer.drawRoundedRect(x, centerY - dot / 2, dot, dot, 1, dot / 2, true);
  }

  const int labelX = x + dot + 12;
  renderer.drawText(SMALL_FONT_ID, labelX, textTopForCenter(renderer, SMALL_FONT_ID, centerY), label, true,
                    EpdFontFamily::BOLD, BidiUtils::BidiBaseDir::AUTO, labelTracking);

  int textRight = x + width;
  if (!when.empty()) {
    const int whenWidth = renderer.getTextWidth(UI_10_FONT_ID, when.c_str());
    renderer.drawText(UI_10_FONT_ID, textRight - whenWidth, textTopForCenter(renderer, UI_10_FONT_ID, centerY),
                      when.c_str(), true);
    textRight -= whenWidth + 12;
  }
  const int textX = labelX + statusLabelWidth;
  const std::string shown = renderer.truncatedText(UI_10_FONT_ID, text.c_str(), textRight - textX);
  renderer.drawText(UI_10_FONT_ID, textX, textTopForCenter(renderer, UI_10_FONT_ID, centerY), shown.c_str(), true);
}

void CrossBindTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                                         const int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                                         bool& bufferRestored, std::function<bool()> storeCoverBuffer) const {
  const ThemeMetrics& metrics = UITheme::getInstance().getMetrics();
  const int cardX = rect.x + metrics.contentSidePadding;
  const int cardY = rect.y;
  const int cardW = rect.width - 2 * metrics.contentSidePadding;
  const int cardH = rect.height;
  const int coverX = cardX + cardPadLeft;
  const int coverY = cardY + cardPadTop;
  const int coverH = metrics.homeCoverHeight;
  const auto& status = reading_status::current();

  if (recentBooks.empty()) {
    renderer.drawRoundedRect(cardX, cardY, cardW, cardH, 1, radius, true);
    drawEmptyRecents(renderer, Rect{cardX, cardY, cardW, cardH});
    return;
  }
  const RecentBook& book = recentBooks[0];
  const bool selected = selectorIndex == 0;

  // The cover is the only slow part (a BMP read from SD), so it is drawn once
  // and kept in the snapshot buffer; everything else redraws on each pass.
  if (!coverRendered) {
    bool hasCover = false;
    if (!book.coverBmpPath.empty()) {
      HalFile file;
      if (Storage.openFileForRead("HOME", UITheme::getCoverThumbPath(book.coverBmpPath, coverH), file)) {
        Bitmap bitmap(file);
        if (bitmap.parseHeaders() == BmpReaderError::Ok) {
          hasCover = drawCoverThumbFill(renderer, bitmap, Rect{coverX, coverY, coverWidth, coverH});
        }
        file.close();
      }
    }
    if (!hasCover) drawCoverPlaceholder(renderer, Rect{coverX, coverY, coverWidth, coverH});
    renderer.drawRect(coverX, coverY, coverWidth, coverH, true);
    coverBufferStored = storeCoverBuffer();
    coverRendered = coverBufferStored;
  } else if (!bufferRestored) {
    // Snapshot lost: draw the frame so the card never shows an empty hole.
    drawCoverPlaceholder(renderer, Rect{coverX, coverY, coverWidth, coverH});
    renderer.drawRect(coverX, coverY, coverWidth, coverH, true);
  }

  // Card frame: the selected card gets a heavier border and a wider inset bar
  // (the web's current-resume accent).
  if (selected) {
    renderer.drawRoundedRect(cardX, cardY, cardW, cardH, 2, radius, true);
    renderer.fillRect(cardX, cardY + 1, 6, cardH - 2, true);
  } else {
    renderer.drawRoundedRect(cardX, cardY, cardW, cardH, 1, radius, true);
    renderer.fillRect(cardX, cardY + 1, 3, cardH - 2, true);
  }

  // Progress pinned to the cover's bottom edge; title and chapter fill the
  // space above it, two lines each at most and ellipsized, and the chapter
  // gives up lines first when a long title leaves no room.
  const int textX = coverX + coverWidth + coverTextGap;
  const int textW = cardX + cardW - cardPadRight - textX;
  constexpr int titleFont = SERIF_12_FONT_ID;
  constexpr int chapterFont = SERIF_10_FONT_ID;  // italic-only face
  constexpr int barHeight = 5;
  const int barY = coverY + coverH - barHeight - 2;
  const int progressCenter = barY - 14;
  const int textBottom =
      status.percent >= 0 ? progressCenter - renderer.getLineHeight(SMALL_FONT_ID) / 2 - 8 : coverY + coverH;

  int y = coverY;
  const int titleLineHeight = renderer.getLineHeight(titleFont);
  const int titleMaxLines = std::clamp((textBottom - y) / titleLineHeight, 1, 2);
  for (const auto& line : renderer.wrappedText(titleFont, book.title.c_str(), textW, titleMaxLines)) {
    renderer.drawText(titleFont, textX, y, line.c_str(), true);
    y += titleLineHeight;
  }
  const std::string& chapter = status.chapter.empty() ? book.author : status.chapter;
  const int chapterLineHeight = renderer.getLineHeight(chapterFont);
  y += 4;
  const int chapterMaxLines = std::min(2, (textBottom - y) / chapterLineHeight);
  if (!chapter.empty() && chapterMaxLines > 0) {
    const int chapterTop = y;
    for (const auto& line : renderer.wrappedText(chapterFont, chapter.c_str(), textW, chapterMaxLines)) {
      renderer.drawText(chapterFont, textX, y, line.c_str(), true);
      y += chapterLineHeight;
    }
    // Secondary to the title, like the web's muted chapter line.
    lightenInk(renderer, textX, chapterTop, textW, y - chapterTop);
  }

  if (status.percent >= 0) {
    renderer.drawText(SMALL_FONT_ID, textX, textTopForCenter(renderer, SMALL_FONT_ID, progressCenter),
                      tr(STR_HOME_PROGRESS), true, EpdFontFamily::BOLD, BidiUtils::BidiBaseDir::AUTO, labelTracking);
    char percent[8];
    snprintf(percent, sizeof(percent), "%d%%", status.percent);
    const int percentWidth = renderer.getTextWidth(UI_10_FONT_ID, percent, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, textX + textW - percentWidth,
                      textTopForCenter(renderer, UI_10_FONT_ID, progressCenter), percent, true, EpdFontFamily::BOLD);
    renderer.fillRectDither(textX, barY, textW, barHeight, Color::LightGray);
    renderer.fillRect(textX, barY, textW * status.percent / 100, barHeight, true);
  }

  // Sync status under a hairline: library (Bindery) and this book's KOSync.
  const int innerRight = cardX + cardW - cardPadRight;
  const int ruleY = coverY + coverH + 14;
  renderer.drawLine(coverX, ruleY, innerRight, ruleY, true);
  const int rowHeight = std::max(24, renderer.getLineHeight(UI_10_FONT_ID) + 4);
  const int rowsTop = ruleY + 8;
  const int rowWidth = innerRight - coverX;

  drawStatusRow(renderer, coverX, rowsTop, rowWidth, rowHeight, status.librarySynced, tr(STR_HOME_LIBRARY_STATUS),
                status.librarySynced ? tr(STR_HOME_LIBRARY_SYNCED) : tr(STR_HOME_LIBRARY_NEVER),
                reading_status::formatWhen(status.librarySyncedAt));

  std::string kosyncText;
  const std::string device = status.kosyncDevice.empty() ? "?" : status.kosyncDevice;
  switch (status.kosync) {
    case reading_status::KosyncKind::Pulled:
      kosyncText = formatted(StrId::STR_HOME_KOSYNC_PULLED_FORMAT, device);
      break;
    case reading_status::KosyncKind::Same:
      kosyncText = formatted(StrId::STR_HOME_KOSYNC_SAME_FORMAT, device);
      break;
    case reading_status::KosyncKind::Sent:
      kosyncText = tr(STR_HOME_KOSYNC_SENT);
      break;
    case reading_status::KosyncKind::None:
      kosyncText = tr(STR_HOME_KOSYNC_NONE);
      break;
  }
  drawStatusRow(renderer, coverX, rowsTop + rowHeight + 4, rowWidth, rowHeight,
                status.kosync != reading_status::KosyncKind::None, tr(STR_HOME_KOSYNC_STATUS), kosyncText,
                reading_status::formatWhen(status.kosyncAt));
}
