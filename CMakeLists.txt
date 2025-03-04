cmake_minimum_required(VERSION 3.28)
project(CppWebForge)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(ENABLE_THREAD_SANITIZER "Enable Thread Sanitizer" OFF)
option(ENABLE_MEMORY_SANITIZER "Enable Memory Sanitizer" OFF)
option(ENABLE_PERFORMANCE_TESTING "Enable Performance Testing" OFF)
option(ENABLE_TESTS "Enable testing" OFF)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

if (ENABLE_THREAD_SANITIZER)
    set(SANITIZER_FLAGS "-fsanitize=thread")
elseif (ENABLE_MEMORY_SANITIZER)
    set(SANITIZER_FLAGS "-fsanitize=memory" "-fsanitize-memory-track-origins" "-fno-omit-frame-pointer")
else()
    set(SANITIZER_FLAGS "")
endif()

if (ENABLE_PERFORMANCE_TESTING)
    set(PERFORMANCE_FLAGS "-g" "-O2")
    add_compile_definitions(PERFORMANCE_TESTING)
    # perf record -e cycles:u,instructions:u,cache-misses:u,context-switches -g ./main
else()
    set(PERFORMANCE_FLAGS "")
endif()

set(WARNING_FLAGS "-Wall" "-Wextra" "-Werror")

file(GLOB_RECURSE SRC_FILES "src/*.cpp")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-g)
  add_compile_options(-O0)
  add_compile_definitions(DEBUG_MODE)
endif()

if (ENABLE_TESTS)
  add_compile_definitions(DEBUG_MODE)
  add_compile_definitions(ENABLE_PERFORMANCE_TESTING)
endif()

include(FetchContent)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib
    GIT_TAG v0.19.0
)

set(BUILD_TESTING OFF CACHE BOOL "Disable Inja tests")
set(INJA_USE_EMBEDDED_JSON ON CACHE BOOL "Use embedded JSON library")
set(INJA_BUILD_TESTS OFF CACHE BOOL "Disable Inja tests")

FetchContent_Declare(
    inja
    GIT_REPOSITORY https://github.com/pantor/inja
    GIT_TAG v3.4.0
)

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")

FetchContent_MakeAvailable(httplib inja)

if(NOT TARGET inja::inja)
    add_library(inja::inja INTERFACE IMPORTED)
    target_include_directories(inja::inja INTERFACE 
        ${inja_SOURCE_DIR}/include
        ${inja_SOURCE_DIR}/third_party/include
    )
endif()

add_library(cppwebforge STATIC ${SRC_FILES})
set_target_properties(cppwebforge PROPERTIES
    CXX_CLANG_TIDY "clang-tidy;-checks=clang-analyzer-*,bugprone-*,performance-*,portability-*,readability-*"
)
target_compile_options(cppwebforge PRIVATE ${SANITIZER_FLAGS} ${PERFORMANCE_FLAGS} ${WARNING_FLAGS})
target_link_options(cppwebforge PRIVATE ${SANITIZER_FLAGS})
target_link_libraries(cppwebforge PRIVATE 
    OpenSSL::SSL 
    OpenSSL::Crypto 
    ZLIB::ZLIB
    Threads::Threads
    inja::inja
)

target_include_directories(cppwebforge PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${inja_SOURCE_DIR}/include
    ${inja_SOURCE_DIR}/third_party/include
    ${httplib_SOURCE_DIR}
)

if(ENABLE_TESTS)
  include(FetchContent)

  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.16.0
  )
  FetchContent_MakeAvailable(googletest)

  find_package(CURL REQUIRED)
  file(GLOB_RECURSE TEST_FILES "tests/*.cpp")
  add_executable(cppwebforge_tests ${TEST_FILES})

  target_link_libraries(cppwebforge_tests
    PRIVATE
    cppwebforge
    gtest_main
    gmock_main
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    Threads::Threads
  )

  target_include_directories(cppwebforge_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${inja_SOURCE_DIR}/include
    ${inja_SOURCE_DIR}/third_party/include
    ${httplib_SOURCE_DIR}
  )

  enable_testing()
  include(GoogleTest)
  gtest_discover_tests(cppwebforge_tests)
endif()

add_custom_target(clean_project
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaning project-specific build files"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}/CMakeFiles/cppwebforge.dir" || true
    COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_BINARY_DIR}/libcppwebforge.a" || true
    COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_BINARY_DIR}/cppwebforge_tests" || true
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}/CMakeFiles/cppwebforge_tests.dir" || true
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}/Testing" || true
    COMMENT "Cleaning project-specific build files"
)
