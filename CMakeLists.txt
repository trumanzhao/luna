cmake_minimum_required(VERSION 3.1)
project(luna)
aux_source_directory(src luna_src)
aux_source_directory(lz4 lz4_src)
set(CMAKE_CXX_STANDARD 17)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++11" SUPPORT_CXX11)
if (NOT SUPPORT_CXX11)
    message(FATAL_ERROR "compiler not support c++11")
endif()

check_cxx_compiler_flag("-std=c++17" SUPPORT_CXX17)
if (SUPPORT_CXX17)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/luna.h luna.h COPYONLY)
else()
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/luna11.h luna.h COPYONLY)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lz4)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_MACOSX_RPATH 0)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang" )
    include_directories("/usr/local/include")
    set(CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS} -undefined dynamic_lookup")
endif()

add_library(luna SHARED ${luna_src} ${lz4_src})

export(PACKAGE luna)
export(TARGETS luna FILE luna-exports.cmake)
configure_file(luna-config.cmake.in luna-config.cmake)

install(TARGETS luna RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/luna.h src/lua_archiver.h src/var_int.h DESTINATION include)

