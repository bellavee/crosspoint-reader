#pragma once

#include <cstdint>
#include <string>

// Reading time and finished books, kept on the device for the Quire home's
// stats column. Fed by the reader: each active-reading session (the same
// summary plugins receive, see ReaderSession) adds its seconds to the local
// day it started on, and reaching the end of a book counts it once for the
// year. Everything lives in one small JSON file:
//   /.crosspoint/reading-stats.json
//   {"days": [[<local day>, <seconds>], ...], "year": 2026, "finished": [<path hash>, ...]}
// Days are counted since 1970-01-01 in local time; only the last DAYS_KEPT are
// kept, so the file stays a few hundred bytes.
namespace reading_stats {

constexpr int WEEK_DAYS = 7;

struct Summary {
  bool clockValid = false;  // false: no trusted wall clock, nothing below is meaningful
  int todayMinutes = 0;
  int streakDays = 0;              // consecutive days with reading, ending today (or yesterday)
  int weekMinutes = 0;             // the last seven days, today included
  int dayMinutes[WEEK_DAYS] = {};  // oldest first, today last
  int todayWeekday = 0;            // 0 = Monday
  int year = 0;                    // e.g. 2026
  int finishedThisYear = 0;
};

// Adds a session of `seconds` to the local day containing `startUtc`.
void recordSession(int64_t startUtc, uint32_t seconds);
// Counts the book as finished this year, once.
void recordFinished(const std::string& bookPath);
// Reads the file; cheap enough for one call per home entry.
Summary summary();

// Whole-book percentage the reader last showed for a book, kept in the book's
// cache folder (one byte) so lists can draw progress without opening the book.
void recordBookPercent(const std::string& bookPath, int percent);
// -1 when the book was never opened since this was introduced.
int bookPercent(const std::string& bookPath);

}  // namespace reading_stats
