cmake_minimum_required (VERSION 3.28)

project (EVOThreads
  HOMEPAGE_URL "https://github.com/evo-i/threads"
  VERSION 1.0.0
  DESCRIPTION "C11 Threads emulation library."
  LANGUAGES C)

find_package (Threads REQUIRED)

set (CMAKE_C_STANDARD 17)
set (CMAKE_C_STANDARD_REQUIRED ON)

include (GNUInstallDirs)
include (CheckIncludeFile)
include (CheckSymbolExists)
include (CheckStructHasMember)
include (CheckCSourceCompiles)
include (CMakePackageConfigHelpers)

check_include_file (pthread.h HAVE_PTHREAD)

check_include_file (pthreads.h HAVE_PTHREAD)

check_symbol_exists (thrd_create threads.h HAVE_THRD_CREATE)

check_symbol_exists (timespec_get time.h HAVE_TIMESPEC_GET)

check_struct_has_member ("struct timespec"
  "tv_sec" "time.h"
    HAVE_STRUCT_TIMESPEC_TV_SEC LANGUAGE C)

check_struct_has_member ("struct timespec"
  "tv_nsec" "time.h"
  HAVE_STRUCT_TIMESPEC_TV_NSEC LANGUAGE C)

if (HAVE_STRUCT_TIMESPEC_TV_SEC AND HAVE_STRUCT_TIMESPEC_TV_NSEC)
   set (HAVE_STRUCT_TIMESPEC TRUE)
endif ()

if (UNIX AND NOT HAVE_THRD_CREATE)
  set (EVO_THREADS_SRC_FILE "src/src/evo/threads/posix.c")
endif (UNIX AND NOT HAVE_THRD_CREATE)

if (WIN32 AND NOT CYGWIN AND NOT HAVE_THRD_CREATE)
  set (EVO_THREADS_SRC_FILE "src/src/evo/threads/win32.c")
endif (WIN32 AND NOT CYGWIN AND NOT HAVE_THRD_CREATE)

add_library (threads
  ${EVO_THREADS_SRC_FILE}
  "src/include/evo/threads/time.h"
  "src/src/evo/threads/time.c")

target_include_directories (threads
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if (BUILD_SHARED_LIBS)
  target_compile_definitions (threads
    PRIVATE
      -DEVO_THREADS_DLL
      -DEVO_THREADS_DLL_EXPORTS)
endif (BUILD_SHARED_LIBS)


if (HAVE_PTHREAD)
  target_compile_definitions (threads
    PUBLIC
      -DHAVE_PTHREAD)
endif ()

if (HAVE_STRUCT_TIMESPEC)
  target_compile_definitions(threads
    PUBLIC
      -DHAVE_STRUCT_TIMESPEC)
endif ()

if (HAVE_TIMESPEC_GET)
   target_compile_definitions(threads
    PUBLIC
      -DHAVE_TIMESPEC_GET)
endif ()

target_link_libraries (threads
  PUBLIC
    Threads::Threads)

install (
  TARGETS threads
  EXPORT EVOThreads-targets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
                      "${CMAKE_INSTALL_INCLUDEDIR}/evo/threads"
)

install (
  EXPORT EVOThreads-targets
  FILE EVOThreads-targets.cmake
  NAMESPACE Evo::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/EVOThreads"
)

configure_package_config_file(
  "cmake/config.cmake.in"
  ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

install(
  FILES "${PROJECT_BINARY_DIR}/EVOThreads-config.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/EVOThreads"
)

install (FILES
  "src/include/evo/threads/exports.h"
  "src/include/evo/threads/threads.h"
  "src/include/evo/threads/time.h"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
