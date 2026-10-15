include_guard(GLOBAL)
# Caller provides SDL3, Python3 and the SNESRecomp runner.cmake helpers.
# Sources/assets resolve against this module, not the caller's game directory.
get_filename_component(RECOMP_MENU_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
# Dear ImGui v1.91.9b, commit f5befd2d29e66809cd1110a152e375a7f1981f06.
# Local source checkout keeps production builds reproducible and offline.
set(IMGUI_ROOT "${RECOMP_MENU_ROOT}/external/imgui")
add_library(recomp_menu_imgui STATIC
  ${IMGUI_ROOT}/imgui.cpp ${IMGUI_ROOT}/imgui_draw.cpp
  ${IMGUI_ROOT}/imgui_tables.cpp ${IMGUI_ROOT}/imgui_widgets.cpp
  ${IMGUI_ROOT}/backends/imgui_impl_sdl3.cpp
  ${IMGUI_ROOT}/backends/imgui_impl_sdlrenderer3.cpp)
# Embedded SVG bytes: edits/additions rebuild the catalog; runtime is offline.
file(GLOB_RECURSE RECOMP_MENU_BUTTON_SVGS CONFIGURE_DEPENDS "${RECOMP_MENU_ROOT}/assets/buttons/*.svg")
set(RECOMP_MENU_BUTTON_CATALOG "${CMAKE_CURRENT_BINARY_DIR}/recomp_menu_button_assets.cpp")
# A copied SVG can be older than the generated catalog. The list manifest
# invalidates the catalog on additions/removals regardless of file timestamps.
set(RECOMP_MENU_BUTTON_LIST "${CMAKE_CURRENT_BINARY_DIR}/recomp_menu_button_list.txt")
file(GENERATE OUTPUT "${RECOMP_MENU_BUTTON_LIST}" CONTENT "${RECOMP_MENU_BUTTON_SVGS}")
add_custom_command(OUTPUT "${RECOMP_MENU_BUTTON_CATALOG}"
  COMMAND "${Python3_EXECUTABLE}" "${RECOMP_MENU_ROOT}/scripts/embed_buttons.py"
    "${RECOMP_MENU_ROOT}/assets/buttons" "${RECOMP_MENU_BUTTON_CATALOG}"
  DEPENDS ${RECOMP_MENU_BUTTON_SVGS} "${RECOMP_MENU_BUTTON_LIST}" "${RECOMP_MENU_ROOT}/scripts/embed_buttons.py"
  VERBATIM)
# Reusable SNES menu: C ABI, C backend/watchdog and C++ ImGui implementation.
add_library(recomp_menu_core STATIC
  "${RECOMP_MENU_ROOT}/src/recomp_menu_core.c" "${RECOMP_MENU_ROOT}/src/recomp_menu_core.h" "${RECOMP_MENU_ROOT}/src/recomp_menu_host.c" "${RECOMP_MENU_ROOT}/src/recomp_menu_host.h"
  "${RECOMP_MENU_ROOT}/src/recomp_menu_ui.cpp" "${RECOMP_MENU_ROOT}/src/recomp_menu_widgets.cpp" "${RECOMP_MENU_ROOT}/src/recomp_menu_icons.cpp"
  "${RECOMP_MENU_ROOT}/src/recomp_menu_cursor.c" "${RECOMP_MENU_ROOT}/src/recomp_menu_boot.c" "${RECOMP_MENU_ROOT}/src/recomp_menu_checkpoint.c"
  "${RECOMP_MENU_ROOT}/src/recomp_menu_controls.c" "${RECOMP_MENU_ROOT}/src/recomp_menu_hotkeys.c" "${RECOMP_MENU_ROOT}/src/recomp_menu_devices.c"
  "${RECOMP_MENU_ROOT}/src/recomp_menu_font.cpp" "${RECOMP_MENU_ROOT}/src/recomp_menu_svg.c" "${RECOMP_MENU_ROOT}/src/recomp_menu_metadata.c"
  "${RECOMP_MENU_ROOT}/src/recomp_menu_types.h" "${RECOMP_MENU_ROOT}/src/recomp_menu_snes.h" "${RECOMP_MENU_BUTTON_CATALOG}")
target_sources(recomp_menu_core PRIVATE
  "${RECOMP_MENU_ROOT}/src/recomp_menu_paths.c" "${RECOMP_MENU_ROOT}/src/recomp_menu_paths.h")
if(RECOMP_MENU_FLATPAK)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT RECOMP_MENU_FLATPAK_APP_ID OR NOT RECOMP_MENU_INSTALLED_ASSETS)
    message(FATAL_ERROR "Flatpak menu requires Linux, an application ID and the installed asset directory")
  endif()
  target_compile_definitions(recomp_menu_core PRIVATE RECOMP_MENU_FLATPAK=1
    RECOMP_MENU_FLATPAK_APP_ID="${RECOMP_MENU_FLATPAK_APP_ID}"
    RECOMP_MENU_INSTALLED_ASSETS="${RECOMP_MENU_INSTALLED_ASSETS}")
endif()
target_include_directories(recomp_menu_core PUBLIC "${RECOMP_MENU_ROOT}/src" "${SNESRECOMP_ROOT}/runner/src"
  PRIVATE "${RECOMP_MENU_ROOT}/external/nanosvg")
add_library(SNES::MenuCore ALIAS recomp_menu_core)
target_link_libraries(recomp_menu_core PUBLIC recomp_menu_imgui SDL3::SDL3)
snesrecomp_target_sdl(recomp_menu_core)
target_include_directories(recomp_menu_imgui PRIVATE "${RECOMP_MENU_ROOT}/external/nanosvg")
target_include_directories(recomp_menu_imgui PRIVATE "${RECOMP_MENU_ROOT}/src")
# Replace the public checkbox entry points for every menu consumer, including
# CheckboxFlags. Keep upstream sources untouched and retain stock internals.
set_source_files_properties("${IMGUI_ROOT}/imgui_widgets.cpp" PROPERTIES
  COMPILE_DEFINITIONS "Checkbox=RecompStockCheckbox;CheckboxFlags=RecompStockCheckboxFlags")
target_include_directories(recomp_menu_imgui PUBLIC ${IMGUI_ROOT} ${IMGUI_ROOT}/backends)
target_link_libraries(recomp_menu_imgui PUBLIC SDL3::SDL3)
# Keep the licensed Open Sans fallback available even when assets are missing.
set(RECOMP_MENU_FONT_ASSET "${RECOMP_MENU_ROOT}/assets/fonts/OpenSans-Regular.ttf")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${RECOMP_MENU_FONT_ASSET}")
file(READ "${RECOMP_MENU_FONT_ASSET}" RECOMP_MENU_FONT_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," RECOMP_MENU_FONT_BYTES "${RECOMP_MENU_FONT_HEX}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/recomp_menu_bundled_font.cpp"
  "#include <cstddef>\nstatic const unsigned char font[] = {${RECOMP_MENU_FONT_BYTES}};\nconst unsigned char *RecompMenuBundledFontData(std::size_t *size) { *size = sizeof(font); return font; }\n")
target_sources(recomp_menu_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/recomp_menu_bundled_font.cpp")

    if(WIN32)
      target_link_libraries(recomp_menu_core PRIVATE advapi32 user32)
    endif()
    if(APPLE)
      find_library(RECOMP_MENU_CORETEXT CoreText REQUIRED)
      find_library(RECOMP_MENU_COREFOUNDATION CoreFoundation REQUIRED)
      target_link_libraries(recomp_menu_core PRIVATE ${RECOMP_MENU_CORETEXT} ${RECOMP_MENU_COREFOUNDATION})
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      find_package(Fontconfig QUIET)
      if(Fontconfig_FOUND)
        target_link_libraries(recomp_menu_core PRIVATE Fontconfig::Fontconfig)
        target_compile_definitions(recomp_menu_core PRIVATE RECOMP_MENU_HAVE_FONTCONFIG=1)
      endif()
    endif()

