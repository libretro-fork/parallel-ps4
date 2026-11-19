# Vulkan headers

The headers of Khronos' Vulkan-Headers that the core and its tests include, and its licence,
copied unchanged from https://github.com/KhronosGroup/Vulkan-Headers at commit
ee3b5caaa7e372715873c7b9c390ee1c3ca5db25 ("Update for Vulkan-Docs 1.4.230").

Kept: `include/vulkan/` vulkan.h, vulkan_core.h, vk_platform.h and the platform headers the
core asks for (vulkan_beta.h, vulkan_win32.h, vulkan_android.h, vulkan_metal.h); vulkan.hpp and
the headers it reads (vulkan_enums, _funcs, _handles, _structs, _hpp_macros, _format_traits,
_to_string); and `include/vk_video/`, which vulkan_core.h includes. The registry, the C++
modules and the other platforms' headers are left out.

To move them on, copy the same files from a newer commit over these.
