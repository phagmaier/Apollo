cmake_minimum_required(VERSION 3.25)
project(ApolloSpike LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(APOLLO_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
if(APOLLO_SANITIZERS)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_custom_target(check_fonts ALL
  COMMAND "${CMAKE_COMMAND}" "-DFONT_DIR=${CMAKE_CURRENT_SOURCE_DIR}/assets/courier-prime"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_fonts.cmake"
  VERBATIM)

add_library(apollo_layout engine/wrap.cpp engine/layout.cpp fountain/import.cpp)
target_include_directories(apollo_layout PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(apollo_layout PRIVATE -Wall -Wextra -Wpedantic)
add_executable(layout_probe tools/layout_probe.cpp)
target_link_libraries(layout_probe PRIVATE apollo_layout)

enable_testing()
add_executable(layout_tests tests/layout_tests.cpp)
target_link_libraries(layout_tests PRIVATE apollo_layout)
target_compile_definitions(layout_tests PRIVATE APOLLO_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/layout")
add_test(NAME layout_tests COMMAND layout_tests)
