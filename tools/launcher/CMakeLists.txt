# Launcher of the native build (docs/LAUNCHER.md). Dear ImGui 1.91.8 from the engine tree, its SDL3
# backends from build/deps (tools/fetch_deps.sh). Build:
#   cmake -S tools/launcher -B build/launcher -G Ninja -DCMAKE_CXX_COMPILER=clang++ && ninja -C build/launcher
cmake_minimum_required(VERSION 3.24)
project(xray_launcher CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(IMGUI_DIR "${ROOT}/monolith/src/3rd party/imgui" CACHE PATH "Dear ImGui sources (engine tree)")
set(IMGUI_SDL3_DIR "${ROOT}/build/deps/imgui-sdl3-1.91.8" CACHE PATH "ImGui SDL3 backends")
if(NOT EXISTS "${IMGUI_SDL3_DIR}/imgui_impl_sdl3.cpp")
    message(FATAL_ERROR "ImGui SDL3 backends missing in ${IMGUI_SDL3_DIR} (run tools/fetch_deps.sh)")
endif()
find_package(SDL3 REQUIRED CONFIG)

add_executable(xray-launcher main.cpp core.cpp
    ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_custom.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_SDL3_DIR}/imgui_impl_sdl3.cpp ${IMGUI_SDL3_DIR}/imgui_impl_sdlrenderer3.cpp)
target_include_directories(xray-launcher PRIVATE ${IMGUI_DIR} ${IMGUI_SDL3_DIR})
target_link_libraries(xray-launcher PRIVATE SDL3::SDL3)
