#pragma once
#include <string>
#include <cstddef>
// Returns an installed system font, or the redistributable asset beside the executable.
std::string RecompMenuFontPath(bool system_fonts = true);
const unsigned char *RecompMenuBundledFontData(std::size_t *size);
