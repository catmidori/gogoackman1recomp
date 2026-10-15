# Source preparation; Flatpak executes this once before CMake configuration.
get_filename_component(ACKMAN_PATCH_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(SNESRECOMP_ROOT "${ACKMAN_PATCH_SOURCE_ROOT}/external/snesrecomp")
find_package(Git REQUIRED)
file(STRINGS "${ACKMAN_PATCH_SOURCE_ROOT}/framework.lock" EXPECTED_REV LIMIT_COUNT 1)
execute_process(COMMAND "${GIT_EXECUTABLE}" -c "safe.directory=${SNESRECOMP_ROOT}" -C "${SNESRECOMP_ROOT}" rev-parse HEAD
  OUTPUT_VARIABLE ACTUAL_REV OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT ACTUAL_REV STREQUAL EXPECTED_REV)
  message(FATAL_ERROR "Framework revision differs from framework.lock")
endif()
# Keep git apply from reintroducing CRLF on hosts with core.autocrlf=true.
# These settings are scoped to this invocation; user/global Git config is intact.
set(ACKMAN_PATCH_GIT "${GIT_EXECUTABLE}" -c "safe.directory=${SNESRECOMP_ROOT}"
  -c core.autocrlf=false -c core.eol=lf -C "${SNESRECOMP_ROOT}")
# Unwind overlapping tail patches in reverse order before checking the stack.
foreach(ACKMAN_TAIL_PATCH ackman-osd-scale ackman-gameplay-devices ackman-unified-state ackman-graphics-coldboot ackman-safe-boot-raster ackman-png-shader-tree ackman-shaders-capture ackman-device-events ackman-rumble ackman-menu-loop ackman-mod-catalog-shell ackman-hold-rewind ackman-audio-load ackman-active-mods ackman-hotkeys ackman-controls ackman-av-settings ackman-dialog-pause ackman-autosave ackman-menu-settings)
set(ACKMAN_SETTINGS_PATCH "${ACKMAN_PATCH_SOURCE_ROOT}/patches/${ACKMAN_TAIL_PATCH}.patch")
execute_process(COMMAND ${ACKMAN_PATCH_GIT}
  apply --reverse --check "${ACKMAN_SETTINGS_PATCH}" RESULT_VARIABLE ACKMAN_SETTINGS_PRESENT
  OUTPUT_QUIET ERROR_QUIET)
if(ACKMAN_SETTINGS_PRESENT EQUAL 0)
  execute_process(COMMAND ${ACKMAN_PATCH_GIT}
    apply --reverse "${ACKMAN_SETTINGS_PATCH}" COMMAND_ERROR_IS_FATAL ANY)
endif()
endforeach()
foreach(ACKMAN_PATCH_NAME clocked-beam timing-crossing ackman-raster script-savestate ackman-state ackman-reset ackman-menu ackman-sdl-menu ackman-config ackman-system-shortcuts ackman-overlay ackman-alt-input ackman-obj-clamp ackman-menu-settings ackman-autosave ackman-dialog-pause ackman-av-settings ackman-controls ackman-hotkeys ackman-active-mods ackman-audio-load ackman-hold-rewind ackman-mod-catalog-shell ackman-menu-loop ackman-rumble ackman-device-events ackman-shaders-capture ackman-png-shader-tree ackman-safe-boot-raster ackman-graphics-coldboot ackman-unified-state ackman-gameplay-devices ackman-host-report-gnu ackman-osd-scale)
set(ACKMAN_BEAM_PATCH "${ACKMAN_PATCH_SOURCE_ROOT}/patches/${ACKMAN_PATCH_NAME}.patch")
execute_process(COMMAND ${ACKMAN_PATCH_GIT}
  apply --reverse --check "${ACKMAN_BEAM_PATCH}" RESULT_VARIABLE BEAM_PATCH_PRESENT
  OUTPUT_QUIET ERROR_QUIET)
if(NOT BEAM_PATCH_PRESENT EQUAL 0)
  execute_process(COMMAND ${ACKMAN_PATCH_GIT}
    apply --check "${ACKMAN_BEAM_PATCH}" COMMAND_ERROR_IS_FATAL ANY)
  execute_process(COMMAND ${ACKMAN_PATCH_GIT}
    apply "${ACKMAN_BEAM_PATCH}" COMMAND_ERROR_IS_FATAL ANY)
endif()
endforeach()
