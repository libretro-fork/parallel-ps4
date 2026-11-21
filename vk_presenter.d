vk_presenter.o: src/video_core/renderer_vulkan/vk_presenter.cpp \
  src/common/elf_info.h src/common/assert.h src/common/logging/log.h \
  src/common/logging/classes.h src/common/bit_field.h \
  src/common/concepts.h src/common/io_file.h src/common/types.h \
  src/common/enum.h src/common/singleton.h \
  src/common/embedded_resources.h src/common/path_util.h \
  src/core/debug_state.h src/core/emulator_settings.h \
  libretro/libretro-common/include/boolean.h \
  libretro/libretro-common/include/retro_common_api.h \
  src/core/libraries/system/systemservice.h \
  src/core/libraries/system/userservice.h src/input/controller.h \
  libretro/libretro-common/include/rthreads/retro_seqlock.h \
  libretro/libretro-common/include/retro_atomic.h \
  libretro/libretro-common/include/retro_inline.h src/common/combiner.h \
  libretro/libretro-common/include/queues/mpsc_stack.h \
  libretro/libretro-common/include/rthreads/retro_eventcount.h \
  src/common/ring_buffer_queue.h src/core/libraries/pad/pad.h \
  src/video_core/buffer_cache/buffer.h \
  src/video_core/renderer_vulkan/vk_common.h \
  libretro/libretro-common/include/vulkan/vulkan_symbol_wrapper.h \
  src/video_core/buffer_cache/guest_import.h \
  src/video_core/overlay/ovl.h src/video_core/overlay/vk_overlay.h \
  src/video_core/renderer_vulkan/frame_notes.h \
  src/video_core/renderer_vulkan/vk_memory.h \
  src/video_core/renderer_vulkan/vk_heap.h \
  src/video_core/renderer_vulkan/vk_platform.h \
  src/common/string_literal.h \
  src/video_core/renderer_vulkan/vk_presenter.h \
  src/common/bounded_threadsafe_queue.h \
  src/core/libraries/videoout/buffer.h libretro/libretro_bridge.h \
  src/video_core/renderer_vulkan/host_passes/pp_pass.h \
  src/video_core/renderer_vulkan/vk_instance.h \
  src/video_core/renderer_vulkan/vk_output_images.h \
  src/video_core/renderer_vulkan/vk_runtime.h src/common/interval_set.h \
  src/common/small_vector.h \
  src/video_core/renderer_vulkan/vk_staging_buffer_pool.h \
  src/video_core/texture_cache/image.h src/common/incremental_id.h \
  src/common/lru_cache.h src/common/target_frames.h \
  src/video_core/texture_cache/host_compatibility.h \
  src/video_core/texture_cache/image_info.h \
  src/video_core/amdgpu/cb_db_extent.h src/video_core/amdgpu/tiling.h \
  src/video_core/texture_cache/tile.h src/common/alignment.h \
  src/video_core/texture_cache/types.h src/common/slot_vector.h \
  src/video_core/texture_cache/image_view.h \
  src/video_core/amdgpu/pixel_format.h \
  src/video_core/amdgpu/regs_depth.h src/video_core/amdgpu/resource.h \
  src/video_core/renderer_vulkan/vk_scheduler.h src/common/node_pool.h \
  src/common/stop_thread.h \
  libretro/libretro-common/include/rthreads/rthreads.h \
  libretro/libretro-common/include/retro_miscellaneous.h \
  src/common/unique_function.h src/video_core/amdgpu/regs_color.h \
  src/video_core/amdgpu/regs_primitive.h \
  src/video_core/renderer_vulkan/vk_resource_pool.h src/common/u64_map.h \
  src/video_core/renderer_vulkan/vk_semaphore.h \
  src/video_core/texture_cache/texture_cache.h \
  src/common/multi_level_page_table.h src/shader_recompiler/resource.h \
  src/shader_recompiler/ir/type.h \
  src/video_core/texture_cache/blit_helper.h \
  src/video_core/host_shaders/host_spirv.h \
  src/video_core/texture_cache/sampler.h \
  src/video_core/amdgpu/regs_texture.h \
  src/video_core/texture_cache/tile_manager.h \
  src/video_core/renderer_vulkan/vk_rasterizer.h src/common/published.h \
  libretro/libretro-common/include/queues/retro_retire.h \
  src/video_core/buffer_cache/buffer_cache.h \
  src/video_core/buffer_cache/fault_manager.h \
  src/video_core/buffer_cache/range_set.h src/video_core/page_manager.h \
  src/video_core/buffer_cache/region_definitions.h \
  src/video_core/renderer_vulkan/vk_pipeline_cache.h \
  src/common/ptr_table.h src/shader_recompiler/profile.h \
  src/shader_recompiler/recompiler.h src/common/object_pool.h \
  src/shader_recompiler/backend/spirv/spv_module.h \
  libretro/libretro-common/include/gfx/shaders/spirv_words.h \
  src/shader_recompiler/frontend/control_flow_graph.h \
  src/common/intrusive_list_cpp.h src/common/intrusive_list.h \
  src/shader_recompiler/frontend/instruction.h \
  src/shader_recompiler/frontend/opcodes.h \
  src/shader_recompiler/ir/condition.h \
  src/shader_recompiler/ir/basic_block.h \
  src/shader_recompiler/ir/microinstruction.h \
  src/shader_recompiler/ir/opcodes.h \
  src/shader_recompiler/ir/opcodes.inc src/shader_recompiler/ir/ssa.h \
  src/shader_recompiler/ir/reg.h src/shader_recompiler/ir/value.h \
  src/shader_recompiler/ir/attribute.h src/shader_recompiler/ir/patch.h \
  src/shader_recompiler/ir/program.h src/shader_recompiler/info.h \
  src/shader_recompiler/backend/bindings.h \
  src/shader_recompiler/frontend/copy_shader.h \
  src/shader_recompiler/frontend/tessellation.h \
  src/shader_recompiler/ir/passes/srt.h src/shader_recompiler/params.h \
  src/shader_recompiler/runtime_info.h \
  src/video_core/amdgpu/regs_shader.h \
  src/video_core/amdgpu/regs_vertex.h \
  src/shader_recompiler/ir/abstract_syntax_list.h \
  src/shader_recompiler/specialization.h \
  src/shader_recompiler/frontend/fetch_shader.h \
  src/video_core/cache_storage.h src/video_core/blob_pack.h \
  src/video_core/renderer_vulkan/vk_compute_pipeline.h \
  src/video_core/renderer_vulkan/vk_pipeline_common.h \
  src/video_core/renderer_vulkan/vk_graphics_pipeline.h \
  libretro/libretro-common/include/encodings/xxh3.h \
  libretro/libretro-common/include/retro_endianness.h \
  src/video_core/renderer_vulkan/vk_vertex_capture.h \
  libretro/libretro-common/include/file/file_path.h \
  libretro/libretro-common/include/libretro.h \
  libretro/libretro-common/include/features/features_cpu.h
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
src/common/embedded_resources.h:
src/common/path_util.h:
src/core/debug_state.h:
src/core/emulator_settings.h:
libretro/libretro-common/include/boolean.h:
libretro/libretro-common/include/retro_common_api.h:
src/core/libraries/system/systemservice.h:
src/core/libraries/system/userservice.h:
src/input/controller.h:
libretro/libretro-common/include/rthreads/retro_seqlock.h:
libretro/libretro-common/include/retro_atomic.h:
libretro/libretro-common/include/retro_inline.h:
src/common/combiner.h:
libretro/libretro-common/include/queues/mpsc_stack.h:
libretro/libretro-common/include/rthreads/retro_eventcount.h:
src/common/ring_buffer_queue.h:
src/core/libraries/pad/pad.h:
src/video_core/buffer_cache/buffer.h:
src/video_core/renderer_vulkan/vk_common.h:
libretro/libretro-common/include/vulkan/vulkan_symbol_wrapper.h:
src/video_core/buffer_cache/guest_import.h:
src/video_core/overlay/ovl.h:
src/video_core/overlay/vk_overlay.h:
src/video_core/renderer_vulkan/frame_notes.h:
src/video_core/renderer_vulkan/vk_memory.h:
src/video_core/renderer_vulkan/vk_heap.h:
src/video_core/renderer_vulkan/vk_platform.h:
src/common/string_literal.h:
src/video_core/renderer_vulkan/vk_presenter.h:
src/common/bounded_threadsafe_queue.h:
src/core/libraries/videoout/buffer.h:
libretro/libretro_bridge.h:
src/video_core/renderer_vulkan/host_passes/pp_pass.h:
src/video_core/renderer_vulkan/vk_instance.h:
src/video_core/renderer_vulkan/vk_output_images.h:
src/video_core/renderer_vulkan/vk_runtime.h:
src/common/interval_set.h:
src/common/small_vector.h:
src/video_core/renderer_vulkan/vk_staging_buffer_pool.h:
src/video_core/texture_cache/image.h:
src/common/incremental_id.h:
src/common/lru_cache.h:
src/common/target_frames.h:
src/video_core/texture_cache/host_compatibility.h:
src/video_core/texture_cache/image_info.h:
src/video_core/amdgpu/cb_db_extent.h:
src/video_core/amdgpu/tiling.h:
src/video_core/texture_cache/tile.h:
src/common/alignment.h:
src/video_core/texture_cache/types.h:
src/common/slot_vector.h:
src/video_core/texture_cache/image_view.h:
src/video_core/amdgpu/pixel_format.h:
src/video_core/amdgpu/regs_depth.h:
src/video_core/amdgpu/resource.h:
src/video_core/renderer_vulkan/vk_scheduler.h:
src/common/node_pool.h:
src/common/stop_thread.h:
libretro/libretro-common/include/rthreads/rthreads.h:
libretro/libretro-common/include/retro_miscellaneous.h:
src/common/unique_function.h:
src/video_core/amdgpu/regs_color.h:
src/video_core/amdgpu/regs_primitive.h:
src/video_core/renderer_vulkan/vk_resource_pool.h:
src/common/u64_map.h:
src/video_core/renderer_vulkan/vk_semaphore.h:
src/video_core/texture_cache/texture_cache.h:
src/common/multi_level_page_table.h:
src/shader_recompiler/resource.h:
src/shader_recompiler/ir/type.h:
src/video_core/texture_cache/blit_helper.h:
src/video_core/host_shaders/host_spirv.h:
src/video_core/texture_cache/sampler.h:
src/video_core/amdgpu/regs_texture.h:
src/video_core/texture_cache/tile_manager.h:
src/video_core/renderer_vulkan/vk_rasterizer.h:
src/common/published.h:
libretro/libretro-common/include/queues/retro_retire.h:
src/video_core/buffer_cache/buffer_cache.h:
src/video_core/buffer_cache/fault_manager.h:
src/video_core/buffer_cache/range_set.h:
src/video_core/page_manager.h:
src/video_core/buffer_cache/region_definitions.h:
src/video_core/renderer_vulkan/vk_pipeline_cache.h:
src/common/ptr_table.h:
src/shader_recompiler/profile.h:
src/shader_recompiler/recompiler.h:
src/common/object_pool.h:
src/shader_recompiler/backend/spirv/spv_module.h:
libretro/libretro-common/include/gfx/shaders/spirv_words.h:
src/shader_recompiler/frontend/control_flow_graph.h:
src/common/intrusive_list_cpp.h:
src/common/intrusive_list.h:
src/shader_recompiler/frontend/instruction.h:
src/shader_recompiler/frontend/opcodes.h:
src/shader_recompiler/ir/condition.h:
src/shader_recompiler/ir/basic_block.h:
src/shader_recompiler/ir/microinstruction.h:
src/shader_recompiler/ir/opcodes.h:
src/shader_recompiler/ir/opcodes.inc:
src/shader_recompiler/ir/ssa.h:
src/shader_recompiler/ir/reg.h:
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
src/shader_recompiler/runtime_info.h:
src/video_core/amdgpu/regs_shader.h:
src/video_core/amdgpu/regs_vertex.h:
src/shader_recompiler/ir/abstract_syntax_list.h:
src/shader_recompiler/specialization.h:
src/shader_recompiler/frontend/fetch_shader.h:
src/video_core/cache_storage.h:
src/video_core/blob_pack.h:
src/video_core/renderer_vulkan/vk_compute_pipeline.h:
src/video_core/renderer_vulkan/vk_pipeline_common.h:
src/video_core/renderer_vulkan/vk_graphics_pipeline.h:
libretro/libretro-common/include/encodings/xxh3.h:
libretro/libretro-common/include/retro_endianness.h:
src/video_core/renderer_vulkan/vk_vertex_capture.h:
libretro/libretro-common/include/file/file_path.h:
libretro/libretro-common/include/libretro.h:
libretro/libretro-common/include/features/features_cpu.h:
