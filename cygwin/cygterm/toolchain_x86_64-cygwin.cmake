﻿set(CMAKE_SYSROOT /usr/x86_64-pc-cygwin/sys-root)
set(CMAKE_C_COMPILER x86_64-pc-cygwin-gcc)
set(CMAKE_CXX_COMPILER x86_64-pc-cygwin-g++)
set(CMAKE_RC_COMPILER x86_64-pc-cygwin-windres)
if(NOT EXISTS ${CMAKE_SYSROOT})
  unset(CMAKE_RC_COMPILER)
  unset(CMAKE_SYSROOT)
endif()
