cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version} LANGUAGES C CXX)

# This plugin needs the frontend API (dock + streaming events) and Qt Widgets.
option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

find_package(obs-frontend-api REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)
if(MSVC)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /utf-8)
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/core/Models.cpp
    src/core/StreamTimer.cpp
    src/core/StorageManager.cpp
    src/core/CategoryManager.cpp
    src/core/SessionManager.cpp
    src/core/ExportManager.cpp
    src/hotkeys/HotkeyManager.cpp
    src/ui/MarkerToast.cpp
    src/ui/MarkerButton.cpp
    src/ui/CategoryDialogs.cpp
    src/ui/SessionsDialog.cpp
    src/ui/SettingsDialog.cpp
    src/ui/MarkerDock.cpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# Optional: headless test of the core logic (no OBS needed). cmake -DBUILD_SMOKE_TEST=ON
option(BUILD_SMOKE_TEST "Build the core logic smoke test" OFF)
if(BUILD_SMOKE_TEST)
  add_executable(core-smoke-test tests/core_smoke_test.cpp
    src/core/Models.cpp src/core/StreamTimer.cpp src/core/StorageManager.cpp
    src/core/CategoryManager.cpp src/core/SessionManager.cpp src/core/ExportManager.cpp)
  target_include_directories(core-smoke-test PRIVATE src)
  target_link_libraries(core-smoke-test PRIVATE Qt6::Core)
  target_compile_features(core-smoke-test PRIVATE cxx_std_17)
  set_target_properties(core-smoke-test PROPERTIES AUTOMOC ON)
  if(MSVC)
    target_compile_options(core-smoke-test PRIVATE /utf-8)
  endif()
endif()
