# SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF)
set_directory_properties(PROPERTIES
    EXCLUDE_FROM_ALL ON
    SYSTEM ON
)

# Set CMP0069 policy to "NEW" in order to ensure consistent behavior when building external targets with LTO enabled
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

if (MSVC)
    # Silence "deprecation" warnings
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_SCL_SECURE_NO_WARNINGS)
endif()

# fmtlib
if (NOT TARGET fmt::fmt)
    add_subdirectory(fmt)
endif()

# vulkan-headers
if (NOT TARGET Vulkan::Headers)
    set(VULKAN_HEADERS_ENABLE_MODULE OFF)
    add_subdirectory(vulkan-headers)
    set(VulkanHeaders_VERSION "1.4.330")
endif()

# VMA
if (NOT TARGET GPUOpen::VulkanMemoryAllocator)
    add_subdirectory(vma)
endif()

# glslang
if (NOT TARGET glslang::glslang)
    set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "")
    set(ENABLE_GLSLANG_BINARIES ON CACHE BOOL "" FORCE)
    set(ENABLE_SPVREMAPPER OFF CACHE BOOL "")
    set(ENABLE_CTEST OFF CACHE BOOL "")
    set(ENABLE_HLSL OFF CACHE BOOL "")
    set(BUILD_EXTERNAL OFF CACHE BOOL "")
    set(ENABLE_OPT OFF CACHE BOOL "")
    add_subdirectory(glslang)
    file(COPY glslang/SPIRV DESTINATION glslang/glslang FILES_MATCHING PATTERN "*.h")
    target_include_directories(glslang INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/glslang")
endif()

# Xbyak
if (NOT TARGET xbyak::xbyak)
    add_subdirectory(xbyak)
endif()

# MagicEnum
if (NOT TARGET magic_enum::magic_enum)
    add_subdirectory(magic_enum)
endif()

# Zydis
if (NOT TARGET Zydis::Zydis)
    option(ZYDIS_BUILD_TOOLS "" OFF)
    option(ZYDIS_BUILD_EXAMPLES "" OFF)
    add_subdirectory(zydis)
endif()

# sirit
add_subdirectory(sirit)
if (WIN32)
    target_compile_options(sirit PRIVATE $<$<CXX_COMPILER_ID:Clang>:-Wno-error=unused-command-line-argument>)
endif()

# Dear ImGui
add_library(Dear_ImGui
        imgui/imgui.cpp
        imgui/imgui_demo.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_internal.h
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
)
target_include_directories(Dear_ImGui INTERFACE imgui/)

# libusb
if (NOT TARGET libusb::usb)
    if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
        # YOU MUST USE NATIVE LIBUSB
        # using anything else will crash instantly, also freebsd will NOT like it
        # no you cant vendor this libusb, its builtin on freebsd
        find_package(libusb)
    else()
        add_subdirectory(libusb)
        add_library(libusb::usb ALIAS usb-1.0)
    endif()
endif()


# GCN Headers
add_subdirectory(gcn)

# macOS Vulkan driver
if (APPLE AND NOT ENABLE_SYSTEM_VULKAN)
    add_subdirectory(vulkan-loader)
    set(VULKAN_LOADER_PATH "${CMAKE_CURRENT_BINARY_DIR}/vulkan-loader/loader/libvulkan.dylib" PARENT_SCOPE)

    add_subdirectory(mesa-kosmickrisp)
    set(VULKAN_DRIVER_PATH "${CMAKE_CURRENT_BINARY_DIR}/mesa-kosmickrisp/outputs/libvulkan_kosmickrisp.dylib" PARENT_SCOPE)
    set(VULKAN_ICD_PATH "${CMAKE_CURRENT_BINARY_DIR}/mesa-kosmickrisp/outputs/kosmickrisp_mesa_icd.json" PARENT_SCOPE)
endif()
