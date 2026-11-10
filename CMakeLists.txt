cmake_minimum_required(VERSION 3.19)
project(aquarium CXX)

# ---------------------------------------------------------------------------
#  AQUARIUM - a transparent desktop fish tank
#
#  The animals come from the sea creature generator in
#  ../RoguelikeGame/src/creature/sea, copied verbatim into src/creature/sea.
#  The window is a per-pixel-alpha layered window built the way VRMPet builds
#  its overlay (see src/transparent_window.h).
#
#    cmake -S . -B build -A x64
#    cmake --build build --config Release
#    build/run/aquarium.exe
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SEA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/creature/sea/*.cpp")

add_executable(aquarium WIN32
    src/main.cpp
    src/tank.cpp
    src/transparent_window.cpp
    src/creature/CreatureSheetExport.cpp
    ${SEA_SOURCES}
)

target_include_directories(aquarium PRIVATE src)

# user32 - the layered window, hotkeys, menus
# gdi32  - the DIB section the layered window is fed from
# shell32 - CommandLineToArgvW
target_link_libraries(aquarium PRIVATE user32 gdi32 shell32)

if(MSVC)
    target_compile_options(aquarium PRIVATE /W3 /permissive- /utf-8)
    target_compile_definitions(aquarium PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(aquarium PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/run"
)
