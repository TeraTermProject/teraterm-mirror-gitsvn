﻿# cmake -DCMAKE_GENERATOR="Visual Studio 17 2022" -DARCHITECTURE=Win32 -P buildall.cmake
# cmake -DCMAKE_GENERATOR="Visual Studio 16 2019" -DARCHITECTURE=Win32 -P buildall.cmake
# cmake -DCMAKE_GENERATOR="Visual Studio 16 2019" -DARCHITECTURE=x64 -P buildall.cmake
# cmake -DCMAKE_GENERATOR="Visual Studio 15 2017" -P buildall.cmake
# cmake -DCMAKE_GENERATOR="Visual Studio 15 2017 Win64" -P buildall.cmake
# .\cmake-3.11.4-win32-x86\bin\cmake.exe -DCMAKE_GENERATOR="Visual Studio 8 2005" -P buildall.cmake
# cmake -DCMAKE_GENERATOR="Unix Makefiles" -P buildall.cmake
# cmake -DCMAKE_GENERATOR="Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=../mingw.toolchain.cmake -P buildall.cmake
# cmake -DCMAKE_GENERATOR="NMake Makefiles" -DBUILD_SSL_LIBRARY=OFF -P buildall.cmake

set(BUILD_OPENSSL1 OFF)
set(BUILD_OPENSSL3 OFF)

if("${CMAKE_GENERATOR}" STREQUAL "")
  message(FATAL_ERROR "set CMAKE_GENERATOR!")
endif()

if(NOT "${ARCHITECTURE}" STREQUAL "")
  set(ARCHITECTURE_OPTION -DARCHITECTURE=${ARCHITECTURE})
endif()

if(NOT DEFINED BUILD_SSL_LIBRARY)
  if((${CMAKE_GENERATOR} MATCHES "Visual Studio 8 2005") OR
      (${CMAKE_GENERATOR} MATCHES "Visual Studio 9 2008"))
    set(BUILD_SSL_LIBRARY OFF)
  else()
    set(BUILD_SSL_LIBRARY ON)
  endif()
endif()

message("BUILD_SSL_LIBRARY=${BUILD_SSL_LIBRARY}")

# install tools
include(${CMAKE_CURRENT_LIST_DIR}/../buildtools/checkperl.cmake)
message("perl=${PERL}")

# build
message("oniguruma")
execute_process(
  COMMAND ${CMAKE_COMMAND} -DCMAKE_GENERATOR=${CMAKE_GENERATOR} ${ARCHITECTURE_OPTION} -P buildoniguruma.cmake
  )
message("zlib")
execute_process(
  COMMAND ${CMAKE_COMMAND} -DCMAKE_GENERATOR=${CMAKE_GENERATOR} ${ARCHITECTURE_OPTION} -P buildzlib.cmake
  )
message("putty")
execute_process(
  COMMAND ${CMAKE_COMMAND} -P buildputty.cmake
  )
message("SFMT")
execute_process(
  COMMAND ${CMAKE_COMMAND} -DCMAKE_GENERATOR=${CMAKE_GENERATOR} ${ARCHITECTURE_OPTION} -P buildsfmt.cmake
  )
if(BUILD_SSL_LIBRARY)
  if(BUILD_OPENSSL1)
    message("openssl 1.1")
    execute_process(
      COMMAND ${CMAKE_COMMAND} -DCMAKE_GENERATOR=${CMAKE_GENERATOR} ${ARCHITECTURE_OPTION} -P openssl11.cmake
    )
  endif(BUILD_OPENSSL1)
  if(BUILD_OPENSSL3)
    message("openssl 3")
    execute_process(
      COMMAND ${CMAKE_COMMAND} -DCMAKE_GENERATOR=${CMAKE_GENERATOR} ${ARCHITECTURE_OPTION} -P openssl3.cmake
    )
  endif(BUILD_OPENSSL3)
endif(BUILD_SSL_LIBRARY)
message("cJSON")
execute_process(
  COMMAND ${CMAKE_COMMAND} -P buildcjson.cmake
  )
message("argon2")
execute_process(
  COMMAND ${CMAKE_COMMAND} -P buildargon2.cmake
  )
if(BUILD_SSL_LIBRARY)
  message("libressl")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -DCMAKE_GENERATOR=${CMAKE_GENERATOR} ${ARCHITECTURE_OPTION} -P buildlibressl.cmake
    )
endif(BUILD_SSL_LIBRARY)

message("done buildall.cmake")
