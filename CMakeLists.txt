cmake_minimum_required(VERSION 3.15)

project(BlipExtensions VERSION 0.1.0 LANGUAGES CXX)

option(SCSYNTH "Build the scsynth plugin target" ON)
option(SUPERNOVA "Build the supernova plugin target" OFF)
option(STRICT "Enable strict compiler warnings" OFF)
option(BLIP_EXTENSIONS_SANITIZE
    "Enable address and undefined-behavior sanitizers for native tests" OFF)
option(BLIP_EXTENSIONS_BUILD_BENCHMARKS
    "Build the native DSP microbenchmark" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/stage"
        CACHE PATH "Install prefix" FORCE)
endif()

if(NOT SC_PATH)
    message(FATAL_ERROR
        "SC_PATH must point to the matching SuperCollider source tree")
endif()

get_filename_component(SC_PATH_FOR_MODULES "${SC_PATH}" ABSOLUTE
    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${SC_PATH_FOR_MODULES}/tools/cmake_gen")

include(SuperColliderServerPlugin)
sc_do_initial_compiler_config()
sc_check_sc_path("${SC_PATH}")

set(blip_extensions_cpp_files
    plugins/BlipExtensions.cpp
)

set(blip_extensions_sc_files
    SCClassLibrary/BlipExtensions.sc
)

file(GLOB blip_extensions_help_files CONFIGURE_DEPENDS
    HelpSource/Classes/*.schelp)

sc_add_server_plugin(
    "BlipExtensions"
    "BlipExtensions"
    "${blip_extensions_cpp_files}"
    "${blip_extensions_sc_files}"
    "${blip_extensions_help_files}"
)

foreach(target BlipExtensions_scsynth BlipExtensions_supernova)
    if(TARGET ${target})
        # Keep strict diagnostics focused on this project. SuperCollider
        # 3.14.1's public interface headers trigger GCC -Wshadow in an
        # allocator constructor when the upstream module also enables
        # -Werror, which otherwise breaks MinGW cross-builds.
        target_include_directories(${target} SYSTEM PRIVATE
            "${SC_PATH_FOR_MODULES}/include/plugin_interface"
            "${SC_PATH_FOR_MODULES}/include/common"
            "${SC_PATH_FOR_MODULES}/common")
        target_include_directories(${target} PRIVATE include)
        if(STRICT)
            if(MSVC)
                target_compile_options(${target} PRIVATE /W4 /permissive-)
            else()
                target_compile_options(${target} PRIVATE
                    -Wall -Wextra -Wpedantic -Wshadow
                    -Wno-unused-parameter)
            endif()
        endif()
    endif()
endforeach()

if(TARGET BlipExtensions_scsynth)
    set_target_properties(BlipExtensions_scsynth
        PROPERTIES OUTPUT_NAME "BlipExtensions")
endif()

if(TARGET BlipExtensions_supernova)
    set_target_properties(BlipExtensions_supernova
        PROPERTIES OUTPUT_NAME "BlipExtensions_supernova")
endif()

install(FILES README.md CHANGELOG.md LICENSE ORIGINS.md
    DESTINATION BlipExtensions)
install(DIRECTORY docs/ DESTINATION BlipExtensions/docs)
install(DIRECTORY prototypes/ DESTINATION BlipExtensions/prototypes)
install(DIRECTORY benchmarks/ DESTINATION BlipExtensions/benchmarks)
install(DIRECTORY tests/ DESTINATION BlipExtensions/tests
    PATTERN "reference" EXCLUDE
    PATTERN "renders" EXCLUDE)
install(DIRECTORY examples/ DESTINATION BlipExtensions/examples)
install(FILES HelpSource/Guides/BlipExtensions.schelp
    DESTINATION BlipExtensions/HelpSource/Guides)

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(blip_extensions_reference_tests
        tests/reference/BlipDSPTests.cpp)
    target_include_directories(blip_extensions_reference_tests PRIVATE include)

    if(STRICT)
        if(MSVC)
            target_compile_options(blip_extensions_reference_tests PRIVATE /W4 /permissive-)
        else()
            target_compile_options(blip_extensions_reference_tests PRIVATE
                -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        endif()
    endif()

    if(BLIP_EXTENSIONS_SANITIZE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(blip_extensions_reference_tests PRIVATE
                -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(blip_extensions_reference_tests PRIVATE
                -fsanitize=address,undefined)
        else()
            message(FATAL_ERROR
                "BLIP_EXTENSIONS_SANITIZE requires Clang or GCC")
        endif()
    endif()

    add_test(NAME blip_extensions_reference_tests
        COMMAND blip_extensions_reference_tests)
endif()

if(BLIP_EXTENSIONS_BUILD_BENCHMARKS AND NOT CMAKE_CROSSCOMPILING)
    add_executable(blip_extensions_dsp_benchmark
        benchmarks/dsp_benchmark.cpp)
    target_include_directories(blip_extensions_dsp_benchmark PRIVATE include)
endif()
