// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scm_rev.h"
#include "common/serdes.h"
#include "common/string_util.h"
#include "core/emulator_settings.h"
#include "shader_recompiler/frontend/fetch_shader.h"
#include "shader_recompiler/info.h"
#include "video_core/cache_storage.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"

namespace Serialization {
/* You should increment versions below once corresponding serialization scheme is changed. */
static constexpr u32 ShaderBinaryVersion = 14u; // 14: debug names only when dumping
static constexpr u32 ShaderMetaVersion = 7u;    // 7: the whole Info, the walker as made
static constexpr u32 PipelineKeyVersion = 3u;
} // namespace Serialization

namespace Vulkan {

void RegisterPipelineData(const ComputePipelineKey& key,
                          ComputePipeline::SerializationSupport& sdata) {
    if (!Storage::DataBase::Instance().IsOpened()) {
        return;
    }

    Serialization::Archive ar{};
    Serialization::Writer pldata{ar};

    pldata.Write(Serialization::PipelineKeyVersion);
    pldata.Write(u32{1}); // compute

    key.Serialize(ar);
    sdata.Serialize(ar);

    char name[32];
    snprintf(name, sizeof(name), "c_0x%016zx", key.value);
    size_t size = 0;
    u8* const bytes = ar.TakeOff(&size);
    Storage::DataBase::Instance().Save(Storage::BlobType::PipelineKey, name, bytes, size);
}

void RegisterPipelineData(const GraphicsPipelineKey& key, u64 hash,
                          GraphicsPipeline::SerializationSupport& sdata) {
    if (!Storage::DataBase::Instance().IsOpened()) {
        return;
    }

    Serialization::Archive ar{};
    Serialization::Writer pldata{ar};

    pldata.Write(Serialization::PipelineKeyVersion);
    pldata.Write(u32{0}); // graphics

    key.Serialize(ar);
    sdata.Serialize(ar);

    char name[32];
    snprintf(name, sizeof(name), "g_0x%016" PRIx64, hash);
    size_t size = 0;
    u8* const bytes = ar.TakeOff(&size);
    Storage::DataBase::Instance().Save(Storage::BlobType::PipelineKey, name, bytes, size);
}

void RegisterShaderMeta(const Shader::Info& info,
                        const std::optional<Shader::Gcn::FetchShaderData>& fetch_shader_data,
                        const Shader::StageSpecialization& spec, size_t perm_hash,
                        size_t perm_idx) {
    if (!Storage::DataBase::Instance().IsOpened()) {
        return;
    }

    Serialization::Archive ar;
    Serialization::Writer meta{ar};

    meta.Write(Serialization::ShaderMetaVersion);
    meta.Write(Serialization::ShaderBinaryVersion);

    meta.Write(perm_hash);
    meta.Write(perm_idx);

    spec.Serialize(ar);
    info.Serialize(ar);

    char name[32];
    snprintf(name, sizeof(name), "0x%016zx", perm_hash);
    size_t size = 0;
    u8* const bytes = ar.TakeOff(&size);
    Storage::DataBase::Instance().Save(Storage::BlobType::ShaderMeta, name, bytes, size);
}

void RegisterShaderBinary(Common::Span<const u32> spv, u64 pgm_hash, size_t perm_idx) {
    if (!Storage::DataBase::Instance().IsOpened()) {
        return;
    }

    // The one copy, for the writer that saves it later: the words lie in the recompiler's
    // module, which the next shader is written into
    char name[48];
    snprintf(name, sizeof(name), "0x%016" PRIx64 "_%zu", pgm_hash, perm_idx);
    Storage::DataBase::Instance().SaveCopy(Storage::BlobType::ShaderBinary, name, spv.data(),
                                           spv.size_bytes());
}

bool LoadShaderMeta(Serialization::Archive& ar, Shader::Info& info,
                    Shader::StageSpecialization& spec, size_t& perm_idx) {
    Serialization::Reader meta{ar};

    u32 meta_version{};
    meta.Read(meta_version);
    if (meta_version != Serialization::ShaderMetaVersion) {
        return false;
    }

    u32 binary_version{};
    meta.Read(binary_version);
    if (binary_version != Serialization::ShaderBinaryVersion) {
        return false;
    }

    u64 perm_hash_ar{};
    meta.Read(perm_hash_ar);
    meta.Read(perm_idx);

    spec.Deserialize(ar);
    info.Deserialize(ar);
    return true;
}

void ComputePipelineKey::Serialize(Serialization::Archive& ar) const {
    Serialization::Writer key{ar};
    key.Write(value);
}

bool ComputePipelineKey::Deserialize(Serialization::Archive& ar) {
    Serialization::Reader key{ar};
    key.Read(value);
    return true;
}

void ComputePipeline::SerializationSupport::Serialize(Serialization::Archive& ar) const {
    // Nothing here yet
    return;
}

bool ComputePipeline::SerializationSupport::Deserialize(Serialization::Archive& ar) {
    // Nothing here yet
    return true;
}

bool PipelineCache::LoadComputePipeline(Serialization::Archive& ar) {
    compute_key.Deserialize(ar);

    ComputePipeline::SerializationSupport sdata{};
    sdata.Deserialize(ar);

    char meta_name[32];
    snprintf(meta_name, sizeof(meta_name), "0x%016zx", compute_key.value);
    const Common::Span<const u8> meta_blob =
        Storage::DataBase::Instance().Load(Storage::BlobType::ShaderMeta, meta_name);
    if (meta_blob.empty()) {
        return false;
    }

    Serialization::Archive meta_ar{meta_blob};

    if (!LoadPipelineStage(meta_ar, 0)) {
        return false;
    }

    ASSERT(!FindComputePipeline(compute_key));
    ptr_table_insert(&compute_pipelines, compute_key.value,
                     new ComputePipeline(instance, scheduler, desc_heap, profile,
                                         static_cast<VkPipelineCache>(pipeline_cache), compute_key,
                                         *infos[0], static_cast<VkShaderModule>(modules[0]), sdata,
                                         true));

    for (u32 i = 0; i < MaxShaderStages; ++i) {
        infos[i] = nullptr;
        modules[i] = VkShaderModule{};
    }

    return true;
}

void GraphicsPipelineKey::Serialize(Serialization::Archive& ar) const {
    Serialization::Writer key{ar};

    key.Write(this, sizeof(*this));
}

bool GraphicsPipelineKey::Deserialize(Serialization::Archive& ar) {
    Serialization::Reader key{ar};

    key.Read(this, sizeof(*this));
    return true;
}

void GraphicsPipeline::SerializationSupport::Serialize(Serialization::Archive& ar) const {
    Serialization::Writer sdata{ar};

    sdata.Write(&vertex_attributes, sizeof(vertex_attributes));
    sdata.Write(&vertex_bindings, sizeof(vertex_bindings));
    sdata.Write(&divisors, sizeof(divisors));
    sdata.Write(multisampling);
    sdata.Write(tcs);
    sdata.Write(tes);
}

bool GraphicsPipeline::SerializationSupport::Deserialize(Serialization::Archive& ar) {
    Serialization::Reader sdata{ar};

    sdata.Read(&vertex_attributes, sizeof(vertex_attributes));
    sdata.Read(&vertex_bindings, sizeof(vertex_bindings));
    sdata.Read(&divisors, sizeof(divisors));
    sdata.Read(multisampling);
    sdata.Read(tcs);
    sdata.Read(tes);
    return true;
}

bool PipelineCache::LoadGraphicsPipeline(Serialization::Archive& ar) {
    graphics_key.Deserialize(ar);

    GraphicsPipeline::SerializationSupport sdata{};
    sdata.Deserialize(ar);

    for (int stage_idx = 0; stage_idx < MaxShaderStages; ++stage_idx) {
        const size_t& hash = graphics_key.stage_hashes[stage_idx];
        if (!hash) {
            continue;
        }

        char meta_name[32];
        snprintf(meta_name, sizeof(meta_name), "0x%016" PRIx64, static_cast<u64>(hash));
        const Common::Span<const u8> meta_blob =
            Storage::DataBase::Instance().Load(Storage::BlobType::ShaderMeta, meta_name);
        if (meta_blob.empty()) {
            return false;
        }

        Serialization::Archive meta_ar{meta_blob};

        if (!LoadPipelineStage(meta_ar, stage_idx)) {
            return false;
        }
    }

    const u64 pipeline_hash = graphics_key.Hash();
    ASSERT(!FindGraphicsPipeline(graphics_key, pipeline_hash));
    ptr_table_insert(&graphics_pipelines, pipeline_hash,
                     new GraphicsPipeline(instance, scheduler, desc_heap, profile, graphics_key,
                                          static_cast<VkPipelineCache>(pipeline_cache), infos,
                                          runtime_infos, fetch_shader, modules, sdata, true));

    for (u32 i = 0; i < MaxShaderStages; ++i) {
        infos[i] = nullptr;
        modules[i] = VkShaderModule{};
    }
    fetch_shader = nullptr;

    return true;
}

bool PipelineCache::LoadPipelineStage(Serialization::Archive& ar, size_t stage) {
    std::unique_ptr<Program> program = std::make_unique<Program>();
    Shader::StageSpecialization spec{};
    spec.info = &program->info;
    size_t perm_idx{};
    if (!LoadShaderMeta(ar, program->info, spec, perm_idx)) {
        return false;
    }

    // Mapped where it lies: a mapping starts on a page, so the words are aligned
    char spv_name[48];
    snprintf(spv_name, sizeof(spv_name), "0x%016" PRIx64 "_%zu", program->info.pgm_hash, perm_idx);
    const Common::Span<const u8> spv_blob =
        Storage::DataBase::Instance().Load(Storage::BlobType::ShaderBinary, spv_name);
    if (spv_blob.size() < sizeof(u32)) {
        return false;
    }
    const Common::Span<const u32> spv{reinterpret_cast<const u32*>(spv_blob.data()),
                                      spv_blob.size() / sizeof(u32)};

    // Permutation hash depends on shader variation index. To prevent collisions, we need insert it
    // at the exact position rather than append

    VkShaderModule module{};

    Program* cached = FindProgram(program->info.pgm_hash);
    if (!cached) {
        module = CompileSPV(spv, instance.GetDevice());
        cached = program.release();
        ptr_table_insert(&program_cache, cached->info.pgm_hash, cached);
    } else {
        u32 idx = 0;
        while (idx < cached->modules.size() && !(cached->modules[idx].spec == spec)) {
            ++idx;
        }
        if (idx < cached->modules.size()) {
            // A matching permutation is valid only at its original index. A different index means
            // the store holds entries from more than one cache generation, so this pipeline is
            // left to compile at runtime.
            if (perm_idx != idx) {
                LOG_WARNING(
                    Render_Vulkan,
                    "Cached permutation %zu of %s_%" PRIx64 " conflicts with index %u, skipping "
                    "preload",
                    perm_idx, Shader::NameOf(program->info.hw_stage), program->info.pgm_hash, idx);
                return false;
            }
            module = cached->modules[idx].module;
        } else {
            module = CompileSPV(spv, instance.GetDevice());
        }
    }
    cached->InsertPermut(module, std::move(spec), perm_idx, true);

    infos[stage] = &cached->info;
    modules[stage] = module;
    if (Shader::Gcn::FetchShaderData& fetch = cached->modules[perm_idx].spec.fetch_shader_data;
        !fetch.Empty()) {
        fetch_shader = &fetch;
    }

    return true;
}

void PipelineCache::WarmUp() {
    if (!emu_settings_get_pipeline_cache_enabled()) {
        return;
    }

    Storage::DataBase::Instance().Open();

    // The shaders kept are the shader recompiler's output, and the rest of the renderer reads
    // them as this build lays them out: a cache another build of the core made is started
    // again, where it was read as if this one had made it (Bloodborne crashed in its own code
    // on the shaders an earlier build had kept)
    {
        const char* const build = Common::g_scm_rev[0] ? Common::g_scm_rev : Common::g_scm_date;
        const size_t build_len = strlen(build);
        bool same_build = false;
        {
            // Let go before the files are removed: Windows keeps a mapped file
            const Common::Span<const u8> stamp =
                Storage::DataBase::Instance().Load(Storage::BlobType::ShaderProfile, "build");
            same_build = !stamp.empty() && stamp.size() == build_len &&
                         memcmp(stamp.data(), build, build_len) == 0;
            if (!stamp.empty() && !same_build) {
                LOG_INFO(Render, "Pipeline cache made by another build: started again");
            }
        }
        if (!same_build) {
            Storage::DataBase::Instance().Clear();
            Storage::DataBase::Instance().SaveCopy(Storage::BlobType::ShaderProfile, "build", build,
                                                   build_len);
            Storage::DataBase::Instance().SaveCopy(Storage::BlobType::ShaderProfile, "profile",
                                                   &profile, sizeof(profile));
            return;
        }
    }

    // Check if cache is compatible
    const Common::Span<const u8> profile_data =
        Storage::DataBase::Instance().Load(Storage::BlobType::ShaderProfile, "profile");
    if (profile_data.empty()) {
        Storage::DataBase::Instance().SaveCopy(Storage::BlobType::ShaderProfile, "profile",
                                               &profile, sizeof(profile));
        return;
    }
    if (profile_data.size() != sizeof(Shader::Profile)) {
        LOG_WARNING(Render,
                    "Pipeline cache profile has unexpected size (%zu != %llu). Ignoring the cache",
                    profile_data.size(), static_cast<unsigned long long>(sizeof(Shader::Profile)));
        Storage::DataBase::Instance().Close();
        return;
    }

    Shader::Profile cached_profile{};
    memcpy(&cached_profile, profile_data.data(), sizeof(cached_profile));
    if (cached_profile != profile) {
        LOG_WARNING(Render,
                    "Pipeline cache isn't compatible with current system. Ignoring the cache");
        Storage::DataBase::Instance().Close();
        return;
    }

    // The pipelines loaded below are made into the driver's cache as it was kept
    CreateDriverCache(true);

    u32 num_pipelines{};
    u32 num_total_pipelines{};

    WarmUpCount count{this, 0, 0};
    Storage::DataBase::Instance().ForEachBlob(Storage::BlobType::PipelineKey, WarmUpBlob, &count);
    num_pipelines = count.loaded;
    num_total_pipelines = count.total;

    LOG_INFO(Render, "Preloaded %u pipelines", num_pipelines);
    if (num_total_pipelines > num_pipelines) {
        LOG_WARNING(Render, "%u stale pipelines were found. Consider re-generating the cache",
                    num_total_pipelines - num_pipelines);
    }
}

void PipelineCache::WarmUpBlob(void* user, Common::Span<const u8> data) {
    WarmUpCount& count = *static_cast<WarmUpCount*>(user);
    ++count.total;

    Serialization::Archive ar{data};
    Serialization::Reader pldata{ar};

    u32 version{};
    pldata.Read(version);
    if (version != Serialization::PipelineKeyVersion) {
        return;
    }

    u32 is_compute{};
    pldata.Read(is_compute);

    bool result{};
    if (is_compute) {
        result = count.cache->LoadComputePipeline(ar);
    } else {
        result = count.cache->LoadGraphicsPipeline(ar);
    }
    if (result) {
        ++count.loaded;
    }
}

void PipelineCache::Sync() {
    Storage::DataBase::Instance().Close();
}

} // namespace Vulkan

namespace Shader {

// What the translation found besides InfoPersistent, kept so that a pipeline from the cache is
// described as the one compiled was: field by field, as some hold padding or a map
/// InfoExtras' writer and reader of a field
struct InfoFieldWriter {
    Serialization::Writer& writer;
    template <typename T>
    void operator()(const T& field) const {
        writer.Write(&field, sizeof(field));
    }
};
struct InfoFieldReader {
    Serialization::Reader& reader;
    template <typename T>
    void operator()(T& field) const {
        reader.Read(&field, sizeof(field));
    }
};

template <typename Io, typename InfoT>
static void InfoExtras(Io&& io, InfoT& info) {
    io(info.loads);
    io(info.stores);
    io(info.readconst_types);
    io(info.uses_patches);
    io(info.has_storage_images);
    io(info.has_discard);
    io(info.has_image_gather);
    io(info.has_image_query);
    io(info.has_flatbuf);
    io(info.uses_buffer_atomic_float_min_max);
    io(info.uses_image_atomic_float_min_max);
    io(info.uses_lane_id);
    io(info.uses_shader_clock);
    io(info.uses_group_quad);
    io(info.uses_group_shuffle);
    io(info.uses_group_ballot);
    io(info.shared_types);
    io(info.uses_fp16);
    io(info.uses_fp64);
    io(info.uses_pack_10_11_11);
    io(info.uses_unpack_10_11_11);
    io(info.uses_buffer_int64_atomics);
    io(info.uses_shared_int64_atomics);
    io(info.stores_tess_level_outer);
    io(info.stores_tess_level_inner);
    io(info.translation_failed);
    io(info.fs_interpolation);
    io(info.gs_copy_data.num_attrs);
    io(info.gs_copy_data.output_vertices);
    io(info.gs_copy_data.num_comps);
}

void Info::Serialize(Serialization::Archive& ar) const {
    Serialization::Writer info{ar};

    info.Write(this, sizeof(InfoPersistent));
    info.Write(flattened_ud_buf);
    // The walker as it was made, not as the fault handler may have rewritten it since
    info.Write(&srt_info, sizeof(srt_info));
    ASSERT(srt_walker_code.size() == srt_info.walker_func_size);
    if (srt_info.walker_func_size) {
        info.Write(srt_walker_code.data(), srt_walker_code.size());
    }
    InfoExtras(InfoFieldWriter{info}, *this);
    info.Write(static_cast<u64>(gs_copy_data.attr_map.size()));
    for (const std::pair<const u32, std::pair<IR::Attribute, u32>>& entry : gs_copy_data.attr_map) {
        info.Write(entry.first);
        info.Write(entry.second.first);
        info.Write(entry.second.second);
    }
}

bool Info::Deserialize(Serialization::Archive& ar) {
    Serialization::Reader info{ar};

    info.Read(this, sizeof(Shader::InfoPersistent));
    info.Read(flattened_ud_buf);
    info.Read(&srt_info, sizeof(srt_info));
    srt_info.walker_func = nullptr;
    srt_walker_code.clear();
    if (srt_info.walker_func_size) {
        srt_walker_code.assign(ar.ReadPtr(), ar.ReadPtr() + srt_info.walker_func_size);
        srt_info.walker_func = RegisterWalkerCode(ar.ReadPtr(), srt_info.walker_func_size);
        ar.Advance(srt_info.walker_func_size);
    }
    InfoExtras(InfoFieldReader{info}, *this);
    u64 attrs{};
    info.Read(attrs);
    gs_copy_data.attr_map.clear();
    for (u64 i = 0; i < attrs; ++i) {
        u32 key{};
        std::pair<IR::Attribute, u32> value{};
        info.Read(key);
        info.Read(value.first);
        info.Read(value.second);
        gs_copy_data.attr_map.emplace(key, value);
    }
    return true;
}

void Gcn::FetchShaderData::Serialize(Serialization::Archive& ar) const {
    Serialization::Writer fetch{ar};
    ar.Grow(6 + attributes.size() * sizeof(VertexAttribute));

    fetch.Write(size);
    fetch.Write(vertex_offset_sgpr);
    fetch.Write(instance_offset_sgpr);
    fetch.Write(attributes);
}

bool Gcn::FetchShaderData::Deserialize(Serialization::Archive& ar) {
    Serialization::Reader fetch{ar};

    fetch.Read(size);
    fetch.Read(vertex_offset_sgpr);
    fetch.Read(instance_offset_sgpr);
    fetch.Read(attributes);

    return true;
}

void StageSpecialization::Serialize(Serialization::Archive& ar) const {
    Serialization::Writer spec{ar};

    spec.Write(start);
    spec.Write(runtime_info);

    // As std::bitset::to_string spelled it, behind its length: '0' or '1', the last bit first
    char bits[MaxStageResources];
    for (size_t i = 0; i < MaxStageResources; ++i) {
        bits[i] = bitset[MaxStageResources - 1 - i] ? '1' : '0';
    }
    spec.Write(static_cast<size_t>(MaxStageResources));
    spec.Write(bits, sizeof(bits));

    if (!fetch_shader_data.Empty()) {
        spec.Write(sizeof(fetch_shader_data));
        fetch_shader_data.Serialize(ar);
    } else {
        spec.Write(size_t{0});
    }

    spec.Write(vs_attribs);
    spec.Write(buffers);
    spec.Write(images);
    spec.Write(fmasks);
    spec.Write(samplers);
}

bool StageSpecialization::Deserialize(Serialization::Archive& ar) {
    Serialization::Reader spec{ar};

    spec.Read(start);
    spec.Read(runtime_info);

    size_t num_bits{};
    spec.Read(num_bits);
    if (num_bits > MaxStageResources) {
        return false;
    }
    char bits[MaxStageResources];
    spec.Read(bits, num_bits);
    bitset.reset();
    for (size_t i = 0; i < num_bits; ++i) {
        if (bits[num_bits - 1 - i] == '1') {
            bitset[i] = true;
        } else if (bits[num_bits - 1 - i] != '0') {
            return false;
        }
    }

    u64 fetch_data_size{};
    spec.Read(fetch_data_size);

    if (fetch_data_size) {
        Gcn::FetchShaderData fetch_data;
        fetch_data.Deserialize(ar);
        fetch_shader_data = fetch_data;
    }

    spec.Read(vs_attribs);
    spec.Read(buffers);
    spec.Read(images);
    spec.Read(fmasks);
    spec.Read(samplers);

    return true;
}

} // namespace Shader
