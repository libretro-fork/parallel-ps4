// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <ranges>

#include "common/hash.h"
#include "common/io_file.h"
#include "common/path_util.h"
#include "common/string_util.h"
#include "common/thread_name.h"
#include "core/emulator_settings.h"
#include "core/memory.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/frontend/fetch_shader.h"
#include "shader_recompiler/info.h"
#include "shader_recompiler/recompiler.h"
#include "shader_recompiler/runtime_info.h"
#include "video_core/amdgpu/liverpool.h"
#include "video_core/amdgpu/sample_locs.h"
#include "video_core/cache_storage.h"
#include "video_core/renderer_vulkan/liverpool_to_vk.h"
#include "video_core/renderer_vulkan/pipeline_cache_blob.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_pipeline_serialization.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"

namespace Vulkan {

// What the compiler thread is handed, and gives back. Made when a draw finds something
// missing, which is when a compile would have allocated anyway.
// An SRT walker's machine code by its hash, its length the seed: what a job compares a walker
// against, where it kept a copy of the code for that alone
static u64 WalkerId(const std::vector<u8>& code) {
    return xxh3_64(code.data(), code.size(), code.size());
}

enum class PipelineCache::JobKind : u8 {
    Discover,    // a shader not seen before: its walker (Shader::GuestSnapshot)
    Program,     // its translation, permutation 0
    Permutation, // another permutation of a translated shader
    Check,       // a permutation the cache gave, translated again to be compared
    Graphics,    // a graphics pipeline, from shaders all translated
};

struct PipelineCache::CompileJob {
    mpsc_stack_node_t node; // first: the stacks link jobs through it
    using Kind = JobKind;
    Kind kind{};

    // Discover, Program, Permutation, Check: the shader, with the user data copied as the draw
    // had it, and its code where the game has it (not copied). The binary info that follows
    // the code, with the hash and CRC the draw saw, is looked at before and after the
    // translation, through the backing page table (CodeUnchanged): code the game reused
    // meanwhile is not translated, or what was made from it is thrown away, and the job goes
    // again at a later draw.
    Shader::HwStage hw_stage{};
    Shader::SwStage sw_stage{};
    u32 user_data[Shader::ShaderParams::NumShaderUserData]{};
    std::span<const u32> code;
    VAddr code_info{};
    u64 code_hash{};
    u32 code_crc{};
    // The code is the core's own, in none of the game's memory (sceGnmSetEmbeddedVsShader's
    // fullscreen triangle): it cannot change, and the game's memory has nothing to compare
    bool code_static{};
    bool code_changed{};

    // What the translation reads of the game's memory, taken at the draw (CaptureSnapshot)
    Shader::GuestSnapshot snapshot{};
    std::vector<u32> snap_flat;
    Shader::Gcn::FetchShaderData snap_fetch{};
    bool has_snap_fetch{};
    Common::SmallVector<Shader::SnapshotRead, 40> snap_reads; // attributes and the tess V#
    Shader::TessellationDataConstantBuffer snap_tess{};
    /// The walker the snapshot was taken with (WalkerId): a translation that makes another (it
    /// should not) was made from a flat buffer of another layout, and is not used
    u64 snap_walker{};
    bool stale{};
    /// Discover: the translation went on to the end (GuestSnapshot::finished), and the job has
    /// made the program as a Program job does
    bool finished{};
    u64 hash{};
    size_t perm_idx{};
    Shader::RuntimeInfo runtime{};
    Shader::Backend::Bindings start{};

    // Program: the translated program. Permutation: the module.
    /// Owned by the job until filed
    Program* program{};
    vk::ShaderModule module{};

    // Check: what the cache kept, and what the translation made
    u64 kept_walker{}; // WalkerId
    u32 kept_flatbuf_dw{};
    u32 kept_resources[4]{};
    bool kept_dma{};
    Shader::Info fresh;
    bool same_spv{}, same_walker{}, same_resources{};

    // Graphics: the key, and copies of the stages as the draw had them, with the vertex
    // buffers' V#s
    Shader::GuestSnapshot stage_snapshots[MaxShaderStages]{};
    GraphicsPipelineKey key{};
    u64 key_hash{};
    Shader::Info stage_infos[MaxShaderStages]{};
    bool has_stage[MaxShaderStages]{};
    u32 stage_user_data[MaxShaderStages][Shader::ShaderParams::NumShaderUserData]{};
    Shader::RuntimeInfo runtimes[MaxShaderStages]{};
    Shader::Gcn::FetchShaderData fetch{};
    bool has_fetch{};
    vk::ShaderModule modules[MaxShaderStages]{};
    GraphicsPipeline* pipeline{};
    GraphicsPipeline::SerializationSupport sdata{};

    ~CompileJob() {
        delete program;
    }

    Shader::ShaderParams Params() const {
        return {.user_data =
                    std::span<const u32, Shader::ShaderParams::NumShaderUserData>{user_data},
                .code = code,
                .hash = hash};
    }
};

using Shader::HwStage;
using Shader::Output;
using Shader::SwStage;

constexpr static u32 SpirvVersion1_6 = 0x00010600U;

constexpr static VkDescriptorPoolSize DescriptorHeapSizes[] = {
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 512}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8192},
    {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 8192}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024},
    {VK_DESCRIPTOR_TYPE_SAMPLER, 1024},
};

static u32 MapOutputs(std::span<Shader::OutputMap, 3> outputs, const AmdGpu::VsOutputControl& ctl) {
    u32 num_outputs = 0;

    if (ctl.vs_out_misc_enable) {
        Shader::OutputMap& misc_vec = outputs[num_outputs++];
        misc_vec[0] = ctl.use_vtx_point_size ? Output::PointSize : Output::None;
        misc_vec[1] = ctl.use_vtx_edge_flag
                          ? Output::EdgeFlag
                          : (ctl.use_vtx_gs_cut_flag ? Output::GsCutFlag : Output::None);
        misc_vec[2] =
            ctl.use_vtx_kill_flag
                ? Output::KillFlag
                : (ctl.use_vtx_render_target_idx ? Output::RenderTargetIndex : Output::None);
        misc_vec[3] = ctl.use_vtx_viewport_idx ? Output::ViewportIndex : Output::None;
    }

    if (ctl.vs_out_ccdist0_enable) {
        Shader::OutputMap& ccdist0 = outputs[num_outputs++];
        ccdist0[0] = ctl.IsClipDistEnabled(0)
                         ? Output::ClipDist0
                         : (ctl.IsCullDistEnabled(0) ? Output::CullDist0 : Output::None);
        ccdist0[1] = ctl.IsClipDistEnabled(1)
                         ? Output::ClipDist1
                         : (ctl.IsCullDistEnabled(1) ? Output::CullDist1 : Output::None);
        ccdist0[2] = ctl.IsClipDistEnabled(2)
                         ? Output::ClipDist2
                         : (ctl.IsCullDistEnabled(2) ? Output::CullDist2 : Output::None);
        ccdist0[3] = ctl.IsClipDistEnabled(3)
                         ? Output::ClipDist3
                         : (ctl.IsCullDistEnabled(3) ? Output::CullDist3 : Output::None);
    }

    if (ctl.vs_out_ccdist1_enable) {
        Shader::OutputMap& ccdist1 = outputs[num_outputs++];
        ccdist1[0] = ctl.IsClipDistEnabled(4)
                         ? Output::ClipDist4
                         : (ctl.IsCullDistEnabled(4) ? Output::CullDist4 : Output::None);
        ccdist1[1] = ctl.IsClipDistEnabled(5)
                         ? Output::ClipDist5
                         : (ctl.IsCullDistEnabled(5) ? Output::CullDist5 : Output::None);
        ccdist1[2] = ctl.IsClipDistEnabled(6)
                         ? Output::ClipDist6
                         : (ctl.IsCullDistEnabled(6) ? Output::CullDist6 : Output::None);
        ccdist1[3] = ctl.IsClipDistEnabled(7)
                         ? Output::ClipDist7
                         : (ctl.IsCullDistEnabled(7) ? Output::CullDist7 : Output::None);
    }

    return num_outputs;
}

/// What every stage's runtime info takes from its program's registers
static void BuildCommon(Shader::RuntimeInfo& info, const AmdGpu::ShaderProgram& program) {
    info.props.fp_denorm_mode32 = program.settings.fp_denorm_mode32;
    info.props.fp_denorm_mode16_64 = program.settings.fp_denorm_mode64;
    info.props.fp_round_mode32 = program.settings.fp_round_mode32;
    info.props.fp_round_mode16_64 = program.settings.fp_round_mode64;
    info.props.num_allocated_vgprs = program.NumVgprs();
    info.props.num_user_data = program.settings.num_user_regs;
    info.props.num_input_vgprs = program.settings.vgpr_comp_cnt;
    info.props.dx10_clamp = program.settings.dx10_clamp;
}

/// The primitive a tessellation's output reaches the geometry shader as
static AmdGpu::PrimitiveType TessellatedPrimitive(AmdGpu::TessellationTopology topology) {
    switch (topology) {
    case AmdGpu::TessellationTopology::Point:
        return AmdGpu::PrimitiveType::PointList;
    case AmdGpu::TessellationTopology::Line:
        return AmdGpu::PrimitiveType::LineList;
    case AmdGpu::TessellationTopology::TriangleCw:
    case AmdGpu::TessellationTopology::TriangleCcw:
        return AmdGpu::PrimitiveType::TriangleList;
    default:
        UNREACHABLE();
    }
}

const Shader::RuntimeInfo& PipelineCache::BuildRuntimeInfo(HwStage stage, SwStage l_stage) {
    Shader::RuntimeInfo& info = runtime_infos[u32(l_stage)];
    const AmdGpu::Regs& regs = liverpool->regs;
    info.Initialize(stage, l_stage);
    switch (stage) {
    case HwStage::Local: {
        BuildCommon(info, regs.ls_program);
        Shader::TessellationDataConstantBuffer tess_constants{};
        const Shader::Info* hull_info = infos[u32(SwStage::TessellationControl)];
        hull_info->ReadTessConstantBuffer(tess_constants);
        info.hw.ls.ls_stride = tess_constants.ls_stride;
        break;
    }
    case HwStage::Hull:
        BuildCommon(info, regs.hs_program);
        break;
    case HwStage::Export:
        BuildCommon(info, regs.es_program);
        info.hw.es.vertex_data_size = regs.vgt_esgs_ring_itemsize;
        break;
    case HwStage::Geometry: {
        BuildCommon(info, regs.gs_program);
        info.hw.gs.num_outputs = MapOutputs(info.hw.gs.outputs, regs.vs_output_control);
        info.hw.gs.output_vertices = regs.vgt_gs_max_vert_out;
        info.hw.gs.num_invocations =
            regs.vgt_gs_instance_cnt.IsEnabled() ? regs.vgt_gs_instance_cnt.count : 1;
        if (regs.stage_enable.raw == AmdGpu::ShaderStageEnable::LsHsEsGs) {
            info.hw.gs.in_primitive = TessellatedPrimitive(regs.tess_config.topology);
        } else {
            info.hw.gs.in_primitive = regs.primitive_type;
        }
        for (u32 stream_id = 0; stream_id < Shader::GsMaxOutputStreams; ++stream_id) {
            info.hw.gs.out_primitive[stream_id] =
                regs.vgt_gs_out_prim_type.GetPrimitiveType(stream_id);
        }
        info.hw.gs.in_vertex_data_size = regs.vgt_esgs_ring_itemsize;
        info.hw.gs.out_vertex_data_size = regs.vgt_gs_vert_itemsize[0];
        info.hw.gs.mode = regs.vgt_gs_mode.mode;
        const Shader::ShaderParams params_vc = AmdGpu::GetParams(regs.vs_program);
        info.hw.gs.vs_copy = params_vc.code;
        info.hw.gs.vs_copy_hash = params_vc.hash;
        DumpShader(info.hw.gs.vs_copy, info.hw.gs.vs_copy_hash, Shader::HwStage::Vertex, 0,
                   "copy.bin");
        break;
    }
    case HwStage::Vertex: {
        BuildCommon(info, regs.vs_program);
        info.hw.vs.user_clip_plane_mask = regs.clipper_control.user_clip_plane_enable;
        info.hw.vs.num_outputs = MapOutputs(info.hw.vs.outputs, regs.vs_output_control);
        info.hw.vs.emulate_depth_negative_one_to_one =
            !instance.IsDepthClipControlSupported() &&
            regs.clipper_control.clip_space == AmdGpu::ClipSpace::MinusWToW;
        info.hw.vs.clip_disable = regs.IsClipDisabled();
        break;
    }
    case HwStage::Fragment: {
        BuildCommon(info, regs.ps_program);
        info.hw.fs.en_flags = regs.ps_input_ena;
        info.hw.fs.addr_flags = regs.ps_input_addr;
        info.hw.fs.num_inputs = regs.num_interp;
        info.hw.fs.front_face_all_bits = regs.barycentric_control.front_face_all_bits;
        info.hw.fs.depth_before_shader = regs.depth_shader_control.depth_before_shader;
        info.hw.fs.num_samples =
            regs.ps_input_addr.sample_coverage_ena && regs.ps_input_ena.sample_coverage_ena
                ? regs.aa_config.NumSamples()
                : 1;
        info.hw.fs.z_export_format = regs.z_export_format;
        u8 stencil_ref_export_enable = regs.depth_shader_control.stencil_op_val_export_enable |
                                       regs.depth_shader_control.stencil_test_val_export_enable;
        info.hw.fs.mrtz_mask = regs.depth_shader_control.z_export_enable |
                               (stencil_ref_export_enable << 1) |
                               (regs.depth_shader_control.mask_export_enable << 2) |
                               (regs.depth_shader_control.coverage_to_mask_enable << 3);
        const AmdGpu::BlendControl& cb0_blend = regs.blend_control[0];
        if (cb0_blend.enable) {
            info.hw.fs.dual_source_blending =
                LiverpoolToVK::IsDualSourceBlendFactor(cb0_blend.color_dst_factor) ||
                LiverpoolToVK::IsDualSourceBlendFactor(cb0_blend.color_src_factor);
            if (cb0_blend.separate_alpha_blend) {
                info.hw.fs.dual_source_blending |=
                    LiverpoolToVK::IsDualSourceBlendFactor(cb0_blend.alpha_dst_factor) ||
                    LiverpoolToVK::IsDualSourceBlendFactor(cb0_blend.alpha_src_factor);
            }
        } else {
            info.hw.fs.dual_source_blending = false;
        }
        for (u32 i = 0; i < regs.num_interp; i++) {
            info.hw.fs.inputs[i] = {
                .param_index = u16(regs.ps_inputs[i].input_offset),
                .is_default = u16(regs.ps_inputs[i].use_default),
                .is_flat = u16(regs.ps_inputs[i].flat_shade),
                .default_value = u16(regs.ps_inputs[i].default_value),
            };
        }
        for (u32 i = 0; i < Shader::MaxColorBuffers; i++) {
            info.hw.fs.color_buffers[i] = graphics_key.color_buffers[i];
        }
        // Lowered user clip planes ride the same emulation path as guest-exported distances, so
        // the fragment side arms whenever the hardware vertex stage lowers them, keeping its input
        // locations in sync with the shifted vertex outputs.
        const bool lowers_user_clip_planes =
            regs.clipper_control.user_clip_plane_enable &&
            !regs.stage_enable.IsStageEnabled(static_cast<u32>(HwStage::Geometry));
        info.hw.fs.clip_distance_emulation =
            ((regs.vs_output_control.clip_distance_enable &&
              !regs.stage_enable.IsStageEnabled(static_cast<u32>(HwStage::Local))) ||
             lowers_user_clip_planes) &&
            profile.needs_clip_distance_emulation;
        break;
    }
    case HwStage::Compute: {
        const AmdGpu::ComputeProgram& cs_pgm = liverpool->GetCsRegs();
        info.props.num_user_data = cs_pgm.settings.num_user_regs;
        info.props.num_allocated_vgprs = cs_pgm.settings.num_vgprs * 4;
        info.props.fp_denorm_mode32 = cs_pgm.settings.fp_denorm_mode32;
        info.props.fp_denorm_mode16_64 = cs_pgm.settings.fp_denorm_mode64;
        info.props.fp_round_mode32 = cs_pgm.settings.fp_round_mode32;
        info.props.fp_round_mode16_64 = cs_pgm.settings.fp_round_mode64;
        info.props.dx10_clamp = cs_pgm.settings.dx10_clamp;
        info.hw.cs.workgroup_size = {cs_pgm.num_thread_x.full, cs_pgm.num_thread_y.full,
                                     cs_pgm.num_thread_z.full};
        info.hw.cs.tgid_enable = {cs_pgm.IsTgidEnabled(0), cs_pgm.IsTgidEnabled(1),
                                  cs_pgm.IsTgidEnabled(2)};
        info.hw.cs.shared_memory_size = cs_pgm.SharedMemSize();
        break;
    }
    default:
        break;
    }
    switch (l_stage) {
    case SwStage::Vertex:
        info.sw.vs.step_rate_0 = regs.vgt_instance_step_rate_0;
        info.sw.vs.step_rate_1 = regs.vgt_instance_step_rate_1;
        info.sw.vs.vertex_sgpr_offset = draw_indirect_params.vertex_sgpr_offset;
        info.sw.vs.instance_sgpr_offset = draw_indirect_params.instance_sgpr_offset;
        info.sw.vs.is_indirect = draw_indirect_params.is_indirect;
        info.sw.vs.tess_emulated_primitive =
            regs.primitive_type == AmdGpu::PrimitiveType::RectList ||
            regs.primitive_type == AmdGpu::PrimitiveType::QuadList;
        break;
    case SwStage::TessellationControl: {
        info.sw.tcs.num_input_control_points = regs.ls_hs_config.hs_input_control_points;
        info.sw.tcs.num_threads = regs.ls_hs_config.hs_output_control_points;
        info.sw.tcs.tess_type = regs.tess_config.type;
        info.sw.tcs.offchip_lds_enable = regs.hs_program.settings.oc_lds_en;
        break;
    }
    case SwStage::TessellationEval: {
        info.sw.tes.tess_type = regs.tess_config.type;
        info.sw.tes.tess_topology = regs.tess_config.topology;
        info.sw.tes.tess_partitioning = regs.tess_config.partitioning;
        break;
    }
    default:
        break;
    }
    return info;
}

PipelineCache::PipelineCache(const Instance& instance_, Scheduler& scheduler_,
                             AmdGpu::Liverpool* liverpool_, u32 sparse_page_shift)
    : instance{instance_}, scheduler{scheduler_}, liverpool{liverpool_},
      desc_heap{instance, scheduler.GetWorkSemaphore(), DescriptorHeapSizes,
                sizeof(DescriptorHeapSizes) / sizeof(DescriptorHeapSizes[0])} {
    // Sized for a large game's shaders and pipelines, so they seldom grow mid-game
    ptr_table_init(&program_cache, 4096);
    ptr_table_init(&compute_pipelines, 1024);
    ptr_table_init(&graphics_pipelines, 8192);
    const vk::PhysicalDeviceVulkan12Properties& vk12_props = instance.GetVk12Properties();
    profile = Shader::Profile{
        .max_viewport_width = instance.GetMaxViewportWidth(),
        .max_viewport_height = instance.GetMaxViewportHeight(),
        .max_shared_memory_size = instance.MaxComputeSharedMemorySize(),
        .supported_spirv = SpirvVersion1_6,
        .subgroup_size = instance.SubgroupSize(),
        .sparse_page_shift = sparse_page_shift,
        .support_int8 = instance.IsShaderInt8Supported(),
        .support_int16 = instance.IsShaderInt16Supported(),
        .support_int64 = instance.IsShaderInt64Supported(),
        .support_float16 = instance.IsShaderFloat16Supported(),
        .support_float64 = instance.IsShaderFloat64Supported(),
        .supports_denorm_behavior_independence =
            vk12_props.denormBehaviorIndependence != vk::ShaderFloatControlsIndependence::eNone,
        .supports_rounding_mode_independence =
            vk12_props.roundingModeIndependence != vk::ShaderFloatControlsIndependence::eNone,
        .support_fp16_denorm_preserve = bool(vk12_props.shaderDenormPreserveFloat16),
        .support_fp16_denorm_flush = bool(vk12_props.shaderDenormFlushToZeroFloat16),
        .support_fp16_round_to_zero = bool(vk12_props.shaderRoundingModeRTZFloat16),
        .support_fp32_denorm_preserve = bool(vk12_props.shaderDenormPreserveFloat32),
        .support_fp32_denorm_flush = bool(vk12_props.shaderDenormFlushToZeroFloat32),
        .support_fp32_round_to_zero = bool(vk12_props.shaderRoundingModeRTZFloat32),
        .support_fp64_denorm_preserve = bool(vk12_props.shaderDenormPreserveFloat64),
        .support_fp64_denorm_flush = bool(vk12_props.shaderDenormFlushToZeroFloat64),
        .support_fp64_round_to_zero = bool(vk12_props.shaderRoundingModeRTZFloat64),
        .support_fp16_signed_zero_inf_nan_preserve =
            bool(vk12_props.shaderSignedZeroInfNanPreserveFloat16),
        .support_fp32_signed_zero_inf_nan_preserve =
            bool(vk12_props.shaderSignedZeroInfNanPreserveFloat32),
        .support_fp64_signed_zero_inf_nan_preserve =
            bool(vk12_props.shaderSignedZeroInfNanPreserveFloat64),
        .supports_image_load_store_lod = instance_.IsImageLoadStoreLodSupported(),
        .supports_native_cube_calc = instance_.IsAmdGcnShaderSupported(),
        .supports_trinary_minmax = instance_.IsAmdShaderTrinaryMinMaxSupported(),
        .supports_buffer_fp32_atomic_min_max =
            instance_.IsShaderAtomicFloatBuffer32MinMaxSupported(),
        .supports_image_fp32_atomic_min_max = instance_.IsShaderAtomicFloatImage32MinMaxSupported(),
        .supports_buffer_int64_atomics = instance_.IsBufferInt64AtomicsSupported(),
        .supports_shared_int64_atomics = instance_.IsSharedInt64AtomicsSupported(),
        .supports_workgroup_explicit_memory_layout =
            instance_.IsWorkgroupMemoryExplicitLayoutSupported(),
        .supports_amd_shader_explicit_vertex_parameter =
            instance_.IsAmdShaderExplicitVertexParameterSupported(),
        .supports_fragment_shader_barycentric = instance_.IsFragmentShaderBarycentricSupported(),
        .supports_shader_subgroup_clock = instance_.IsShaderSubgroupClockSupported(),
        .needs_manual_interpolation = instance.IsFragmentShaderBarycentricSupported() &&
                                      instance.GetDriverID() == vk::DriverId::eNvidiaProprietary,
        .needs_lds_barriers = instance.GetDriverID() == vk::DriverId::eNvidiaProprietary ||
                              instance.GetDriverID() == vk::DriverId::eMesaKosmickrisp,
        .needs_buffer_offsets = instance.StorageMinAlignment() > 4,
        .needs_unorm_fixup = instance.GetDriverID() == vk::DriverId::eMesaKosmickrisp,
        .needs_clip_distance_emulation = instance.GetDriverID() == vk::DriverId::eNvidiaProprietary,
        .supports_shader_stencil_export = instance_.IsShaderStencilExportSupported(),
        .fast_shader_math = emu_settings_get_fast_shader_math(),
    };
    // The shader patches, looked for once: a game's shaders are compiled while it plays, and
    // with none to find each one went to the disk for its patch
    {
        const std::filesystem::path patch_dir =
            Common::FS::GetUserPath(Common::FS::PathType::ShaderDir) / "patch";
        if (!Common::FS::Exists(patch_dir)) {
            Common::FS::CreateDirectories(patch_dir);
        }
        Common::FS::ForEachEntry(patch_dir, [this](std::string_view, bool is_dir) {
            has_shader_patches = has_shader_patches || !is_dir;
        });
    }
    WarmUp();
    if (!pipeline_cache) {
        CreateDriverCache(false);
    }
    // What the driver built is kept for the next start, written as the disk cache closes (the
    // emulator's process is ended then, and this cache with it)
    if (Storage::DataBase::Instance().IsOpened()) {
        driver_cache_hook = {
            [](void* self) { static_cast<PipelineCache*>(self)->SaveDriverCache(); }, this};
        Storage::DataBase::Instance().SetCloseHook(&driver_cache_hook);
    }

    async_compile = emu_settings_get_async_shaders();
    mpsc_stack_init(&compile_jobs);
    mpsc_stack_init(&compile_results);
    retro_eventcount_init(&compile_jobs_ec);
    retro_eventcount_init(&compile_results_ec);
    stop_thread_start(
        &compiler, [](void* self) { static_cast<PipelineCache*>(self)->CompilerThread(); }, this);
}

PipelineCache::~PipelineCache() {
    Storage::DataBase::Instance().SetCloseHook(nullptr);
    stop_thread_request(&compiler);
    retro_eventcount_notify(&compile_jobs_ec);
    stop_thread_join(&compiler);
    for (mpsc_stack_node_t* node = mpsc_stack_drain(&compile_results); node;) {
        CompileJob* job = reinterpret_cast<CompileJob*>(node);
        node = node->next;
        delete job->pipeline;
        delete job;
    }
    for (mpsc_stack_node_t* node = mpsc_stack_drain(&compile_jobs); node;) {
        CompileJob* job = reinterpret_cast<CompileJob*>(node);
        node = node->next;
        delete job;
    }
    retro_eventcount_free(&compile_jobs_ec);
    retro_eventcount_free(&compile_results_ec);
    size_t pos = 0;
    while (GraphicsPipeline* p =
               static_cast<GraphicsPipeline*>(ptr_table_next(&graphics_pipelines, &pos))) {
        delete p;
    }
    pos = 0;
    while (ComputePipeline* p =
               static_cast<ComputePipeline*>(ptr_table_next(&compute_pipelines, &pos))) {
        delete p;
    }
    pos = 0;
    const VkDevice device = static_cast<VkDevice>(instance.GetDevice());
    while (Program* p = static_cast<Program*>(ptr_table_next(&program_cache, &pos))) {
        // Each module is its program's own, and every pipeline made from it is gone by now
        for (const Program::Module& module : p->modules) {
            vkDestroyShaderModule(device, static_cast<VkShaderModule>(module.module), nullptr);
        }
        delete p;
    }
    ptr_table_free(&graphics_pipelines);
    ptr_table_free(&compute_pipelines);
    ptr_table_free(&program_cache);
    vkDestroyPipelineCache(device, pipeline_cache, nullptr);
}

void PipelineCache::CreateDriverCache(bool from_disk) {
    const VkDevice device = static_cast<VkDevice>(instance.GetDevice());
    VkPipelineCacheCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if (from_disk) {
        // Mapped, and handed to the driver where it lies
        const std::optional<Common::FS::FileView> kept =
            Storage::DataBase::Instance().Load(Storage::BlobType::DriverCache, "driver");
        if (kept && pipeline_cache_blob_fits(kept->data(), kept->size(), instance.GetVendorID(),
                                             instance.GetDeviceID(),
                                             instance.GetPipelineCacheUUID().data())) {
            info.initialDataSize = kept->size();
            info.pInitialData = kept->data();
            const VkResult result = vkCreatePipelineCache(device, &info, nullptr, &pipeline_cache);
            if (result == VK_SUCCESS) {
                LOG_INFO(Render_Vulkan, "Driver pipeline cache: %zu bytes kept", kept->size());
                return;
            }
            pipeline_cache = VK_NULL_HANDLE;
            LOG_WARNING(Render_Vulkan, "Driver pipeline cache not taken: %s", ResultName(result));
            info.initialDataSize = 0;
            info.pInitialData = nullptr;
        } else if (kept) {
            LOG_INFO(Render_Vulkan, "Driver pipeline cache of another GPU or driver: not taken");
        }
    }
    const VkResult result = vkCreatePipelineCache(device, &info, nullptr, &pipeline_cache);
    ASSERT_MSG(result == VK_SUCCESS, "Failed to create pipeline cache: %s", ResultName(result));
}

void PipelineCache::SaveDriverCache() {
    if (!pipeline_cache) {
        return;
    }
    // The driver's cache is its own to lock: read while pipelines are made into it. The driver
    // writes it straight into the vector handed to the disk cache.
    const VkDevice device = static_cast<VkDevice>(instance.GetDevice());
    std::vector<u8> data;
    VkResult result;
    do {
        size_t size = 0;
        result = vkGetPipelineCacheData(device, pipeline_cache, &size, nullptr);
        if (result != VK_SUCCESS || size == 0) {
            return;
        }
        data.resize(size);
        result = vkGetPipelineCacheData(device, pipeline_cache, &size, data.data());
        data.resize(size);
    } while (result == VK_INCOMPLETE);
    if (result != VK_SUCCESS || data.empty()) {
        return;
    }
    Storage::DataBase::Instance().Save(Storage::BlobType::DriverCache, "driver",
                                       static_cast<std::vector<u8>&&>(data));
}

static int MatchGraphicsKey(const void* pipeline, const void* key) {
    return static_cast<const GraphicsPipeline*>(pipeline)->GetGraphicsKey() ==
           *static_cast<const GraphicsPipelineKey*>(key);
}

GraphicsPipeline* PipelineCache::FindGraphicsPipeline(const GraphicsPipelineKey& key,
                                                      u64 hash) const {
    return static_cast<GraphicsPipeline*>(
        ptr_table_find(&graphics_pipelines, hash, MatchGraphicsKey, &key));
}

// Whether every image the draw renders into is small. What a game carries from one frame to the
// next is kept in such images at the end of a chain that makes it smaller and smaller (exposure,
// the scene's average luminance, eye adaptation): a draw into one skipped while the draws that
// read it run leaves a value the next frames build on (black, or not a number, for good).
bool PipelineCache::DrawsIntoSmallTargets() const {
    constexpr u64 SmallTargetTexels = 256 * 256;
    const AmdGpu::Regs& regs = liverpool->regs;
    bool any = false;
    if (regs.color_control.mode != AmdGpu::ColorControl::OperationMode::Disable) {
        for (s32 cb = 0; cb < AmdGpu::NUM_COLOR_BUFFERS; ++cb) {
            const AmdGpu::ColorBuffer& col_buf = regs.color_buffers[cb];
            if (!col_buf || !regs.color_target_mask.GetMask(cb)) {
                continue;
            }
            any = true;
            if (u64{col_buf.Pitch()} * col_buf.Height() > SmallTargetTexels) {
                return false;
            }
        }
    }
    if (regs.depth_buffer.DepthValid() || regs.depth_buffer.StencilValid()) {
        any = true;
        if (u64{regs.depth_buffer.Pitch()} * regs.depth_buffer.Height() > SmallTargetTexels) {
            return false;
        }
    }
    return any;
}

const GraphicsPipeline* PipelineCache::GetGraphicsPipeline(const DrawIndirectParams params,
                                                           MaySkip may_skip, void* ctx) {
    draw_indirect_params = params;
    // Asynchronous: a draw whose pipeline is not ready is skipped, but one into small images,
    // or into an image the game did not draw into the frame before (@may_skip), only waits
    // for it, as compute work does. Worked out only if the pipeline turns out not to be
    // ready: a draw whose pipeline is made pays nothing for the option.
    int skip = -1;
    for (;;) {
        DrainCompiled();
        stage_pending = false;
        if (!RefreshGraphicsKey()) {
            if (!stage_pending || SkipUnready(skip, may_skip, ctx)) {
                return nullptr;
            }
            WaitCompiled();
            continue;
        }
        const u64 pipeline_hash = graphics_key.Hash();
        if (GraphicsPipeline* pipeline = FindGraphicsPipeline(graphics_key, pipeline_hash)) {
            return pipeline;
        }
        if (!IsPipelinePending(pipeline_hash)) {
            if (num_pending_pipelines == MaxPendingPipelines) {
                // As many in the making as there is room for: this one waits its turn
                if (SkipUnready(skip, may_skip, ctx)) {
                    return nullptr;
                }
                WaitCompiled();
                continue;
            }
            LOG_INFO(Render_Vulkan, "Compiling graphics pipeline 0x%llx",
                     static_cast<unsigned long long>(pipeline_hash));
            CompileJob* job = new CompileJob;
            job->kind = CompileJob::Kind::Graphics;
            job->key = graphics_key;
            job->key_hash = pipeline_hash;
            for (u32 stage = 0; stage < MaxShaderStages; ++stage) {
                if (!infos[stage]) {
                    continue;
                }
                job->has_stage[stage] = true;
                job->stage_infos[stage] = *infos[stage];
                const std::span<const u32> ud = infos[stage]->user_data;
                const size_t ud_count = ud.size() < Shader::ShaderParams::NumShaderUserData
                                            ? ud.size()
                                            : Shader::ShaderParams::NumShaderUserData;
                memcpy(job->stage_user_data[stage], ud.data(), ud_count * sizeof(u32));
                job->stage_infos[stage].user_data =
                    std::span<const u32>{job->stage_user_data[stage], ud.size()};
                job->runtimes[stage] = runtime_infos[stage];
                job->modules[stage] = modules[stage];
            }
            if (fetch_shader) {
                job->fetch = *fetch_shader;
                job->has_fetch = true;
            }
            // Made on the compiler thread from what the draw saw: the vertex buffers' V#s
            // (their formats) read now, the stages' flat buffers copied with them
            for (u32 stage = 0; stage < MaxShaderStages; ++stage) {
                if (!infos[stage]) {
                    continue;
                }
                Shader::GuestSnapshot& snap = job->stage_snapshots[stage];
                if (infos[stage]->sw_stage == Shader::SwStage::Vertex && job->has_fetch) {
                    for (const Shader::Gcn::VertexAttribute& attrib : job->fetch.attributes) {
                        job->snap_reads.push_back({attrib.sgpr_base, attrib.dword_offset,
                                                   infos[stage]->ReadUdReg<std::array<u32, 4>>(
                                                       attrib.sgpr_base, attrib.dword_offset)});
                    }
                    snap.fetch = &job->fetch;
                    snap.reads = {job->snap_reads.data(), job->snap_reads.size()};
                }
                snap.flat = job->stage_infos[stage].flattened_ud_buf;
                job->stage_infos[stage].snapshot = &snap;
            }
            pending_pipelines[num_pending_pipelines++] = pipeline_hash;
            Submit(job);
        }
        if (SkipUnready(skip, may_skip, ctx)) {
            return nullptr;
        }
        WaitCompiled();
    }
}

const ComputePipeline* PipelineCache::GetComputePipeline() {
    // The game reads back what a dispatch writes: it waits for its shader, whatever the option
    for (;;) {
        DrainCompiled();
        stage_pending = false;
        RefreshComputeKey();
        if (!stage_pending) {
            break;
        }
        WaitCompiled();
    }
    ComputePipeline* pipeline = FindComputePipeline(compute_key);
    if (!pipeline) {
        const u64 pipeline_hash = compute_key.value;
        LOG_INFO(Render_Vulkan, "Compiling compute pipeline 0x%llx",
                 static_cast<unsigned long long>(pipeline_hash));

        ComputePipeline::SerializationSupport sdata{};
        pipeline = new ComputePipeline(
            instance, scheduler, desc_heap, profile, static_cast<VkPipelineCache>(pipeline_cache),
            compute_key, *infos[0], static_cast<VkShaderModule>(modules[0]), sdata, false);
        ptr_table_insert(&compute_pipelines, compute_key.value, pipeline);
        RegisterPipelineData(compute_key, sdata);
        ++num_new_pipelines;
    }
    return pipeline;
}

bool PipelineCache::RefreshGraphicsKey() {
    memset(&graphics_key, 0, sizeof(GraphicsPipelineKey));
    const AmdGpu::Regs& regs = liverpool->regs;
    GraphicsPipelineKey& key = graphics_key;

    const bool db_enabled = regs.depth_buffer.DepthValid() || regs.depth_buffer.StencilValid();

    key.z_format = regs.depth_buffer.DepthValid() ? regs.depth_buffer.z_info.format
                                                  : AmdGpu::DepthBuffer::ZFormat::Invalid;
    key.stencil_format = regs.depth_buffer.StencilValid()
                             ? regs.depth_buffer.stencil_info.format
                             : AmdGpu::DepthBuffer::StencilFormat::Invalid;
    key.depth_clamp_enable = !regs.depth_render_override.disable_viewport_clamp;
    key.depth_clip_enable = regs.clipper_control.ZclipEnable();
    key.clip_space = regs.clipper_control.clip_space;
    key.provoking_vtx_last = regs.polygon_control.provoking_vtx_last;
    key.prim_type = regs.primitive_type;
    key.polygon_mode = regs.polygon_control.PolyMode();
    key.patch_control_points =
        regs.stage_enable.hs_en ? regs.ls_hs_config.hs_input_control_points : 0;
    key.logic_op = regs.color_control.rop3;
    key.depth_samples = db_enabled ? regs.depth_buffer.NumSamples() : 1;
    key.num_samples = key.depth_samples;
    key.cb_shader_mask = regs.color_shader_mask;

    const bool skip_cb_binding =
        regs.color_control.mode == AmdGpu::ColorControl::OperationMode::Disable;

    // First pass to fill render target information needed by shader recompiler
    for (s32 cb = 0; cb < AmdGpu::NUM_COLOR_BUFFERS && !skip_cb_binding; ++cb) {
        const AmdGpu::ColorBuffer& col_buf = regs.color_buffers[cb];
        if (!col_buf || !regs.color_target_mask.GetMask(cb)) {
            // No attachment bound or writing to it is disabled.
            continue;
        }

        // Fill color target information
        Shader::PsColorBuffer& color_buffer = key.color_buffers[cb];
        color_buffer.data_format = col_buf.GetDataFmt();
        color_buffer.num_format = col_buf.GetNumberFmt();
        color_buffer.num_conversion = col_buf.GetNumberConversion();
        color_buffer.export_format = regs.color_export_format.GetFormat(cb);
        color_buffer.swizzle = col_buf.Swizzle();

        const AmdGpu::BlendControl& bc = regs.blend_control[cb];
        color_buffer.blend_self_scale =
            bc.enable && !col_buf.info.blend_bypass &&
            (bc.color_func == AmdGpu::BlendControl::BlendFunc::Min ||
             bc.color_func == AmdGpu::BlendControl::BlendFunc::Max) &&
            bc.color_src_factor == AmdGpu::BlendControl::BlendFactor::SrcColor &&
            bc.color_dst_factor == AmdGpu::BlendControl::BlendFactor::DstColor;
    }

    // Compile and bind shader stages
    if (!RefreshGraphicsStages()) {
        return false;
    }

    // Second pass to mask out render targets not written by shader and fill remaining info
    u8 color_samples = 0;
    bool all_color_samples_same = true;
    for (s32 cb = 0; cb < key.num_color_attachments && !skip_cb_binding; ++cb) {
        const AmdGpu::ColorBuffer& col_buf = regs.color_buffers[cb];
        const u32 target_mask = regs.color_target_mask.GetMask(cb);
        if (!col_buf || !target_mask) {
            continue;
        }
        if ((key.mrt_mask & (1u << cb)) == 0) {
            memset(&key.color_buffers[cb], 0, sizeof(Shader::PsColorBuffer));
            continue;
        }

        // Fill color blending information
        if (regs.blend_control[cb].enable && !col_buf.info.blend_bypass) {
            key.blend_controls[cb] = regs.blend_control[cb];
        }

        // Apply swizzle to target mask
        key.write_masks[cb] = key.color_buffers[cb].swizzle.ApplyMask(target_mask);

        // Fill color samples
        const u8 prev_color_samples = color_samples;
        color_samples = col_buf.NumSamples();
        all_color_samples_same &= color_samples == prev_color_samples || prev_color_samples == 0;
        key.color_samples[cb] = color_samples;
        key.num_samples = key.num_samples > color_samples ? key.num_samples : color_samples;
    }

    // Force all color samples to match depth samples to avoid unsupported MSAA configuration
    if (color_samples != 0) {
        const bool depth_mismatch = db_enabled && color_samples != key.depth_samples;
        // A fragment shader that runs once for each sample (it reads the sample's number, or an
        // input taken at the sample) writes a colour for each. With fewer colour samples than
        // depth samples (what VK_NV_framebuffer_mixed_samples allows, and forbids together with
        // sample shading) the samples past the colour target's have nowhere to go: Sekiro's
        // checkerboard pass, one colour sample over two of depth, lost the GPU device that way
        // with a write to address nothing. Such a pass gets as many colour samples as depth.
        const Shader::Info* fs_info = infos[static_cast<u32>(Shader::SwStage::Fragment)];
        const bool per_sample =
            regs.ps_input_addr.persp_sample_ena || regs.ps_input_addr.linear_sample_ena ||
            (fs_info && (fs_info->loads.Get(Shader::IR::Attribute::SampleIndex) ||
                         fs_info->loads.Get(Shader::IR::Attribute::BaryCoordNoPerspSample) ||
                         fs_info->loads.Get(Shader::IR::Attribute::BaryCoordSmoothSample)));
        if (!all_color_samples_same && !instance.IsMixedAnySamplesSupported() ||
            all_color_samples_same && depth_mismatch &&
                (!instance.IsMixedDepthSamplesSupported() ||
                 (per_sample && color_samples < key.depth_samples))) {
            memset(key.color_samples, key.depth_samples, sizeof(key.color_samples));
            key.num_samples = key.depth_samples;
        }
    }

    // A pass whose samples the game has moved (a checkerboard does, from row to row and frame
    // to frame) gets them where the game put them, on a device that can
    key.sample_locations = 0;
    if (const u32 grid = key.num_samples > 1 ? instance.SampleLocationGrid(key.num_samples) : 0) {
        u8 locs[AMDGPU_SAMPLE_LOCS_MAX];
        key.sample_locations =
            amdgpu_sample_locs(regs.aa_sample_locs, key.num_samples, grid, locs) != 0;
    }

    return true;
}

bool PipelineCache::RefreshGraphicsStages() {
    const AmdGpu::Regs& regs = liverpool->regs;
    GraphicsPipelineKey& key = graphics_key;
    fetch_shader = nullptr;

    Shader::Backend::Bindings binding{};

    for (u32 i = 0; i < MaxShaderStages; ++i) {
        infos[i] = nullptr;
        modules[i] = vk::ShaderModule{};
    }

    BindStage(binding, HwStage::Fragment, SwStage::Fragment);
    if (stage_pending) {
        return false;
    }

    const Shader::Info* fs_info = infos[static_cast<u32>(SwStage::Fragment)];
    key.mrt_mask = fs_info ? fs_info->mrt_mask : 0u;
    // One past the highest colour target written
    u32 num_color_attachments = 0;
    for (u32 mask = key.mrt_mask; mask; mask >>= 1) {
        ++num_color_attachments;
    }
    key.num_color_attachments = num_color_attachments;

    switch (regs.stage_enable.raw) {
    case AmdGpu::ShaderStageEnable::VgtStages::EsGs:
        if (!instance.IsGeometryStageSupported()) {
            LOG_WARNING(Render_Vulkan, "Geometry shader stage unsupported, skipping");
            return false;
        }
        if (regs.vgt_gs_mode.onchip || regs.vgt_strmout_config.raw) {
            LOG_WARNING(Render_Vulkan, "Geometry shader features unsupported, skipping");
            return false;
        }
        if (!BindStage(binding, HwStage::Export, SwStage::Vertex)) {
            return false;
        }
        if (!BindStage(binding, HwStage::Geometry, SwStage::Geometry)) {
            return false;
        }
        break;
    case AmdGpu::ShaderStageEnable::VgtStages::LsHs:
        if (!instance.IsTessellationSupported()) {
            return false;
        }
        if (!BindStage(binding, HwStage::Hull, SwStage::TessellationControl)) {
            return false;
        }
        if (!BindStage(binding, HwStage::Vertex, SwStage::TessellationEval)) {
            return false;
        }
        if (!BindStage(binding, HwStage::Local, SwStage::Vertex)) {
            return false;
        }
        break;
    case AmdGpu::ShaderStageEnable::VgtStages::LsHsEsGs:
        if (!instance.IsTessellationSupported()) {
            return false;
        }
        if (!instance.IsGeometryStageSupported()) {
            LOG_WARNING(Render_Vulkan, "Geometry shader stage unsupported, skipping");
            return false;
        }
        if (regs.vgt_gs_mode.onchip || regs.vgt_strmout_config.raw) {
            LOG_WARNING(Render_Vulkan, "Geometry shader features unsupported, skipping");
            return false;
        }
        if (!BindStage(binding, HwStage::Hull, SwStage::TessellationControl)) {
            return false;
        }
        if (!BindStage(binding, HwStage::Export, SwStage::TessellationEval)) {
            return false;
        }
        if (!BindStage(binding, HwStage::Local, SwStage::Vertex)) {
            return false;
        }
        if (!BindStage(binding, HwStage::Geometry, SwStage::Geometry)) {
            return false;
        }
        break;
    case AmdGpu::ShaderStageEnable::VgtStages::Vs:
        BindStage(binding, HwStage::Vertex, SwStage::Vertex);
        break;
    default:
        LOG_WARNING(Render_Vulkan, "unimplemented shader stage %u", (u32)regs.stage_enable.raw);
        return false;
    }

    if (stage_pending) {
        return false;
    }
    const Shader::Info* vs_info = infos[static_cast<u32>(SwStage::Vertex)];
    if (vs_info && fetch_shader && !instance.IsVertexInputDynamicState()) {
        // Without vertex input dynamic state, the pipeline needs to specialize on format.
        // Stride will still be handled outside the pipeline using dynamic state.
        u32 vertex_binding = 0;
        for (const Shader::Gcn::VertexAttribute& attrib : fetch_shader->attributes) {
            const AmdGpu::Buffer buffer = attrib.GetSharp(*vs_info);
            ASSERT_MSG(vertex_binding < MaxVertexBufferCount,
                       "Vertex attribute binding count exceeded limit: %u >= %u", vertex_binding,
                       MaxVertexBufferCount);
            key.vertex_buffer_formats[vertex_binding++] = static_cast<VkFormat>(
                Vulkan::LiverpoolToVK::SurfaceFormat(buffer.GetDataFmt(), buffer.GetNumberFmt()));
        }
    }

    return true;
}

bool PipelineCache::RefreshComputeKey() {
    Shader::Backend::Bindings binding{};
    const AmdGpu::ComputeProgram& cs_pgm = liverpool->GetCsRegs();
    const Shader::ShaderParams cs_params = AmdGpu::GetParams(cs_pgm);
    const Result result = GetProgram(HwStage::Compute, SwStage::Compute, cs_params, binding);
    infos[0] = result.info;
    modules[0] = result.module;
    compute_key.value = result.hash;
    return true;
}

vk::ShaderModule PipelineCache::CompileModule(Shader::Info& info, Shader::RuntimeInfo& runtime_info,
                                              const std::span<const u32>& code, size_t perm_idx,
                                              Shader::Backend::Bindings& binding) {
    LOG_INFO(Render_Vulkan, "Compiling %s shader 0x%" PRIx64 " %s", Shader::NameOf(info.hw_stage),
             info.pgm_hash, perm_idx != 0 ? "(permutation)" : "");
    DumpShader(code, info.pgm_hash, info.hw_stage, perm_idx, "bin");

    const Shader::IR::Program ir_program =
        Shader::TranslateProgram(code, pools, info, runtime_info, profile);
    return ModuleFromProgram(info, runtime_info, ir_program, perm_idx, binding);
}

vk::ShaderModule PipelineCache::ModuleFromProgram(Shader::Info& info,
                                                  Shader::RuntimeInfo& runtime_info,
                                                  const Shader::IR::Program& ir_program,
                                                  size_t perm_idx,
                                                  Shader::Backend::Bindings& binding) {
    const std::span<const u32> spv =
        Shader::Backend::SPIRV::EmitSPIRV(pools.spv, profile, runtime_info, ir_program, binding);
    DumpShader(spv, info.pgm_hash, info.hw_stage, perm_idx, "spv");

    vk::ShaderModule module;

    const std::optional<Common::FS::FileView> patch_file =
        GetShaderPatch(info.pgm_hash, info.hw_stage, perm_idx, "spv");
    const std::span<const u32> patch =
        patch_file ? std::span<const u32>{reinterpret_cast<const u32*>(patch_file->data()),
                                          patch_file->size() / sizeof(u32)}
                   : std::span<const u32>{};
    const bool is_patched = !patch.empty() && emu_settings_get_patch_shaders();
    if (is_patched) {
        LOG_INFO(Loader, "Loaded patch for %s shader 0x%" PRIx64, Shader::NameOf(info.hw_stage),
                 info.pgm_hash);
        module = CompileSPV(patch, instance.GetDevice());
    } else {
        module = CompileSPV(spv, instance.GetDevice());
    }

    RegisterShaderBinary(spv, info.pgm_hash, perm_idx);

    char name[ShaderNameSize];
    GetShaderName(name, sizeof(name), info.hw_stage, info.pgm_hash, static_cast<int>(perm_idx));
    Vulkan::SetObjectName(instance.GetDevice(), module, "%s", name);
    return module;
}

/// The drawing thread's half of a translation: what it will read of the game's memory, read
/// now, at the draw (Shader::GuestSnapshot). @info is the shader's description with this
/// draw's user data, its flat buffer refreshed (RefreshFlatBuf).
bool PipelineCache::CodeUnchanged(const CompileJob& job) {
    if (job.code_static) {
        return true;
    }
    AmdGpu::BinaryInfo now{};
    Core::Memory::Instance()->CopySparseMemory(job.code_info, reinterpret_cast<u8*>(&now),
                                               sizeof(now));
    return now.Valid() && now.shader_hash == job.code_hash && now.crc32 == job.code_crc;
}

void PipelineCache::CaptureSnapshot(CompileJob& job, const Shader::Info& info) {
    job.snap_flat = info.flattened_ud_buf;
    job.snap_walker = WalkerId(info.srt_walker_code);
    if (info.has_fetch_shader) {
        Shader::Gcn::FetchShaderData fetch;
        if (Shader::Gcn::ParseFetchShader(info, fetch)) {
            for (const Shader::Gcn::VertexAttribute& attrib : fetch.attributes) {
                job.snap_reads.push_back(
                    {attrib.sgpr_base, attrib.dword_offset,
                     info.ReadUdReg<std::array<u32, 4>>(attrib.sgpr_base, attrib.dword_offset)});
            }
            job.snap_fetch = static_cast<Shader::Gcn::FetchShaderData&&>(fetch);
            job.has_snap_fetch = true;
        }
    }
    if (info.tess_consts_dword_offset >= 0) {
        const u32 ptr = static_cast<u32>(info.tess_consts_ptr_base);
        const u32 offset = static_cast<u32>(info.tess_consts_dword_offset);
        job.snap_reads.push_back({ptr, offset, info.ReadUdReg<std::array<u32, 4>>(ptr, offset)});
        info.ReadTessConstantBuffer(job.snap_tess);
        job.snapshot.tess = &job.snap_tess;
    }
    job.snapshot.flat = job.snap_flat;
    job.snapshot.fetch = job.has_snap_fetch ? &job.snap_fetch : nullptr;
    job.snapshot.reads = {job.snap_reads.data(), job.snap_reads.size()};
}

PipelineCache::Result PipelineCache::GetProgram(HwStage hw_stage, SwStage sw_stage,
                                                const Shader::ShaderParams& params,
                                                Shader::Backend::Bindings& binding) {
    const Shader::RuntimeInfo& runtime_info = BuildRuntimeInfo(hw_stage, sw_stage);

    Program* program = FindProgram(params.hash);
    if (!program) {
        // Filed now, so that it is compiled once. The compiler thread first makes its walker
        // (a discovery, which reads nothing of the game's but the code), then, from a snapshot
        // the next draw takes, translates it.
        program = new Program(hw_stage, sw_stage, params);
        program->pending = true;
        ptr_table_insert(&program_cache, params.hash, program);
    }
    if (program->pending && !program->discovery_submitted) {
        CompileJob* job = MakeJob(hw_stage, sw_stage, params, runtime_info, binding,
                                  CompileJob::Kind::Discover, 0, program);
        job->snapshot.discover_only = true;
        job->snapshot.finished = &job->finished;
        program->discovery_submitted = true;
        Submit(job);
        return PendingResult();
    }
    if (program->pending) {
        if (program->discovered && !program->submitted) {
            Shader::Info& info = program->info;
            info.pgm_base = params.Base();
            info.user_data = params.user_data;
            info.RefreshFlatBuf();
            CompileJob* job = MakeJob(hw_stage, sw_stage, params, runtime_info, binding,
                                      CompileJob::Kind::Program, 0, program);
            CaptureSnapshot(*job, info);
            program->submitted = true;
            Submit(job);
        }
        return PendingResult();
    }

    Shader::Info& info = program->info;
    info.pgm_base = params.Base(); // Needs to be actualized for inline cbuffer address fixup
    info.user_data = params.user_data;
    info.RefreshFlatBuf();
    Shader::StageSpecialization spec(info, runtime_info, profile, binding);

    size_t found = 0;
    while (found < program->modules.size() && !(program->modules[found].spec == spec)) {
        ++found;
    }
    if (found == program->modules.size()) {
        // Another permutation: filed now with its specialization, compiled on the thread
        const size_t perm_idx = program->modules.size();
        program->modules.push_back(
            {vk::ShaderModule{}, static_cast<Shader::StageSpecialization&&>(spec), false, true});
        CompileJob* job = MakeJob(hw_stage, sw_stage, params, runtime_info, binding,
                                  CompileJob::Kind::Permutation, perm_idx, program);
        CaptureSnapshot(*job, info);
        Submit(job);
        return PendingResult();
    }
    Program::Module& module = program->modules[found];
    if (module.pending) {
        if (module.again) {
            module.again = false;
            CompileJob* job = MakeJob(hw_stage, sw_stage, params, runtime_info, binding,
                                      CompileJob::Kind::Permutation, found, program);
            CaptureSnapshot(*job, info);
            Submit(job);
        }
        return PendingResult();
    }

    const Shader::Backend::Bindings start = binding;
    info.AddBindings(binding);
    const size_t perm_idx = found;
    const u64 perm_hash = HashCombine(params.hash, perm_idx);
    if (module.from_cache) {
        // Checked against a translation on the thread; used meanwhile
        module.from_cache = false;
        CompileJob* job = MakeJob(hw_stage, sw_stage, params, runtime_info, binding,
                                  CompileJob::Kind::Check, perm_idx, program);
        CaptureSnapshot(*job, info);
        job->start = start;
        job->kept_walker = WalkerId(info.srt_walker_code);
        job->kept_flatbuf_dw = info.srt_info.flattened_bufsize_dw;
        job->kept_resources[0] = static_cast<u32>(info.buffers.size());
        job->kept_resources[1] = static_cast<u32>(info.images.size());
        job->kept_resources[2] = static_cast<u32>(info.samplers.size());
        job->kept_resources[3] = static_cast<u32>(info.fmasks.size());
        job->kept_dma = info.uses_dma;
        Submit(job);
    }
    if (Shader::Gcn::FetchShaderData& fetch = module.spec.fetch_shader_data; !fetch.Empty()) {
        fetch_shader = &fetch;
    }
    return Result{&program->info, module.module, perm_hash};
}

void PipelineCache::Submit(CompileJob* job) {
    mpsc_stack_push(&compile_jobs, &job->node);
    retro_eventcount_notify(&compile_jobs_ec);
}

// Graphics pipelines the compiler thread makes before the driver's cache is kept again, once it
// has nothing left to compile: often enough that little is lost to a process ended without a
// close, seldom enough that a cache of megabytes is not written for each pipeline
static constexpr u32 DriverCacheSaveAfter = 64;

void PipelineCache::CompilerThread() {
    thread_name_set("shadPS4:ShaderCompiler");
    while (!stop_thread_stopping(&compiler)) {
        const int key = retro_eventcount_prepare_wait(&compile_jobs_ec);
        mpsc_stack_node_t* node = mpsc_stack_drain(&compile_jobs);
        if (!node) {
            if (stop_thread_stopping(&compiler)) {
                retro_eventcount_cancel_wait(&compile_jobs_ec);
                break;
            }
            // Nothing left to compile: what the driver built since its cache was last kept is
            // kept now, off the game's threads, should the process end without a close
            if (driver_cache_unsaved >= DriverCacheSaveAfter) {
                retro_eventcount_cancel_wait(&compile_jobs_ec);
                driver_cache_unsaved = 0;
                SaveDriverCache();
                continue;
            }
            retro_eventcount_commit_wait(&compile_jobs_ec, key);
            continue;
        }
        retro_eventcount_cancel_wait(&compile_jobs_ec);
        // Oldest first, as they were handed over
        for (node = mpsc_stack_reverse(node); node;) {
            CompileJob* job = reinterpret_cast<CompileJob*>(node);
            node = node->next;
            if (stop_thread_stopping(&compiler)) {
                delete job;
                continue;
            }
            RunJob(*job);
            mpsc_stack_push(&compile_results, &job->node);
            retro_eventcount_notify(&compile_results_ec);
        }
    }
}

// The compiler thread's half: nothing here touches the tables
void PipelineCache::RunJob(CompileJob& job) {
    const Shader::ShaderParams params = job.Params();
    if (job.kind != CompileJob::Kind::Graphics && !CodeUnchanged(job)) {
        job.code_changed = true; // the game has reused it: done again at a later draw
        return;
    }
    RunJobOn(job, params);
    if (job.kind != CompileJob::Kind::Graphics && !CodeUnchanged(job)) {
        // Changed while it was being translated: what was made from it goes
        job.code_changed = true;
        if (job.module) {
            vkDestroyShaderModule(static_cast<VkDevice>(instance.GetDevice()),
                                  static_cast<VkShaderModule>(job.module), nullptr);
            job.module = vk::ShaderModule{};
        }
        if (job.program) {
            for (Program::Module& module : job.program->modules) {
                vkDestroyShaderModule(static_cast<VkDevice>(instance.GetDevice()),
                                      static_cast<VkShaderModule>(module.module), nullptr);
            }
            job.program->modules.clear();
        }
    }
}

void PipelineCache::RunJobOn(CompileJob& job, const Shader::ShaderParams& params) {
    switch (job.kind) {
    case CompileJob::Kind::Discover: {
        delete job.program;
        job.program = new Program(job.hw_stage, job.sw_stage, params);
        Shader::Info& info = job.program->info;
        info.snapshot = &job.snapshot;
        Shader::RuntimeInfo runtime = job.runtime;
        const Shader::IR::Program ir_program =
            Shader::TranslateProgram(params.code, pools, info, runtime, profile);
        if (!job.finished) {
            info.snapshot = nullptr;
            break;
        }
        // It read nothing of the game's but its user data: made here, as a Program job would
        // make it from the next draw, with the translation already done
        LOG_INFO(Render_Vulkan, "Compiling %s shader 0x%" PRIx64 " (from its discovery)",
                 Shader::NameOf(info.hw_stage), info.pgm_hash);
        DumpShader(params.code, info.pgm_hash, info.hw_stage, 0, "bin");
        Shader::Backend::Bindings binding = job.start;
        const vk::ShaderModule module = ModuleFromProgram(info, runtime, ir_program, 0, binding);
        Shader::StageSpecialization spec(info, runtime, profile, job.start);
        info.snapshot = nullptr;
        RegisterShaderMeta(info, spec.fetch_shader_data, spec, HashCombine(job.hash, 0), 0);
        job.program->AddPermut(module, static_cast<Shader::StageSpecialization&&>(spec));
        break;
    }
    case CompileJob::Kind::Program: {
        delete job.program;
        job.program = new Program(job.hw_stage, job.sw_stage, params);
        Shader::Info& info = job.program->info;
        info.snapshot = &job.snapshot;
        Shader::Backend::Bindings binding = job.start;
        const vk::ShaderModule module = CompileModule(info, job.runtime, params.code, 0, binding);
        Shader::StageSpecialization spec(info, job.runtime, profile, job.start);
        info.snapshot = nullptr;
        job.stale = WalkerId(info.srt_walker_code) != job.snap_walker;
        if (job.stale) {
            // Its flat buffer was of another layout: thrown away, the walker it made kept
            vkDestroyShaderModule(static_cast<VkDevice>(instance.GetDevice()),
                                  static_cast<VkShaderModule>(module), nullptr);
            break;
        }
        RegisterShaderMeta(info, spec.fetch_shader_data, spec, HashCombine(job.hash, 0), 0);
        job.program->AddPermut(module, static_cast<Shader::StageSpecialization&&>(spec));
        break;
    }
    case CompileJob::Kind::Permutation: {
        Shader::Info new_info(job.hw_stage, job.sw_stage, params);
        new_info.snapshot = &job.snapshot;
        Shader::Backend::Bindings binding = job.start;
        job.module = CompileModule(new_info, job.runtime, params.code, job.perm_idx, binding);
        new_info.snapshot = nullptr;
        if (WalkerId(new_info.srt_walker_code) != job.snap_walker) {
            LOG_WARNING(Render_Vulkan,
                        "%s shader 0x%" PRIx64 " permutation %zu: another SRT walker",
                        Shader::NameOf(job.hw_stage), job.hash, job.perm_idx);
        }
        break;
    }
    case CompileJob::Kind::Check: {
        job.fresh = Shader::Info(job.hw_stage, job.sw_stage, params);
        job.fresh.snapshot = &job.snapshot;
        Shader::RuntimeInfo runtime = job.runtime;
        Shader::Backend::Bindings binding = job.start;
        const Shader::IR::Program ir_program =
            Shader::TranslateProgram(params.code, pools, job.fresh, runtime, profile);
        const std::span<const u32> spv =
            Shader::Backend::SPIRV::EmitSPIRV(pools.spv, profile, runtime, ir_program, binding);
        job.fresh.snapshot = nullptr;
        char kept_name[40];
        snprintf(kept_name, sizeof(kept_name), "0x%016" PRIx64 "_%zu", job.hash, job.perm_idx);
        const std::optional<Common::FS::FileView> kept_spv =
            Storage::DataBase::Instance().Load(Storage::BlobType::ShaderBinary, kept_name);
        job.same_spv = kept_spv && kept_spv->size() == spv.size_bytes() &&
                       memcmp(kept_spv->data(), spv.data(), kept_spv->size()) == 0;
        job.same_walker = WalkerId(job.fresh.srt_walker_code) == job.kept_walker &&
                          job.fresh.srt_info.flattened_bufsize_dw == job.kept_flatbuf_dw;
        job.same_resources = job.kept_resources[0] == static_cast<u32>(job.fresh.buffers.size()) &&
                             job.kept_resources[1] == static_cast<u32>(job.fresh.images.size()) &&
                             job.kept_resources[2] == static_cast<u32>(job.fresh.samplers.size()) &&
                             job.kept_resources[3] == static_cast<u32>(job.fresh.fmasks.size()) &&
                             job.kept_dma == job.fresh.uses_dma;
        if (!job.same_spv || !job.same_walker || !job.same_resources) {
            job.module = CompileSPV(spv, instance.GetDevice());
        }
        break;
    }
    case CompileJob::Kind::Graphics: {
        const Shader::Info* stage_ptrs[MaxShaderStages]{};
        for (u32 stage = 0; stage < MaxShaderStages; ++stage) {
            stage_ptrs[stage] = job.has_stage[stage] ? &job.stage_infos[stage] : nullptr;
        }
        job.pipeline = new GraphicsPipeline(
            instance, scheduler, desc_heap, profile, job.key,
            static_cast<VkPipelineCache>(pipeline_cache), stage_ptrs, job.runtimes,
            job.has_fetch ? &job.fetch : nullptr, ModuleHandles(job.modules), job.sdata, false);
        ++driver_cache_unsaved;
        for (Shader::Info& info : job.stage_infos) {
            info.snapshot = nullptr;
        }
        break;
    }
    }
}

bool PipelineCache::DrainCompiled() {
    mpsc_stack_node_t* node = mpsc_stack_drain(&compile_results);
    if (!node) {
        return false;
    }
    for (node = mpsc_stack_reverse(node); node;) {
        CompileJob* job = reinterpret_cast<CompileJob*>(node);
        node = node->next;
        FileJob(*job);
        delete job;
    }
    return true;
}

void PipelineCache::WaitCompiled() {
    const int key = retro_eventcount_prepare_wait(&compile_results_ec);
    if (!mpsc_stack_empty(&compile_results)) {
        retro_eventcount_cancel_wait(&compile_results_ec);
        return;
    }
    retro_eventcount_commit_wait(&compile_results_ec, key);
}

bool PipelineCache::IsPipelinePending(u64 hash) const {
    for (u32 i = 0; i < num_pending_pipelines; ++i) {
        if (pending_pipelines[i] == hash) {
            return true;
        }
    }
    return false;
}

// The drawing thread's half: files what the compiler thread made
void PipelineCache::FileJob(CompileJob& job) {
    switch (job.kind) {
    case CompileJob::Kind::Discover: {
        // Its walker, for the next draw's snapshot (GetProgram)
        Program* placeholder = FindProgram(job.hash);
        ASSERT(placeholder && placeholder->pending && !placeholder->discovered);
        if (job.code_changed) {
            placeholder->discovery_submitted = false;
            break;
        }
        if (job.finished) {
            // Made whole: filed as a Program job's is
            *placeholder = static_cast<Program&&>(*job.program);
            for (Program::Module& module : placeholder->modules) {
                module.spec.info = &placeholder->info;
            }
            placeholder->info.user_data = {};
            placeholder->pending = false;
            break;
        }
        placeholder->info = static_cast<Shader::Info&&>(job.program->info);
        placeholder->info.user_data = {};
        placeholder->discovered = true;
        break;
    }
    case CompileJob::Kind::Program: {
        Program* placeholder = FindProgram(job.hash);
        ASSERT(placeholder && placeholder->pending && placeholder->submitted);
        if (job.code_changed) {
            placeholder->submitted = false;
            break;
        }
        if (job.stale) {
            // Taken again at the next draw, with the walker this translation made
            placeholder->info = static_cast<Shader::Info&&>(job.program->info);
            placeholder->info.user_data = {};
            placeholder->submitted = false;
            break;
        }
        *placeholder = static_cast<Program&&>(*job.program);
        // The specializations point at the description they were made from
        for (Program::Module& module : placeholder->modules) {
            module.spec.info = &placeholder->info;
        }
        placeholder->info.user_data = {};
        placeholder->pending = false;
        break;
    }
    case CompileJob::Kind::Permutation: {
        Program* program = FindProgram(job.hash);
        ASSERT(program && job.perm_idx < program->modules.size());
        Program::Module& module = program->modules[job.perm_idx];
        if (job.code_changed) {
            module.again = true; // still pending: submitted again at the next draw
            break;
        }
        module.module = job.module;
        module.pending = false;
        RegisterShaderMeta(program->info, module.spec.fetch_shader_data, module.spec,
                           HashCombine(job.hash, job.perm_idx), job.perm_idx);
        break;
    }
    case CompileJob::Kind::Check:
        if (job.code_changed) {
            // Checked again at a later draw
            if (Program* program = FindProgram(job.hash);
                program && job.perm_idx < program->modules.size()) {
                program->modules[job.perm_idx].from_cache = true;
            }
            break;
        }
        if (!job.same_spv || !job.same_walker || !job.same_resources) {
            ReplaceCachedPermutation(job);
        }
        break;
    case CompileJob::Kind::Graphics: {
        for (u32 i = 0; i < num_pending_pipelines; ++i) {
            if (pending_pipelines[i] == job.key_hash) {
                pending_pipelines[i] = pending_pipelines[--num_pending_pipelines];
                break;
            }
        }
        // The pipeline binds from the cache's descriptions, not the copies it was made from
        const Shader::Info* stage_ptrs[MaxShaderStages]{};
        for (u32 stage = 0; stage < MaxShaderStages; ++stage) {
            if (job.has_stage[stage]) {
                const Program* program = FindProgram(job.stage_infos[stage].pgm_hash);
                stage_ptrs[stage] = program ? &program->info : nullptr;
            }
        }
        job.pipeline->SetStages(stage_ptrs);
        if (FindGraphicsPipeline(job.key, job.key_hash)) {
            // Made twice (its shader was replaced meanwhile): the first stays
            scheduler.DeferOperation([p = job.pipeline] { delete p; });
            break;
        }
        ptr_table_insert(&graphics_pipelines, job.key_hash, job.pipeline);
        RegisterPipelineData(job.key, job.key_hash, job.sdata);
        ++num_new_pipelines;
        break;
    }
    }
    job.pipeline = nullptr;
}

void PipelineCache::ReplaceCachedPermutation(CompileJob& job) {
    Program* program = FindProgram(job.hash);
    if (!program || program->pending || job.perm_idx >= program->modules.size()) {
        return;
    }
    Program::Module& cached = program->modules[job.perm_idx];
    LOG_WARNING(
        Render_Vulkan,
        "Pipeline cache: %s shader 0x%" PRIx64 " permutation %zu differs from its translation "
        "(SPIR-V %s, SRT walker %s, resources %s); the translation is used, and the cache "
        "is started again at the next start",
        Shader::NameOf(job.hw_stage), job.hash, job.perm_idx, job.same_spv ? "same" : "differs",
        job.same_walker ? "same" : "differs", job.same_resources ? "same" : "differ");

    // The cache is started again at the next start (WarmUp: another build's stamp)
    Storage::DataBase::Instance().Save(Storage::BlobType::ShaderProfile, "build",
                                       std::vector<u8>{'-'});

    // The pipelines made with the kept permutation go, once the GPU is done with them
    const u64 perm_hash = HashCombine(job.hash, job.perm_idx);
    struct StalePipeline {
        u64 hash;
        GraphicsPipeline* pipeline;
    };
    Common::SmallVector<StalePipeline, 16> stale;
    size_t pos = 0;
    while (GraphicsPipeline* p =
               static_cast<GraphicsPipeline*>(ptr_table_next(&graphics_pipelines, &pos))) {
        const GraphicsPipelineKey& key = p->GetGraphicsKey();
        for (u32 stage = 0; stage < MaxShaderStages; ++stage) {
            if (key.stage_hashes[stage] == perm_hash) {
                stale.push_back({key.Hash(), p});
                break;
            }
        }
    }
    for (size_t i = 0; i < stale.size(); ++i) {
        GraphicsPipeline* const p = stale[i].pipeline;
        ptr_table_erase(&graphics_pipelines, stale[i].hash, p);
        scheduler.DeferOperation([p] { delete p; });
    }
    if (ComputePipeline* p = FindComputePipeline(ComputePipelineKey{perm_hash})) {
        ptr_table_erase(&compute_pipelines, perm_hash, p);
        scheduler.DeferOperation([p] { delete p; });
    }

    const VkShaderModule old_module = static_cast<VkShaderModule>(cached.module);
    cached.module = job.module;
    scheduler.DeferOperation([device = static_cast<VkDevice>(instance.GetDevice()), old_module] {
        vkDestroyShaderModule(device, old_module, nullptr);
    });
    // The permutations share the program's description: the translation's from now on
    program->info = static_cast<Shader::Info&&>(job.fresh);
    program->info.user_data = {};
    for (Program::Module& module : program->modules) {
        module.spec.info = &program->info;
    }
}

bool PipelineCache::SkipUnready(int& skip, MaySkip may_skip, void* ctx) const {
    if (skip < 0) {
        skip = async_compile && !DrawsIntoSmallTargets() && (!may_skip || may_skip(ctx));
    }
    return skip != 0;
}

bool PipelineCache::BindStage(Shader::Backend::Bindings& binding, HwStage stage_in,
                              SwStage stage_out) {
    const AmdGpu::Regs& regs = liverpool->regs;
    GraphicsPipelineKey& key = graphics_key;
    const u32 stage_in_idx = static_cast<u32>(stage_in);
    const u32 stage_out_idx = static_cast<u32>(stage_out);
    if (!regs.stage_enable.IsStageEnabled(stage_in_idx)) {
        key.stage_hashes[stage_out_idx] = 0;
        infos[stage_out_idx] = nullptr;
        return false;
    }

    const AmdGpu::ShaderProgram* pgm = regs.ProgramForStage(stage_in_idx);
    if (!pgm || !pgm->Address<u32*>()) {
        key.stage_hashes[stage_out_idx] = 0;
        infos[stage_out_idx] = nullptr;
        return false;
    }

    const Shader::ShaderParams params = AmdGpu::GetParams(*pgm);
    const Result result = GetProgram(stage_in, stage_out, params, binding);
    infos[stage_out_idx] = result.info;
    modules[stage_out_idx] = result.module;
    key.stage_hashes[stage_out_idx] = result.hash;
    // Being compiled: the stages after it bind after its resources, which are not known yet
    return !stage_pending;
}

PipelineCache::CompileJob* PipelineCache::MakeJob(HwStage hw_stage, SwStage sw_stage,
                                                  const Shader::ShaderParams& params,
                                                  const Shader::RuntimeInfo& runtime_info,
                                                  const Shader::Backend::Bindings& binding,
                                                  JobKind kind, size_t perm_idx, Program* program) {
    CompileJob* job = new CompileJob;
    job->kind = kind;
    job->hw_stage = hw_stage;
    job->sw_stage = sw_stage;
    memcpy(job->user_data, params.user_data.data(), sizeof(job->user_data));
    // The game's code, borrowed, with what tells whether it is still the same
    (void)program;
    const AmdGpu::BinaryInfo& info = AmdGpu::SearchBinaryInfo(params.code.data());
    job->code = params.code;
    job->code_info = reinterpret_cast<VAddr>(&info);
    job->code_hash = info.shader_hash;
    job->code_crc = info.crc32;
    u64 backing_offset{};
    (void)Core::Memory::Instance()->BackingRun(job->code_info, sizeof(info), &backing_offset);
    job->code_static = backing_offset == ~u64{0};
    job->hash = params.hash;
    job->perm_idx = perm_idx;
    job->runtime = runtime_info;
    job->start = binding;
    return job;
}

PipelineCache::Result PipelineCache::PendingResult() {
    stage_pending = true;
    return Result{nullptr, vk::ShaderModule{}, 0};
}

void PipelineCache::GetShaderName(char* out, size_t size, Shader::HwStage stage, u64 hash,
                                  int perm) {
    if (perm >= 0) {
        snprintf(out, size, "%s_0x%016" PRIx64 "_%d", Shader::NameOf(stage), hash, perm);
    } else {
        snprintf(out, size, "%s_0x%016" PRIx64, Shader::NameOf(stage), hash);
    }
}

void PipelineCache::DumpShader(std::span<const u32> code, u64 hash, Shader::HwStage stage,
                               size_t perm_idx, const char* ext) {
    if (!emu_settings_get_dump_shaders()) {
        return;
    }

    using namespace Common::FS;
    const std::filesystem::path dump_dir = GetUserPath(PathType::ShaderDir) / "dumps";
    if (!Common::FS::Exists(dump_dir)) {
        Common::FS::CreateDirectories(dump_dir);
    }
    char name[ShaderNameSize];
    GetShaderName(name, sizeof(name), stage, hash, static_cast<int>(perm_idx));
    char filename[ShaderNameSize + 16];
    snprintf(filename, sizeof(filename), "%s.%s", name, ext);
    const IOFile file{dump_dir / filename, FileAccessMode::Create};
    file.WriteSpan(code);
}

std::optional<Common::FS::FileView> PipelineCache::GetShaderPatch(u64 hash, Shader::HwStage stage,
                                                                  size_t perm_idx,
                                                                  const char* ext) {
    if (!has_shader_patches) {
        return {};
    }
    using namespace Common::FS;
    char name[ShaderNameSize];
    GetShaderName(name, sizeof(name), stage, hash, static_cast<int>(perm_idx));
    char filename[ShaderNameSize + 16];
    snprintf(filename, sizeof(filename), "%s.%s", name, ext);
    const std::filesystem::path filepath = GetUserPath(PathType::ShaderDir) / "patch" / filename;
    if (!Common::FS::Exists(filepath)) {
        return {};
    }
    // Mapped where it lies: a mapping starts on a page, so the words are aligned
    return FileView::Map(filepath);
}
} // namespace Vulkan
