cache_storage.o: src/video_core/cache_storage.cpp src/common/elf_info.h \
  src/common/assert.h src/common/logging/log.h \
  src/common/logging/classes.h src/common/bit_field.h \
  src/common/concepts.h src/common/io_file.h src/common/types.h \
  src/common/enum.h src/common/singleton.h src/common/path_util.h \
  src/common/thread_name.h src/core/emulator_settings.h \
  libretro/libretro-common/include/boolean.h \
  libretro/libretro-common/include/retro_common_api.h \
  src/video_core/cache_storage.h \
  libretro/libretro-common/include/retro_atomic.h \
  libretro/libretro-common/include/retro_inline.h \
  src/video_core/blob_pack.h \
  src/video_core/renderer_vulkan/vk_instance.h \
  src/video_core/renderer_vulkan/vk_platform.h \
  src/video_core/renderer_vulkan/vk_common.h \
  libretro/libretro-common/include/vulkan/vulkan_symbol_wrapper.h \
  src/video_core/renderer_vulkan/vk_pipeline_cache.h \
  libretro/libretro-common/include/queues/mpsc_stack.h \
  libretro/libretro-common/include/rthreads/retro_eventcount.h \
  src/common/ptr_table.h src/common/stop_thread.h \
  libretro/libretro-common/include/rthreads/rthreads.h \
  libretro/libretro-common/include/retro_miscellaneous.h \
  src/shader_recompiler/profile.h src/shader_recompiler/recompiler.h \
  src/common/object_pool.h \
  src/shader_recompiler/backend/spirv/spv_module.h \
  libretro/libretro-common/include/gfx/shaders/spirv_words.h \
  src/shader_recompiler/frontend/control_flow_graph.h \
  src/common/intrusive_list_cpp.h src/common/intrusive_list.h \
  src/common/small_vector.h src/shader_recompiler/frontend/instruction.h \
  src/shader_recompiler/frontend/opcodes.h \
  src/shader_recompiler/ir/condition.h \
  src/shader_recompiler/ir/basic_block.h \
  src/shader_recompiler/ir/microinstruction.h \
  src/shader_recompiler/ir/opcodes.h src/shader_recompiler/ir/type.h \
  src/shader_recompiler/ir/opcodes.inc src/shader_recompiler/ir/ssa.h \
  src/shader_recompiler/ir/reg.h src/video_core/amdgpu/pixel_format.h \
  src/shader_recompiler/ir/value.h src/shader_recompiler/ir/attribute.h \
  src/shader_recompiler/ir/patch.h src/shader_recompiler/ir/program.h \
  src/shader_recompiler/info.h src/shader_recompiler/backend/bindings.h \
  src/shader_recompiler/frontend/copy_shader.h \
  src/shader_recompiler/frontend/tessellation.h \
  src/shader_recompiler/ir/passes/srt.h src/shader_recompiler/params.h \
  src/shader_recompiler/resource.h src/video_core/amdgpu/resource.h \
  src/common/alignment.h src/video_core/amdgpu/tiling.h \
  src/shader_recompiler/runtime_info.h \
  src/video_core/amdgpu/regs_shader.h \
  src/video_core/amdgpu/regs_vertex.h \
  src/shader_recompiler/ir/abstract_syntax_list.h \
  src/shader_recompiler/specialization.h \
  src/shader_recompiler/frontend/fetch_shader.h \
  src/video_core/renderer_vulkan/vk_compute_pipeline.h \
  src/video_core/renderer_vulkan/vk_pipeline_common.h \
  src/video_core/renderer_vulkan/vk_graphics_pipeline.h \
  libretro/libretro-common/include/encodings/xxh3.h \
  libretro/libretro-common/include/retro_endianness.h \
  src/video_core/amdgpu/regs_color.h src/video_core/amdgpu/regs_depth.h \
  src/video_core/amdgpu/regs_primitive.h \
  src/video_core/renderer_vulkan/vk_resource_pool.h src/common/u64_map.h
src/common/elf_info.h:
src/common/assert.h:
src/common/logging/log.h:
src/common/logging/classes.h:
src/common/bit_field.h:
src/common/concepts.h:
src/common/io_file.h:
src/common/types.h:
src/common/enum.h:
src/common/singleton.h:
src/common/path_util.h:
src/common/thread_name.h:
src/core/emulator_settings.h:
libretro/libretro-common/include/boolean.h:
libretro/libretro-common/include/retro_common_api.h:
src/video_core/cache_storage.h:
libretro/libretro-common/include/retro_atomic.h:
libretro/libretro-common/include/retro_inline.h:
src/video_core/blob_pack.h:
src/video_core/renderer_vulkan/vk_instance.h:
src/video_core/renderer_vulkan/vk_platform.h:
src/video_core/renderer_vulkan/vk_common.h:
libretro/libretro-common/include/vulkan/vulkan_symbol_wrapper.h:
src/video_core/renderer_vulkan/vk_pipeline_cache.h:
libretro/libretro-common/include/queues/mpsc_stack.h:
libretro/libretro-common/include/rthreads/retro_eventcount.h:
src/common/ptr_table.h:
src/common/stop_thread.h:
libretro/libretro-common/include/rthreads/rthreads.h:
libretro/libretro-common/include/retro_miscellaneous.h:
src/shader_recompiler/profile.h:
src/shader_recompiler/recompiler.h:
src/common/object_pool.h:
src/shader_recompiler/backend/spirv/spv_module.h:
libretro/libretro-common/include/gfx/shaders/spirv_words.h:
src/shader_recompiler/frontend/control_flow_graph.h:
src/common/intrusive_list_cpp.h:
src/common/intrusive_list.h:
src/common/small_vector.h:
src/shader_recompiler/frontend/instruction.h:
src/shader_recompiler/frontend/opcodes.h:
src/shader_recompiler/ir/condition.h:
src/shader_recompiler/ir/basic_block.h:
src/shader_recompiler/ir/microinstruction.h:
src/shader_recompiler/ir/opcodes.h:
src/shader_recompiler/ir/type.h:
src/shader_recompiler/ir/opcodes.inc:
src/shader_recompiler/ir/ssa.h:
src/shader_recompiler/ir/reg.h:
src/video_core/amdgpu/pixel_format.h:
src/shader_recompiler/ir/value.h:
src/shader_recompiler/ir/attribute.h:
src/shader_recompiler/ir/patch.h:
src/shader_recompiler/ir/program.h:
src/shader_recompiler/info.h:
src/shader_recompiler/backend/bindings.h:
src/shader_recompiler/frontend/copy_shader.h:
src/shader_recompiler/frontend/tessellation.h:
src/shader_recompiler/ir/passes/srt.h:
src/shader_recompiler/params.h:
src/shader_recompiler/resource.h:
src/video_core/amdgpu/resource.h:
src/common/alignment.h:
src/video_core/amdgpu/tiling.h:
src/shader_recompiler/runtime_info.h:
src/video_core/amdgpu/regs_shader.h:
src/video_core/amdgpu/regs_vertex.h:
src/shader_recompiler/ir/abstract_syntax_list.h:
src/shader_recompiler/specialization.h:
src/shader_recompiler/frontend/fetch_shader.h:
src/video_core/renderer_vulkan/vk_compute_pipeline.h:
src/video_core/renderer_vulkan/vk_pipeline_common.h:
src/video_core/renderer_vulkan/vk_graphics_pipeline.h:
libretro/libretro-common/include/encodings/xxh3.h:
libretro/libretro-common/include/retro_endianness.h:
src/video_core/amdgpu/regs_color.h:
src/video_core/amdgpu/regs_depth.h:
src/video_core/amdgpu/regs_primitive.h:
src/video_core/renderer_vulkan/vk_resource_pool.h:
src/common/u64_map.h:
