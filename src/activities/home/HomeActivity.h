#pragma once
#include <algorithm>
#include <functional>
#include <vector>

#include "./FileBrowserActivity.h"
#include "RecentBooksStore.h"
#include "activities/Activity.h"
#include "components/CoverGridHomeUi.h"
#include "util/ButtonNavigator.h"

struct Rect;

class HomeActivity final : public Activity {
  std::unique_ptr<CoverGridHomeUi> coverGridUi;
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;
  bool recentsLoading = false;
  bool recentsLoaded = false;
  bool firstRenderDone = false;
  bool hasOpdsServers = false;
  bool hasContinueReading = false;
  bool coverRendered = false;      // Track if cover has been rendered once
  bool coverBufferStored = false;  // Track if cover buffer is stored
  uint8_t* coverBuffer = nullptr;  // HomeActivity's own buffer for cover image
  size_t coverBufferSize = 0;      // Bytes allocated to coverBuffer
  // Logical rect last passed to drawRecentBookCover. The cover snapshot only
  // needs to cover this region, not the entire framebuffer, so we cache the
  // tile instead of all 48 KB. Set in render() before the call.
  int coverRectX = 0;
  int coverRectY = 0;
  int coverRectW = 0;
  int coverRectH = 0;
  std::vector<RecentBook> recentBooks;
  const HomeMenuItem initialMenuItem;
  const bool cleanInitialRefresh;

  // Menu rows below the recent books, in display order. Compact themes drop
  // OPDS Browser and File Transfer; OPDS also needs a configured server.
  static std::vector<HomeMenuItem> menuOrder(bool hasOpdsUrl, bool compact) {
    std::vector<HomeMenuItem> items = {HomeMenuItem::FILE_BROWSER, HomeMenuItem::LIBRARY, HomeMenuItem::BINDERY};
    if (!compact) {
      if (hasOpdsUrl) items.push_back(HomeMenuItem::OPDS_BROWSER);
      items.push_back(HomeMenuItem::FILE_TRANSFER);
    }
    items.push_back(HomeMenuItem::SETTINGS_MENU);
    return items;
  }

  // Convert HomeMenuItem to menu index (used in onEnter); a hidden item maps to the first row.
  static int menuItemToIndex(HomeMenuItem item, bool hasOpdsUrl, bool compact) {
    const auto items = menuOrder(hasOpdsUrl, compact);
    const auto it = std::find(items.begin(), items.end(), item);
    return it == items.end() ? 0 : static_cast<int>(it - items.begin());
  }

  // Convert menu index to HomeMenuItem (used in loop)
  static HomeMenuItem indexToMenuItem(int idx, bool hasOpdsUrl, bool compact) {
    const auto items = menuOrder(hasOpdsUrl, compact);
    return idx >= 0 && idx < static_cast<int>(items.size()) ? items[idx] : HomeMenuItem::NONE;
  }
  static bool compactMenu();
  void onSelectBook(const std::string& path);
  void onFileBrowserOpen();
  void onLibraryOpen();
  void onSettingsOpen();
  void onFileTransferOpen();
  void onOpdsBrowserOpen();
  void onBinderyOpen();

  int getMenuItemCount() const;
  bool storeCoverBuffer();    // Store frame buffer for cover image
  bool restoreCoverBuffer();  // Restore frame buffer from stored cover
  void freeCoverBuffer();     // Free the stored cover buffer
  void loadRecentBooks(int maxBooks);
  void loadRecentCovers(int coverHeight);
  void fillCoverGridFromLibrary();
  void resolveGridCoverPaths();
  void loadGridCover(RecentBook& book, int height, bool& showingLoading, Rect& popupRect);

 public:
  explicit HomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                        HomeMenuItem initialMenuItemValue = HomeMenuItem::NONE, bool cleanInitialRefresh = false)
      : Activity("Home", renderer, mappedInput),
        initialMenuItem(initialMenuItemValue),
        cleanInitialRefresh(cleanInitialRefresh) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isHomeActivity() const override { return true; }
};
