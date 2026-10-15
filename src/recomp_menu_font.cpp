#ifdef _WIN32
#include <windows.h>
#endif
#include "recomp_menu_font.h"
#include "recomp_menu_paths.h"
#include <SDL3/SDL.h>
#include <vector>
#ifdef __APPLE__
#include <CoreText/CoreText.h>

static std::string coretext_font() {
    CTFontRef font = CTFontCreateUIFontForLanguage(kCTFontUIFontMenuItem, 0, nullptr);
    if (!font) return {};
    CFTypeRef attribute = CTFontCopyAttribute(font, kCTFontURLAttribute);
    std::string path;
    if (attribute && CFGetTypeID(attribute) == CFURLGetTypeID()) {
        CFStringRef filename = CFURLCopyFileSystemPath(static_cast<CFURLRef>(attribute), kCFURLPOSIXPathStyle);
        if (filename) {
            CFIndex size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(filename), kCFStringEncodingUTF8) + 1;
            std::vector<char> bytes(static_cast<size_t>(size));
            if (CFStringGetCString(filename, bytes.data(), size, kCFStringEncodingUTF8)) path = bytes.data();
            CFRelease(filename);
        }
    }
    if (attribute) CFRelease(attribute);
    CFRelease(font);
    return path;
}
#endif
#ifdef RECOMP_MENU_HAVE_FONTCONFIG
#include <fontconfig/fontconfig.h>

static std::string fontconfig_font() {
    // Respect user aliases and distribution configuration, without global teardown.
    FcConfig *config = FcInitLoadConfigAndFonts();
    if (!config) return {};
    FcPattern *request = FcNameParse(reinterpret_cast<const FcChar8 *>("sans-serif"));
    std::string path;
    if (request) {
        FcPatternAddBool(request, FC_SCALABLE, FcTrue);
        FcPatternAddBool(request, FC_OUTLINE, FcTrue);
        if (FcConfigSubstitute(config, request, FcMatchPattern)) {
            FcDefaultSubstitute(request);
            FcResult result;
            FcPattern *match = FcFontMatch(config, request, &result);
            if (match) {
                FcChar8 *file = nullptr;
                if (FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch && file)
                    path = reinterpret_cast<const char *>(file);
                FcPatternDestroy(match);
            }
        }
        FcPatternDestroy(request);
    }
    FcConfigDestroy(config);
    return path;
}
#endif

static bool exists(const std::string &path) {
    SDL_PathInfo info;
    return SDL_GetPathInfo(path.c_str(), &info) && info.type == SDL_PATHTYPE_FILE;
}

#ifdef _WIN32
static std::string utf8(const wchar_t *value) {
    int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (!size) return {};
    std::vector<char> bytes(size);
    WideCharToMultiByte(CP_UTF8, 0, value, -1, bytes.data(), size, nullptr, nullptr);
    return bytes.data();
}
static std::string windows_font() {
    NONCLIENTMETRICSW metrics{}; metrics.cbSize = sizeof(metrics);
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) return {};
    wchar_t directory[MAX_PATH];
    if (!GetWindowsDirectoryW(directory, MAX_PATH)) return {};
    std::string fonts = utf8(directory) + "/Fonts/";
    HKEY key;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
                      0, KEY_READ, &key) != ERROR_SUCCESS) return {};
    std::string result;
    for (DWORD i = 0; ; ++i) {
        wchar_t name[512]{}, value[2048]{};
        DWORD name_count = 512, value_bytes = sizeof(value) - sizeof(wchar_t), type;
        LONG status = RegEnumValueW(key, i, name, &name_count, nullptr, &type,
                                   reinterpret_cast<BYTE *>(value), &value_bytes);
        if (status == ERROR_NO_MORE_ITEMS) break;
        if (status != ERROR_SUCCESS || type != REG_SZ) continue;
        std::wstring label(name);
        auto suffix = label.find(L" (");
        if (suffix != std::wstring::npos) label.resize(suffix);
        if (_wcsicmp(label.c_str(), metrics.lfMenuFont.lfFaceName)) continue;
        std::string file = utf8(value);
        std::string path = file.find(':') != std::string::npos ? file : fonts + file;
        if (exists(path)) { result = path; break; }
    }
    RegCloseKey(key);
    return result;
}
#endif

std::string RecompMenuFontPath(bool system_fonts) {
#ifdef RECOMP_MENU_FLATPAK
    (void)system_fonts;
    return std::string(RecompMenuFontDirectory()) + "OpenSans-Regular.ttf";
#else
    if (system_fonts) {
#ifdef _WIN32
        std::string path = windows_font();
        if (!path.empty()) return path;
#elif defined(__linux__)
#ifdef RECOMP_MENU_HAVE_FONTCONFIG
        std::string path = fontconfig_font();
        if (!path.empty() && exists(path)) return path;
#endif
        // Fontconfig absent or no usable match: immediately use bundled Open Sans.
#elif defined(__APPLE__)
        std::string path = coretext_font();
        if (!path.empty() && exists(path)) return path;
#endif
    }
    const char *base = RecompMenuFontDirectory();
    return std::string(base ? base : "./") + "OpenSans-Regular.ttf";
#endif
}
