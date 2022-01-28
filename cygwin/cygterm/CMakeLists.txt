cmake_minimum_required(VERSION 3.11)

message("CMAKE_COMMAND=${CMAKE_COMMAND}")
message("CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}")
message("CMAKE_HOST_SYSTEM_NAME=${CMAKE_HOST_SYSTEM_NAME}")
if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "MSYS")
  message("MSYS2TERM=ON")
  set(MSYS2TERM ON)
endif()

if (MSYS2TERM)
  set(PACKAGE_NAME "msys2term")
else()
  set(PACKAGE_NAME "cygterm")
endif()

project(${PACKAGE_NAME})
ENABLE_LANGUAGE(RC)

add_executable(
  ${PACKAGE_NAME}
  cygterm.cc
  cygterm.rc
  cygterm_cfg.cc
  cygterm_cfg.h
  )

target_sources(
  ${PACKAGE_NAME}
  PRIVATE
  ../../teraterm/common/ttlib.h
#  ../../teraterm/common/ttlib_static_cpp.cpp
  ../../teraterm/common/ttlib_static_dirs.cpp
  ../../teraterm/common/asprintf.cpp
  ../../teraterm/common/asprintf.h
  ../../teraterm/common/win32helper.cpp
  ../../teraterm/common/win32helper.h
  ../../teraterm/common/compat_win.cpp
  ../../teraterm/common/ttknownfolders.c
  ../../teraterm/common/dllutil.cpp
  ../../teraterm/common/dllutil.h
  ../../teraterm/common/codeconv.cpp
  ../../teraterm/common/codeconv.h
  )

if (MSYS2TERM)
  target_compile_definitions(
    ${PACKAGE_NAME}
    PRIVATE
    MSYS2=1
    )
else()
  target_compile_definitions(
    ${PACKAGE_NAME}
    PRIVATE
    CYGWIN=1
    )
endif()

target_compile_options(
  ${PACKAGE_NAME}
  PRIVATE
  -D_GNU_SOURCE
  -fno-exceptions
  -Wall -Wextra
  )

target_include_directories(
  ${PACKAGE_NAME}
  PRIVATE
  ../../teraterm/common
  )

target_link_options(
  ${PACKAGE_NAME}
  PRIVATE
  -mwindows
  )

target_link_libraries(
  ${PACKAGE_NAME}
  PRIVATE
  shell32
  ole32
  )

install(
  TARGETS ${PACKAGE_NAME}
  DESTINATION .
  )
install(
  FILES ${PACKAGE_NAME}.cfg
  DESTINATION .
  )
