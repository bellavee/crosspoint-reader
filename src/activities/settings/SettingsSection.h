#pragma once

#include <I18n.h>

#include <cstddef>
#include <cstdint>

// Top-level groups on the Settings screen, in display order. End doubles as
// "no section" where one is optional.
enum class SettingsSection : uint8_t { General, Display, Reader, Controls, Library, Network, End };

inline constexpr size_t SETTINGS_SECTION_COUNT = static_cast<size_t>(SettingsSection::End);

// One-line summaries under the section titles (themes with settingsSectionHints).
inline constexpr StrId SETTINGS_SECTION_HINTS[SETTINGS_SECTION_COUNT] = {
    StrId::STR_SETTINGS_GENERAL_HINT,  StrId::STR_SETTINGS_DISPLAY_HINT, StrId::STR_SETTINGS_READER_HINT,
    StrId::STR_SETTINGS_CONTROLS_HINT, StrId::STR_SETTINGS_LIBRARY_HINT, StrId::STR_SETTINGS_NETWORK_HINT,
};

inline constexpr StrId SETTINGS_SECTION_TITLES[SETTINGS_SECTION_COUNT] = {
    StrId::STR_SETTINGS_GENERAL, StrId::STR_CAT_DISPLAY, StrId::STR_CAT_READER,
    StrId::STR_CAT_CONTROLS,     StrId::STR_LIBRARY,     StrId::STR_SETTINGS_NETWORK,
};
