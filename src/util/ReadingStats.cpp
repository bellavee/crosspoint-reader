#include "ReadingStats.h"

#include <ArduinoJson.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <ctime>
#include <functional>

#include "util/BookCacheUtils.h"

namespace reading_stats {
namespace {
constexpr char STATS_PATH[] = "/.crosspoint/reading-stats.json";
// Enough for the week chart and a month-long streak.
constexpr int DAYS_KEPT = 35;
// Dedup list for the yearly count; a reader finishing more is not limited,
// the oldest hashes just stop deduplicating.
constexpr size_t MAX_FINISHED = 200;
// Before this the clock was never set (RTCs boot at 2000-01-01).
constexpr int64_t MIN_VALID_UTC = 1704067200;  // 2024-01-01

bool readJson(JsonDocument& doc) {
  HalFile file;
  if (!Storage.openFileForRead("STATS", STATS_PATH, file)) return false;
  return !deserializeJson(doc, file);
}

// Write-then-rename so a power cut never leaves a torn JSON file.
bool writeJson(const JsonDocument& doc) {
  const std::string tmp = std::string(STATS_PATH) + ".tmp";
  {
    HalFile file;
    if (!Storage.openFileForWrite("STATS", tmp, file)) return false;
    if (serializeJson(doc, file) == 0) return false;
  }
  Storage.remove(STATS_PATH);
  return Storage.rename(tmp.c_str(), STATS_PATH);
}

// Days since 1970-01-01 for a civil date (Howard Hinnant's days_from_civil).
int daysFromCivil(int y, const int m, const int d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const int yoe = y - era * 400;
  const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

int localDay(const struct tm& t) { return daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday); }

// Today's local date; false without a trusted clock.
bool today(struct tm& out) {
  if (!halClock.localTime(out)) return false;
  struct tm copy = out;
  return static_cast<int64_t>(mktime(&copy)) >= MIN_VALID_UTC;
}

uint32_t pathHash(const std::string& path) { return static_cast<uint32_t>(std::hash<std::string>{}(path)); }
}  // namespace

void recordSession(const int64_t startUtc, const uint32_t seconds) {
  if (startUtc < MIN_VALID_UTC || seconds == 0) return;
  const time_t when = static_cast<time_t>(startUtc);
  struct tm start{};
  localtime_r(&when, &start);
  const int day = localDay(start);

  JsonDocument doc;
  if (!readJson(doc) || !doc.is<JsonObject>()) doc.to<JsonObject>();
  JsonArray days = doc["days"].is<JsonArray>() ? doc["days"].as<JsonArray>() : doc["days"].to<JsonArray>();

  bool added = false;
  for (JsonVariant v : days) {
    JsonArray entry = v.as<JsonArray>();
    if ((entry[0] | -1) == day) {
      entry[1] = (entry[1] | 0u) + seconds;
      added = true;
      break;
    }
  }
  if (!added) {
    JsonArray entry = days.add<JsonArray>();
    entry.add(day);
    entry.add(seconds);
  }
  // Keep the newest DAYS_KEPT days (a session may land on an older day than
  // the newest entry, e.g. after a clock change, so search for the oldest).
  while (days.size() > DAYS_KEPT) {
    size_t oldest = 0;
    for (size_t i = 1; i < days.size(); ++i) {
      if ((days[i][0] | 0) < (days[oldest][0] | 0)) oldest = i;
    }
    days.remove(oldest);
  }
  if (!writeJson(doc)) LOG_ERR("STATS", "Could not save %s", STATS_PATH);
}

void recordFinished(const std::string& bookPath) {
  struct tm now{};
  if (!today(now)) return;
  const int year = now.tm_year + 1900;

  JsonDocument doc;
  if (!readJson(doc) || !doc.is<JsonObject>()) doc.to<JsonObject>();
  if ((doc["year"] | 0) != year) {
    doc["year"] = year;
    doc.remove("count");
    doc["finished"].to<JsonArray>();
  }
  JsonArray finished =
      doc["finished"].is<JsonArray>() ? doc["finished"].as<JsonArray>() : doc["finished"].to<JsonArray>();
  const uint32_t hash = pathHash(bookPath);
  for (const JsonVariant v : finished) {
    if (v.as<uint32_t>() == hash) return;
  }
  // The count is kept apart from the dedup list, which drops its oldest hashes when full.
  doc["count"] = (doc["count"] | static_cast<int>(finished.size())) + 1;
  finished.add(hash);
  while (finished.size() > MAX_FINISHED) finished.remove(0);
  if (!writeJson(doc)) LOG_ERR("STATS", "Could not save %s", STATS_PATH);
}

Summary summary() {
  Summary out;
  struct tm now{};
  if (!today(now)) return out;
  out.clockValid = true;
  out.year = now.tm_year + 1900;
  out.todayWeekday = (now.tm_wday + 6) % 7;
  const int todayDay = localDay(now);

  JsonDocument doc;
  if (!readJson(doc)) return out;

  // Seconds per day of the kept window, today at the end.
  int seconds[DAYS_KEPT] = {};
  for (JsonVariantConst v : doc["days"].as<JsonArrayConst>()) {
    JsonArrayConst entry = v.as<JsonArrayConst>();
    const int age = todayDay - (entry[0] | INT32_MIN / 2);
    if (age >= 0 && age < DAYS_KEPT) seconds[DAYS_KEPT - 1 - age] += entry[1] | 0;
  }
  for (int i = 0; i < WEEK_DAYS; ++i) {
    const int s = seconds[DAYS_KEPT - WEEK_DAYS + i];
    out.dayMinutes[i] = s / 60;
    out.weekMinutes += s;
  }
  out.weekMinutes /= 60;
  out.todayMinutes = out.dayMinutes[WEEK_DAYS - 1];
  // A streak survives a day not read yet: count back from yesterday when
  // today is still empty.
  int i = DAYS_KEPT - 1;
  if (seconds[i] == 0) --i;
  while (i >= 0 && seconds[i] > 0) {
    ++out.streakDays;
    --i;
  }

  if ((doc["year"] | 0) == out.year) {
    out.finishedThisYear = doc["count"] | static_cast<int>(doc["finished"].as<JsonArrayConst>().size());
  }
  return out;
}

void recordBookPercent(const std::string& bookPath, const int percent) {
  if (percent < 0 || percent > 100) return;
  const std::string dir = bookCacheDir(bookPath);
  if (dir.empty() || !Storage.exists(dir.c_str())) return;
  const uint8_t value = static_cast<uint8_t>(percent);
  if (bookPercent(bookPath) == value) return;  // spare the SD write
  HalFile file;
  if (!Storage.openFileForWrite("STATS", dir + "/percent.bin", file)) return;
  file.write(&value, 1);
}

int bookPercent(const std::string& bookPath) {
  const std::string dir = bookCacheDir(bookPath);
  if (dir.empty()) return -1;
  HalFile file;
  if (!Storage.openFileForRead("STATS", dir + "/percent.bin", file)) return -1;
  uint8_t value = 0;
  if (file.read(&value, 1) != 1 || value > 100) return -1;
  return value;
}

}  // namespace reading_stats
