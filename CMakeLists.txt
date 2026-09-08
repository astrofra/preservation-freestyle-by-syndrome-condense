cmake_minimum_required(VERSION 3.21)
project(freestyle_preservation VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(klx_unpack klx_unpack.c)
set_target_properties(klx_unpack PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin$<0:>"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

if(MSVC)
    target_compile_options(klx_unpack PRIVATE /W4 /WX)
else()
    target_compile_options(klx_unpack PRIVATE -Wall -Wextra -Werror)
    if(MINGW)
        target_link_options(klx_unpack PRIVATE -municode)
    endif()
endif()

option(FREESTYLE_BUILD_PLAYER "Build the portable Freestyle reconstruction" ON)
if(FREESTYLE_BUILD_PLAYER)
    add_subdirectory(src)
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(FREESTYLE_BUILD_PLAYER AND Python3_Interpreter_FOUND)
    add_custom_target(freestyle_web
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_web.py"
            --bin-dir "$<TARGET_FILE_DIR:freestyle>" --package
        DEPENDS freestyle_export_web
        COMMENT "Preparing the standalone JavaScript/WebGL site")
endif()

include(CTest)
if(BUILD_TESTING)
    if(FREESTYLE_BUILD_PLAYER)
        add_executable(freestyle_scene_tests tests/scene.cpp)
        target_link_libraries(freestyle_scene_tests PRIVATE freestyle_scene)
        add_test(NAME freestyle_motion COMMAND freestyle_scene_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/demo-assets/cds-freestyle")
        add_test(NAME freestyle_assets COMMAND freestyle --verify-assets --assets
            "${CMAKE_CURRENT_SOURCE_DIR}/demo-assets/cds-freestyle")
    endif()
    if(Python3_Interpreter_FOUND)
        add_test(NAME klx_regression COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/unpack.py" "$<TARGET_FILE:klx_unpack>")
    endif()
endif()
