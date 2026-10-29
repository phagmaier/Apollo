cmake_minimum_required(VERSION 3.25)
project(ApolloSpike LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(APOLLO_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(APOLLO_BUILD_PDF "Build the Qt PDF exporter and its integration checks" ON)
option(APOLLO_BUILD_EDITOR "Build the QML screenplay editor and its checks" ON)
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
  add_library(apollo_export_snapshot export/snapshot.cpp)
  target_link_libraries(apollo_export_snapshot PUBLIC apollo_layout Qt6::Core)
  target_compile_options(apollo_export_snapshot PRIVATE -Wall -Wextra -Wpedantic)
  add_library(apollo_pdf export/pdf.cpp)
  target_link_libraries(apollo_pdf PUBLIC apollo_layout Qt6::Gui)
  target_compile_options(apollo_pdf PRIVATE -Wall -Wextra -Wpedantic)
  add_executable(pdf_export tools/pdf_export.cpp)
  target_link_libraries(pdf_export PRIVATE apollo_pdf apollo_export_snapshot)
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

if(APOLLO_BUILD_EDITOR)
  if(NOT APOLLO_BUILD_PDF)
    message(FATAL_ERROR "APOLLO_BUILD_EDITOR requires APOLLO_BUILD_PDF for editor/PDF agreement")
  endif()
  find_package(Qt6 6.12 REQUIRED COMPONENTS Gui Quick Test)
  add_library(apollo_source source/screenplay_format.cpp source/project.cpp source/recovery.cpp source/project_access.cpp source/project_creation.cpp source/fountain_intake.cpp source/fountain_conversion.cpp source/fountain_working.cpp source/fountain_export.cpp)
  target_link_libraries(apollo_source PUBLIC apollo_layout Qt6::Core)
  target_compile_options(apollo_source PRIVATE -Wall -Wextra -Wpedantic)
  add_executable(source_tests tests/source_tests.cpp)
  set_target_properties(source_tests PROPERTIES AUTOMOC ON)
  target_link_libraries(source_tests PRIVATE apollo_source Qt6::Test)
  target_compile_definitions(source_tests PRIVATE APOLLO_SOURCE_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/source")
  add_test(NAME source_tests COMMAND source_tests)
  add_executable(recovery_tests tests/recovery_tests.cpp)
  set_target_properties(recovery_tests PROPERTIES AUTOMOC ON)
  target_link_libraries(recovery_tests PRIVATE apollo_source Qt6::Test)
  add_test(NAME recovery_tests COMMAND recovery_tests)
  add_executable(project_creation_tests tests/project_creation_tests.cpp)
  set_target_properties(project_creation_tests PROPERTIES AUTOMOC ON)
  target_link_libraries(project_creation_tests PRIVATE apollo_source Qt6::Test)
  add_test(NAME project_creation_tests COMMAND project_creation_tests)
  add_executable(fountain_intake_tests tests/fountain_intake_tests.cpp)
  set_target_properties(fountain_intake_tests PROPERTIES AUTOMOC ON)
  target_link_libraries(fountain_intake_tests PRIVATE apollo_source Qt6::Test)
  target_compile_definitions(fountain_intake_tests PRIVATE APOLLO_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
  add_test(NAME fountain_intake_tests COMMAND fountain_intake_tests)
  add_executable(fountain_conversion_tests tests/fountain_conversion_tests.cpp)
  set_target_properties(fountain_conversion_tests PROPERTIES AUTOMOC ON)
  target_link_libraries(fountain_conversion_tests PRIVATE apollo_source Qt6::Test)
  target_compile_definitions(fountain_conversion_tests PRIVATE APOLLO_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
  add_test(NAME fountain_conversion_tests COMMAND fountain_conversion_tests)
  add_executable(fountain_export_tests tests/fountain_export_tests.cpp)
  set_target_properties(fountain_export_tests PROPERTIES AUTOMOC ON)
  target_link_libraries(fountain_export_tests PRIVATE apollo_source Qt6::Test)
  target_compile_definitions(fountain_export_tests PRIVATE APOLLO_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
  add_test(NAME fountain_export_tests COMMAND fountain_export_tests)
  add_library(apollo_editor_core editor/screenplay_layout.cpp editor/editor_commands.cpp editor/editor_view.cpp editor/pdf_export_job.cpp editor/fountain_export_job.cpp
                                 editor/autosave.cpp editor/session_state.cpp editor/project_library.cpp editor/project_access_flow.cpp)
  set_target_properties(apollo_editor_core PROPERTIES AUTOMOC ON)
  target_link_libraries(apollo_editor_core PUBLIC apollo_layout apollo_source apollo_export_snapshot Qt6::Gui Qt6::Quick)
  target_compile_options(apollo_editor_core PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_definitions(apollo_editor_core PRIVATE
    APOLLO_EDITOR_QML="${CMAKE_CURRENT_SOURCE_DIR}/app/WritingDesk.qml"
    APOLLO_ACCESS_QML="${CMAKE_CURRENT_SOURCE_DIR}/app/ProjectAccess.qml")
  target_compile_definitions(apollo_editor_core PRIVATE APOLLO_INTAKE_QML="${CMAKE_CURRENT_SOURCE_DIR}/app/ImportedMaterial.qml")
  add_executable(apollo_editor tools/apollo_editor.cpp)
  target_link_libraries(apollo_editor PRIVATE apollo_editor_core apollo_pdf)
  add_dependencies(apollo_editor pdf_export)
  add_executable(measure_fountain_workflow tools/measure-fountain-workflow.cpp)
  target_link_libraries(measure_fountain_workflow PRIVATE apollo_editor_core Qt6::Test)
  target_compile_options(measure_fountain_workflow PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_definitions(measure_fountain_workflow PRIVATE
    APOLLO_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/courier-prime")
  add_dependencies(measure_fountain_workflow pdf_export)
  add_executable(project_access_tests tests/project_access_tests.cpp)
  set_target_properties(project_access_tests PROPERTIES AUTOMOC ON)
  target_link_libraries(project_access_tests PRIVATE apollo_source Qt6::Test)
  add_test(NAME project_access_tests COMMAND project_access_tests)
  add_test(NAME fountain_intake_process_tests COMMAND "${UV_EXECUTABLE}" run "${CMAKE_CURRENT_SOURCE_DIR}/tests/fountain_intake_process_tests.py"
    --editor "$<TARGET_FILE:apollo_editor>" --output "${CMAKE_CURRENT_BINARY_DIR}/intake-process-tests")
  set_tests_properties(fountain_intake_process_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PYTHONDONTWRITEBYTECODE=1" TIMEOUT 90)
  add_executable(session_state_tests tests/session_state_tests.cpp)
  set_target_properties(session_state_tests PROPERTIES AUTOMOC ON)
  target_link_libraries(session_state_tests PRIVATE apollo_editor_core Qt6::Test)
  add_test(NAME session_state_tests COMMAND session_state_tests)
  set_tests_properties(session_state_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_executable(project_library_tests tests/project_library_tests.cpp)
  set_target_properties(project_library_tests PROPERTIES AUTOMOC ON)
  target_link_libraries(project_library_tests PRIVATE apollo_editor_core Qt6::Test)
  add_test(NAME project_library_tests COMMAND project_library_tests)
  add_executable(project_access_flow_tests tests/project_access_flow_tests.cpp)
  set_target_properties(project_access_flow_tests PROPERTIES AUTOMOC ON)
  target_link_libraries(project_access_flow_tests PRIVATE apollo_editor_core Qt6::Test)
  target_compile_definitions(project_access_flow_tests PRIVATE
    APOLLO_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/courier-prime"
    APOLLO_IMPORT_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/import"
    APOLLO_ARTIFACT_DIR="${CMAKE_CURRENT_BINARY_DIR}/project-access-tests")
  add_dependencies(project_access_flow_tests pdf_export)
  add_test(NAME project_access_flow_tests COMMAND project_access_flow_tests)
  set_tests_properties(project_access_flow_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  target_compile_definitions(apollo_editor PRIVATE APOLLO_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/courier-prime")
  add_executable(editor_tests tests/editor_tests.cpp)
  set_target_properties(editor_tests PROPERTIES AUTOMOC ON)
  target_link_libraries(editor_tests PRIVATE apollo_editor_core Qt6::Test)
  target_compile_definitions(editor_tests PRIVATE
    APOLLO_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/courier-prime"
    APOLLO_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/layout"
    APOLLO_LOOK_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/prototype/The Last Broadcast.fountain"
    APOLLO_ARTIFACT_DIR="${CMAKE_CURRENT_BINARY_DIR}/editor-tests")
  add_test(NAME editor_tests COMMAND editor_tests)
  set_tests_properties(editor_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_executable(page_break_layout_tests tests/page_break_layout_tests.cpp)
  set_target_properties(page_break_layout_tests PROPERTIES AUTOMOC ON)
  target_link_libraries(page_break_layout_tests PRIVATE apollo_editor_core Qt6::Test)
  target_compile_definitions(page_break_layout_tests PRIVATE APOLLO_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/courier-prime")
  add_test(NAME page_break_layout_tests COMMAND page_break_layout_tests)
  set_tests_properties(page_break_layout_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_executable(autosave_tests tests/autosave_tests.cpp)
  set_target_properties(autosave_tests PROPERTIES AUTOMOC ON)
  target_link_libraries(autosave_tests PRIVATE apollo_editor_core Qt6::Test)
  target_compile_definitions(autosave_tests PRIVATE
    APOLLO_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/courier-prime"
    APOLLO_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/layout"
    APOLLO_LOOK_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/prototype/The Last Broadcast.fountain"
    APOLLO_ARTIFACT_DIR="${CMAKE_CURRENT_BINARY_DIR}/autosave-tests")
  add_test(NAME autosave_tests COMMAND autosave_tests)
  set_tests_properties(autosave_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_executable(export_job_tests tests/export_job_tests.cpp)
  set_target_properties(export_job_tests PROPERTIES AUTOMOC ON)
  target_link_libraries(export_job_tests PRIVATE apollo_editor_core Qt6::Test)
  target_compile_definitions(export_job_tests PRIVATE
    APOLLO_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/courier-prime"
    APOLLO_EXPORTER="$<TARGET_FILE:pdf_export>")
  add_dependencies(export_job_tests pdf_export)
  add_test(NAME export_job_tests COMMAND export_job_tests)
  set_tests_properties(export_job_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME editor_pdf_tests COMMAND "${UV_EXECUTABLE}" run "${CMAKE_CURRENT_SOURCE_DIR}/tests/editor_pdf_tests.py"
    --editor "$<TARGET_FILE:apollo_editor>" --output "${CMAKE_CURRENT_BINARY_DIR}/editor-pdf-tests")
  set_tests_properties(editor_pdf_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PYTHONDONTWRITEBYTECODE=1")
  add_test(NAME autosave_crash_tests COMMAND "${UV_EXECUTABLE}" run "${CMAKE_CURRENT_SOURCE_DIR}/tests/autosave_crash_tests.py"
    --editor "$<TARGET_FILE:apollo_editor>" --source "${CMAKE_CURRENT_SOURCE_DIR}/prototype/The Last Broadcast.fountain"
    --output "${CMAKE_CURRENT_BINARY_DIR}/autosave-crash-tests")
  set_tests_properties(autosave_crash_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PYTHONDONTWRITEBYTECODE=1")
  add_test(NAME project_lock_tests COMMAND "${UV_EXECUTABLE}" run "${CMAKE_CURRENT_SOURCE_DIR}/tests/project_lock_tests.py"
    --editor "$<TARGET_FILE:apollo_editor>" --source "${CMAKE_CURRENT_SOURCE_DIR}/prototype/The Last Broadcast.fountain"
    --output "${CMAKE_CURRENT_BINARY_DIR}/project-lock-tests")
  set_tests_properties(project_lock_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PYTHONDONTWRITEBYTECODE=1" TIMEOUT 60)
  add_test(NAME fountain_export_process_tests COMMAND "${UV_EXECUTABLE}" run "${CMAKE_CURRENT_SOURCE_DIR}/tests/fountain_export_process_tests.py"
    --editor "$<TARGET_FILE:apollo_editor>" --flow-tests "$<TARGET_FILE:project_access_flow_tests>"
    --output "${CMAKE_CURRENT_BINARY_DIR}/fountain-export-process-tests")
  set_tests_properties(fountain_export_process_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PYTHONDONTWRITEBYTECODE=1" TIMEOUT 90)
  # aw-parser comes with the pinned afterwriting that tools/afterwriting-oracle.sh installs.
  if(DEFINED ENV{XDG_CACHE_HOME})
    set(apollo_cache "$ENV{XDG_CACHE_HOME}")
  else()
    set(apollo_cache "$ENV{HOME}/.cache")
  endif()
  set(APOLLO_AW_PARSER "${apollo_cache}/apollo/afterwriting/node_modules/.pnpm/aw-parser@0.1.1/node_modules/aw-parser"
      CACHE PATH "aw-parser 0.1.1, the independent Fountain export oracle")
  find_program(NODE_EXECUTABLE node)
  if(NODE_EXECUTABLE AND EXISTS "${APOLLO_AW_PARSER}/parser.js")
    add_test(NAME fountain_export_oracle_tests COMMAND "${UV_EXECUTABLE}" run "${CMAKE_CURRENT_SOURCE_DIR}/tests/fountain_export_process_tests.py"
      --editor "$<TARGET_FILE:apollo_editor>" --flow-tests "$<TARGET_FILE:project_access_flow_tests>"
      --output "${CMAKE_CURRENT_BINARY_DIR}/fountain-export-oracle-tests"
      --aw-parser "${APOLLO_AW_PARSER}" --node "${NODE_EXECUTABLE}")
    set_tests_properties(fountain_export_oracle_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PYTHONDONTWRITEBYTECODE=1" TIMEOUT 90)
  else()
    message(STATUS "Independent Fountain oracle not configured: run tools/afterwriting-oracle.sh once (needs node and pnpm), then reconfigure")
  endif()
  add_test(NAME project_access_process_tests COMMAND "${UV_EXECUTABLE}" run "${CMAKE_CURRENT_SOURCE_DIR}/tests/project_access_process_tests.py"
    --editor "$<TARGET_FILE:apollo_editor>" --output "${CMAKE_CURRENT_BINARY_DIR}/project-access-process-tests")
  set_tests_properties(project_access_process_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PYTHONDONTWRITEBYTECODE=1" TIMEOUT 90)
endif()
