#include "BinderySyncActivity.h"

#include <ArduinoJson.h>
#include <Epub.h>
#include <FontCacheManager.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <KOReaderCredentialStore.h>
#include <LibraryBuilder.h>
#include <Logging.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdarg>
#include <cstring>

#include "MappedInputManager.h"
#include "SdCardFontSystem.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/reader/ProgressFile.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"
#include "util/BookCacheUtils.h"
#include "util/ReadingStatus.h"

namespace {
constexpr char BINDERY_DIR[] = "/Bindery";
constexpr char STATE_PATH[] = "/.crosspoint/bindery.json";
constexpr char RUN_PATH[] = "/.crosspoint/bindery-run.json";
constexpr char LOG_PATH[] = "/.crosspoint/bindery.log";
constexpr char LIBRARY_TMP[] = "/.crosspoint/bindery-library.tmp";
constexpr char LIBRARY_ENDPOINT[] = "/api/koreader/library";
constexpr char KOSYNC_SUFFIX[] = "/api/kosync";
constexpr char PART_SUFFIX[] = ".part";
// progress.bin is a few dozen bytes; anything larger is not ours to carry over.
constexpr size_t MAX_PROGRESS_BYTES = 256;
constexpr int PROGRESS_STEP_PERCENT = 10;
constexpr unsigned long PROGRESS_MIN_UPDATE_MS = 5000;
// A file gets this many tries per run, each on a fresh heap, before it is
// reported as failed.
constexpr int MAX_ATTEMPTS_PER_FILE = 2;
// Hard stop for a run that keeps rebooting without finishing.
constexpr int MAX_RESTARTS_PER_RUN = 40;
// Below this, a further TLS session would likely fail mid-handshake; reboot
// for a clean heap first. Comfortably above HttpDownloader's own floor.
constexpr uint32_t COMFORT_FREE_HEAP = HttpDownloader::MIN_TLS_FREE_HEAP + 20000;
constexpr uint32_t COMFORT_MAX_ALLOC = HttpDownloader::MIN_TLS_MAX_ALLOC + 12000;

// A path component from the server: one plain name, never a traversal.
bool isSafeName(const char* name) {
  if (name == nullptr || *name == '\0' || *name == '.') return false;
  return std::strchr(name, '/') == nullptr && std::strchr(name, '\\') == nullptr;
}

bool isReadableBook(const std::string& path) {
  return FsHelpers::hasEpubExtension(path) || FsHelpers::hasTxtExtension(path) || FsHelpers::hasXtcExtension(path);
}

bool heapIsComfortable() {
  return ESP.getFreeHeap() >= COMFORT_FREE_HEAP && ESP.getMaxAllocHeap() >= COMFORT_MAX_ALLOC;
}

// Rewrites a replaced book's cache but keeps where the reader was: the layout
// caches belong to the old file, the saved position still means the same
// chapter (bellasekai only appends or retranslates chapters).
void resetBookCacheKeepingProgress(const std::string& path) {
  if (!FsHelpers::hasEpubExtension(path)) {
    clearBookCache(path);
    return;
  }
  const Epub epub(path, "/.crosspoint");
  const std::string& cachePath = epub.getCachePath();
  uint8_t progress[MAX_PROGRESS_BYTES];
  size_t progressLen = 0;
  {
    HalFile file;
    if (Storage.openFileForRead("BINDERY", cachePath + "/progress.bin", file)) {
      const int read = file.read(progress, sizeof(progress));
      progressLen = read > 0 && file.available() == 0 ? static_cast<size_t>(read) : 0;
    }
  }
  epub.clearCache();
  if (progressLen > 0 && Storage.mkdir(cachePath.c_str())) {
    ProgressFile::writeAtomic(cachePath, progress, progressLen);
  }
}

// Write-then-rename so a power cut never leaves a torn JSON file.
bool writeJsonAtomic(const char* path, const JsonDocument& doc) {
  const std::string tmp = std::string(path) + ".tmp";
  {
    HalFile file;
    if (!Storage.openFileForWrite("BINDERY", tmp, file)) return false;
    if (serializeJson(doc, file) == 0) return false;
  }
  Storage.remove(path);
  return Storage.rename(tmp.c_str(), path);
}
}  // namespace

// --- Log (USB-locked devices have no serial console) ---

void BinderySyncActivity::startLog() { Storage.remove(LOG_PATH); }

void BinderySyncActivity::log(const char* format, ...) {
  char line[192];
  va_list args;
  va_start(args, format);
  const int len = vsnprintf(line, sizeof(line) - 1, format, args);
  va_end(args);
  if (len <= 0) return;
  const size_t used = std::min(static_cast<size_t>(len), sizeof(line) - 2);
  line[used] = '\n';
  LOG_INF("BINDERY", "%.*s", static_cast<int>(used), line);
  HalFile file = Storage.open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND);
  if (file) file.write(line, used + 1);
}

// --- Lifecycle ---

void BinderySyncActivity::onEnter() {
  Activity::onEnter();
  state = State::WIFI_SELECTION;

  Storage.mkdir("/.crosspoint");
  if (resuming) {
    loadRun();
    log("resume #%d (heap %u, block %u)", restarts, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  } else {
    clearRun();
    startLog();
    log("start (heap %u, block %u)", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  }

  if (!resolveBaseUrl()) {
    requestUpdate();
    return;
  }

  WiFi.mode(WIFI_STA);
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void BinderySyncActivity::onExit() {
  Activity::onExit();
  library.clear();
  tracked.clear();

  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void BinderySyncActivity::onWifiSelectionComplete(const bool connected) {
  if (!connected) {
    log("wifi not connected");
    clearRun();
    finish();
    return;
  }
  runSync();
}

void BinderySyncActivity::finishWithError(const char* message) {
  log("error: %s", message);
  clearRun();
  RenderLock lock(*this);
  errorMessage = message;
  state = State::ERROR;
}

// --- Server ---

bool BinderySyncActivity::resolveBaseUrl() {
  if (!KOREADER_STORE.hasCredentials() || KOREADER_STORE.getServerUrl().empty()) {
    finishWithError(tr(STR_BINDERY_NOT_CONFIGURED));
    return false;
  }
  std::string url = KOREADER_STORE.getBaseUrl();
  const size_t suffixLen = sizeof(KOSYNC_SUFFIX) - 1;
  if (url.size() <= suffixLen || url.compare(url.size() - suffixLen, suffixLen, KOSYNC_SUFFIX) != 0) {
    finishWithError(tr(STR_BINDERY_NOT_CONFIGURED));
    return false;
  }
  url.resize(url.size() - suffixLen);
  baseUrl = std::move(url);
  return true;
}

bool BinderySyncActivity::fetchLibrary() {
  // Through a temp file, like the font manifest: the TLS session is closed and
  // its buffers freed before the JSON document is built.
  const std::string url = baseUrl + LIBRARY_ENDPOINT;
  const auto result = HttpDownloader::downloadToFile(url, LIBRARY_TMP, nullptr, nullptr, KOREADER_STORE.getUsername(),
                                                     KOREADER_STORE.getPassword());
  if (result != HttpDownloader::OK) {
    log("library fetch failed: %d", static_cast<int>(result));
    Storage.remove(LIBRARY_TMP);
    return false;
  }

  JsonDocument doc;
  DeserializationError err;
  {
    HalFile file;
    if (!Storage.openFileForRead("BINDERY", LIBRARY_TMP, file)) {
      Storage.remove(LIBRARY_TMP);
      return false;
    }
    JsonDocument filter;
    filter["books"][0]["download"] = true;
    filter["books"][0]["filename"] = true;
    filter["books"][0]["folder"] = true;
    filter["books"][0]["version"] = true;
    filter["books"][0]["title"] = true;
    err = deserializeJson(doc, file, DeserializationOption::Filter(filter));
  }
  Storage.remove(LIBRARY_TMP);
  if (err) {
    log("library parse error: %s", err.c_str());
    return false;
  }

  // No "books" array means a malformed answer, not an empty library: never
  // treat it as "delete everything".
  JsonArrayConst books = doc["books"].as<JsonArrayConst>();
  if (books.isNull()) {
    log("library response has no books array");
    return false;
  }

  library.clear();
  library.reserve(books.size());
  for (JsonObjectConst book : books) {
    const char* filename = book["filename"] | "";
    const char* folder = book["folder"] | "";
    const char* download = book["download"] | "";
    if (!isSafeName(filename) || (*folder != '\0' && !isSafeName(folder)) || download[0] != '/') {
      log("skip malformed entry: %s", filename);
      continue;
    }
    LibraryFile file;
    file.path.reserve(sizeof(BINDERY_DIR) + std::strlen(folder) + std::strlen(filename) + 2);
    file.path = BINDERY_DIR;
    if (*folder != '\0') file.path.append("/").append(folder);
    file.path.append("/").append(filename);
    // PDFs and other documents the plugin syncs to KOReader cannot open here.
    if (!isReadableBook(file.path)) continue;
    file.download = download;
    file.version = book["version"] | "";
    file.title = book["title"] | filename;
    library.push_back(std::move(file));
  }
  log("library: %u readable of %u", static_cast<unsigned>(library.size()), static_cast<unsigned>(books.size()));
  return true;
}

// --- Tracked files (what this activity downloaded) ---

void BinderySyncActivity::loadTracked() {
  tracked.clear();
  HalFile file;
  if (!Storage.openFileForRead("BINDERY", STATE_PATH, file)) return;
  JsonDocument doc;
  if (deserializeJson(doc, file)) {
    log("unreadable %s; starting fresh", STATE_PATH);
    return;
  }
  JsonObjectConst files = doc["files"].as<JsonObjectConst>();
  tracked.reserve(files.size());
  for (JsonPairConst entry : files) {
    tracked.push_back(TrackedFile{entry.key().c_str(), entry.value() | ""});
  }
}

bool BinderySyncActivity::saveTracked() const {
  JsonDocument doc;
  JsonObject files = doc["files"].to<JsonObject>();
  for (const auto& entry : tracked) files[entry.path] = entry.version;
  return writeJsonAtomic(STATE_PATH, doc);
}

const BinderySyncActivity::TrackedFile* BinderySyncActivity::findTracked(const std::string& path) const {
  for (const auto& entry : tracked) {
    if (entry.path == path) return &entry;
  }
  return nullptr;
}

void BinderySyncActivity::track(const std::string& path, const std::string& version) {
  for (auto& entry : tracked) {
    if (entry.path == path) {
      entry.version = version;
      saveTracked();
      return;
    }
  }
  tracked.push_back(TrackedFile{path, version});
  saveTracked();
}

void BinderySyncActivity::untrack(const std::string& path) {
  for (auto it = tracked.begin(); it != tracked.end(); ++it) {
    if (it->path == path) {
      tracked.erase(it);
      saveTracked();
      return;
    }
  }
}

void BinderySyncActivity::removeUntracked() {
  // Copy the paths first: untrack() edits `tracked` while we walk it.
  std::vector<std::string> gone;
  for (const auto& entry : tracked) {
    bool inLibrary = false;
    for (const auto& file : library) {
      if (file.path == entry.path) {
        inLibrary = true;
        break;
      }
    }
    if (!inLibrary) gone.push_back(entry.path);
  }
  for (const auto& path : gone) {
    if (Storage.exists(path.c_str()) && !Storage.remove(path.c_str())) {
      log("could not delete %s", path.c_str());
      continue;
    }
    clearBookCache(path);
    untrack(path);
    removed++;
    log("removed %s", path.c_str());
  }
}

// --- Run state across reboots ---

void BinderySyncActivity::loadRun() {
  HalFile file;
  if (!Storage.openFileForRead("BINDERY", RUN_PATH, file)) return;
  JsonDocument doc;
  if (deserializeJson(doc, file)) return;
  added = doc["added"] | 0;
  updated = doc["updated"] | 0;
  removed = doc["removed"] | 0;
  failed = doc["failed"] | 0;
  restarts = doc["restarts"] | 0;
  attempts.clear();
  for (JsonPairConst entry : doc["attempts"].as<JsonObjectConst>()) {
    attempts.push_back(Attempt{entry.key().c_str(), entry.value() | 0});
  }
  abandoned.clear();
  for (JsonVariantConst path : doc["abandoned"].as<JsonArrayConst>()) {
    abandoned.emplace_back(path | "");
  }
}

void BinderySyncActivity::saveRun() const {
  JsonDocument doc;
  doc["added"] = added;
  doc["updated"] = updated;
  doc["removed"] = removed;
  doc["failed"] = failed;
  doc["restarts"] = restarts;
  JsonObject tries = doc["attempts"].to<JsonObject>();
  for (const auto& entry : attempts) tries[entry.path] = entry.count;
  JsonArray gaveUp = doc["abandoned"].to<JsonArray>();
  for (const auto& path : abandoned) gaveUp.add(path);
  writeJsonAtomic(RUN_PATH, doc);
}

void BinderySyncActivity::clearRun() { Storage.remove(RUN_PATH); }

int BinderySyncActivity::bumpAttempt(const std::string& path) {
  for (auto& entry : attempts) {
    if (entry.path == path) return ++entry.count;
  }
  attempts.push_back(Attempt{path, 1});
  return 1;
}

bool BinderySyncActivity::isAbandoned(const std::string& path) const {
  for (const auto& entry : abandoned) {
    if (entry == path) return true;
  }
  return false;
}

void BinderySyncActivity::restartToContinue() {
  restarts++;
  log("restart for a fresh heap (#%d, heap %u, block %u)", restarts, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  saveRun();
  WiFi.disconnect(false);
  delay(30);
  silentRestartToBindery();
}

// --- Sync ---

BinderySyncActivity::Outcome BinderySyncActivity::downloadOne(const LibraryFile& file) {
  const size_t slash = file.path.rfind('/');
  const std::string dir = file.path.substr(0, slash);
  if (!Storage.exists(dir.c_str()) && !Storage.mkdir(dir.c_str())) {
    log("mkdir failed: %s", dir.c_str());
    return Outcome::FAILED;
  }

  if (auto* fcm = renderer.getFontCacheManager()) fcm->releaseSdFontCaches();
  const uint32_t heap = ESP.getFreeHeap();
  const uint32_t block = ESP.getMaxAllocHeap();
  if (heap < HttpDownloader::MIN_TLS_FREE_HEAP || block < HttpDownloader::MIN_TLS_MAX_ALLOC) {
    log("low heap before %s (heap %u, block %u)", file.path.c_str(), heap, block);
    return Outcome::LOW_HEAP;
  }

  // Download beside the old copy and swap only once complete: a failed or
  // cancelled update leaves the book that was already readable.
  const std::string part = file.path + PART_SUFFIX;
  int lastRenderedPercent = -1;
  unsigned long lastRenderMs = 0;
  const unsigned long startMs = millis();
  const auto result = HttpDownloader::downloadToFile(
      baseUrl + file.download, part,
      [this, &lastRenderedPercent, &lastRenderMs](const size_t downloaded, const size_t total) {
        fileProgress = downloaded;
        fileTotal = total;
        // The loop is blocked for the transfer; pump input so Back can cancel.
        mappedInput.update(true);
        if (mappedInput.wasReleased(MappedInputManager::Button::Back)) cancelRequested = true;
        if (mappedInput.wasHomeGesture()) {
          cancelRequested = true;
          goHomeRequested = true;
        }
        // Each repaint is an e-ink refresh: only on visible progress steps.
        const int percent = total > 0 ? static_cast<int>(static_cast<uint64_t>(downloaded) * 100 / total) : 0;
        const unsigned long now = millis();
        if (percent >= 100 || lastRenderedPercent < 0 || percent >= lastRenderedPercent + PROGRESS_STEP_PERCENT ||
            now - lastRenderMs >= PROGRESS_MIN_UPDATE_MS) {
          lastRenderedPercent = percent;
          lastRenderMs = now;
          requestUpdate(true);
        }
      },
      &cancelRequested, KOREADER_STORE.getUsername(), KOREADER_STORE.getPassword());
  log("%s: result %d, %u/%u bytes, %lus (heap %u/%u, block %u/%u before/after)", file.path.c_str(),
      static_cast<int>(result), static_cast<unsigned>(fileProgress), static_cast<unsigned>(fileTotal),
      (millis() - startMs) / 1000, heap, ESP.getFreeHeap(), block, ESP.getMaxAllocHeap());
  if (result == HttpDownloader::ABORTED) return Outcome::CANCELLED;
  if (result != HttpDownloader::OK) return Outcome::FAILED;

  const bool replacing = Storage.exists(file.path.c_str());
  if (replacing && !Storage.remove(file.path.c_str())) {
    log("could not replace %s", file.path.c_str());
    Storage.remove(part.c_str());
    return Outcome::FAILED;
  }
  if (!Storage.rename(part.c_str(), file.path.c_str())) {
    log("rename failed: %s", file.path.c_str());
    Storage.remove(part.c_str());
    return Outcome::FAILED;
  }
  if (replacing) {
    resetBookCacheKeepingProgress(file.path);
    updated++;
  } else {
    clearBookCache(file.path);
    added++;
  }
  track(file.path, file.version);
  return Outcome::OK;
}

void BinderySyncActivity::runSync() {
  {
    RenderLock lock(*this);
    // Every way out from here restarts the device (onExit, restartToContinue)
    // and the boot loads the SD fonts again. Until then they only hold heap
    // the 16KB TLS records need: with a CJK UI font loaded, downloads died
    // mid-stream. The sync screens draw with the built-in fonts.
    const uint32_t heapBefore = ESP.getFreeHeap();
    sdFontSystem.unloadAll(renderer);
    log("sd fonts unloaded (heap %u -> %u, block %u)", heapBefore, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    state = State::FETCHING;
  }
  requestUpdateAndWait();

  if (auto* fcm = renderer.getFontCacheManager()) fcm->releaseSdFontCaches();
  if (!fetchLibrary()) {
    finishWithError(tr(STR_BINDERY_FETCH_FAILED));
    return;
  }

  loadTracked();
  removeUntracked();

  // Indices of files that are missing or at an older version.
  std::vector<size_t> pending;
  pending.reserve(library.size());
  for (size_t i = 0; i < library.size(); i++) {
    if (isAbandoned(library[i].path)) continue;
    const auto* known = findTracked(library[i].path);
    if (!known || known->version != library[i].version || !Storage.exists(library[i].path.c_str())) {
      pending.push_back(i);
    }
  }
  log("pending: %u", static_cast<unsigned>(pending.size()));

  {
    RenderLock lock(*this);
    state = State::SYNCING;
    downloadCount = pending.size();
    currentIndex = 0;
  }

  for (const size_t index : pending) {
    if (cancelRequested) break;
    const LibraryFile& file = library[index];

    // A finished transfer leaves the heap fragmented; start the next one on a
    // clean heap rather than let it fail half way.
    if (sessionDownloads > 0 && !heapIsComfortable() && restarts < MAX_RESTARTS_PER_RUN) {
      restartToContinue();
      return;
    }

    {
      RenderLock lock(*this);
      currentTitle = file.title;
      fileProgress = 0;
      fileTotal = 0;
    }
    requestUpdateAndWait();

    const Outcome outcome = downloadOne(file);
    if (outcome == Outcome::OK) {
      sessionDownloads++;
    } else if (outcome != Outcome::CANCELLED) {
      // Low heap is not the file's fault; any other failure counts a try.
      const int tries = outcome == Outcome::LOW_HEAP ? 0 : bumpAttempt(file.path);
      if (tries >= MAX_ATTEMPTS_PER_FILE || restarts >= MAX_RESTARTS_PER_RUN) {
        log("giving up on %s after %d tries", file.path.c_str(), tries);
        abandoned.push_back(file.path);
        failed++;
      } else {
        // Retry on a clean heap: most failures here are TLS running short.
        restartToContinue();
        return;
      }
    }
    currentIndex++;
  }

  if (added + updated + removed > 0) library::markLibraryIndexDirty();
  log("done: +%d ~%d -%d !%d, %d restarts", added, updated, removed, failed, restarts);
  clearRun();
  reading_status::recordLibrarySync(static_cast<int>(tracked.size()));

  if (goHomeRequested) {
    onGoHome();
    return;
  }
  RenderLock lock(*this);
  state = State::COMPLETE;
}

// --- Input & rendering ---

void BinderySyncActivity::loop() {
  if (state != State::COMPLETE && state != State::ERROR) return;
  int x = 0;
  int y = 0;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm) || mappedInput.wasScreenTapped(x, y)) {
    onGoHome();
  }
}

void BinderySyncActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const auto centerY = (pageHeight - lineHeight) / 2;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BINDERY));

  MappedInputManager::Labels labels = mappedInput.mapLabels("", "", "", "");
  char line[96];
  switch (state) {
    case State::WIFI_SELECTION:
    case State::FETCHING:
      renderer.drawCenteredText(UI_10_FONT_ID, centerY, tr(STR_BINDERY_FETCHING));
      break;
    case State::SYNCING: {
      snprintf(line, sizeof(line), tr(STR_BINDERY_PROGRESS_FORMAT), static_cast<int>(currentIndex + 1),
               static_cast<int>(downloadCount));
      renderer.drawCenteredText(UI_10_FONT_ID, centerY - 2 * lineHeight, line);
      renderer.drawCenteredText(UI_10_FONT_ID, centerY - lineHeight, currentTitle.c_str());
      const size_t percent = fileTotal > 0 ? fileProgress * 100 / fileTotal : 0;
      GUI.drawProgressBar(renderer,
                          Rect{metrics.contentSidePadding, centerY + metrics.verticalSpacing,
                               pageWidth - metrics.contentSidePadding * 2, metrics.progressBarHeight},
                          percent, 100);
      labels = mappedInput.mapLabels(tr(STR_CANCEL), "", "", "");
      break;
    }
    case State::COMPLETE:
      renderer.drawCenteredText(UI_10_FONT_ID, centerY - lineHeight, tr(STR_BINDERY_DONE), true, EpdFontFamily::BOLD);
      snprintf(line, sizeof(line), tr(STR_BINDERY_SUMMARY_FORMAT), added, updated, removed);
      renderer.drawCenteredText(UI_10_FONT_ID, centerY + metrics.verticalSpacing, line);
      if (failed > 0) {
        snprintf(line, sizeof(line), tr(STR_BINDERY_FAILED_FORMAT), failed);
        renderer.drawCenteredText(UI_10_FONT_ID, centerY + metrics.verticalSpacing + lineHeight, line);
      }
      labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
      break;
    case State::ERROR:
      renderer.drawCenteredText(UI_10_FONT_ID, centerY - lineHeight, tr(STR_ERROR_MSG), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, centerY + metrics.verticalSpacing, errorMessage.c_str());
      labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
      break;
  }
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
