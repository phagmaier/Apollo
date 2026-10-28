cmake_minimum_required(VERSION 3.25)
project(ApolloSpike LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(APOLLO_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(APOLLO_BUILD_PDF "Build the Qt PDF exporter and its integration checks" ON)
option(APOLLO_BUILD_EDITOR_PROBE "Build the QML TextEdit feasibility probe and its checks" ON)
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
add_executable(geometry_tests tests/geometry_tests.cpp)
target_link_libraries(geometry_tests PRIVATE apollo_layout)
add_test(NAME geometry_tests COMMAND geometry_tests)

if(APOLLO_BUILD_PDF)
  find_package(Qt6 6.12 REQUIRED COMPONENTS Gui)
  add_library(apollo_pdf export/pdf.cpp)
  target_link_libraries(apollo_pdf PUBLIC apollo_layout Qt6::Gui)
  target_compile_options(apollo_pdf PRIVATE -Wall -Wextra -Wpedantic)
  add_executable(pdf_export tools/pdf_export.cpp)
  target_link_libraries(pdf_export PRIVATE apollo_pdf)
  target_compile_definitions(pdf_export PRIVATE APOLLO_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/courier-prime")
  find_program(UV_EXECUTABLE uv REQUIRED)
  foreach(tool pdftotext pdffonts pdfinfo pdftohtml pdftoppm)
    find_program(${tool}_EXECUTABLE ${tool} REQUIRED)
  endforeach()
  add_test(NAME pdf_tests COMMAND "${UV_EXECUTABLE}" run "${CMAKE_CURRENT_SOURCE_DIR}/tests/pdf_tests.py"
    --exporter "$<TARGET_FILE:pdf_export>" --probe "$<TARGET_FILE:layout_probe>"
    --output "${CMAKE_CURRENT_BINARY_DIR}/pdf-tests")
  set_tests_properties(pdf_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PYTHONDONTWRITEBYTECODE=1")
endif()

if(APOLLO_BUILD_EDITOR_PROBE)
  find_package(Qt6 6.12 REQUIRED COMPONENTS Gui Quick Test)
  add_library(apollo_editor_probe editor/screenplay_layout.cpp editor/probe_view.cpp)
  set_target_properties(apollo_editor_probe PROPERTIES AUTOMOC ON)
  target_link_libraries(apollo_editor_probe PUBLIC apollo_layout Qt6::Gui Qt6::Quick)
  target_compile_options(apollo_editor_probe PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_definitions(apollo_editor_probe PRIVATE APOLLO_EDITOR_QML="${CMAKE_CURRENT_SOURCE_DIR}/editor/TextEditProbe.qml")
  add_executable(textedit_probe tools/textedit_probe.cpp)
  target_link_libraries(textedit_probe PRIVATE apollo_editor_probe)
  target_compile_definitions(textedit_probe PRIVATE APOLLO_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/courier-prime")
  add_executable(textedit_probe_tests tests/textedit_probe_tests.cpp)
  set_target_properties(textedit_probe_tests PROPERTIES AUTOMOC ON)
  target_link_libraries(textedit_probe_tests PRIVATE apollo_editor_probe Qt6::Test)
  target_compile_definitions(textedit_probe_tests PRIVATE
    APOLLO_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/courier-prime"
    APOLLO_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/layout"
    APOLLO_ARTIFACT_DIR="${CMAKE_CURRENT_BINARY_DIR}/textedit-probe")
  add_test(NAME textedit_probe_tests COMMAND textedit_probe_tests)
  set_tests_properties(textedit_probe_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
