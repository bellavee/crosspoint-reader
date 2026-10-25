#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"

/**
 * One-tap library sync with a bellasekai server ("Bindery" on the home menu).
 *
 * Mirrors the KOReader plugin's "Sync library": fetch the device's library list,
 * download new books, re-download books whose version changed, delete books that
 * left the device's collection. Files land in /Bindery under the server's fixed
 * names (bellasekai-<id>.epub), so KOReader Sync's filename matching works as is.
 *
 * Server and account come from the KOReader Sync settings: the sync URL is
 * "<bellasekai>/api/kosync", so stripping that suffix gives the server root.
 *
 * Only files this activity downloaded are ever deleted: they are tracked in
 * /.crosspoint/bindery.json together with the version each was downloaded at.
 *
 * Every HTTPS download leaves the C3 heap more fragmented, and a few in a row
 * starve the next TLS handshake. So a run spans reboots: when the heap runs low
 * or a download fails, the run is saved to /.crosspoint/bindery-run.json and the
 * device silently restarts straight back into this activity (resuming = true)
 * with a clean heap. The SD fonts are unloaded for the run for the same reason
 * (the exit restart reloads them). /.crosspoint/bindery.log records each step, since USB-locked
 * devices have no serial console.
 */
class BinderySyncActivity final : public Activity {
 public:
  explicit BinderySyncActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool resuming = false)
      : Activity("BinderySync", renderer, mappedInput), resuming(resuming) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  // The sync runs synchronously inside onWifiSelectionComplete(); keep the
  // device awake on the result screens too so the summary stays readable.
  bool preventAutoSleep() override { return state != State::WIFI_SELECTION; }

 private:
  enum class State { WIFI_SELECTION, FETCHING, SYNCING, COMPLETE, ERROR };
  enum class Outcome { OK, LOW_HEAP, FAILED, CANCELLED };

  struct LibraryFile {
    std::string path;      // absolute SD path
    std::string download;  // server path, e.g. /api/koreader/books/<id>
    std::string version;
    std::string title;
  };

  struct TrackedFile {
    std::string path;
    std::string version;
  };

  struct Attempt {
    std::string path;
    int count = 0;
  };

  const bool resuming;
  State state = State::WIFI_SELECTION;
  std::string baseUrl;
  std::vector<LibraryFile> library;
  std::vector<TrackedFile> tracked;

  // Run state, carried across the heap-defrag reboots.
  int added = 0;
  int updated = 0;
  int removed = 0;
  int failed = 0;
  int restarts = 0;
  std::vector<Attempt> attempts;       // failed tries per file this run
  std::vector<std::string> abandoned;  // files given up on this run

  // Progress of the current boot's slice of the run.
  size_t currentIndex = 0;
  size_t downloadCount = 0;
  size_t fileProgress = 0;
  size_t fileTotal = 0;
  int sessionDownloads = 0;
  std::string currentTitle;
  bool cancelRequested = false;
  bool goHomeRequested = false;
  std::string errorMessage;

  void onWifiSelectionComplete(bool connected);
  void runSync();
  bool resolveBaseUrl();
  bool fetchLibrary();
  void loadTracked();
  bool saveTracked() const;
  void track(const std::string& path, const std::string& version);
  void untrack(const std::string& path);
  const TrackedFile* findTracked(const std::string& path) const;
  void removeUntracked();
  Outcome downloadOne(const LibraryFile& file);
  void finishWithError(const char* message);

  void loadRun();
  void saveRun() const;
  static void clearRun();
  int bumpAttempt(const std::string& path);
  bool isAbandoned(const std::string& path) const;
  void restartToContinue();
  static void startLog();
  static void log(const char* format, ...);
};
