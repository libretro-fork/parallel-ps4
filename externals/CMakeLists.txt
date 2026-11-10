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

# Xbyak
if (NOT TARGET xbyak::xbyak)
    add_subdirectory(xbyak)
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
