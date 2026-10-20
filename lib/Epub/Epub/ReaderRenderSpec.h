#pragma once
#include <cstdint>

// Paragraph spacing modes, stored in CrossPointSettings::extraParagraphSpacing.
enum ParagraphSpacingMode : uint8_t {
  PARAGRAPH_SPACING_OFF = 0,
  PARAGRAPH_SPACING_EXTRA = 1,  // half a line after every paragraph
  PARAGRAPH_SPACING_HALF = 2,   // `p + p { margin-top: .5em }`
  PARAGRAPH_SPACING_COUNT
};

// First-line indent modes, stored in CrossPointSettings::paragraphIndent. Negative (hanging)
// CSS indents are kept in every mode.
enum ParagraphIndentMode : uint8_t {
  PARAGRAPH_INDENT_BOOK = 0,  // CSS text-indent, else three spaces
  PARAGRAPH_INDENT_OFF = 1,
  PARAGRAPH_INDENT_1EM = 2,
  PARAGRAPH_INDENT_1_5EM = 3,
  PARAGRAPH_INDENT_2EM = 4,
  PARAGRAPH_INDENT_COUNT
};

// The resolved text-rendering configuration a reader hands to the layout
// engine. Section-cache validation keys on every field: a section file built
// with a different spec is discarded and rebuilt.
//
// Build one via CrossPointSettings::readerRenderSpec(width, height), which
// fills every field: the settings-derived ones from the store, the viewport
// from the caller. Taking the viewport as arguments is what keeps a spec from
// existing in a half-filled state — the 0 defaults below are a last-resort
// backstop (a 0x0 viewport lays out nothing), not an invitation to omit it.
struct ReaderRenderSpec {
  int fontId = 0;
  float lineCompression = 1.0f;
  uint8_t extraParagraphSpacing = PARAGRAPH_SPACING_OFF;
  uint8_t paragraphIndent = PARAGRAPH_INDENT_BOOK;
  int8_t characterSpacing = 0;
  uint8_t wordSpacingPercent = 100;
  uint8_t paragraphAlignment = 0;
  uint16_t viewportWidth = 0;
  uint16_t viewportHeight = 0;
  bool hyphenationEnabled = false;
  bool embeddedStyle = true;
  uint8_t imageRendering = 0;
  bool focusReadingEnabled = false;
};
