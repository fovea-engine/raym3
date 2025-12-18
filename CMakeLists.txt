cmake_minimum_required(VERSION 3.20)
project(raym3 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RAYM3_USE_YOGA "Enable Yoga layout support" ON)
set(YOGA_DIR "" CACHE PATH "Path to Yoga directory (auto-detected if not set and yoga is a sibling directory)")
option(RAYM3_USE_INPUT_LAYERS "Enable layered input system with render queue" ON)
option(RAYM3_EMBED_RESOURCES "Embed SVG icons and fonts into the static library" OFF)
option(RAYM3_EMBED_ALL_ICONS "Embed all icons instead of only used ones (ignored if RAYM3_EMBED_RESOURCES is OFF)" OFF)


file(GLOB_RECURSE RAYM3_SOURCES
    "src/*.cpp"
)

file(GLOB_RECURSE RAYM3_HEADERS
    "include/raym3/*.h"
)

add_library(raym3 STATIC
    ${RAYM3_SOURCES}
    ${RAYM3_HEADERS}
)

target_include_directories(raym3 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(raym3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

include(FetchContent)

if(NOT TARGET raylib)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(raylib)
endif()

if(TARGET raylib)
    target_link_libraries(raym3 PUBLIC raylib)
else()
    message(FATAL_ERROR "raylib target not found. raym3 requires raylib.")
endif()

if(NOT ANDROID)
    add_executable(example_test
        examples/test.cpp
    )
    target_link_libraries(example_test PRIVATE raym3)
    set_target_properties(example_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    add_executable(example_layout
        examples/layout_test.cpp
    )
    target_link_libraries(example_layout PRIVATE raym3)

    add_executable(example_input_layers
        examples/input_layers_test.cpp
    )
    target_link_libraries(example_input_layers PRIVATE raym3)

    add_custom_target(examples
        DEPENDS example_test example_layout example_input_layers
    )
endif()

if(RAYM3_USE_YOGA)
    if(TARGET yogacore)
        message(STATUS "Yoga support enabled (using existing yogacore target)")
        target_link_libraries(raym3 PUBLIC yogacore)
        target_compile_definitions(raym3 PUBLIC RAYM3_USE_YOGA=1)
    else()
        if(NOT YOGA_DIR OR YOGA_DIR STREQUAL "")
            set(YOGA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../yoga")
        endif()
        
        if(EXISTS "${YOGA_DIR}/CMakeLists.txt")
            add_subdirectory(${YOGA_DIR} ${CMAKE_BINARY_DIR}/yoga EXCLUDE_FROM_ALL)
        endif()
        
        if(TARGET yogacore)
            target_link_libraries(raym3 PUBLIC yogacore)
            target_include_directories(raym3 PRIVATE ${YOGA_DIR})
            target_compile_definitions(raym3 PUBLIC RAYM3_USE_YOGA=1)
            message(STATUS "Yoga support enabled (found at ${YOGA_DIR})")
        else()
            message(WARNING "yogacore target not found. Building raym3 without Yoga support.")
            message(WARNING "  Set YOGA_DIR to the yoga directory, or ensure yoga is added as a subdirectory before raym3.")
            target_compile_definitions(raym3 PUBLIC RAYM3_USE_YOGA=0)
        endif()
    endif()
else()
    target_compile_definitions(raym3 PUBLIC RAYM3_USE_YOGA=0)
endif()

if(RAYM3_USE_INPUT_LAYERS)
    target_compile_definitions(raym3 PUBLIC RAYM3_USE_INPUT_LAYERS=1)
    message(STATUS "Input layers support enabled")
else()
    target_compile_definitions(raym3 PUBLIC RAYM3_USE_INPUT_LAYERS=0)
endif()




set_target_properties(raym3 PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

set(RAYM3_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/raym3/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/raym3/config.h"
    @ONLY
)

target_include_directories(raym3 PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

include(cmake/ExtractUsedIcons.cmake)

if(RAYM3_EMBED_RESOURCES)
    if(RAYM3_EMBED_ALL_ICONS)
        message(STATUS "Embedding all icons (RAYM3_EMBED_ALL_ICONS=ON)")
        file(GLOB_RECURSE ALL_SVG_FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/*/*.svg")
        set(USED_ICON_NAMES "")
        foreach(svg_file ${ALL_SVG_FILES})
            get_filename_component(icon_name "${svg_file}" NAME_WE)
            list(APPEND USED_ICON_NAMES "${icon_name}")
        endforeach()
        list(REMOVE_DUPLICATES USED_ICON_NAMES)
        list(LENGTH USED_ICON_NAMES ICON_COUNT)
        message(STATUS "Will embed all ${ICON_COUNT} unique icons")
    else()
        extract_used_icons("${CMAKE_CURRENT_SOURCE_DIR}/src" USED_ICON_NAMES)
        extract_used_icons("${CMAKE_CURRENT_SOURCE_DIR}/include" USED_ICON_NAMES_INCLUDE)
        list(APPEND USED_ICON_NAMES ${USED_ICON_NAMES_INCLUDE})
        list(REMOVE_DUPLICATES USED_ICON_NAMES)
        
        if(USED_ICON_NAMES)
            list(LENGTH USED_ICON_NAMES ICON_COUNT)
            message(STATUS "Found ${ICON_COUNT} unique icons used in code")
            message(STATUS "Icons to embed: ${USED_ICON_NAMES}")
        else()
            message(WARNING "No icons found in source code. Embedding all icons.")
            file(GLOB_RECURSE ALL_SVG_FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/*/*.svg")
            foreach(svg_file ${ALL_SVG_FILES})
                get_filename_component(icon_name "${svg_file}" NAME_WE)
                list(APPEND USED_ICON_NAMES "${icon_name}")
            endforeach()
            list(REMOVE_DUPLICATES USED_ICON_NAMES)
        endif()
    endif()
    
    set(EMBEDDED_RESOURCES_H "${CMAKE_CURRENT_BINARY_DIR}/src/EmbeddedResources.h")
    file(WRITE "${EMBEDDED_RESOURCES_H}" "#pragma once\n\n")
    file(APPEND "${EMBEDDED_RESOURCES_H}" "#include <string>\n#include <unordered_map>\n\n")
    file(APPEND "${EMBEDDED_RESOURCES_H}" "namespace raym3 {\n\n")
    
    if(USED_ICON_NAMES)
        file(APPEND "${EMBEDDED_RESOURCES_H}" "// Embedded SVG Icons (only used icons)\n")
        file(APPEND "${EMBEDDED_RESOURCES_H}" "static std::unordered_map<std::string, std::string> embedded_icons = {\n")
        
        set(ICON_VARIATIONS filled outlined round sharp two-tone)
        set(ICON_COUNT 0)
        
        foreach(icon_name ${USED_ICON_NAMES})
            foreach(variation ${ICON_VARIATIONS})
                set(svg_file "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/${variation}/${icon_name}.svg")
                if(EXISTS "${svg_file}")
                    file(READ "${svg_file}" svg_content)
                    string(REPLACE "\\" "\\\\" escaped_content "${svg_content}")
                    string(REPLACE "\"" "\\\"" escaped_content "${escaped_content}")
                    string(REPLACE "\n" "\\n" escaped_content "${escaped_content}")
                    string(REPLACE "\r" "" escaped_content "${escaped_content}")
                    
                    file(APPEND "${EMBEDDED_RESOURCES_H}" "    {\"${variation}/${icon_name}\", \"${escaped_content}\"},\n")
                    math(EXPR ICON_COUNT "${ICON_COUNT} + 1")
                endif()
            endforeach()
        endforeach()
        
        file(APPEND "${EMBEDDED_RESOURCES_H}" "};\n\n")
        message(STATUS "Embedded ${ICON_COUNT} icon files (${USED_ICON_NAMES} icons × 5 variations)")
    else()
        file(APPEND "${EMBEDDED_RESOURCES_H}" "// No icons found to embed\n")
        file(APPEND "${EMBEDDED_RESOURCES_H}" "static std::unordered_map<std::string, std::string> embedded_icons = {};\n\n")
    endif()
    
    file(APPEND "${EMBEDDED_RESOURCES_H}" "} // namespace raym3\n")
    
    target_compile_definitions(raym3 PRIVATE RAYM3_EMBED_RESOURCES=1)
    target_include_directories(raym3 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src")
else()
    target_compile_definitions(raym3 PRIVATE RAYM3_EMBED_RESOURCES=0)
endif()

install(DIRECTORY resources/
    DESTINATION share/raym3/resources
    FILES_MATCHING PATTERN "*.ttf" PATTERN "*.svg"
)




