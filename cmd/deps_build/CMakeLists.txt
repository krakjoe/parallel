cmake_minimum_required(VERSION 3.14)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_SOURCE_DIR}/cmake")

include (CheckIncludeFile)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckTypeSize)
include (config_h_in)
include (get_version)

project(pthreads4w VERSION ${PTHREADS4W_VERSION} LANGUAGES C CXX)

set(PTW32_VER ${PROJECT_VERSION_MAJOR}${EXTRAVERSION})

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "MinSizeRel" CACHE STRING "Choose the type of build, options are: Debug, Release, or MinSizeRel." FORCE)
    message(STATUS "No build type specified, defaulting to MinSizeRel.")
endif()

message(STATUS "")
message(STATUS "")
message(STATUS "Generator ......... ${CMAKE_GENERATOR}")
if(NOT CMAKE_CONFIGURATION_TYPES)
    set(the_build_type ${CMAKE_BUILD_TYPE})
else ()
    set(the_build_type "Generator-defined, one of ${CMAKE_CONFIGURATION_TYPES}")
endif ()
message(STATUS "Build Type ........ ${the_build_type}")
message(STATUS "Version ........... ${PTHREADS4W_VERSION}")

## Deprecated
## set(CMAKE_DEBUG_POSTFIX d)

# Uncomment this if config.h defines RETAIN_WSALASTERROR
#set(XLIBS wsock32.lib)


# Initial include path set. Look in the build directory first, where the
# generated config.h resides, before looking in the source directory.
include_directories("${CMAKE_BINARY_DIR}/" "${CMAKE_SOURCE_DIR}/")

#################################
# Target Arch                   #
#################################
include (target_arch)

get_target_arch(TARGET_ARCH)
set(build_defines "")
if(${TARGET_ARCH} STREQUAL "ARM")
    set(build_defines __PTW32_ARCHARM _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE=1)
elseif(${TARGET_ARCH} STREQUAL "ARM64")
    set(build_defines __PTW32_ARCHARM64 _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE=1)
elseif(${TARGET_ARCH} STREQUAL "x86_64")
    set(build_defines __PTW32_ARCHAMD64)
elseif(${TARGET_ARCH} STREQUAL "x86")
    set(build_defines __PTW32_ARCHX86)
elseif(${TARGET_ARCH} STREQUAL "x64")
    set(build_defines __PTW32_ARCHX64)
else()
    MESSAGE(ERROR "\"${TARGET_ARCH}\" not supported in version.rc")
endif()

message(STATUS "Target ............ ${TARGET_ARCH}")
set(CMAKE_C_LIBRARY_ARCHITECTURE ${TARGET_ARCH})
set(CMAKE_CXX_LIBRARY_ARCHITECTURE ${TARGET_ARCH})
set(CMAKE_LIBRARY_ARCHITECTURE ${TARGET_ARCH})

if(MSVC)
    message(STATUS "MSVC Version ...... ${MSVC_VERSION}")
else()
    message(STATUS "C Compiler ID ..... ${CMAKE_C_COMPILER_ID}")
endif()

#################################
# Install Path                  #
#################################
if (DIST_ROOT)
  set(CMAKE_INSTALL_PREFIX "${DIST_ROOT}")
elseif (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/PTHREADS-BUILT" CACHE PATH "Install path prefix (default)" FORCE)
endif()

message(STATUS "Installing to ${CMAKE_INSTALL_PREFIX}")

message(STATUS "")
message(STATUS "")

#################################
# configuration features    #
#################################
check_include_file(errno.h HAVE_ERRNO_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(memory.h HAVE_MEMORY_H)
check_include_file(signal.h HAVE_SIGNAL_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(stdlib.h HAVE_STDLIB_H)
check_include_file(strings.h HAVE_STRINGS_H)
check_include_file(string.h HAVE_STRING_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(time.h HAVE_TIME_H)
check_include_file(unistd.h HAVE_UNISTD_H)
check_function_exists(calloc HAVE_CALLOC)
check_type_size(sigset_t HAVE_SIGSET_T)
check_type_size("struct timespec" HAVE_STRUCT_TIMESPEC)
check_symbol_exists(_beginthreadex process.h HAVE__BEGINTHREADEX)
check_symbol_exists(DuplicateHandle windows.h HAVE_DUPLICATEHANDLE)

create_config_h_in()
configure_file("${CMAKE_BINARY_DIR}/config.h.in" "${CMAKE_BINARY_DIR}/config.h")

#################################
# Defs                          #
#################################

if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /errorReport:none /nologo ")

    # C++ Exceptions
    # (Note: If you are using Microsoft VC++6.0, the library needs to be built
    # with /EHa instead of /EHs or else cancellation won't work properly.)
    if(MSVC_VERSION EQUAL 1200)
        set(VCEFLAGS  "/EHa" "/TP")
    else()
        set(VCEFLAGS  "/EHs" "/TP")
    endif()

    set(build_defines ${build_defines} HAVE_CONFIG_H __PTW32_RC_MSC)
elseif (${CMAKE_C_COMPILER_ID} STREQUAL "GNU")
    set(build_defines ${build_defines} HAVE_CONFIG_H)
endif()


# Update filename with proper version info
configure_file(${CMAKE_SOURCE_DIR}/cmake/version.rc.in ${CMAKE_BINARY_DIR}/version.rc @ONLY)

#################################
# Libraries                     #
#################################

function(shared_lib type def)
    set(targ pthread${type}${PTW32_VER})

    if (${type} STREQUAL "VCE")
        add_library(${targ} SHARED pthread.cxx ${CMAKE_BINARY_DIR}/version.rc)
        set_target_properties(${targ} PROPERTIES LINKER_LANGUAGE CXX)
        target_compile_options(${targ} PUBLIC "${VCEFLAGS}")
    elseif (${CMAKE_C_COMPILER_ID} STREQUAL "GNU")
        add_library(${targ} SHARED pthread.cxx ${CMAKE_BINARY_DIR}/version.rc)
	target_compile_options(${targ} PRIVATE "-mthreads")
	if (${type} STREQUAL "GCE")
            set_target_properties(${targ} PROPERTIES LINKER_LANGUAGE CXX)
	    target_compile_options(${targ} PRIVATE "-xc++")
	    target_link_libraries(${targ} stdc++)
	    target_link_options(${targ} PRIVATE "-shared-libgcc")
	endif (${type} STREQUAL "GCE")
    else ()
        add_library(${targ} SHARED pthread.c ${CMAKE_BINARY_DIR}/version.rc)
    endif (${type} STREQUAL "VCE")

    target_compile_definitions(${targ} PRIVATE ${def} ${build_defines} __PTW32_BUILD_INLINED)
    if (MSVC)
      # Set resource compiler definition...
      target_compile_definitions(${targ} PRIVATE __PTW32_RC_MSC)
    endif ()
    set_target_properties(${targ} PROPERTIES DEBUG_POSTFIX "d")

    message(STATUS "Shared library ${targ}")

    install(
      TARGETS ${targ}
      ARCHIVE # DESTINATION ${TARGET_ARCH}/${CONFIG}/lib
      LIBRARY # DESTINATION ${TARGET_ARCH}/${CONFIG}/lib
      RUNTIME # DESTINATION ${TARGET_ARCH}/${CONFIG}/bin
    )

    install(
        FILES $<TARGET_PDB_FILE:${targ}>
        CONFIGURATION Debug
        DESTINATION lib
        OPTIONAL
    )
endfunction()

function(static_lib type def)
    set(targ libpthread${type}${PTW32_VER})

    if(${type} STREQUAL "VCE")
        add_library(${targ} STATIC pthread.cxx)
        target_compile_options(${targ} PRIVATE "/EHs")
    elseif (${CMAKE_C_COMPILER_ID} STREQUAL "GNU")
	if (${type} STREQUAL "GCE")
            add_library(${targ} STATIC pthread.cxx)
	    target_compile_options(${targ} PRIVATE "-xc++")
	    target_link_libraries(${targ} stdc++)
	    target_link_options(${targ} PRIVATE "-shared-libgcc")
	endif (${type} STREQUAL "GCE")
    else ()
        add_library(${targ} STATIC pthread.c)
    endif()

    target_compile_definitions(${targ} PRIVATE "${def}" ${build_defines} __PTW32_STATIC_LIB __PTW32_BUILD_INLINED)
    set_target_properties(${targ} PROPERTIES DEBUG_POSTFIX "d")

    message(STATUS "Static library ${targ}")

    install(
      TARGETS ${targ}
      ARCHIVE # DESTINATION ${TARGET_ARCH}/${CONFIG}/lib
      LIBRARY # DESTINATION ${TARGET_ARCH}/${CONFIG}/lib
      RUNTIME # DESTINATION ${TARGET_ARCH}/${CONFIG}/bin
    )
endfunction()

if (MSVC)
  shared_lib ( VCE __PTW32_CLEANUP_CXX )
  shared_lib ( VSE __PTW32_CLEANUP_SEH )
  shared_lib ( VC  __PTW32_CLEANUP_C )

  static_lib ( VCE __PTW32_CLEANUP_CXX )
  static_lib ( VSE __PTW32_CLEANUP_SEH )
  static_lib ( VC  __PTW32_CLEANUP_C )
elseif (${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
  ## NOTE: MinGW64 does support SEH exception handling, BUT it does not yet
  ## have the MSVC keywords "__try", "__except" and "__finally".
  shared_lib ( GCE __PTW32_CLEANUP_CXX )
  shared_lib ( GC  __PTW32_CLEANUP_C )

  static_lib ( GCE __PTW32_CLEANUP_CXX )
  static_lib ( GC  __PTW32_CLEANUP_C )
endif (MSVC)

#################################
# Install                       #
#################################
install(FILES _ptw32.h pthread.h sched.h semaphore.h TYPE INCLUDE)

#################################
# Test                          #
#################################
option(ENABLE_TESTS "Enable Test code build" FALSE)

#TODO determine if cross compile...
if(ENABLE_TESTS AND (NOT CMAKE_CROSSCOMPILING AND BUILD_TESTING))
  include (CTest)
  add_subdirectory(tests)
endif()
