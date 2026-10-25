#include "QuireTheme.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalGPIO.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include "CrossPointSettings.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/ReadingStatus.h"

namespace {
constexpr int radius = 6;
constexpr int side = 22;       // page margin
constexpr int columnGap = 18;  // cover to stats column
constexpr int capsTracking = 2;
constexpr int hintWidth = 106;
constexpr int hintHeight = 50;
constexpr int hintTop = 6;  // inside the hint band
#ifdef OMIT_FONTS
constexpr int bigFont = UI_12_FONT_ID;
#else
constexpr int bigFont = NOTOSANS_18_FONT_ID;  // 18pt: the mock's 37.5px numeral
#endif

// Caps labels uppercase ASCII only: byte-wise toupper would leave a mixed-case
// word in scripts with diacritics ("HôM NAY"), so those keep their case.
std::string capsLabel(const char* text) {
  std::string out = text ? text : "";
  if (std::any_of(out.begin(), out.end(), [](const char c) { return static_cast<unsigned char>(c) >= 0x80; })) {
    return out;
  }
  for (auto& c : out) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
  return out;
}

std::string formatted(const StrId id, const int a) {
  char text[48];
  snprintf(text, sizeof(text), I18N.get(id), a);
  return text;
}

std::string formatted(const StrId id, const int a, const int b) {
  char text[48];
  snprintf(text, sizeof(text), I18N.get(id), a, b);
  return text;
}

std::string formatted(const StrId id, const std::string& arg) {
  char text[96];
  snprintf(text, sizeof(text), I18N.get(id), arg.c_str());
  return text;
}

// "2h ago" within a day, the date beyond.
std::string relativeWhen(const int64_t utc) {
  const int64_t now = reading_status::nowUtc();
  if (utc <= 0 || now <= 0 || now < utc) return reading_status::formatWhen(utc);
  const int64_t elapsed = now - utc;
  if (elapsed < 3600) return formatted(StrId::STR_QUIRE_MINUTES_AGO_FORMAT, std::max<int>(1, elapsed / 60));
  if (elapsed < 86400) return formatted(StrId::STR_QUIRE_HOURS_AGO_FORMAT, static_cast<int>(elapsed / 3600));
  return reading_status::formatWhen(utc);
}

// White out the pixels outside a radius-r arc at each corner, so a cover
// bitmap drawn square reads as a rounded card.
void roundCorners(const GfxRenderer& renderer, const Rect rect, const int r) {
  for (int dy = 0; dy < r; ++dy) {
    for (int dx = 0; dx < r; ++dx) {
      const float cx = r - dx - 0.5f;
      const float cy = r - dy - 0.5f;
      if (cx * cx + cy * cy <= static_cast<float>(r * r)) continue;
      renderer.drawPixel(rect.x + dx, rect.y + dy, false);
      renderer.drawPixel(rect.x + rect.width - 1 - dx, rect.y + dy, false);
      renderer.drawPixel(rect.x + dx, rect.y + rect.height - 1 - dy, false);
      renderer.drawPixel(rect.x + rect.width - 1 - dx, rect.y + rect.height - 1 - dy, false);
    }
  }
}

// Draws the book's cover thumb into `slot`, a placeholder when there is none.
void drawCover(const GfxRenderer& renderer, const std::string& thumbPath, const Rect slot) {
  bool drawn = false;
  if (!thumbPath.empty()) {
    HalFile file;
    if (Storage.openFileForRead("QUIRE", thumbPath, file)) {
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() == BmpReaderError::Ok) drawn = BaseTheme::drawCoverThumbFill(renderer, bitmap, slot);
      file.close();
    }
  }
  if (!drawn) BaseTheme::drawCoverPlaceholder(renderer, slot);
  roundCorners(renderer, slot, radius);
}

// Rounded outline with a solid fill for `percent`, the mock's reading bar.
void drawProgress(const GfxRenderer& renderer, const int x, const int y, const int width, const int height,
                  const int percent) {
  const int fill = width * std::clamp(percent, 0, 100) / 100;
  if (fill > 0) renderer.fillRoundedRect(x, y, std::max(fill, height), height, height / 2, Color::Black);
  renderer.drawRoundedRect(x, y, width, height, 2, height / 2, true);
}

void drawCaps(const GfxRenderer& renderer, const int x, const int y, const char* text, const bool black = true) {
  const std::string caps = capsLabel(text);
  renderer.drawText(SMALL_FONT_ID, x, y, caps.c_str(), black, EpdFontFamily::REGULAR, BidiUtils::BidiBaseDir::AUTO,
                    capsTracking);
}

int capsWidth(const GfxRenderer& renderer, const char* text) {
  const std::string caps = capsLabel(text);
  return renderer.getTextAdvanceX(SMALL_FONT_ID, caps.c_str(), EpdFontFamily::REGULAR, capsTracking);
}

// One status line: a dot (filled = done) and its text.
void drawStatusRow(const GfxRenderer& renderer, const int x, const int y, const int width, const bool done,
                   const std::string& text) {
  constexpr int dot = 9;
  const int lineHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int dotY = y + (lineHeight - dot) / 2;
  if (done) {
    renderer.fillRoundedRect(x, dotY, dot, dot, dot / 2, Color::Black);
  } else {
    renderer.drawRoundedRect(x, dotY, dot, dot, 1, dot / 2, true);
  }
  const int textX = x + dot + 10;
  const std::string shown = renderer.truncatedText(SMALL_FONT_ID, text.c_str(), x + width - textX);
  renderer.drawText(SMALL_FONT_ID, textX, y, shown.c_str(), true);
}

std::vector<std::string> weekdayInitials() {
  std::vector<std::string> out;
  out.reserve(reading_stats::WEEK_DAYS);
  const std::string all = I18N.get(StrId::STR_QUIRE_WEEKDAYS);
  size_t start = 0;
  while (out.size() < reading_stats::WEEK_DAYS && start <= all.size()) {
    const size_t end = all.find(' ', start);
    out.push_back(all.substr(start, end == std::string::npos ? std::string::npos : end - start));
    if (end == std::string::npos) break;
    start = end + 1;
  }
  while (out.size() < reading_stats::WEEK_DAYS) out.emplace_back("");
  return out;
}
}  // namespace

// --- Header and hints ---

void QuireTheme::drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle,
                            const bool backButton) const {
  const ThemeMetrics& metrics = UITheme::getInstance().getMetrics();
  // Touch boards keep the FreeInkUI header for its tappable back button; short
  // bands (home's status strip) have no room for a title row.
  if (title == nullptr || gpio.hasTouch() || rect.height < metrics.headerHeight) {
    BaseTheme::drawHeader(renderer, rect, title, subtitle, backButton);
    return;
  }
  // Clock and battery strip only, then the title row below it.
  BaseTheme::drawHeader(renderer, rect, nullptr, nullptr, false);
  drawHeaderTitleRow(renderer, rect, title, subtitle, 0);
}

// Bold title on the left, an optional caps label (a count) on the right,
// sharing one baseline.
bool QuireTheme::drawHeaderTitleRow(const GfxRenderer& renderer, Rect band, const char* title, const char* label,
                                    const int rightReserve) const {
  if (title == nullptr || gpio.hasTouch()) return false;
  const ThemeMetrics& metrics = UITheme::getInstance().getMetrics();
  const int left = band.x + metrics.headerSidePadding;
  const int right = band.x + band.width - metrics.headerSidePadding - rightReserve;
  const int rowTop = band.y + metrics.batteryBarHeight;
  const int titleY = rowTop + std::max(0, (band.y + band.height - rowTop - renderer.getLineHeight(UI_12_FONT_ID)) / 2);
  const int baseline = titleY + renderer.getFontAscenderSize(UI_12_FONT_ID);

  int titleRight = right;
  if (label != nullptr && label[0] != '\0') {
    const int labelX = right - capsWidth(renderer, label);
    drawCaps(renderer, labelX, baseline - renderer.getFontAscenderSize(SMALL_FONT_ID), label);
    titleRight = labelX - 16;
  }
  const std::string shown =
      renderer.truncatedText(UI_12_FONT_ID, title, std::max(0, titleRight - left), EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, left, titleY, shown.c_str(), true, EpdFontFamily::BOLD);
  return true;
}

void QuireTheme::drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                                 const char* btn4) const {
  const char* labels[] = {btn1, btn2, btn3, btn4};
  drawHints(renderer, labels, 1, false);  // Confirm is the main action
}

void QuireTheme::drawButtonHintsPrimary(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                                        const char* btn4, const int primary) const {
  const char* labels[] = {btn1, btn2, btn3, btn4};
  // The home's buttons are its only menu, so they show on touch boards too.
  drawHints(renderer, labels, primary, true);
}

// Text-only buttons over the physical keys; the primary one is a black pill.
void QuireTheme::drawHints(GfxRenderer& renderer, const char* const labels[4], const int primary,
                           const bool always) const {
  if (gpio.hasTouch() && !always) return;

  const GfxRenderer::Orientation origOrientation = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  const ThemeMetrics& metrics = UITheme::getInstance().getMetrics();
  const int top = renderer.getScreenHeight() - metrics.buttonHintsHeight + hintTop;
  // The physical key positions, as in BaseTheme::drawButtonHints.
  constexpr int narrowSlots[] = {25, 130, 245, 350};
  constexpr int wideSlots[] = {38, 154, 268, 384};
  const int* slots = renderer.getScreenWidth() >= 528 ? wideSlots : narrowSlots;
  const bool grayscale = renderer.getRenderMode() != GfxRenderer::BW && !renderer.grayPlanesAreAbsolute();

  for (int i = 0; i < 4; ++i) {
    if (labels[i] == nullptr || labels[i][0] == '\0') continue;
    const int x = slots[i];
    // Zero gray-plane bits leave the monochrome hint from the base pass intact.
    renderer.fillRect(x, top, hintWidth, hintHeight, grayscale);
    if (grayscale) continue;
    const bool isPrimary = i == primary;
    const auto style = isPrimary ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    if (isPrimary) renderer.fillRoundedRect(x, top, hintWidth, hintHeight, radius, Color::Black);
    const std::string label = renderer.truncatedText(UI_10_FONT_ID, labels[i], hintWidth - 8, style);
    const int width = renderer.getTextWidth(UI_10_FONT_ID, label.c_str(), style);
    const int y = top + std::max(1, (hintHeight - renderer.getTextHeight(UI_10_FONT_ID)) / 2);
    renderer.drawText(UI_10_FONT_ID, x + (hintWidth - width) / 2, y, label.c_str(), !isPrimary, style);
  }

  renderer.setOrientation(origOrientation);
}

// --- Home ---

Rect QuireTheme::coverRect(const GfxRenderer&, const Rect tile) {
  // 58% of the inner width at 2:3: 280x420 on the X3's 528px panel.
  const int width = (tile.width - 2 * side) * 58 / 100;
  return Rect{tile.x + side, tile.y, width, width * 3 / 2};
}

int QuireTheme::homeCoverThumbHeight(const GfxRenderer& renderer) const {
  return coverRect(renderer, UITheme::getContentArea(renderer)).height;
}

Rect QuireTheme::homeCoverSnapshotRect(const GfxRenderer& renderer, const Rect tile) const {
  return coverRect(renderer, tile);
}

void QuireTheme::drawRecentBookCover(GfxRenderer& renderer, const Rect rect, const std::vector<RecentBook>& recentBooks,
                                     const int /*selectorIndex*/, bool& coverRendered, bool& coverBufferStored,
                                     bool& bufferRestored, std::function<bool()> storeCoverBuffer) const {
  const Rect cover = coverRect(renderer, rect);
  const bool hasBook = !recentBooks.empty();
  const auto& status = reading_status::current();

  // The cover is the only slow part (a BMP read from SD), so it is drawn once
  // and kept in the snapshot buffer; everything else redraws on each pass.
  if (!coverRendered) {
    drawCover(renderer,
              hasBook && !recentBooks[0].coverBmpPath.empty()
                  ? UITheme::getCoverThumbPath(recentBooks[0].coverBmpPath, cover.height)
                  : std::string(),
              cover);
    coverBufferStored = storeCoverBuffer();
    coverRendered = coverBufferStored;
  } else if (!bufferRestored) {
    // Snapshot lost: draw the placeholder so the page never shows a hole.
    drawCover(renderer, std::string(), cover);
  }

  const int columnX = cover.x + cover.width + columnGap;
  drawStats(renderer, columnX, cover.y, rect.x + rect.width - side - columnX, cover.height);

  // The book's place: title, chapter, progress.
  const int left = rect.x + side;
  const int width = rect.width - 2 * side;
  int y = cover.y + cover.height + 22;
  if (!hasBook) {
    renderer.drawText(SERIF_12_FONT_ID, left, y, tr(STR_QUIRE_NO_BOOK), true);
    y += renderer.getLineHeight(SERIF_12_FONT_ID);
  } else {
    const RecentBook& book = recentBooks[0];
    const std::string title = renderer.truncatedText(SERIF_12_FONT_ID, book.title.c_str(), width);
    renderer.drawText(SERIF_12_FONT_ID, left, y, title.c_str(), true);
    y += renderer.getLineHeight(SERIF_12_FONT_ID);
    const std::string& subline = status.chapter.empty() ? book.author : status.chapter;
    if (!subline.empty()) {
      // SERIF_10 is an italic-only face.
      const std::string shown = renderer.truncatedText(SERIF_10_FONT_ID, subline.c_str(), width);
      renderer.drawText(SERIF_10_FONT_ID, left, y, shown.c_str(), true);
      y += renderer.getLineHeight(SERIF_10_FONT_ID);
    }
    if (status.percent >= 0) {
      y += 12;
      char percent[8];
      snprintf(percent, sizeof(percent), "%d%%", status.percent);
      renderer.drawText(UI_10_FONT_ID, left, y, percent, true, EpdFontFamily::BOLD);
      if (status.chapterNumber > 0 && status.chapterCount > 0) {
        const std::string where = formatted(StrId::STR_QUIRE_CHAPTER_FORMAT, status.chapterNumber, status.chapterCount);
        const int whereX = left + width - renderer.getTextWidth(SMALL_FONT_ID, where.c_str());
        const int whereY =
            y + renderer.getFontAscenderSize(UI_10_FONT_ID) - renderer.getFontAscenderSize(SMALL_FONT_ID);
        renderer.drawText(SMALL_FONT_ID, whereX, whereY, where.c_str(), true);
      }
      y += renderer.getLineHeight(UI_10_FONT_ID) + 2;
      drawProgress(renderer, left, y, width, 8, status.percent);
      y += 8;
    }
  }

  // Sync status: the Bindery library and this book's KOReader Sync.
  y += 20;
  const int rowStep = renderer.getLineHeight(SMALL_FONT_ID) + 4;
  std::string library = std::string(tr(STR_BINDERY)) + " ";
  if (status.librarySynced) {
    library += tr(STR_HOME_LIBRARY_SYNCED);
    const std::string when = relativeWhen(status.librarySyncedAt);
    if (!when.empty()) library += " \xC2\xB7 " + when;
  } else {
    library += tr(STR_HOME_LIBRARY_NEVER);
  }
  drawStatusRow(renderer, left, y, width, status.librarySynced, library);
  if (!hasBook) return;

  std::string kosync = "KOSync ";
  const std::string device = status.kosyncDevice.empty() ? "?" : status.kosyncDevice;
  switch (status.kosync) {
    case reading_status::KosyncKind::Pulled:
      kosync += formatted(StrId::STR_HOME_KOSYNC_PULLED_FORMAT, device);
      break;
    case reading_status::KosyncKind::Same:
      kosync += formatted(StrId::STR_HOME_KOSYNC_SAME_FORMAT, device);
      break;
    case reading_status::KosyncKind::Sent:
      kosync += tr(STR_HOME_KOSYNC_SENT);
      break;
    case reading_status::KosyncKind::None:
      kosync += tr(STR_HOME_KOSYNC_NONE);
      break;
  }
  if (status.kosync != reading_status::KosyncKind::None) {
    const std::string when = relativeWhen(status.kosyncAt);
    if (!when.empty()) kosync += " \xC2\xB7 " + when;
  }
  drawStatusRow(renderer, left, y + rowStep, width, status.kosync != reading_status::KosyncKind::None, kosync);
}

// Today, streak, the week and the year's books, over a seven-day bar chart
// pinned to the bottom of the column (the cover's height).
void QuireTheme::drawStats(const GfxRenderer& renderer, const int x, const int y, const int width,
                           const int height) const {
  const auto& stats = reading_status::current().stats;
  const char* none = "\xE2\x80\x93";  // en dash
  const int smallLine = renderer.getLineHeight(SMALL_FONT_ID);
  int cursor = y;

  drawCaps(renderer, x, cursor, tr(STR_QUIRE_TODAY));
  cursor += smallLine;
  if (stats.clockValid) {
    const std::string minutes = std::to_string(stats.todayMinutes);
    renderer.drawText(bigFont, x, cursor, minutes.c_str(), true, EpdFontFamily::BOLD);
    const int unitX = x + renderer.getTextWidth(bigFont, minutes.c_str(), EpdFontFamily::BOLD) + 6;
    const int unitY = cursor + renderer.getFontAscenderSize(bigFont) - renderer.getFontAscenderSize(UI_10_FONT_ID);
    renderer.drawText(UI_10_FONT_ID, unitX, unitY, tr(STR_QUIRE_MIN), true);
  } else {
    renderer.drawText(bigFont, x, cursor, none, true, EpdFontFamily::BOLD);
  }
  cursor += renderer.getFontAscenderSize(bigFont) + 10;

  const int goal = SETTINGS.readingGoalBooks();
  char yearLabel[24];
  snprintf(yearLabel, sizeof(yearLabel), tr(STR_QUIRE_BOOKS_YEAR_FORMAT), stats.year % 100);
  const struct {
    const char* label;
    std::string value;
  } rows[] = {
      {tr(STR_QUIRE_STREAK), stats.clockValid ? formatted(StrId::STR_QUIRE_DAYS_FORMAT, stats.streakDays) : none},
      {tr(STR_QUIRE_THIS_WEEK), stats.clockValid ? formatted(StrId::STR_QUIRE_HOURS_MINUTES_FORMAT,
                                                             stats.weekMinutes / 60, stats.weekMinutes % 60)
                                                 : none},
      {yearLabel, !stats.clockValid ? std::string(none)
                  : goal > 0        ? formatted(StrId::STR_QUIRE_OF_FORMAT, stats.finishedThisYear, goal)
                                    : std::to_string(stats.finishedThisYear)},
  };
  for (const auto& row : rows) {
    cursor += 12;
    drawCaps(renderer, x, cursor, row.label);
    cursor += smallLine;
    const std::string value = renderer.truncatedText(UI_12_FONT_ID, row.value.c_str(), width, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, x, cursor, value.c_str(), true, EpdFontFamily::BOLD);
    cursor += renderer.getLineHeight(UI_12_FONT_ID) - 4;
  }

  // Week chart: past days dithered, today solid, empty days just the baseline.
  constexpr int barGap = 5;
  const int baseline = y + height - smallLine - 4;
  const int chartHeight = std::min(72, baseline - cursor - 10);
  if (chartHeight < 20) return;
  const int barWidth = (width - (reading_stats::WEEK_DAYS - 1) * barGap) / reading_stats::WEEK_DAYS;
  int peak = 1;
  for (const int m : stats.dayMinutes) peak = std::max(peak, m);
  const auto initials = weekdayInitials();
  for (int i = 0; i < reading_stats::WEEK_DAYS; ++i) {
    const int barX = x + i * (barWidth + barGap);
    const bool today = i == reading_stats::WEEK_DAYS - 1;
    const int minutes = stats.dayMinutes[i];
    if (minutes > 0) {
      const int barHeight = std::max(4, chartHeight * minutes / peak);
      if (today) {
        renderer.fillRoundedRect(barX, baseline - barHeight, barWidth, barHeight, 3, true, true, false, false,
                                 Color::Black);
      } else {
        renderer.fillRectDither(barX, baseline - barHeight, barWidth, barHeight, Color::DarkGray);
      }
    }
    const int weekday = (stats.todayWeekday - (reading_stats::WEEK_DAYS - 1 - i) + 2 * 7) % 7;
    const std::string& initial = initials[static_cast<size_t>(weekday)];
    const auto style = today ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    const int labelWidth = renderer.getTextWidth(SMALL_FONT_ID, initial.c_str(), style);
    renderer.drawText(SMALL_FONT_ID, barX + (barWidth - labelWidth) / 2, baseline + 4, initial.c_str(), true, style);
  }
  renderer.fillRect(x, baseline, width, 2, true);
}

// --- Popups ---

Rect QuireTheme::drawPopup(const GfxRenderer& renderer, const char* message) const {
  constexpr int padX = 28;
  constexpr int padY = 18;
  constexpr int barSpace = 16;  // room for fillPopupProgress under the text
  const int screenWidth = renderer.getScreenWidth();
  const std::string text = renderer.truncatedText(UI_12_FONT_ID, message, screenWidth - 2 * side - 2 * padX);
  const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, text.c_str());
  const int w = std::min(screenWidth - 2 * side, std::max(textWidth + 2 * padX, screenWidth / 2));
  const int h = renderer.getLineHeight(UI_12_FONT_ID) + 2 * padY + barSpace;
  const int x = (screenWidth - w) / 2;
  const int y = renderer.getScreenHeight() * 2 / 5 - h / 2;

  renderer.fillRoundedRect(x, y, w, h, radius, Color::White);
  renderer.drawRoundedRect(x, y, w, h, 2, radius, true);
  renderer.drawText(UI_12_FONT_ID, x + (w - textWidth) / 2, y + padY, text.c_str(), true);
  renderer.displayBuffer();
  return Rect{x, y, w, h};
}

void QuireTheme::fillPopupProgress(const GfxRenderer& renderer, const Rect& layout, const int progress) const {
  constexpr int padX = 28;
  constexpr int barHeight = 8;
  const int barY = layout.y + layout.height - 18 - barHeight / 2;
  drawProgress(renderer, layout.x + padX, barY, layout.width - 2 * padX, barHeight, progress);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

// The cover, a caps "Opening", the title and what is happening, centered.
bool QuireTheme::drawBookOpening(const GfxRenderer& renderer, const BookOpening& book) const {
  renderer.clearScreen();
  const int screenWidth = renderer.getScreenWidth();
  int coverHeight = UITheme::getInstance().libraryCoverHeight(renderer);
  if (coverHeight <= 0) coverHeight = 240;
  const Rect cover{(screenWidth - coverHeight * 2 / 3) / 2, renderer.getScreenHeight() * 19 / 100, coverHeight * 2 / 3,
                   coverHeight};
  drawCover(renderer, book.coverPath, cover);

  int y = cover.y + cover.height + 30;
  const std::string opening = capsLabel(tr(STR_QUIRE_OPENING));
  const int openingWidth =
      renderer.getTextAdvanceX(SMALL_FONT_ID, opening.c_str(), EpdFontFamily::REGULAR, capsTracking);
  renderer.drawText(SMALL_FONT_ID, (screenWidth - openingWidth) / 2, y, opening.c_str(), true, EpdFontFamily::REGULAR,
                    BidiUtils::BidiBaseDir::AUTO, capsTracking);
  y += renderer.getLineHeight(SMALL_FONT_ID) + 4;
  if (book.title != nullptr && book.title[0] != '\0') {
    const std::string title = renderer.truncatedText(SERIF_12_FONT_ID, book.title, screenWidth - 2 * side);
    renderer.drawCenteredText(SERIF_12_FONT_ID, y, title.c_str(), true);
    y += renderer.getLineHeight(SERIF_12_FONT_ID) + 4;
  }
  if (book.status != nullptr && book.status[0] != '\0') {
    renderer.drawCenteredText(UI_10_FONT_ID, y, book.status, true);
  }
  renderer.displayBuffer();
  return true;
}
