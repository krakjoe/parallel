
set(TARGET_ARCH_DETECT_CODE "
#if defined(_M_ARM)
#  error cmake_arch ARM
#elif defined(_M_ARM64)
#  error cmake_arch ARM64
#elif defined(_M_AMD64)
#  error cmake_arch x86_64
#elif defined(_M_X64)
#  error cmake_arch x64
#elif defined(_M_IX86)
#  error cmake_arch x86
#elif defined(__MINGW64__) || defined(__x86_64__)
/* NOTE: MinGW64 gcc defined BOTH __MINGW32__ and __MINGW64__, so order
   is important. */
#  error cmake_arch x64
#elif defined(__MINGW32__)
#  error cmake_arch x86
#else
#error cmake_arch unknown
#endif
")

function(get_target_arch out)

    file(WRITE 
        "${CMAKE_BINARY_DIR}/target_arch_detect.c"
        "${TARGET_ARCH_DETECT_CODE}")

    try_run(
        run_result_unused compile_result_unused
        "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/target_arch_detect.c"
        COMPILE_OUTPUT_VARIABLE TARGET_ARCH)

    # parse compiler output
    string(REGEX MATCH "cmake_arch ([a-zA-Z0-9_]+)" TARGET_ARCH "${TARGET_ARCH}")
    string(REPLACE "cmake_arch " "" TARGET_ARCH "${TARGET_ARCH}")

    set(${out} "${TARGET_ARCH}" PARENT_SCOPE)

endfunction()
