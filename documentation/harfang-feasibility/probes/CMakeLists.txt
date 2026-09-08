cmake_minimum_required(VERSION 3.20)
project(freestyle_harfang_animation_probe LANGUAGES CXX)
get_filename_component(FREESTYLE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(HARFANG_SOURCE "${FREESTYLE_ROOT}/analysis/harfang-feasibility/source" CACHE PATH "Pinned HARFANG source subset")
add_executable(animation_probe animation_probe.cpp
    "${FREESTYLE_ROOT}/src/scene.cpp" "${FREESTYLE_ROOT}/src/lwob.cpp")
target_compile_features(animation_probe PRIVATE cxx_std_17)
target_include_directories(animation_probe PRIVATE "${FREESTYLE_ROOT}/src"
    "${HARFANG_SOURCE}/harfang" "${HARFANG_SOURCE}/extern")
if(MSVC)
    target_compile_options(animation_probe PRIVATE /utf-8)
endif()
