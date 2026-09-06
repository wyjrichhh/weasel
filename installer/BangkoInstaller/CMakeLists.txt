cmake_minimum_required(VERSION 3.21)
project(BangkoInstaller LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(BangkoInstaller WIN32
  main.cpp
  app.manifest
)

target_link_libraries(BangkoInstaller PRIVATE Qt6::Widgets msi.lib)
target_compile_options(BangkoInstaller PRIVATE /utf-8)

# Qt 运行库与 BangkoSettings 同目录共存（output\），无需单独 windeployqt
add_custom_command(TARGET BangkoInstaller POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:BangkoInstaller> $ENV{WEASEL_OUT})
