set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
if(UNIX AND NOT APPLE)
    option(GLFW_BUILD_WAYLAND "Enable the additional GLFW Wayland backend" OFF)
endif()
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(../vendor/glfw ${CMAKE_BINARY_DIR}/vendor/glfw)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

# libxm v0.2's upstream build assumes GCC; compile the unchanged C sources here.
add_library(freestyle_xm STATIC
    ../vendor/libxm/src/xm.c ../vendor/libxm/src/load.c
    ../vendor/libxm/src/play.c ../vendor/libxm/src/context.c)
target_include_directories(freestyle_xm PUBLIC ../vendor/libxm/include)
target_compile_definitions(freestyle_xm PRIVATE
    XM_DEBUG=0 XM_DEFENSIVE=1 XM_BIG_ENDIAN=0
    XM_LINEAR_INTERPOLATION=1 XM_RAMPING=1 XM_STRINGS=1)
set_target_properties(freestyle_xm PROPERTIES C_STANDARD 11)

add_library(freestyle_scene STATIC scene.cpp lwob.cpp)
target_include_directories(freestyle_scene PUBLIC .)
add_executable(freestyle main.cpp renderer.cpp audio.cpp)
target_include_directories(freestyle PRIVATE
    ../vendor/nxng/restoration/third_party ../vendor/miniaudio)
target_link_libraries(freestyle PRIVATE freestyle_scene freestyle_xm
    glfw OpenGL::GL Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX AND NOT APPLE)
    target_link_libraries(freestyle PRIVATE m)
endif()
if(MSVC)
    target_compile_options(freestyle PRIVATE /W4 /utf-8)
    target_compile_options(freestyle_scene PRIVATE /W4 /utf-8)
    target_compile_definitions(freestyle PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    target_compile_options(freestyle PRIVATE -Wall -Wextra)
    target_compile_options(freestyle_scene PRIVATE -Wall -Wextra)
endif()
if(APPLE)
    target_compile_definitions(freestyle PRIVATE GL_SILENCE_DEPRECATION=1)
endif()
set_target_properties(freestyle PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin$<0:>")
install(TARGETS freestyle RUNTIME DESTINATION .)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/demo-assets/cds-freestyle/" DESTINATION assets)
install(FILES "${PROJECT_SOURCE_DIR}/documentation/PLAYER.md" DESTINATION . RENAME README.md)
install(FILES "${PROJECT_SOURCE_DIR}/vendor/nxng/LICENSE" DESTINATION licenses RENAME nxng-GPL-3.0.txt)
install(FILES "${PROJECT_SOURCE_DIR}/vendor/glfw/LICENSE.md" DESTINATION licenses RENAME glfw.txt)
install(FILES "${PROJECT_SOURCE_DIR}/vendor/libxm/COPYING" DESTINATION licenses RENAME libxm.txt)
install(FILES "${PROJECT_SOURCE_DIR}/vendor/miniaudio/LICENSE" DESTINATION licenses RENAME miniaudio.txt)
install(FILES "${PROJECT_SOURCE_DIR}/vendor/stb-LICENSE.txt" DESTINATION licenses)
install(FILES "${PROJECT_SOURCE_DIR}/vendor/SOURCES.json" DESTINATION licenses)
