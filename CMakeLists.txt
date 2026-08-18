cmake_minimum_required(VERSION 3.16)
project(mod_ai_media VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FREESWITCH REQUIRED freeswitch)

include(FetchContent)
set(USE_TLS ON CACHE BOOL "Build libwsc with TLS" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static dependencies" FORCE)
FetchContent_Declare(
    libwsc
    GIT_REPOSITORY https://github.com/amigniter/libwsc.git
    GIT_TAG 366fa04186cda066165f10326002681adaf65d98
)
FetchContent_MakeAvailable(libwsc)

add_library(ai_media_core STATIC
    src/pcm_ring_buffer.cpp
    src/protocol.cpp
)
target_include_directories(ai_media_core PUBLIC include)

add_library(mod_ai_media MODULE src/mod_ai_media.cpp src/session.cpp)
set_target_properties(mod_ai_media PROPERTIES PREFIX "")
target_include_directories(mod_ai_media PRIVATE
    include
    ${FREESWITCH_INCLUDE_DIRS}
    ${libwsc_SOURCE_DIR}/src
)
target_link_directories(mod_ai_media PRIVATE ${FREESWITCH_LIBRARY_DIRS})
target_compile_options(mod_ai_media PRIVATE ${FREESWITCH_CFLAGS_OTHER})
target_link_libraries(mod_ai_media PRIVATE ai_media_core libwsc ${FREESWITCH_LIBRARIES})

include(CTest)
if(BUILD_TESTING)
    add_executable(ai_media_tests tests/test_main.cpp)
    target_link_libraries(ai_media_tests PRIVATE ai_media_core)
    add_test(NAME ai_media_tests COMMAND ai_media_tests)
endif()
