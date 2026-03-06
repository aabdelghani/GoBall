cmake_minimum_required(VERSION 3.15)

# Cross-compilation toolchain configuration
if(NOT YOCTO_BUILD)
    set(CMAKE_TOOLCHAIN_FILE toolchain-aarch64.cmake)
endif()

# Set C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

project(goball C)

if(YOCTO_BUILD)
    add_compile_definitions(YOCTO_BUILD=1)
endif()

# Debug configuration options
option(ENABLE_DEBUG "Enable debug system" ON)
option(ENABLE_DEBUG_COLORS "Enable colored debug output" ON)
option(ENABLE_DEBUG_TIMESTAMP "Enable timestamps in debug output" ON)
option(ENABLE_DEBUG_TRACE "Enable trace-level debugging" OFF)

# Build type configuration with debug levels
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Set debug levels based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG=1)
    if(ENABLE_DEBUG_TRACE)
        add_compile_definitions(DEBUG_LEVEL=5)  # TRACE level
        message(STATUS "Debug level: TRACE (5)")
    else()
        add_compile_definitions(DEBUG_LEVEL=4)  # DEBUG level  
        message(STATUS "Debug level: DEBUG (4)")
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(DEBUG_LEVEL=2)  # WARN level
    message(STATUS "Debug level: WARN (2)")
else()
    add_compile_definitions(DEBUG_LEVEL=3)  # INFO level (default for other types)
    message(STATUS "Debug level: INFO (3)")
endif()

# Debug features
if(ENABLE_DEBUG_COLORS)
    add_compile_definitions(DEBUG_COLORS=1)
    message(STATUS "Debug colors: ENABLED")
endif()

if(ENABLE_DEBUG_TIMESTAMP)
    add_compile_definitions(DEBUG_TIMESTAMP=1)
    message(STATUS "Debug timestamps: ENABLED")
endif()

# Find and link libgpiod using pkg-config from sysroot
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGPIOD REQUIRED libgpiod)

# SDL2 configuration
if(NOT YOCTO_BUILD)
    set(SDL_FOLDER sdl2-dev-rpi64)
    set(SDL_MIXER_FOLDER sdl2-dev-rpi64)
endif()

# Include directories - ADD DEBUG DIRECTORY
include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/lvgl
    ${PROJECT_SOURCE_DIR}/ui
    ${PROJECT_SOURCE_DIR}/modules/game_modes
    ${PROJECT_SOURCE_DIR}/modules/logic
    ${PROJECT_SOURCE_DIR}/modules/ui_logic
    ${PROJECT_SOURCE_DIR}/modules/sound_logic
    ${PROJECT_SOURCE_DIR}/modules/led_logic
    ${PROJECT_SOURCE_DIR}/utils/piolib/include
    ${PROJECT_SOURCE_DIR}/modules/debug  # Add debug include path
    ${LIBGPIOD_INCLUDE_DIRS}
)
if(NOT YOCTO_BUILD)
    include_directories(${PROJECT_SOURCE_DIR}/${SDL_FOLDER}/include)
endif()

# Add piolib subdirectory
add_subdirectory(utils/piolib)

# Gather source files - ADD DEBUG SOURCES
FILE(GLOB_RECURSE LVGL_Sources CONFIGURE_DEPENDS lvgl/*.c)
FILE(GLOB_RECURSE UI_Sources CONFIGURE_DEPENDS ui/*.c ui/*.cpp)
FILE(GLOB_RECURSE LOGIC_Sources CONFIGURE_DEPENDS modules/logic/*.c)
FILE(GLOB_RECURSE UI_LOGIC_Sources CONFIGURE_DEPENDS modules/ui_logic/*.c)
FILE(GLOB_RECURSE SOUND_LOGIC_Sources CONFIGURE_DEPENDS modules/sound_logic/*.c)
FILE(GLOB_RECURSE LED_LOGIC_Sources CONFIGURE_DEPENDS modules/led_logic/*.c)
FILE(GLOB_RECURSE GAME_MODE_Sources CONFIGURE_DEPENDS modules/game_modes/*.c)
FILE(GLOB_RECURSE DEBUG_Sources CONFIGURE_DEPENDS modules/debug/*.c)  # Add debug sources
FILE(GLOB_RECURSE GAME_VIDEOS_Sources CONFIGURE_DEPENDS modules/game_videos/*.c)
FILE(GLOB_RECURSE PLAYER_NAME_Sources CONFIGURE_DEPENDS modules/player_name/*.c)
FILE(GLOB_RECURSE WIFI_MANAGER_Sources CONFIGURE_DEPENDS modules/wifi_manager/*.c)

# Sound directory path (configurable for Yocto vs development)
if(SOUND_DIR_PATH)
    add_compile_definitions(SOUND_DIR="${SOUND_DIR_PATH}")
endif()

# Create executable - ADD DEBUG SOURCES
add_executable(${PROJECT_NAME}
    main.c
    ${LVGL_Sources}
    ${UI_Sources}
    ${LOGIC_Sources}
    ${UI_LOGIC_Sources}
    ${SOUND_LOGIC_Sources}
    ${LED_LOGIC_Sources}
    ${GAME_MODE_Sources}
    ${DEBUG_Sources}  # Include debug sources
    ${GAME_VIDEOS_Sources}
    ${PLAYER_NAME_Sources}
    ${WIFI_MANAGER_Sources}
)

# Link directories and libraries
if(NOT YOCTO_BUILD)
    target_link_directories(${PROJECT_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/${SDL_FOLDER}/lib
        ${PROJECT_SOURCE_DIR}/rpi5-sysroot/usr/lib/aarch64-linux-gnu
    )
endif()

target_link_libraries(${PROJECT_NAME} 
    SDL2
    SDL2_mixer
    pio
    ${LIBGPIOD_LIBRARIES}
    m  # Add math library for debug timestamps if needed
)

# Set RPATH for runtime library resolution on RPi5
set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH TRUE
    SKIP_BUILD_RPATH FALSE
)