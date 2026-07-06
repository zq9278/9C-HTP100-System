cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME 9C-HTP100-screen)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Restore the user sources that existed in the original MDK project.
file(GLOB_RECURSE USER_HARDWARE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/HardWare/Src/*.c
)

file(GLOB_RECURSE USER_LVGL_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/src/*.c
)

foreach(EXCLUDED_LVGL_DIR IN ITEMS
    "/draw/arm2d/"
    "/draw/nxp/"
    "/draw/renesas/"
    "/draw/sdl/"
    "/draw/swm341_dma2d/"
    "/extra/libs/ffmpeg/"
    "/extra/libs/freetype/"
    "/extra/libs/fsdrv/"
    "/extra/libs/rlottie/"
)
    list(FILTER USER_LVGL_SOURCES EXCLUDE REGEX "${EXCLUDED_LVGL_DIR}")
endforeach()

file(GLOB_RECURSE USER_LVGL_PORT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/porting/*.c
)
list(FILTER USER_LVGL_PORT_SOURCES EXCLUDE REGEX "/lv_port_fs_template\\.c$")
list(FILTER USER_LVGL_PORT_SOURCES EXCLUDE REGEX "/lv_port_disp_template copy\\.c$")

file(GLOB_RECURSE USER_UI_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/demos/slk/*.c
)
list(FILTER USER_UI_SOURCES EXCLUDE REGEX "/images_all\\.c$")

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${USER_HARDWARE_SOURCES}
    ${USER_LVGL_SOURCES}
    ${USER_LVGL_PORT_SOURCES}
    ${USER_UI_SOURCES}
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/HardWare/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/porting
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/demos/slk
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/demos/slk/components
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/demos/slk/fonts
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/demos/slk/images
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/demos/slk/screens
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    LV_CONF_INCLUDE_SIMPLE
    STM32H7
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# Generate a raw binary image alongside the ELF after linking.
if(CMAKE_OBJCOPY)
    set(BINARY_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin)
    set(HEX_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex)

    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${BINARY_OUTPUT_FILE}
        COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${HEX_OUTPUT_FILE}
        BYPRODUCTS
            ${BINARY_OUTPUT_FILE}
            ${HEX_OUTPUT_FILE}
        COMMENT "Generating ${CMAKE_PROJECT_NAME}.bin and ${CMAKE_PROJECT_NAME}.hex"
    )

    set_property(TARGET ${CMAKE_PROJECT_NAME} APPEND PROPERTY
        ADDITIONAL_CLEAN_FILES
        ${BINARY_OUTPUT_FILE}
        ${HEX_OUTPUT_FILE}
    )
endif()
