#pragma once

#include <cstdint>
#include <string>

#include "util/ReadingStats.h"

// What the CrossBind home card shows beside the cover: the featured book's
// chapter and progress, the last finished Bindery library sync, and the last
// KOReader Sync result for that book. Loaded once per home entry
// (HomeActivity::onEnter), never per render: the chapter title needs the
// book's metadata cache.
//
// Both sync records live in small JSON files under /.crosspoint, written by
// the activities that did the syncing:
//   bindery-status.json  {"at": <utc>, "books": <n>}
//   kosync-status.json   {"<book path>": {"k": <kind>, "d": "<device>", "t": <utc>}, ...}
// Times are 0 when the device had no wall clock at the time.
namespace reading_status {

enum class KosyncKind : uint8_t { None = 0, Pulled = 1, Sent = 2, Same = 3 };

struct Snapshot {
  std::string chapter;    // TOC title at the saved position, empty if unknown
  int chapterNumber = 0;  // its 1-based TOC position, 0 if unknown
  int chapterCount = 0;
  int percent = -1;  // saved book progress, -1 if unknown

  bool librarySynced = false;  // a Bindery run has finished at least once
  int64_t librarySyncedAt = 0;
  int libraryBooks = -1;

  KosyncKind kosync = KosyncKind::None;
  std::string kosyncDevice;  // remote device for Pulled / Same
  int64_t kosyncAt = 0;

  reading_stats::Summary stats;  // reading time and finished books (Quire)
};

// Reloads the snapshot; an empty path loads only the library record.
void load(const std::string& bookPath);
const Snapshot& current();

void recordLibrarySync(int bookCount);
void recordKosync(const std::string& bookPath, KosyncKind kind, const std::string& device);

// UTC now, or 0 without a running wall clock.
int64_t nowUtc();
// "20:05" for today, "21/10" for an earlier day, empty when unknown.
std::string formatWhen(int64_t utc);

}  // namespace reading_status
