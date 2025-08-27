cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 99)
project(D3D12Replayer LANGUAGES CXX C)

if (CMAKE_COMPILER_IS_GNUCXX OR (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang"))
    set(D3D12_REPLAYER_CXX_FLAGS -Wshadow -Wall -Wextra -Wno-comment -Wno-missing-field-initializers -Wno-empty-body -ffast-math)
    if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
        set(D3D12_REPLAYER_CXX_FLAGS ${D3D12_REPLAYER_CXX_FLAGS} -Wno-backslash-newline-escape)
    endif()
    if (NOT (${CMAKE_BUILD_TYPE} MATCHES "Release"))
        message("Disabling frame pointer for profiling/debug.")
        set(D3D12_REPLAYER_CXX_FLAGS ${D3D12_REPLAYER_CXX_FLAGS} -fno-omit-frame-pointer)
    endif()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
        message("Enabling SSE3 support.")
        set(D3D12_REPLAYER_CXX_FLAGS ${D3D12_REPLAYER_CXX_FLAGS} -msse3)
    endif()
elseif (MSVC)
    set(D3D12_REPLAYER_CXX_FLAGS /D_CRT_SECURE_NO_WARNINGS /wd4267 /wd4244 /wd4309 /wd4005 /MP /DNOMINMAX)
endif()

add_subdirectory(third_party EXCLUDE)

add_executable(d3d12-replayer d3d12_replayer.cpp
        cli_parser.cpp cli_parser.hpp
        path_utils.cpp path_utils.hpp
        string_helpers.cpp string_helpers.hpp
        logging.cpp logging.hpp)
target_compile_options(d3d12-replayer PRIVATE ${D3D12_REPLAYER_CXX_FLAGS})
target_link_libraries(d3d12-replayer PRIVATE d3d12-replayer-rapidjson SDL3-static Vulkan::Headers)

include(FindPkgConfig)
pkg_check_modules(VKD3D_PROTON IMPORTED_TARGET libvkd3d-proton-d3d12)

if (VKD3D_PROTON_FOUND)
    message("Using system vkd3d-proton install.")
    target_link_libraries(d3d12-replayer PRIVATE PkgConfig::VKD3D_PROTON)
    target_compile_definitions(d3d12-replayer PRIVATE VKD3D_PROTON_SYSTEM)
else()
    message("Dynamically loading D3D12 implementation in runtime.")
    # Use vendored headers and dynamically load.
    target_include_directories(d3d12-replayer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

if (NOT WIN32)
    target_link_libraries(d3d12-replayer PRIVATE dl)
endif()

if (WIN32)
    add_custom_command(TARGET d3d12-replayer POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/D3D12 $<TARGET_FILE_DIR:d3d12-replayer>/D3D12)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/D3D12 DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(TARGETS d3d12-replayer)

if (WIN32 AND CMAKE_COMPILER_IS_GNUCXX)
    target_link_libraries(d3d12-replayer PRIVATE -static gcc stdc++ winpthread)
endif()

