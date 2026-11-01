# Keep the script in the build directory alongside its install manifest.
configure_file("${CMAKE_CURRENT_LIST_DIR}/uninstall.cmake"
               "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" COPYONLY)
add_custom_target(uninstall
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    VERBATIM)
