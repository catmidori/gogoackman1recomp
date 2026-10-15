# Assets can be staged independently to verify the Linux install layout.
function(ackman_install_linux_assets source_root app_id data_dir)
  set(icon "${CMAKE_CURRENT_BINARY_DIR}/flatpak-icon/${app_id}.png")
  add_custom_command(OUTPUT "${icon}"
    COMMAND "${Python3_EXECUTABLE}" "${source_root}/scripts/build_flatpak_icon.py"
      "${source_root}/icon.png" "${icon}"
    DEPENDS "${source_root}/icon.png" "${source_root}/scripts/build_flatpak_icon.py"
    VERBATIM)
  add_custom_target(ackman_flatpak_icon ALL DEPENDS "${icon}")
  if(TARGET AckmanRecomp)
    add_dependencies(AckmanRecomp ackman_flatpak_icon)
  endif()
  install(FILES "${source_root}/${app_id}.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
  install(FILES "${icon}"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
  install(FILES "${source_root}/assets/fonts/OpenSans-Regular.ttf" "${source_root}/assets/fonts/OFL.txt"
    DESTINATION "${data_dir}/fonts")
  install(DIRECTORY "${source_root}/assets/buttons/" DESTINATION "${data_dir}/buttons"
    FILES_MATCHING PATTERN "*.svg")
  install(DIRECTORY "${source_root}/shaders/" DESTINATION "${data_dir}/shaders")
  install(FILES "${source_root}/README.md" DESTINATION "${data_dir}" RENAME readme.md)
  install(FILES "${source_root}/THIRD_PARTY.md"
    "${source_root}/THIRD_PARTY_ATTRIBUTION.md" "${source_root}/LICENSE"
    DESTINATION "${data_dir}")
  # Writable directories are created in XDG storage at first launch, never at
  # build time under the packager's HOME. Only clean templates are installed.
  foreach(folder docs mods rom saves shaders)
    install(FILES "${source_root}/${folder}/Instructions.txt"
      DESTINATION "${data_dir}/user-template/${folder}")
  endforeach()
  install(FILES "${source_root}/README.md" "${source_root}/THIRD_PARTY_ATTRIBUTION.md"
    "${source_root}/LICENSE" "${source_root}/RELEASE_NOTES.md"
    DESTINATION "${data_dir}/user-template")
endfunction()
