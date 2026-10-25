#include "ReadingStatus.h"

#include <ArduinoJson.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <Logging.h>

#include <climits>
#include <ctime>

#include "util/BookProgress.h"

namespace reading_status {
namespace {
constexpr char LIBRARY_PATH[] = "/.crosspoint/bindery-status.json";
constexpr char KOSYNC_PATH[] = "/.crosspoint/kosync-status.json";
// Matches RecentBooksStore::MAX_RECENT_BOOKS: only books still on the home
// screen's recent list can ever be shown.
constexpr size_t MAX_KOSYNC_BOOKS = 10;
// Before this the clock was never set (RTCs boot at 2000-01-01).
constexpr int64_t MIN_VALID_UTC = 1704067200;  // 2024-01-01

Snapshot snapshot;

bool readJson(const char* path, JsonDocument& doc) {
  HalFile file;
  if (!Storage.openFileForRead("STATUS", path, file)) return false;
  return !deserializeJson(doc, file);
}

// Write-then-rename so a power cut never leaves a torn JSON file.
bool writeJson(const char* path, const JsonDocument& doc) {
  const std::string tmp = std::string(path) + ".tmp";
  {
    HalFile file;
    if (!Storage.openFileForWrite("STATUS", tmp, file)) return false;
    if (serializeJson(doc, file) == 0) return false;
  }
  Storage.remove(path);
  return Storage.rename(tmp.c_str(), path);
}
}  // namespace

void load(const std::string& bookPath) {
  snapshot = Snapshot{};

  JsonDocument library;
  if (readJson(LIBRARY_PATH, library)) {
    snapshot.librarySynced = true;
    snapshot.librarySyncedAt = library["at"] | int64_t{0};
    snapshot.libraryBooks = library["books"] | -1;
  }

  if (bookPath.empty()) return;
  snapshot.percent = loadBookProgress(bookPath, &snapshot.chapter);

  JsonDocument kosync;
  if (readJson(KOSYNC_PATH, kosync)) {
    JsonObjectConst entry = kosync[bookPath].as<JsonObjectConst>();
    if (!entry.isNull()) {
      snapshot.kosync = static_cast<KosyncKind>(entry["k"] | 0);
      snapshot.kosyncDevice = entry["d"] | "";
      snapshot.kosyncAt = entry["t"] | int64_t{0};
    }
  }
}

const Snapshot& current() { return snapshot; }

void recordLibrarySync(const int bookCount) {
  JsonDocument doc;
  doc["at"] = nowUtc();
  doc["books"] = bookCount;
  if (!writeJson(LIBRARY_PATH, doc)) LOG_ERR("STATUS", "Could not save %s", LIBRARY_PATH);
}

void recordKosync(const std::string& bookPath, const KosyncKind kind, const std::string& device) {
  JsonDocument doc;
  if (!readJson(KOSYNC_PATH, doc) || !doc.is<JsonObject>()) doc.to<JsonObject>();
  JsonObject books = doc.as<JsonObject>();
  books.remove(bookPath);

  // Drop the oldest record once full, so the file stays a few hundred bytes.
  while (books.size() >= MAX_KOSYNC_BOOKS) {
    std::string oldest;
    int64_t oldestAt = INT64_MAX;
    for (JsonPair entry : books) {
      const int64_t at = entry.value()["t"] | int64_t{0};
      if (oldest.empty() || at < oldestAt) {
        oldest = entry.key().c_str();
        oldestAt = at;
      }
    }
    if (oldest.empty()) break;
    books.remove(oldest);
  }

  JsonObject entry = books[bookPath].to<JsonObject>();
  entry["k"] = static_cast<int>(kind);
  if (!device.empty()) entry["d"] = device;
  entry["t"] = nowUtc();
  if (!writeJson(KOSYNC_PATH, doc)) LOG_ERR("STATUS", "Could not save %s", KOSYNC_PATH);
}

int64_t nowUtc() {
  struct tm local{};
  if (!halClock.localTime(local)) return 0;
  // mktime reads the struct as local time under the TZ halClock applies.
  const int64_t utc = static_cast<int64_t>(mktime(&local));
  return utc >= MIN_VALID_UTC ? utc : 0;
}

std::string formatWhen(const int64_t utc) {
  if (utc < MIN_VALID_UTC) return {};
  const time_t when = static_cast<time_t>(utc);
  struct tm then{};
  localtime_r(&when, &then);

  struct tm now{};
  const bool sameDay = halClock.localTime(now) && now.tm_year == then.tm_year && now.tm_yday == then.tm_yday;
  char text[8];
  if (sameDay) {
    snprintf(text, sizeof(text), "%02d:%02d", then.tm_hour, then.tm_min);
  } else {
    snprintf(text, sizeof(text), "%02d/%02d", then.tm_mday, then.tm_mon + 1);
  }
  return text;
}

}  // namespace reading_status
