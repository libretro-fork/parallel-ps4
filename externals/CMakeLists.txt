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

# Boost
if (NOT TARGET Boost::headers)
    add_subdirectory(ext-boost)
endif()

# spdlog
set(SPDLOG_NO_EXCEPTIONS ON)
set(SPDLOG_DISABLE_GLOBAL_LOGGER ON)
if (TARGET fmt::fmt)
    set(SPDLOG_FMT_EXTERNAL ON)
endif()
add_subdirectory(spdlog EXCLUDE_FROM_ALL SYSTEM)
target_compile_definitions(spdlog PUBLIC SPDLOG_FUNCTION=__func__)

# fmtlib
if (NOT TARGET fmt::fmt)
    add_subdirectory(fmt)
endif()

# LibAtrac9
file(GLOB LIBATRAC9_SOURCES LibAtrac9/C/src/*.c)
add_library(LibAtrac9 STATIC ${LIBATRAC9_SOURCES})
target_include_directories(LibAtrac9 INTERFACE LibAtrac9/C/src)

# FreeType
if (NOT TARGET Freetype::Freetype)
    # Keep optional deps off to reduce build surface: the fonts it reads are not compressed.
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    add_subdirectory(freetype)
    add_library(Freetype::Freetype ALIAS freetype)
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

# RenderDoc
if (NOT TARGET RenderDoc::API)
    add_library(renderdoc INTERFACE)
    target_include_directories(renderdoc INTERFACE ./renderdoc)
    add_library(RenderDoc::API ALIAS renderdoc)
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

# Robin-map
if (NOT TARGET tsl::robin_map)
    add_subdirectory(robin-map)
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

# half
if (NOT TARGET half::half)
    add_library(half INTERFACE)
    target_include_directories(half INTERFACE half/include)
    add_library(half::half ALIAS half)
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

if (APPLE OR ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    # date
    if (NOT TARGET date::date-tz)
        option(BUILD_TZ_LIB "" ON)
        option(USE_SYSTEM_TZ_DB "" ON)
        add_subdirectory(date)
    endif()
    if (NOT TARGET epoll-shim)
        add_subdirectory(epoll-shim)
    endif()
endif()

#windows only
if (WIN32)
add_subdirectory(ext-wepoll)
endif()

# LibreSSL
if (TARGET LibreSSL::SSL OR TARGET LibreSSL::Crypto)
    set(SSL_DEPENDENCIES LibreSSL::SSL LibreSSL::Crypto)
elseif (TARGET OpenSSL::SSL OR TARGET OpenSSL::Crypto)
    set(SSL_DEPENDENCIES OpenSSL::SSL OpenSSL::Crypto)
else()
    set(LIBRESSL_SKIP_INSTALL ON CACHE BOOL "" FORCE)
    set(LIBRESSL_APPS OFF CACHE BOOL "" FORCE)
    set(LIBRESSL_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(libressl EXCLUDE_FROM_ALL)
    get_directory_property(SSL_DEPENDENCIES DIRECTORY libressl DEFINITION OPENSSL_LIBS)
endif()

# cpp-httplib
add_library(Cpp_Httplib INTERFACE)
target_include_directories(Cpp_Httplib INTERFACE cpp-httplib/)

if (DEFINED SSL_DEPENDENCIES)
    target_link_libraries(Cpp_Httplib INTERFACE ${SSL_DEPENDENCIES})
    target_compile_definitions(Cpp_Httplib INTERFACE CPPHTTPLIB_OPENSSL_SUPPORT)
    message(STATUS "cpp-httplib: SSL support enabled (TLS connections)")
else()
    message(STATUS "cpp-httplib: OpenSSL/LibreSSL not available — libSceHttp HTTPS will fall back to mock at runtime.")
endif()

# macOS Vulkan driver
if (APPLE AND NOT ENABLE_SYSTEM_VULKAN)
    add_subdirectory(vulkan-loader)
    set(VULKAN_LOADER_PATH "${CMAKE_CURRENT_BINARY_DIR}/vulkan-loader/loader/libvulkan.dylib" PARENT_SCOPE)

    add_subdirectory(mesa-kosmickrisp)
    set(VULKAN_DRIVER_PATH "${CMAKE_CURRENT_BINARY_DIR}/mesa-kosmickrisp/outputs/libvulkan_kosmickrisp.dylib" PARENT_SCOPE)
    set(VULKAN_ICD_PATH "${CMAKE_CURRENT_BINARY_DIR}/mesa-kosmickrisp/outputs/kosmickrisp_mesa_icd.json" PARENT_SCOPE)
endif()

# abseil (for protobuf)
if (NOT TARGET absl::strings)
    add_subdirectory(abseil-cpp EXCLUDE_FROM_ALL)
endif()

# protobuf
set(protobuf_BUILD_TESTS              OFF CACHE INTERNAL "")
set(protobuf_BUILD_EXAMPLES           OFF CACHE INTERNAL "")
set(protobuf_BUILD_PROTOC_BINARIES    ON  CACHE INTERNAL "")
set(protobuf_INSTALL                  OFF CACHE INTERNAL "")
set(protobuf_WITH_ZLIB                OFF CACHE INTERNAL "")
set(protobuf_BUILD_SHARED_LIBS        OFF CACHE INTERNAL "")
set(protobuf_LOCAL_DEPENDENCIES_ONLY  ON  CACHE INTERNAL "")
set(protobuf_FORCE_FETCH_DEPENDENCIES OFF CACHE INTERNAL "")
if(MSVC)
set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
endif()
add_subdirectory(protobuf EXCLUDE_FROM_ALL)
