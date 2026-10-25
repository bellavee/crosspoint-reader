#pragma once

#include <string>

// Clears the reading cache for a book file if its extension is recognised
// (EPUB, XTC, or TXT). Does nothing for other file types.
void clearBookCache(const std::string& path);

// Returns true if the directory name matches a book cache entry.
bool isBookCacheDirectoryName(const char* name);

// Path of the book's cover thumb at `height` (EPUB or XTC), empty for other
// formats. Derives the same cache key as the Epub/Xtc constructors without
// building a parser object; the file may not exist.
std::string bookThumbPath(const std::string& path, int height);

// The book's cache folder (EPUB, TXT/Markdown or XTC), empty for other
// formats. Same cache key as the Epub/Xtc constructors, no parser built.
std::string bookCacheDir(const std::string& path);
// Delete a local book and its local clipping state after the file is removed.
bool removeBookFile(const std::string& path);
