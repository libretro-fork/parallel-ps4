// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <unordered_map>
#include <variant>
#include <queues/mpsc_stack.h>
#include <rthreads/retro_eventcount.h>
#include "common/io_file.h"
#include "common/ptr_table.h"
#include "common/stop_thread.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/recompiler.h"
#include "shader_recompiler/specialization.h"
#include "video_core/cache_storage.h"
#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_graphics_pipeline.h"
#include "video_core/renderer_vulkan/vk_resource_pool.h"

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
        VkShaderModule module;
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

    void AddPermut(VkShaderModule module, Shader::StageSpecialization&& spec) {
        modules.emplace_back(module, static_cast<Shader::StageSpecialization&&>(spec));
    }

    void InsertPermut(VkShaderModule module, Shader::StageSpecialization&& spec, size_t perm_idx,
                      bool from_cache = false) {
        if (modules.size() < perm_idx + 1) {
            modules.resize(perm_idx + 1); // <-- beware of realloc
        }
        modules[perm_idx] = {module, static_cast<Shader::StageSpecialization&&>(spec), from_cache};
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
    /// The driver's pipeline cache, made from what was kept of it if it is this device's
    void CreateDriverCache(bool from_disk);
    /// The driver's pipeline cache handed to the disk cache, to be written as it is
    void SaveDriverCache();

    bool LoadComputePipeline(Serialization::Archive& ar);
    bool LoadGraphicsPipeline(Serialization::Archive& ar);
    bool LoadPipelineStage(Serialization::Archive& ar, size_t stage);

    /// @may_skip(@ctx): with asynchronous compilation, whether the draw may be left out while
    /// its pipeline is being made (null is returned); otherwise it waits for it. Asked only
    /// when the pipeline is not made, and once.
    using MaySkip = bool (*)(void* ctx);
    const GraphicsPipeline* GetGraphicsPipeline(const DrawIndirectParams params = {},
                                                MaySkip may_skip = nullptr, void* ctx = nullptr);

    const ComputePipeline* GetComputePipeline();

    /// A stage's program: its description, the module of its permutation and the permutation's
    /// hash; all null while it is being compiled
    struct Result {
        const Shader::Info* info;
        VkShaderModule module;
        u64 hash;
    };
    Result GetProgram(Shader::HwStage hw_stage, Shader::SwStage sw_stage,
                      const Shader::ShaderParams& params, Shader::Backend::Bindings& binding);

    /// Room enough for GetShaderName
    static constexpr size_t ShaderNameSize = 48;
    /// A shader's name (its stage and hash, and the permutation where @perm is not negative)
    /// into @out
    static void GetShaderName(char* out, size_t size, Shader::HwStage stage, u64 hash,
                              int perm = -1);

    const Shader::Profile& GetProfile() const {
        return profile;
    }

private:
    bool RefreshGraphicsKey();
    [[nodiscard]] bool DrawsIntoSmallTargets() const;
    bool RefreshGraphicsStages();
    bool RefreshComputeKey();

    void DumpShader(std::span<const u32> code, u64 hash, Shader::HwStage stage, size_t perm_idx,
                    const char* ext);
    std::optional<Common::FS::FileView> GetShaderPatch(u64 hash, Shader::HwStage stage,
                                                       size_t perm_idx, const char* ext);

    /// Asynchronous compilation: whether a draw whose pipeline is not ready is left out;
    /// worked out once a draw (@skip starts at -1)
    bool SkipUnready(int& skip, MaySkip may_skip, void* ctx) const;
    /// Binds a stage's program for a graphics pipeline: false where the stage is off or its
    /// program is being compiled
    bool BindStage(Shader::Backend::Bindings& binding, Shader::HwStage stage_in,
                   Shader::SwStage stage_out);
    /// A stage's program being compiled
    Result PendingResult();
    /// The shader patch folder had files in it when the game started
    bool has_shader_patches = false;
    // ---- The compiler thread ----
    // Shaders are translated and pipelines made on a thread of their own, never on the thread
    // that draws. What a draw needs and does not have is handed over as a job; the draw is
    // skipped (Shader Compilation: Asynchronous) or waits for it (Wait), and a compute
    // dispatch always waits, as the game reads back what it writes. Finished jobs come back to
    // the drawing thread, which files them: the tables are only ever touched by that thread.
    struct CompileJob;
    /// What a CompileJob is for (defined with it in vk_pipeline_cache.cpp)
    enum class JobKind : u8;
    void CompilerThread();
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
    /// A job for a stage's program, with the game's code and user data as the draw has them
    CompileJob* MakeJob(Shader::HwStage hw_stage, Shader::SwStage sw_stage,
                        const Shader::ShaderParams& params, const Shader::RuntimeInfo& runtime_info,
                        const Shader::Backend::Bindings& binding, JobKind kind, size_t perm_idx,
                        Program* program);
    bool IsPipelinePending(u64 hash) const;

    mpsc_stack_t compile_jobs{};
    mpsc_stack_t compile_results{};
    retro_eventcount_t compile_jobs_ec{};
    retro_eventcount_t compile_results_ec{};
    stop_thread_t compiler = STOP_THREAD_INIT;
    /// Set by GetProgram when the stage it was asked for is being compiled
    bool stage_pending = false;
    /// Asynchronous: a draw whose pipeline is being made is skipped rather than waited for
    bool async_compile = true;
    /// The graphics pipelines being made, by their hash
    static constexpr u32 MaxPendingPipelines = 256;
    u64 pending_pipelines[MaxPendingPipelines]{};
    u32 num_pending_pipelines = 0;
    VkShaderModule CompileModule(Shader::Info& info, Shader::RuntimeInfo& runtime_info,
                                 const std::span<const u32>& code, size_t perm_idx,
                                 Shader::Backend::Bindings& binding);
    /// The rest of CompileModule, from a program translated: its SPIR-V, dumped, kept and made
    /// into a Vulkan module
    VkShaderModule ModuleFromProgram(Shader::Info& info, Shader::RuntimeInfo& runtime_info,
                                     const Shader::IR::Program& ir_program, size_t perm_idx,
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
    VkPipelineCache pipeline_cache{VK_NULL_HANDLE};
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
    Shader::RuntimeInfo runtime_infos[MaxShaderStages]{};
    const Shader::Info* infos[MaxShaderStages]{};
    VkShaderModule modules[MaxShaderStages]{};
    Shader::Gcn::FetchShaderData* fetch_shader{};
    GraphicsPipelineKey graphics_key{};
    ComputePipelineKey compute_key{};
    u32 num_new_pipelines{}; // new pipelines added to the cache since the game start
    // The compiler thread's graphics pipelines since the driver's cache was last saved
    u32 driver_cache_unsaved{};
    // Saves the driver's cache as the disk cache closes
    Storage::CloseHook driver_cache_hook{};
};

} // namespace Vulkan
