/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_VDECSW_H
#define SHADPS4_VDECSW_H

#include "common/types.h"

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct vdecsw_decoder* OrbisVdecswDecoder;
typedef void* OrbisVdecswComputeQueue;

typedef u32 OrbisVdecswCodecType;
#define ORBIS_VDECSW_CODEC_TYPE_AVC 1u
#define ORBIS_VDECSW_CODEC_TYPE_HEVC 974921u

typedef struct OrbisVdecswDecoderConfigInfo {
    u64 this_size;
    u32 resource_type;
    OrbisVdecswCodecType codec_type;
    u32 profile;
    u32 max_level;
    s32 max_frame_width;
    s32 max_frame_height;
    s32 max_dpb_frame_count;
    u32 decode_pipeline_depth;
    OrbisVdecswComputeQueue compute_queue;
    u64 cpu_affinity_mask;
    s32 cpu_thread_priority;
    bool optimize_progressive_video;
    bool check_memory_type;
    u8 reserved0;
    s8 extra_dpb_frame_count;
    void* extra_config_info;
    bool disable_sync_decode_output;
    u32 max_pending_sync_count;
} OrbisVdecswDecoderConfigInfo;
typedef char shadps4_vdecsw_h_check_1[(sizeof(OrbisVdecswDecoderConfigInfo) == 0x50) ? 1 : -1];

typedef struct OrbisVdecswDecoderConfigInfoV1 {
    u64 this_size;
    u32 resource_type;
    OrbisVdecswCodecType codec_type;
    u32 profile;
    u32 max_level;
    s32 max_frame_width;
    s32 max_frame_height;
    s32 max_dpb_frame_count;
    u32 decode_pipeline_depth;
    OrbisVdecswComputeQueue compute_queue;
    u64 cpu_affinity_mask;
    s32 cpu_thread_priority;
    bool optimize_progressive_video;
    bool check_memory_type;
    u8 reserved0;
    s8 extra_dpb_frame_count;
    void* extra_config_info;
} OrbisVdecswDecoderConfigInfoV1;
typedef char shadps4_vdecsw_h_check_2[(sizeof(OrbisVdecswDecoderConfigInfoV1) == 0x48) ? 1 : -1];

typedef struct OrbisVdecswDecoderMemoryInfo {
    u64 this_size;
    u64 cpu_memory_size;
    void* cpu_memory;
    u64 gpu_memory_size;
    void* gpu_memory;
    u64 cpu_gpu_memory_size;
    void* cpu_gpu_memory;
    u64 max_frame_buffer_size;
    u32 frame_buffer_alignment;
    u32 reserved0;
} OrbisVdecswDecoderMemoryInfo;
typedef char shadps4_vdecsw_h_check_3[(sizeof(OrbisVdecswDecoderMemoryInfo) == 0x48) ? 1 : -1];

typedef struct OrbisVdecswInputData {
    u64 this_size;
    void* au_data;
    u64 au_size;
    u64 pts_data;
    u64 dts_data;
    u64 attached_data;
} OrbisVdecswInputData;
typedef char shadps4_vdecsw_h_check_4[(sizeof(OrbisVdecswInputData) == 0x30) ? 1 : -1];

typedef struct OrbisVdecswInputResult {
    u64 this_size;
    void* decoded_au;
    u32 output_frame_count;
    u32 reserved0;
} OrbisVdecswInputResult;
typedef char shadps4_vdecsw_h_check_5[(sizeof(OrbisVdecswInputResult) == 0x18) ? 1 : -1];

typedef struct OrbisVdecswOutputInfo {
    u64 this_size;
    bool is_valid;
    bool is_last_frame;
    bool is_error_frame;
    u8 picture_count;
    OrbisVdecswCodecType codec_type;
    u32 frame_width;
    u32 frame_pitch;
    u32 frame_height;
    bool is_discarded_frame;
    void* frame_buffer;
    u64 frame_buffer_size;
    u32 frame_format;
    u32 frame_pitch_in_bytes;
} OrbisVdecswOutputInfo;
typedef char shadps4_vdecsw_h_check_6[(sizeof(OrbisVdecswOutputInfo) == 0x38) ? 1 : -1];

typedef struct OrbisVdecswFrameBuffer {
    u64 this_size;
    void* frame_buffer;
    u64 frame_buffer_size;
} OrbisVdecswFrameBuffer;
typedef char shadps4_vdecsw_h_check_7[(sizeof(OrbisVdecswFrameBuffer) == 0x18) ? 1 : -1];

typedef struct OrbisVdecswComputeMemoryInfo {
    u64 this_size;
    u64 cpu_gpu_memory_size;
    void* cpu_gpu_memory;
} OrbisVdecswComputeMemoryInfo;
typedef char shadps4_vdecsw_h_check_8[(sizeof(OrbisVdecswComputeMemoryInfo) == 0x18) ? 1 : -1];

typedef struct OrbisVdecswComputeConfigInfo {
    u64 this_size;
    u16 compute_pipe_id;
    u16 compute_queue_id;
    bool check_memory_type;
    u8 reserved0;
    u16 reserved1;
} OrbisVdecswComputeConfigInfo;
typedef char shadps4_vdecsw_h_check_9[(sizeof(OrbisVdecswComputeConfigInfo) == 0x10) ? 1 : -1];

typedef struct OrbisVdecswAvcPictureInfo {
    u64 this_size;

    bool is_valid;

    u64 pts_data;
    u64 dts_data;
    u64 attached_data;

    u8 idr_pictureflag;

    u8 profile_idc;
    u8 level_idc;
    u32 pic_width_in_mbs_minus1;
    u32 pic_height_in_map_units_minus1;
    u8 frame_mbs_only_flag;

    u8 frame_cropping_flag;
    u32 frame_crop_left_offset;
    u32 frame_crop_right_offset;
    u32 frame_crop_top_offset;
    u32 frame_crop_bottom_offset;

    u8 aspect_ratio_info_present_flag;
    u8 aspect_ratio_idc;
    u16 sar_width;
    u16 sar_height;

    u8 video_signal_type_present_flag;
    u8 video_format;
    u8 video_full_range_flag;
    u8 colour_description_present_flag;
    u8 colour_primaries;
    u8 transfer_characteristics;
    u8 matrix_coefficients;

    u8 timing_info_present_flag;
    u32 num_units_in_tick;
    u32 time_scale;
    u8 fixed_frame_rate_flag;

    u8 bitstream_restriction_flag;
    u8 max_dec_frame_buffering;

    u8 pic_struct_present_flag;
    u8 pic_struct;
    u8 field_pic_flag;
    u8 bottom_field_flag;

    u8 sequence_parameter_set_present_flag;
    u8 picture_parameter_set_present_flag;
    u8 au_delimiter_present_flag;
    u8 end_of_sequence_present_flag;
    u8 end_of_stream_present_flag;
    u8 filler_data_present_flag;
    u8 picture_timing_sei_present_flag;
    u8 buffering_period_sei_present_flag;

    u8 constraint_set0_flag;
    u8 constraint_set1_flag;
    u8 constraint_set2_flag;
    u8 constraint_set3_flag;
    u8 constraint_set4_flag;
    u8 constraint_set5_flag;
} OrbisVdecswAvcPictureInfo;
typedef char shadps4_vdecsw_h_check_10[(sizeof(OrbisVdecswAvcPictureInfo) == 0x78) ? 1 : -1];

typedef struct OrbisVdecswHevcExtraConfigInfo {
    u64 this_size;
    s32 tier_type;
    s8 max_bit_depth_luma;
    s8 max_bit_depth_chroma;
    u16 reserved0;
    s32 max_temporal_id_to_decode;
    u32 hdr_frame_format;
    u32 gpu_load_level;
} OrbisVdecswHevcExtraConfigInfo;
typedef char shadps4_vdecsw_h_check_11[(sizeof(OrbisVdecswHevcExtraConfigInfo) == 0x20) ? 1 : -1];

typedef struct OrbisVdecswHevcPictureInfo {
    u64 this_size;
    bool is_valid;
    u64 pts_data;
    u64 dts_data;
    u64 attached_data;
    u32 pic_width_in_luma_samples;
    u32 pic_height_in_luma_samples;
    u8 bit_depth_luma_minus8;
    u8 bit_depth_chroma_minus8;
    u8 timing_info_present_flag;
    u32 num_units_in_tick;
    u32 time_scale;
    u32 aspect_ratio_info_present_flag;
    u8 aspect_ratio_idc;
    u16 sar_width;
    u16 sar_height;
    u8 video_signal_type_present_flag;
    u8 video_format;
    u8 video_full_range_flag;
    u8 colour_description_present_flag;
    u8 colour_primaries;
    u8 transfer_characteristics;
    u8 matrix_coeffs;
    u8 frame_field_info_present_flag;
    u32 pic_struct;
    u32 source_scan_type;
    u32 duplicate_flag;
    u32 conformance_window_flag;
    u32 conf_win_left_offset;
    u32 conf_win_right_offset;
    u32 conf_win_top_offset;
    u32 conf_win_bottom_offset;
    u32 default_display_window_flag;
    u32 def_disp_win_left_offset;
    u32 def_disp_win_right_offset;
    u32 def_disp_win_top_offset;
    u32 def_disp_win_bottom_offset;
    u8 chroma_loc_info_present_flag;
    u8 chroma_sample_loc_type_top_field;
    u8 chroma_sample_loc_type_bottom_field;
    u8 field_seq_flag;
    u8 video_parameter_set_present_flag;
    u8 sequence_parameter_set_present_flag;
    u8 picture_parameter_set_present_flag;
    u8 au_delimiter_present_flag;
    u8 end_of_sequence_present_flag;
    u8 end_of_stream_present_flag;
    u8 filler_data_present_flag;
    u8 picture_timing_sei_present_flag;
    u8 buffering_period_sei_present_flag;
    u8 frame_packing_arrangement_sei_present_flag;
    u8 alternative_transfer_characteristics_sei_present_flag;
    u8 idr_pictureflag;
    u8 irap_picture_flag;
    u8 general_profile_space;
    u8 general_tier_flag;
    u8 general_profile_idc;
    u8 general_progressive_source_flag;
    u8 general_interlaced_source_flag;
    u8 general_frame_only_constraint_flag;
    u8 general_level_idc;
    u8 sub_layer_profile_present_flag;
    u8 sub_layer_level_present_flag;
    u8 sub_layer_profile_space;
    u8 sub_layer_tier_flag;
    u8 sub_layer_profile_idc;
    u8 sub_layer_level_idc;
    u8 sub_layer_ordering_info_present_flag;
    u8 max_dec_pic_buffering_minus1;
    u8 preferred_transfer_characteristics;
    u8 frame_cropping_flag;
    u32 frame_crop_left_offset;
    u32 frame_crop_right_offset;
    u32 frame_crop_top_offset;
    u32 frame_crop_bottom_offset;
} OrbisVdecswHevcPictureInfo;
typedef char shadps4_vdecsw_h_check_12[(sizeof(OrbisVdecswHevcPictureInfo) == 0xB8) ? 1 : -1];

/* The library's calls (vdecsw.c) */
s32 PS4_SYSV_ABI sceVdecswQueryComputeMemoryInfo(OrbisVdecswComputeMemoryInfo* compute_mem_info);
s32 PS4_SYSV_ABI
sceVdecswAllocateComputeQueue(const OrbisVdecswComputeConfigInfo* compute_cfg_info_in,
                              const OrbisVdecswComputeMemoryInfo* compute_mem_info_in,
                              OrbisVdecswComputeQueue* compute_queue_out);
s32 PS4_SYSV_ABI sceVdecswReleaseComputeQueue(OrbisVdecswComputeQueue compute_queue_in);
s32 PS4_SYSV_ABI
sceVdecswQueryDecoderMemoryInfo(const OrbisVdecswDecoderConfigInfo* decoder_config_info_in,
                                OrbisVdecswDecoderMemoryInfo* decoder_memory_info_out);
s32 PS4_SYSV_ABI sceVdecswCreateDecoder(const OrbisVdecswDecoderConfigInfo* decoder_config_info_in,
                                        const OrbisVdecswDecoderMemoryInfo* decoder_memory_info_in,
                                        OrbisVdecswDecoder* decoder_instance_out);
s32 PS4_SYSV_ABI sceVdecswDeleteDecoder(OrbisVdecswDecoder decoder_instance_in);
s32 PS4_SYSV_ABI sceVdecswResetDecoder(OrbisVdecswDecoder decoder_instance_in);
s32 PS4_SYSV_ABI sceVdecswSetDecodeInput(OrbisVdecswDecoder decoder_instance_in,
                                         const OrbisVdecswInputData* input_data_in);
s32 PS4_SYSV_ABI sceVdecswSyncDecodeInput(OrbisVdecswDecoder decoder_instance_in,
                                          OrbisVdecswInputResult* input_result_out);
s32 PS4_SYSV_ABI sceVdecswTrySyncDecodeInput(OrbisVdecswDecoder decoder_instance_in,
                                             OrbisVdecswInputResult* input_result_out);
s32 PS4_SYSV_ABI sceVdecswSetDecodeOutput(OrbisVdecswDecoder decoder_instance_in,
                                          OrbisVdecswFrameBuffer* frame_buffer_in_out);
s32 PS4_SYSV_ABI sceVdecswSyncDecodeOutput(OrbisVdecswDecoder decoder_instance_in,
                                           OrbisVdecswOutputInfo* output_info_out);
s32 PS4_SYSV_ABI sceVdecswTrySyncDecodeOutput(OrbisVdecswDecoder decoder_instance_in,
                                              OrbisVdecswOutputInfo* output_info_out);
s32 PS4_SYSV_ABI sceVdecswFinalizeDecodeSequence(OrbisVdecswDecoder decoder_instance_in);
s32 PS4_SYSV_ABI sceVdecswGetPictureInfo(const OrbisVdecswOutputInfo* output_info_in,
                                         void* p_1st_picture_info_out,
                                         void* p_2nd_picture_info_out);
s32 PS4_SYSV_ABI sceVdecswGetAvcPictureInfo(const OrbisVdecswOutputInfo* output_info_in,
                                            OrbisVdecswAvcPictureInfo* p_1st_picture_info_out,
                                            OrbisVdecswAvcPictureInfo* p_2nd_picture_info_out);
s32 PS4_SYSV_ABI sceVdecswGetHevcPictureInfo(const OrbisVdecswOutputInfo* output_info_in,
                                             OrbisVdecswHevcPictureInfo* picture_info_out);

RETRO_END_DECLS

#endif
