cmake_minimum_required(VERSION 3.22)

project(NitroKV LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
include(FetchContent)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Sanitizers)
include(StaticAnalyzers)
include(CompilerWarnings)
include(ProjectOptions)


add_executable(NitroKV main.cpp)
nitrokv_setup_options(NitroKV)
nitrokv_enable_sanitizers(NitroKV)
nitrokv_enable_clang_tidy(NitroKV)
nitrokv_set_warnings(NitroKV)

