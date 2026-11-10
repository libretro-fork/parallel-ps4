/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Written by gen_enum_names.py; do not edit. */
/* clang-format off */

#ifndef SHADPS4_COMMON_ENUM_NAMES_H
#define SHADPS4_COMMON_ENUM_NAMES_H

/* The names of the enums the core prints, as C89 tables sorted by value.
 * enum_name() looks a value up; a value with no name gives "". Where
 * two names share a value, the one declared first is kept. */

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct enum_name_entry {
    int64_t value;
    const char* name;
} enum_name_entry_t;

typedef struct enum_names {
    const enum_name_entry_t* entries;
    size_t count;
} enum_names_t;

const char* enum_name(const enum_names_t* names, int64_t value);
/* Nonzero when @value has a name */
int enum_name_exists(const enum_names_t* names, int64_t value);
/* Every value or'ed together, for flag enums */
uint64_t enum_names_mask(const enum_names_t* names);

extern const enum_names_t enum_names_AmdGpu_BlendControl_BlendFactor;
extern const enum_names_t enum_names_AmdGpu_BlendControl_BlendFunc;
extern const enum_names_t enum_names_AmdGpu_ClipSpace;
extern const enum_names_t enum_names_AmdGpu_ColorBuffer_EndianSwap;
extern const enum_names_t enum_names_AmdGpu_ColorBuffer_RoundMode;
extern const enum_names_t enum_names_AmdGpu_ColorBuffer_SwapMode;
extern const enum_names_t enum_names_AmdGpu_ColorControl_OperationMode;
extern const enum_names_t enum_names_AmdGpu_CompareFunc;
extern const enum_names_t enum_names_AmdGpu_CullMode;
extern const enum_names_t enum_names_AmdGpu_DataFormat;
extern const enum_names_t enum_names_AmdGpu_DepthBuffer_StencilFormat;
extern const enum_names_t enum_names_AmdGpu_DepthBuffer_ZFormat;
extern const enum_names_t enum_names_AmdGpu_EventIndex;
extern const enum_names_t enum_names_AmdGpu_EventType;
extern const enum_names_t enum_names_AmdGpu_FpRoundMode;
extern const enum_names_t enum_names_AmdGpu_FrontFace;
extern const enum_names_t enum_names_AmdGpu_ImageType;
extern const enum_names_t enum_names_AmdGpu_NumberFormat;
extern const enum_names_t enum_names_AmdGpu_PolygonMode;
extern const enum_names_t enum_names_AmdGpu_PrimitiveType;
extern const enum_names_t enum_names_AmdGpu_ProvokingVtxLast;
extern const enum_names_t enum_names_AmdGpu_SourceSelect;
extern const enum_names_t enum_names_AmdGpu_TessellationPartitioning;
extern const enum_names_t enum_names_AmdGpu_TessellationType;
extern const enum_names_t enum_names_AmdGpu_TileMode;
extern const enum_names_t enum_names_Core_FileSys_FileType;
extern const enum_names_t enum_names_Core_MemoryProt;
extern const enum_names_t enum_names_Platform_InterruptId;
extern const enum_names_t enum_names_Core_VMAType;
extern const enum_names_t enum_names_DebugStateType_QueueType;
extern const enum_names_t enum_names_Libraries_Ajm_AjmCodecType;
extern const enum_names_t enum_names_Libraries_Audio3d_OrbisAudio3dBlocking;
extern const enum_names_t enum_names_Libraries_Audio3d_OrbisAudio3dFormat;
extern const enum_names_t enum_names_Libraries_Audio3d_OrbisAudio3dOutputRoute;
extern const enum_names_t enum_names_Libraries_AudioOut_OrbisAudioOutParamAttr;
extern const enum_names_t enum_names_Libraries_AudioOut_OrbisAudioOutParamFormat;
extern const enum_names_t enum_names_Libraries_AudioOut_OrbisAudioOutPort;
extern const enum_names_t enum_names_Libraries_AvPlayer_AvPlayerEvents;
extern const enum_names_t enum_names_Libraries_AvPlayer_AvState;
extern const enum_names_t enum_names_Libraries_CommonDialog_Result;
extern const enum_names_t enum_names_Libraries_CommonDialog_Status;
extern const enum_names_t enum_names_Libraries_JpegEnc_OrbisJpegEncEncodeParamPixelFormat;
extern const enum_names_t enum_names_Libraries_JpegEnc_OrbisJpengEncEncodeParamColorSpace;
extern const enum_names_t enum_names_Libraries_JpegEnc_OrbisJpengEncEncodeParamEncodeMode;
extern const enum_names_t enum_names_Libraries_JpegEnc_OrbisJpengEncEncodeParamSamplingType;
extern const enum_names_t enum_names_Libraries_Kernel_MemoryTypes;
extern const enum_names_t enum_names_Libraries_Kernel_OrbisKernelEvent_Filter;
extern const enum_names_t enum_names_Libraries_MsgDialog_MsgDialogMode;
extern const enum_names_t enum_names_Libraries_Np_NpWebApi_OrbisNpWebApiHttpMethod;
extern const enum_names_t enum_names_Libraries_SaveData_Backup_WorkerStatus;
extern const enum_names_t enum_names_Libraries_SaveData_Dialog_SaveDataDialogMode;
extern const enum_names_t enum_names_Libraries_SaveData_Dialog_SystemMessageType;
extern const enum_names_t enum_names_Libraries_SaveData_OrbisSaveDataParamType;
extern const enum_names_t enum_names_OrbisImeDisableDevice;
extern const enum_names_t enum_names_OrbisImeExtOption;
extern const enum_names_t enum_names_OrbisImeInitExtKeyboardMode;
extern const enum_names_t enum_names_OrbisImeInputMethodState;
extern const enum_names_t enum_names_OrbisImeKeyboardOption;
extern const enum_names_t enum_names_OrbisImeKeycodeState;
extern const enum_names_t enum_names_OrbisImeLanguage;
extern const enum_names_t enum_names_OrbisImeOption;
extern const enum_names_t enum_names_Shader_Gcn_DppCtrl;
extern const enum_names_t enum_names_Shader_Gcn_InstCategory;
extern const enum_names_t enum_names_Shader_Gcn_Opcode;
extern const enum_names_t enum_names_Shader_IR_RegType;
extern const enum_names_t enum_names_Shader_Output;
extern const enum_names_t enum_names_VideoCore_TextureCache_MetaType;

RETRO_END_DECLS

#endif
