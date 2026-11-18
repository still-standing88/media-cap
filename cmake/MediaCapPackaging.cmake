if(WIN32)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(mc_shared false)
    if(BUILD_SHARED_LIBS)
        set(mc_shared true)
    endif()
    set(mc_flex_arg)
    if(MEDIACAP_FLEXAUDIO AND NOT BUILD_SHARED_LIBS)
        set(mc_flex_arg "--rust-archive \"${PROJECT_BINARY_DIR}/src/mediacap_flexaudio.lib\"")
    endif()
    # Evaluated at install time so --prefix overrides and multi-config installs work.
    install(CODE "
      if(CMAKE_INSTALL_CONFIG_NAME STREQUAL \"Debug\")
        set(mc_dep_dir \"${ffmpeg_debug_dir}\")
        set(mc_debug true)
      else()
        set(mc_dep_dir \"${ffmpeg_release_dir}\")
        set(mc_debug false)
      endif()
      execute_process(COMMAND \"${Python3_EXECUTABLE}\" \"${PROJECT_SOURCE_DIR}/scripts/win/install_metadata.py\"
        --prefix \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\" --libdir \"${CMAKE_INSTALL_LIBDIR}\"
        --bindir \"${CMAKE_INSTALL_BINDIR}\" --includedir \"${CMAKE_INSTALL_INCLUDEDIR}\"
        --datadir \"${CMAKE_INSTALL_DATADIR}\" --dependency-dir \"\${mc_dep_dir}\"
        --debug \"\${mc_debug}\" --shared ${mc_shared} ${mc_flex_arg}
        COMMAND_ERROR_IS_FATAL ANY)
    ")
else()
    install(FILES LICENSE COPYING DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/MediaCap)
    install(FILES THIRD_PARTY_NOTICES.md OPTIONAL DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/MediaCap)
    install(DIRECTORY licenses/ OPTIONAL DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/MediaCap/dependencies)
endif()
