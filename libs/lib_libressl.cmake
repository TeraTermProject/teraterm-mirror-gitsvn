
option(TTXSSH_LIBRESSL_370 "use 3.7.0" OFF)

include(${CMAKE_CURRENT_LIST_DIR}/script_support.cmake)

set(LIBRESSL_ROOT ${CMAKE_CURRENT_LIST_DIR}/libressl_${TOOLSET})
if (TTXSSH_LIBRESSL_370)
  set(LIBRESSL_ROOT ${CMAKE_CURRENT_LIST_DIR}/libressl_370_${TOOLSET})
endif()
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(LIBRESSL_ROOT "${LIBRESSL_ROOT}_x64")
endif()

set(LIBRESSL_INCLUDE_DIRS ${LIBRESSL_ROOT}/include)
if(MINGW)
  set(LIBRESSL_LIB
    ${LIBRESSL_ROOT}/lib/libcrypto-47.a
    bcrypt
  )
  if (TTXSSH_LIBRESSL_370)
    set(LIBRESSL_LIB
      ${LIBRESSL_ROOT}/lib/libcrypto-50.a
      bcrypt
    )
  endif()
else()
  if(IS_MULTI_CONFIG)
    set(LIBRESSL_LIB
      debug ${LIBRESSL_ROOT}/lib/crypto-47d.lib
      optimized ${LIBRESSL_ROOT}/lib/crypto-47.lib
    )
    if (TTXSSH_LIBRESSL_370)
      set(LIBRESSL_LIB
        debug ${LIBRESSL_ROOT}/lib/crypto-50d.lib
        optimized ${LIBRESSL_ROOT}/lib/crypto-50.lib
      )
    endif()
  else()
    set(LIBRESSL_LIB ${LIBRESSL_ROOT}/lib/crypto-47.lib)
    if (TTXSSH_LIBRESSL_370)
      set(LIBRESSL_LIB ${LIBRESSL_ROOT}/lib/crypto-50.lib)
    endif()
  endif()
endif()
