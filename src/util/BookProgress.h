#pragma once

#include <string>

// Saved reading percentage, or -1 when unavailable. Never builds reading caches.
// For EPUBs, `chapter` (when given) receives the TOC title at the saved position.
int loadBookProgress(const std::string& path, std::string* chapter = nullptr);
