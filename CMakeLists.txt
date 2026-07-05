cmake_minimum_required(VERSION 3.20)
project(raym3 VERSION 1.0.0 LANGUAGES CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RAYM3_ENABLE_RTL "Enable RTL/bidi via HarfBuzz+SheenBidi (increases binary size)" OFF)
option(RAYM3_FETCH_DEPS "Fetch raym3 third-party dependencies when targets are not provided" ON)
option(RAYM3_BUILD_EXAMPLES "Build raym3 example applications" ON)
option(RAYM3_BUILD_TESTS "Build raym3 tests" OFF)
set(RAYM3_RAYLIB_TAG "6.0" CACHE STRING "raylib git tag/branch/commit to fetch when raylib target is not provided")
set(RAYM3_YOGA_TAG "v3.2.1" CACHE STRING "Yoga git tag/branch/commit to fetch when yogacore target is not provided")
set(RAYM3_ICON_MANIFEST "" CACHE FILEPATH "Optional newline-delimited icon manifest to embed/install when not embedding all icons")
option(RAYM3_INSTALL_ALL_ICONS "Install the full Material icon SVG catalog instead of the core manifest subset" OFF)
set(YOGA_DIR "" CACHE PATH "Path to Yoga directory (auto-detected if not set and yoga is a sibling directory)")
    option(RAYM3_USE_INPUT_LAYERS "Enable layered input system with render queue" ON)

    if(EMSCRIPTEN)
        option(RAYM3_EMBED_RESOURCES "Embed SVG icons and fonts into the static library" ON)
    else()
        option(RAYM3_EMBED_RESOURCES "Embed SVG icons and fonts into the static library" OFF)
    endif()
option(RAYM3_EMBED_ALL_ICONS "Embed all icons instead of only used ones (ignored if RAYM3_EMBED_RESOURCES is OFF)" OFF)


file(GLOB_RECURSE RAYM3_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
)

if(APPLE)
    list(APPEND RAYM3_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/SystemAppearance.mm")
    list(APPEND RAYM3_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/v2/EmojiRasterizerApple.mm")
endif()

file(GLOB_RECURSE RAYM3_HEADERS CONFIGURE_DEPENDS
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
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
)

include(FetchContent)

if(NOT TARGET raylib AND RAYM3_FETCH_DEPS)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG ${RAYM3_RAYLIB_TAG}
    )
    FetchContent_MakeAvailable(raylib)
endif()

if(TARGET raylib)
    target_link_libraries(raym3 PUBLIC raylib)
else()
    message(FATAL_ERROR "raylib target not found. raym3 requires raylib.")
endif()

if(RAYM3_BUILD_EXAMPLES AND NOT ANDROID)
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

    add_executable(example_v2_demo
        examples/v2_demo.cpp
    )
    target_link_libraries(example_v2_demo PRIVATE raym3)

    add_custom_target(examples
        DEPENDS example_test example_layout example_input_layers example_v2_demo
    )
endif()

if(RAYM3_BUILD_TESTS)
    enable_testing()
    add_executable(text_input_selection_test
        tests/text_input_selection_test.cpp
    )
    target_link_libraries(text_input_selection_test PRIVATE raym3)
    add_test(NAME text_input_selection_test COMMAND text_input_selection_test)
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
        elseif(RAYM3_FETCH_DEPS)
            FetchContent_Declare(
                yoga
                GIT_REPOSITORY https://github.com/facebook/yoga.git
                GIT_TAG ${RAYM3_YOGA_TAG}
                SOURCE_SUBDIR yoga
            )
            FetchContent_MakeAvailable(yoga)
        endif()
        
        if(TARGET yogacore)
            target_link_libraries(raym3 PUBLIC yogacore)
            target_include_directories(raym3 PRIVATE ${YOGA_DIR})
            target_compile_definitions(raym3 PUBLIC RAYM3_USE_YOGA=1)
            message(STATUS "Yoga support enabled")
        else()
            if(RAYM3_FETCH_DEPS)
                message(FATAL_ERROR "RAYM3_USE_YOGA=ON but yogacore target was not created. Set YOGA_DIR, provide a yogacore target, or set RAYM3_USE_YOGA=OFF.")
            else()
                message(WARNING "RAYM3_USE_YOGA=ON but RAYM3_FETCH_DEPS=OFF and yogacore target was not found. Building without Yoga support.")
                target_compile_definitions(raym3 PUBLIC RAYM3_USE_YOGA=0)
            endif()
        endif()
    endif()
else()
    target_compile_definitions(raym3 PUBLIC RAYM3_USE_YOGA=0)
endif()

option(RAYM3_WEBGPU "Target the rlwg WebGPU backend on web (emulates GL 3.3) instead of WebGL2/ES3" OFF)
if(RAYM3_WEBGPU)
    target_compile_definitions(raym3 PUBLIC RAYM3_WEBGPU=1)
endif()
if(EMSCRIPTEN AND NOT RAYM3_WEBGPU)
    # Default web path is WebGL2 via GLES3. With the rlwg WebGPU backend the raylib
    # target is compiled for GRAPHICS_API_OPENGL_33 instead (set by raylib_backend_attach),
    # so do NOT force ES3 here — the two defines would conflict.
    target_compile_definitions(raym3 PUBLIC GRAPHICS_API_OPENGL_ES3)
endif()

if(RAYM3_USE_INPUT_LAYERS)
    target_compile_definitions(raym3 PUBLIC RAYM3_USE_INPUT_LAYERS=1)
    message(STATUS "Input layers support enabled")
else()
    target_compile_definitions(raym3 PUBLIC RAYM3_USE_INPUT_LAYERS=0)
endif()

if(RAYM3_ENABLE_RTL)
    target_compile_definitions(raym3 PUBLIC RAYM3_ENABLE_RTL=1)
else()
    target_compile_definitions(raym3 PUBLIC RAYM3_ENABLE_RTL=0)
endif()

if(APPLE)
    find_library(RAYM3_APPKIT_FRAMEWORK AppKit REQUIRED)
    find_library(RAYM3_CORETEXT_FRAMEWORK CoreText REQUIRED)
    find_library(RAYM3_COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
    target_link_libraries(raym3 PRIVATE
        ${RAYM3_APPKIT_FRAMEWORK}
        ${RAYM3_CORETEXT_FRAMEWORK}
        ${RAYM3_COREGRAPHICS_FRAMEWORK}
    )
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
        set(EFFECTIVE_ICON_MANIFEST "${RAYM3_ICON_MANIFEST}")
        if(NOT EFFECTIVE_ICON_MANIFEST)
            set(EFFECTIVE_ICON_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/core-icons.txt")
        endif()
        if(EXISTS "${EFFECTIVE_ICON_MANIFEST}")
            file(STRINGS "${EFFECTIVE_ICON_MANIFEST}" USED_ICON_NAMES)
            list(FILTER USED_ICON_NAMES EXCLUDE REGEX "^[ \t]*($|#)")
            list(TRANSFORM USED_ICON_NAMES STRIP)
            list(REMOVE_DUPLICATES USED_ICON_NAMES)
            list(LENGTH USED_ICON_NAMES ICON_COUNT)
            message(STATUS "Embedding ${ICON_COUNT} icons from ${EFFECTIVE_ICON_MANIFEST}")
        else()
            message(FATAL_ERROR "Icon manifest not found: ${EFFECTIVE_ICON_MANIFEST}. Set RAYM3_ICON_MANIFEST or enable RAYM3_EMBED_ALL_ICONS.")
        endif()
    endif()
    
    set(EMBEDDED_RESOURCES_H "${CMAKE_CURRENT_BINARY_DIR}/src/EmbeddedResources.h")
    file(WRITE "${EMBEDDED_RESOURCES_H}" "#pragma once\n\n")
    file(APPEND "${EMBEDDED_RESOURCES_H}" "#include <string>\n#include <unordered_map>\n\n")
    file(APPEND "${EMBEDDED_RESOURCES_H}" "namespace raym3 {\n\n")
    
    if(USED_ICON_NAMES)
        file(APPEND "${EMBEDDED_RESOURCES_H}" "// Embedded SVG Icons (only used icons)\n")
        file(APPEND "${EMBEDDED_RESOURCES_H}" "struct EmbeddedAsset { const char* path; const char* content; };\n\n")
        file(APPEND "${EMBEDDED_RESOURCES_H}" "static const EmbeddedAsset embedded_assets[] = {\n")
        
        set(ICON_VARIATIONS filled outlined round sharp two-tone)
        set(ICON_COUNT 0)
        
        foreach(icon_name ${USED_ICON_NAMES})
            foreach(variation ${ICON_VARIATIONS})
                set(svg_file "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/${variation}/${icon_name}.svg")
                if(EXISTS "${svg_file}")
                    file(READ "${svg_file}" svg_content)
                    string(REPLACE "\\" "\\\\" escaped_content "${svg_content}")
                    string(REPLACE "\"" "\\\"" escaped_content "${escaped_content}")
                    string(REGEX REPLACE ">[ \t\r\n]+<" "><" escaped_content "${escaped_content}")
                    string(REGEX REPLACE "[ \t\r\n]+" " " escaped_content "${escaped_content}")
                    string(REPLACE "\n" "\\n" escaped_content "${escaped_content}")
                    string(REPLACE "\r" "" escaped_content "${escaped_content}")
                    
                    file(APPEND "${EMBEDDED_RESOURCES_H}" "    {\"${variation}/${icon_name}\", \"${escaped_content}\"},\n")
                    math(EXPR ICON_COUNT "${ICON_COUNT} + 1")
                endif()
            endforeach()
        endforeach()
        
        file(APPEND "${EMBEDDED_RESOURCES_H}" "    {nullptr, nullptr}\n};\n\n")
        message(STATUS "Embedded ${ICON_COUNT} icon files.")
    else()
        file(APPEND "${EMBEDDED_RESOURCES_H}" "// No icons found to embed\n")
        file(APPEND "${EMBEDDED_RESOURCES_H}" "struct EmbeddedAsset { const char* path; const char* content; };\n")
        file(APPEND "${EMBEDDED_RESOURCES_H}" "static const EmbeddedAsset embedded_assets[] = { {nullptr, nullptr} };\n\n")
    endif()
    
    file(APPEND "${EMBEDDED_RESOURCES_H}" "} // namespace raym3\n")
    
    target_compile_definitions(raym3 PRIVATE RAYM3_EMBED_RESOURCES=1)
    target_include_directories(raym3 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src")
else()
    target_compile_definitions(raym3 PRIVATE RAYM3_EMBED_RESOURCES=0)
endif()

install(DIRECTORY resources/fonts
    DESTINATION share/raym3/resources
    FILES_MATCHING PATTERN "*.ttf"
)

if(RAYM3_INSTALL_ALL_ICONS)
    install(DIRECTORY resources/icons
        DESTINATION share/raym3/resources
        FILES_MATCHING PATTERN "*.svg" PATTERN "*.txt"
    )
else()
    set(EFFECTIVE_INSTALL_ICON_MANIFEST "${RAYM3_ICON_MANIFEST}")
    if(NOT EFFECTIVE_INSTALL_ICON_MANIFEST)
        set(EFFECTIVE_INSTALL_ICON_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/core-icons.txt")
    endif()
    if(EXISTS "${EFFECTIVE_INSTALL_ICON_MANIFEST}")
        install(FILES "${EFFECTIVE_INSTALL_ICON_MANIFEST}"
                DESTINATION share/raym3/resources/icons
                RENAME core-icons.txt)
        file(STRINGS "${EFFECTIVE_INSTALL_ICON_MANIFEST}" INSTALL_ICON_NAMES)
        list(FILTER INSTALL_ICON_NAMES EXCLUDE REGEX "^[ \t]*($|#)")
        list(TRANSFORM INSTALL_ICON_NAMES STRIP)
        list(REMOVE_DUPLICATES INSTALL_ICON_NAMES)
        set(ICON_VARIATIONS filled outlined round sharp two-tone)
        foreach(icon_name ${INSTALL_ICON_NAMES})
            foreach(variation ${ICON_VARIATIONS})
                set(icon_file "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/${variation}/${icon_name}.svg")
                if(EXISTS "${icon_file}")
                    install(FILES "${icon_file}"
                            DESTINATION "share/raym3/resources/icons/${variation}")
                endif()
            endforeach()
        endforeach()
    endif()
endif()
