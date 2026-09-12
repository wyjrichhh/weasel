cmake_minimum_required(VERSION 3.21)
project(BangkeSettings LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# rime.dll 使用静态 MSVC 运行库，本程序须保持一致
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT DEFINED VERSION_MAJOR)
  set(VERSION_MAJOR 0)
  set(VERSION_MINOR 1)
  set(VERSION_PATCH 0)
endif()
if(NOT DEFINED VERSION_BUILD)
  set(VERSION_BUILD 0)
endif()

if(NOT DEFINED ENV{BOOST_ROOT})
  message(FATAL_ERROR "BOOST_ROOT not set; run from env.bat / build.bat")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

set(BANGKE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
if(NOT EXISTS ${BANGKE_ROOT}/lib64/rime.lib)
  message(FATAL_ERROR "${BANGKE_ROOT}/lib64/rime.lib not found; run 'build.bat rime' first")
endif()

qt_add_executable(BangkeSettings WIN32
  main.cpp
  Theme.cpp Theme.h
  AIPage.cpp
  Configurator.cpp
  GeneralPage.cpp Configurator.h
  UIStyleSettings.cpp UIStyleSettings.h
  MainWindow.cpp MainWindow.h
  SwitcherPage.cpp SwitcherPage.h
  StylePage.cpp StylePage.h
  DictPage.cpp DictPage.h
  BangkeSettings.rc
  ${BANGKE_ROOT}/BangkeIPC/BangkeClientImpl.cpp
  ${BANGKE_ROOT}/BangkeIPC/PipeChannel.cpp
  ${BANGKE_ROOT}/BangkeRime/BangkeUtility.cpp
)

target_include_directories(BangkeSettings PRIVATE
  ${BANGKE_ROOT}/include
  ${BANGKE_ROOT}/BangkeIPC
  ${BANGKE_ROOT}/BangkeRime
  ${BANGKE_ROOT}/librime/include
  $ENV{BOOST_ROOT}
)

target_link_directories(BangkeSettings PRIVATE
  $ENV{BOOST_ROOT}/stage/lib
  ${BANGKE_ROOT}/lib64
)

target_link_libraries(BangkeSettings PRIVATE Qt6::Widgets rime.lib)

target_compile_definitions(BangkeSettings PRIVATE
  VERSION_MAJOR=${VERSION_MAJOR}
  VERSION_MINOR=${VERSION_MINOR}
  VERSION_PATCH=${VERSION_PATCH}
  VERSION_BUILD=${VERSION_BUILD}
)

target_compile_options(BangkeSettings PRIVATE /utf-8 /Zc:threadSafeInit-)
