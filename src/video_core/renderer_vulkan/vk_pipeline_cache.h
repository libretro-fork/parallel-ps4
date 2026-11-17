// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <thread>
#include <unordered_map>
#include <variant>
#include <queues/mpsc_stack.h>
#include <rthreads/retro_eventcount.h>
#include "common/io_file.h"
#include "common/ptr_table.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/recompiler.h"
#include "shader_recompiler/specialization.h"
#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_graphics_pipeline.h"
#include "video_core/renderer_vulkan/vk_resource_pool.h"
#include "vulkan/vulkan.hpp"

template <>
struct std::hash<vk::ShaderModule> {
    std::size_t operator()(const vk::ShaderModule& module) const noexcept {
        return std::hash<size_t>{}(reinterpret_cast<size_t>((VkShaderModule)module));
    }
};

namespace AmdGpu {
class Liverpool;
}

namespace Serialization {
struct Archive;
}

namespace Shader {
struct Info;
}

namespace Vulkan {

class Instance;
class Scheduler;
class ShaderCache;

struct Program {
    struct Module {
        vk::ShaderModule module;
        Shader::StageSpecialization spec;
        /// Loaded from the pipeline cache, and not yet checked against a translation of the
        /// game's shader (a check job of the compiler thread)
        bool from_cache = false;
        /// Being compiled on the compiler thread: module is not made yet
        bool pending = false;
        /// Its compile found the game's code changed under it: compiled again at the next draw
        bool again = false;
    };
    static constexpr size_t MaxPermutations = 8;
    using ModuleList = Common::SmallVector<Module, MaxPermutations>;

    Shader::Info info;
    ModuleList modules{};
    /// Being translated on the compiler thread: info and modules are not there yet
    bool pending = false;
    /// While pending: the discovery is submitted, then filed (info holds its walker), then the
    /// translation, from a snapshot of the next draw's (Shader::GuestSnapshot), submitted. A
    /// job that found the game's code changed under it is submitted again at the next draw.
    bool discovery_submitted = false;
    bool discovered = false;
    bool submitted = false;

    Program() = default;
    Program(Shader::HwStage stage, Shader::SwStage l_stage, Shader::ShaderParams params)
        : info{stage, l_stage, params} {}

    void AddPermut(vk::ShaderModule module, Shader::StageSpecialization&& spec) {
        modules.emplace_back(module, std::move(spec));
    }

    void InsertPermut(vk::ShaderModule module, Shader::StageSpecialization&& spec, size_t perm_idx,
                      bool from_cache = false) {
        modules.resize(std::max(modules.size(), perm_idx + 1)); // <-- beware of realloc
        modules[perm_idx] = {module, std::move(spec), from_cache};
    }
};

struct DrawIndirectParams {
    u16 vertex_sgpr_offset;
    u32 instance_sgpr_offset;
    bool is_indirect;
};

class PipelineCache {
public:
    explicit PipelineCache(const Instance& instance, Scheduler& scheduler,
                           AmdGpu::Liverpool* liverpool, u32 sparse_page_shift);
    ~PipelineCache();

    void WarmUp();
    void Sync();

    bool LoadComputePipeline(Serialization::Archive& ar);
    bool LoadGraphicsPipeline(Serialization::Archive& ar);
    bool LoadPipelineStage(Serialization::Archive& ar, size_t stage);

    const GraphicsPipeline* GetGraphicsPipeline(const DrawIndirectParams params = {});

    const ComputePipeline* GetComputePipeline();

    using Result = std::tuple<const Shader::Info*, vk::ShaderModule, u64>;
    Result GetProgram(Shader::HwStage hw_stage, Shader::SwStage sw_stage,
                      const Shader::ShaderParams& params, Shader::Backend::Bindings& binding);

    static std::string GetShaderName(Shader::HwStage stage, u64 hash,
                                     std::optional<size_t> perm = {});

    auto& GetProfile() const {
        return profile;
    }

private:
    bool RefreshGraphicsKey();
    bool RefreshGraphicsStages();
    bool RefreshComputeKey();

    void DumpShader(std::span<const u32> code, u64 hash, Shader::HwStage stage, size_t perm_idx,
                    std::string_view ext);
    std::optional<Common::FS::FileView> GetShaderPatch(u64 hash, Shader::HwStage stage,
                                                       size_t perm_idx, std::string_view ext);
    /// The shader patch folder had files in it when the game started
    bool has_shader_patches = false;
    // ---- The compiler thread ----
    // Shaders are translated and pipelines made on a thread of their own, never on the thread
    // that draws. What a draw needs and does not have is handed over as a job; the draw is
    // skipped (Shader Compilation: Asynchronous) or waits for it (Wait), and a compute
    // dispatch always waits, as the game reads back what it writes. Finished jobs come back to
    // the drawing thread, which files them: the tables are only ever touched by that thread.
    struct CompileJob;
    void CompilerThread(std::stop_token stop);
    void Submit(CompileJob* job);
    /// Files what the compiler thread has finished; true when there was any
    bool DrainCompiled();
    /// Parks until the compiler thread finishes a job
    void WaitCompiled();
    void RunJob(CompileJob& job);
    void RunJobOn(CompileJob& job, const Shader::ShaderParams& params);
    /// Whether the game's code a job borrows is still what the draw saw: the binary info after
    /// it, read through the backing page table (CopySparseMemory: what is no longer mapped
    /// reads as zeros, nothing faults), its hash and CRC compared
    static bool CodeUnchanged(const CompileJob& job);
    /// At the draw: what a job's translation reads of the game's memory
    static void CaptureSnapshot(CompileJob& job, const Shader::Info& info);
    void FileJob(CompileJob& job);
    /// A pipeline the cache gave whose shader turned out to differ from its translation: the
    /// translation is used, the pipelines made with it go, the cache starts again
    void ReplaceCachedPermutation(CompileJob& job);
    bool IsPipelinePending(u64 hash) const;

    mpsc_stack_t compile_jobs{};
    mpsc_stack_t compile_results{};
    retro_eventcount_t compile_jobs_ec{};
    retro_eventcount_t compile_results_ec{};
    std::jthread compiler;
    /// Set by GetProgram when the stage it was asked for is being compiled
    bool stage_pending = false;
    /// Asynchronous: a draw whose pipeline is being made is skipped rather than waited for
    bool async_compile = true;
    /// The graphics pipelines being made, by their hash
    std::array<u64, 256> pending_pipelines{};
    u32 num_pending_pipelines = 0;
    vk::ShaderModule CompileModule(Shader::Info& info, Shader::RuntimeInfo& runtime_info,
                                   const std::span<const u32>& code, size_t perm_idx,
                                   Shader::Backend::Bindings& binding);
    const Shader::RuntimeInfo& BuildRuntimeInfo(Shader::HwStage stage, Shader::SwStage l_stage);

    [[nodiscard]] bool IsPipelineCacheDirty() const {
        return num_new_pipelines > 0;
    }

private:
    const Instance& instance;
    Scheduler& scheduler;
    AmdGpu::Liverpool* liverpool;
    DescriptorHeap desc_heap;
    vk::UniquePipelineCache pipeline_cache;
    vk::UniquePipelineLayout pipeline_layout;
    Shader::Profile profile{};
    Shader::Pools pools;
    DrawIndirectParams draw_indirect_params{};
    // C89 tables of the objects, filed by hash; the cache owns what they point at
    ptr_table_t program_cache{};      // Program*, by the shader's hash
    ptr_table_t compute_pipelines{};  // ComputePipeline*, by its key's value
    ptr_table_t graphics_pipelines{}; // GraphicsPipeline*, by the key's xxh3; keys compared
    Program* FindProgram(u64 hash) const {
        return static_cast<Program*>(ptr_table_find(&program_cache, hash, nullptr, nullptr));
    }
    ComputePipeline* FindComputePipeline(const ComputePipelineKey& key) const {
        return static_cast<ComputePipeline*>(
            ptr_table_find(&compute_pipelines, key.value, nullptr, nullptr));
    }
    GraphicsPipeline* FindGraphicsPipeline(const GraphicsPipelineKey& key, u64 hash) const;
    std::array<Shader::RuntimeInfo, MaxShaderStages> runtime_infos{};
    std::array<const Shader::Info*, MaxShaderStages> infos{};
    std::array<vk::ShaderModule, MaxShaderStages> modules{};
    Shader::Gcn::FetchShaderData* fetch_shader{};
    GraphicsPipelineKey graphics_key{};
    ComputePipelineKey compute_key{};
    u32 num_new_pipelines{}; // new pipelines added to the cache since the game start
};

} // namespace Vulkan
