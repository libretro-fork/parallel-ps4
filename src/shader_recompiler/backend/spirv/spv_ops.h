/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The SPIR-V instructions the shader recompiler writes, on spv_module.h: one function each,
 * named as SPIR-V names the instruction, writing its words straight into the module. Ids are
 * plain words. A type or constant is made once (spv_module_decl); an instruction of the
 * GLSL.std.450 or AMD sets imports its set the first time. An instruction that takes a
 * list takes it as a pointer and a count, and the common lengths have their own function
 * (spv_OpAccessChain2 and the like), so that a caller needs no array of its own. C89. */

#ifndef SHADPS4_SPV_OPS_H
#define SHADPS4_SPV_OPS_H

#include <spirv/unified1/AMD_gcn_shader.h>
#include <spirv/unified1/AMD_shader_explicit_vertex_parameter.h>
#include <spirv/unified1/AMD_shader_trinary_minmax.h>
#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.h>

#include "shader_recompiler/backend/spirv/spv_module.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- module-level: capabilities, extensions, entry points, modes, debug ---- */

static SPIRV_WORDS_INLINE void spv_AddCapability(spv_module_t* m, uint32_t cap) {
    spv_module_capability(m, cap);
}

static SPIRV_WORDS_INLINE void spv_AddExtension(spv_module_t* m, const char* ext) {
    spv_module_extension(m, ext);
}

static SPIRV_WORDS_INLINE void spv_AddEntryPoint(spv_module_t* m, uint32_t model, uint32_t fn,
                                                 const char* name, const uint32_t* iface,
                                                 uint32_t n) {
    uint32_t pre[2];
    pre[0] = model;
    pre[1] = fn;
    spv_module_op_str(m, SPV_SEC_ENTRY_POINTS, SpvOpEntryPoint, pre, 2, name, iface, n);
}

/* OpExecutionMode with @n literals */
static SPIRV_WORDS_INLINE void spv_AddExecutionMode(spv_module_t* m, uint32_t fn, uint32_t mode,
                                                    const uint32_t* lits, uint32_t n) {
    uint32_t w[8];
    uint32_t i;
    w[0] = fn;
    w[1] = mode;
    for (i = 0; i < n && i < 6; i++) {
        w[2 + i] = lits[i];
    }
    spv_module_op(m, SPV_SEC_EXEC_MODES, SpvOpExecutionMode, w, 2 + i);
}

static SPIRV_WORDS_INLINE void spv_AddExecutionMode0(spv_module_t* m, uint32_t fn, uint32_t mode) {
    spv_AddExecutionMode(m, fn, mode, NULL, 0);
}

static SPIRV_WORDS_INLINE void spv_AddExecutionMode1(spv_module_t* m, uint32_t fn, uint32_t mode,
                                                     uint32_t a) {
    spv_AddExecutionMode(m, fn, mode, &a, 1);
}

static SPIRV_WORDS_INLINE void spv_AddExecutionMode3(spv_module_t* m, uint32_t fn, uint32_t mode,
                                                     uint32_t a, uint32_t b, uint32_t c) {
    uint32_t w[3];
    w[0] = a;
    w[1] = b;
    w[2] = c;
    spv_AddExecutionMode(m, fn, mode, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_Name(spv_module_t* m, uint32_t target, const char* name) {
    spv_module_op_str(m, SPV_SEC_DEBUG_NAMES, SpvOpName, &target, 1, name, NULL, 0);
    return target;
}

static SPIRV_WORDS_INLINE uint32_t spv_MemberName(spv_module_t* m, uint32_t type, uint32_t member,
                                                  const char* name) {
    uint32_t pre[2];
    pre[0] = type;
    pre[1] = member;
    spv_module_op_str(m, SPV_SEC_DEBUG_NAMES, SpvOpMemberName, pre, 2, name, NULL, 0);
    return type;
}

/* OpString: its id */
static SPIRV_WORDS_INLINE uint32_t spv_String(spv_module_t* m, const char* str) {
    const uint32_t id = spv_module_id(m);
    spv_module_op_str(m, SPV_SEC_DEBUG_STRINGS, SpvOpString, &id, 1, str, NULL, 0);
    return id;
}

/* OpDecorate with @n literals */
static SPIRV_WORDS_INLINE uint32_t spv_Decorate(spv_module_t* m, uint32_t target, uint32_t dec,
                                                const uint32_t* lits, uint32_t n) {
    uint32_t w[8];
    uint32_t i;
    w[0] = target;
    w[1] = dec;
    for (i = 0; i < n && i < 6; i++) {
        w[2 + i] = lits[i];
    }
    spv_module_op(m, SPV_SEC_ANNOTATIONS, SpvOpDecorate, w, 2 + i);
    return target;
}

static SPIRV_WORDS_INLINE uint32_t spv_Decorate0(spv_module_t* m, uint32_t target, uint32_t dec) {
    return spv_Decorate(m, target, dec, NULL, 0);
}

static SPIRV_WORDS_INLINE uint32_t spv_Decorate1(spv_module_t* m, uint32_t target, uint32_t dec,
                                                 uint32_t a) {
    return spv_Decorate(m, target, dec, &a, 1);
}

/* OpMemberDecorate with @n literals */
static SPIRV_WORDS_INLINE uint32_t spv_MemberDecorate(spv_module_t* m, uint32_t type,
                                                      uint32_t member, uint32_t dec,
                                                      const uint32_t* lits, uint32_t n) {
    uint32_t w[8];
    uint32_t i;
    w[0] = type;
    w[1] = member;
    w[2] = dec;
    for (i = 0; i < n && i < 5; i++) {
        w[3 + i] = lits[i];
    }
    spv_module_op(m, SPV_SEC_ANNOTATIONS, SpvOpMemberDecorate, w, 3 + i);
    return type;
}

static SPIRV_WORDS_INLINE uint32_t spv_MemberDecorate0(spv_module_t* m, uint32_t type,
                                                       uint32_t member, uint32_t dec) {
    return spv_MemberDecorate(m, type, member, dec, NULL, 0);
}

static SPIRV_WORDS_INLINE uint32_t spv_MemberDecorate1(spv_module_t* m, uint32_t type,
                                                       uint32_t member, uint32_t dec, uint32_t a) {
    return spv_MemberDecorate(m, type, member, dec, &a, 1);
}

/* ---- types: each made once ---- */

static SPIRV_WORDS_INLINE uint32_t spv_TypeVoid(spv_module_t* m) {
    return spv_module_decl(m, SpvOpTypeVoid, 0, NULL, 0);
}

static SPIRV_WORDS_INLINE uint32_t spv_TypeBool(spv_module_t* m) {
    return spv_module_decl(m, SpvOpTypeBool, 0, NULL, 0);
}

static SPIRV_WORDS_INLINE uint32_t spv_TypeInt(spv_module_t* m, uint32_t width,
                                               uint32_t is_signed) {
    uint32_t w[2];
    w[0] = width;
    w[1] = is_signed ? 1u : 0u;
    return spv_module_decl(m, SpvOpTypeInt, 0, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_TypeSInt(spv_module_t* m, uint32_t width) {
    return spv_TypeInt(m, width, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_TypeUInt(spv_module_t* m, uint32_t width) {
    return spv_TypeInt(m, width, 0);
}

static SPIRV_WORDS_INLINE uint32_t spv_TypeFloat(spv_module_t* m, uint32_t width) {
    return spv_module_decl(m, SpvOpTypeFloat, 0, &width, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_TypeVector(spv_module_t* m, uint32_t component,
                                                  uint32_t count) {
    uint32_t w[2];
    w[0] = component;
    w[1] = count;
    return spv_module_decl(m, SpvOpTypeVector, 0, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_TypeArray(spv_module_t* m, uint32_t element,
                                                 uint32_t length) {
    uint32_t w[2];
    w[0] = element;
    w[1] = length;
    return spv_module_decl(m, SpvOpTypeArray, 0, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_TypeRuntimeArray(spv_module_t* m, uint32_t element) {
    return spv_module_decl(m, SpvOpTypeRuntimeArray, 0, &element, 1);
}

/* As Sirit made them: a struct of the same members is the same struct */
static SPIRV_WORDS_INLINE uint32_t spv_TypeStruct(spv_module_t* m, const uint32_t* members,
                                                  uint32_t n) {
    return spv_module_decl(m, SpvOpTypeStruct, 0, members, n);
}

static SPIRV_WORDS_INLINE uint32_t spv_TypeStruct1(spv_module_t* m, uint32_t a) {
    return spv_TypeStruct(m, &a, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_TypePointer(spv_module_t* m, uint32_t storage,
                                                   uint32_t type) {
    uint32_t w[2];
    w[0] = storage;
    w[1] = type;
    return spv_module_decl(m, SpvOpTypePointer, 0, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_TypeFunction(spv_module_t* m, uint32_t ret,
                                                    const uint32_t* args, uint32_t n) {
    uint32_t w[16];
    uint32_t i;
    w[0] = ret;
    for (i = 0; i < n && i < 15; i++) {
        w[1 + i] = args[i];
    }
    return spv_module_decl(m, SpvOpTypeFunction, 0, w, 1 + i);
}

static SPIRV_WORDS_INLINE uint32_t spv_TypeFunction0(spv_module_t* m, uint32_t ret) {
    return spv_TypeFunction(m, ret, NULL, 0);
}

/* @access ~0u: none */
static SPIRV_WORDS_INLINE uint32_t spv_TypeImage(spv_module_t* m, uint32_t sampled_type,
                                                 uint32_t dim, uint32_t depth, uint32_t arrayed,
                                                 uint32_t ms, uint32_t sampled, uint32_t format,
                                                 uint32_t access) {
    uint32_t w[8];
    w[0] = sampled_type;
    w[1] = dim;
    w[2] = depth;
    w[3] = arrayed ? 1u : 0u;
    w[4] = ms ? 1u : 0u;
    w[5] = sampled;
    w[6] = format;
    w[7] = access;
    return spv_module_decl(m, SpvOpTypeImage, 0, w, access == ~0u ? 7 : 8);
}

static SPIRV_WORDS_INLINE uint32_t spv_TypeSampler(spv_module_t* m) {
    return spv_module_decl(m, SpvOpTypeSampler, 0, NULL, 0);
}

static SPIRV_WORDS_INLINE uint32_t spv_TypeSampledImage(spv_module_t* m, uint32_t image) {
    return spv_module_decl(m, SpvOpTypeSampledImage, 0, &image, 1);
}

/* ---- constants: each made once ---- */

static SPIRV_WORDS_INLINE uint32_t spv_Constant32(spv_module_t* m, uint32_t type, uint32_t bits) {
    return spv_module_decl(m, SpvOpConstant, type, &bits, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_Constant64(spv_module_t* m, uint32_t type, uint64_t bits) {
    uint32_t w[2];
    w[0] = (uint32_t)bits;
    w[1] = (uint32_t)(bits >> 32);
    return spv_module_decl(m, SpvOpConstant, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_ConstantF32(spv_module_t* m, uint32_t type, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return spv_Constant32(m, type, bits);
}

static SPIRV_WORDS_INLINE uint32_t spv_ConstantF64(spv_module_t* m, uint32_t type, double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return spv_Constant64(m, type, bits);
}

static SPIRV_WORDS_INLINE uint32_t spv_ConstantTrue(spv_module_t* m, uint32_t type) {
    return spv_module_decl(m, SpvOpConstantTrue, type, NULL, 0);
}

static SPIRV_WORDS_INLINE uint32_t spv_ConstantFalse(spv_module_t* m, uint32_t type) {
    return spv_module_decl(m, SpvOpConstantFalse, type, NULL, 0);
}

static SPIRV_WORDS_INLINE uint32_t spv_ConstantComposite(spv_module_t* m, uint32_t type,
                                                         const uint32_t* parts, uint32_t n) {
    return spv_module_decl(m, SpvOpConstantComposite, type, parts, n);
}

/* ---- variables ---- */

/* @init 0: none */
static SPIRV_WORDS_INLINE uint32_t spv_AddGlobalVariable(spv_module_t* m, uint32_t ptr_type,
                                                         uint32_t storage, uint32_t init) {
    uint32_t w[2];
    w[0] = storage;
    w[1] = init;
    return spv_module_decl_new(m, SpvOpVariable, ptr_type, w, init ? 2 : 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_AddLocalVariable(spv_module_t* m, uint32_t ptr_type,
                                                        uint32_t storage, uint32_t init) {
    uint32_t w[2];
    w[0] = storage;
    w[1] = init;
    return spv_module_result(m, SpvOpVariable, ptr_type, w, init ? 2 : 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUndef(spv_module_t* m, uint32_t type) {
    return spv_module_decl_new(m, SpvOpUndef, type, NULL, 0);
}

/* ---- functions and control flow ---- */

static SPIRV_WORDS_INLINE uint32_t spv_OpFunction(spv_module_t* m, uint32_t ret, uint32_t control,
                                                  uint32_t fn_type) {
    uint32_t w[2];
    w[0] = control;
    w[1] = fn_type;
    return spv_module_result(m, SpvOpFunction, ret, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFunctionParameter(spv_module_t* m, uint32_t type) {
    return spv_module_result(m, SpvOpFunctionParameter, type, NULL, 0);
}

static SPIRV_WORDS_INLINE void spv_OpFunctionEnd(spv_module_t* m) {
    spv_module_op(m, SPV_SEC_CODE, SpvOpFunctionEnd, NULL, 0);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFunctionCall(spv_module_t* m, uint32_t type, uint32_t fn,
                                                      const uint32_t* args, uint32_t n) {
    uint32_t w[16];
    uint32_t i;
    w[0] = fn;
    for (i = 0; i < n && i < 15; i++) {
        w[1 + i] = args[i];
    }
    return spv_module_result(m, SpvOpFunctionCall, type, w, 1 + i);
}

/* A label id not yet placed */
static SPIRV_WORDS_INLINE uint32_t spv_OpLabel(spv_module_t* m) {
    return spv_module_id(m);
}

/* @label placed here */
static SPIRV_WORDS_INLINE uint32_t spv_AddLabel(spv_module_t* m, uint32_t label) {
    spv_module_op(m, SPV_SEC_CODE, SpvOpLabel, &label, 1);
    return label;
}

static SPIRV_WORDS_INLINE uint32_t spv_AddLabel0(spv_module_t* m) {
    return spv_AddLabel(m, spv_module_id(m));
}

static SPIRV_WORDS_INLINE uint32_t spv_OpBranch(spv_module_t* m, uint32_t target) {
    spv_module_op(m, SPV_SEC_CODE, SpvOpBranch, &target, 1);
    return 0;
}

static SPIRV_WORDS_INLINE uint32_t spv_OpBranchConditional(spv_module_t* m, uint32_t cond,
                                                           uint32_t t, uint32_t f) {
    uint32_t w[3];
    w[0] = cond;
    w[1] = t;
    w[2] = f;
    spv_module_op(m, SPV_SEC_CODE, SpvOpBranchConditional, w, 3);
    return 0;
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSelectionMerge(spv_module_t* m, uint32_t merge,
                                                        uint32_t control) {
    uint32_t w[2];
    w[0] = merge;
    w[1] = control;
    spv_module_op(m, SPV_SEC_CODE, SpvOpSelectionMerge, w, 2);
    return 0;
}

static SPIRV_WORDS_INLINE uint32_t spv_OpLoopMerge(spv_module_t* m, uint32_t merge, uint32_t cont,
                                                   uint32_t control) {
    uint32_t w[3];
    w[0] = merge;
    w[1] = cont;
    w[2] = control;
    spv_module_op(m, SPV_SEC_CODE, SpvOpLoopMerge, w, 3);
    return 0;
}

static SPIRV_WORDS_INLINE void spv_OpReturn(spv_module_t* m) {
    spv_module_op(m, SPV_SEC_CODE, SpvOpReturn, NULL, 0);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpReturnValue(spv_module_t* m, uint32_t value) {
    spv_module_op(m, SPV_SEC_CODE, SpvOpReturnValue, &value, 1);
    return 0;
}

static SPIRV_WORDS_INLINE void spv_OpUnreachable(spv_module_t* m) {
    spv_module_op(m, SPV_SEC_CODE, SpvOpUnreachable, NULL, 0);
}

static SPIRV_WORDS_INLINE void spv_OpDemoteToHelperInvocationEXT(spv_module_t* m) {
    spv_module_op(m, SPV_SEC_CODE, SpvOpDemoteToHelperInvocationEXT, NULL, 0);
}

static SPIRV_WORDS_INLINE void spv_OpEmitVertex(spv_module_t* m) {
    spv_module_op(m, SPV_SEC_CODE, SpvOpEmitVertex, NULL, 0);
}

static SPIRV_WORDS_INLINE void spv_OpEndPrimitive(spv_module_t* m) {
    spv_module_op(m, SPV_SEC_CODE, SpvOpEndPrimitive, NULL, 0);
}

/* OpPhi of @n (value, parent) pairs */
static SPIRV_WORDS_INLINE uint32_t spv_OpPhi(spv_module_t* m, uint32_t type, const uint32_t* pairs,
                                             uint32_t n) {
    return spv_module_result(m, SpvOpPhi, type, pairs, n);
}

/* OpPhi whose values are written later (spv_module_patch_phis), its parents @blocks */
static SPIRV_WORDS_INLINE uint32_t spv_DeferredOpPhi(spv_module_t* m, uint32_t type,
                                                     const uint32_t* blocks, uint32_t n) {
    return spv_module_phi_deferred(m, type, blocks, n);
}

/* ---- memory ---- */

static SPIRV_WORDS_INLINE void spv_SetMemoryModel(spv_module_t* m, uint32_t addressing,
                                                  uint32_t model) {
    uint32_t w[2];
    w[0] = addressing;
    w[1] = model;
    spv_module_op(m, SPV_SEC_MEMORY_MODEL, SpvOpMemoryModel, w, 2);
}

/* OpLoad through a physical pointer, which says its alignment */
static SPIRV_WORDS_INLINE uint32_t spv_OpLoadAligned(spv_module_t* m, uint32_t type, uint32_t ptr,
                                                     uint32_t alignment) {
    uint32_t w[3];
    w[0] = ptr;
    w[1] = SpvMemoryAccessAlignedMask;
    w[2] = alignment;
    return spv_module_result(m, SpvOpLoad, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpLoad(spv_module_t* m, uint32_t type, uint32_t ptr) {
    return spv_module_result(m, SpvOpLoad, type, &ptr, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpStore(spv_module_t* m, uint32_t ptr, uint32_t value) {
    uint32_t w[2];
    w[0] = ptr;
    w[1] = value;
    spv_module_op(m, SPV_SEC_CODE, SpvOpStore, w, 2);
    return 0;
}

static SPIRV_WORDS_INLINE uint32_t spv_OpAccessChain(spv_module_t* m, uint32_t type, uint32_t base,
                                                     const uint32_t* idx, uint32_t n) {
    uint32_t w[16];
    uint32_t i;
    w[0] = base;
    for (i = 0; i < n && i < 15; i++) {
        w[1 + i] = idx[i];
    }
    return spv_module_result(m, SpvOpAccessChain, type, w, 1 + i);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpAccessChain1(spv_module_t* m, uint32_t type, uint32_t base,
                                                      uint32_t a) {
    return spv_OpAccessChain(m, type, base, &a, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpAccessChain2(spv_module_t* m, uint32_t type, uint32_t base,
                                                      uint32_t a, uint32_t b) {
    uint32_t w[2];
    w[0] = a;
    w[1] = b;
    return spv_OpAccessChain(m, type, base, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpAccessChain3(spv_module_t* m, uint32_t type, uint32_t base,
                                                      uint32_t a, uint32_t b, uint32_t c) {
    uint32_t w[3];
    w[0] = a;
    w[1] = b;
    w[2] = c;
    return spv_OpAccessChain(m, type, base, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpImageTexelPointer(spv_module_t* m, uint32_t type,
                                                           uint32_t image, uint32_t coord,
                                                           uint32_t sample) {
    uint32_t w[3];
    w[0] = image;
    w[1] = coord;
    w[2] = sample;
    return spv_module_result(m, SpvOpImageTexelPointer, type, w, 3);
}

/* ---- composites ---- */

static SPIRV_WORDS_INLINE uint32_t spv_OpCompositeConstruct(spv_module_t* m, uint32_t type,
                                                            const uint32_t* parts, uint32_t n) {
    return spv_module_result(m, SpvOpCompositeConstruct, type, parts, n);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpCompositeConstruct2(spv_module_t* m, uint32_t type,
                                                             uint32_t a, uint32_t b) {
    uint32_t w[2];
    w[0] = a;
    w[1] = b;
    return spv_OpCompositeConstruct(m, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpCompositeConstruct3(spv_module_t* m, uint32_t type,
                                                             uint32_t a, uint32_t b, uint32_t c) {
    uint32_t w[3];
    w[0] = a;
    w[1] = b;
    w[2] = c;
    return spv_OpCompositeConstruct(m, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpCompositeConstruct4(spv_module_t* m, uint32_t type,
                                                             uint32_t a, uint32_t b, uint32_t c,
                                                             uint32_t d) {
    uint32_t w[4];
    w[0] = a;
    w[1] = b;
    w[2] = c;
    w[3] = d;
    return spv_OpCompositeConstruct(m, type, w, 4);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpCompositeExtract(spv_module_t* m, uint32_t type,
                                                          uint32_t comp, const uint32_t* idx,
                                                          uint32_t n) {
    uint32_t w[8];
    uint32_t i;
    w[0] = comp;
    for (i = 0; i < n && i < 7; i++) {
        w[1 + i] = idx[i];
    }
    return spv_module_result(m, SpvOpCompositeExtract, type, w, 1 + i);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpCompositeExtract1(spv_module_t* m, uint32_t type,
                                                           uint32_t comp, uint32_t a) {
    return spv_OpCompositeExtract(m, type, comp, &a, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpCompositeExtract2(spv_module_t* m, uint32_t type,
                                                           uint32_t comp, uint32_t a, uint32_t b) {
    uint32_t w[2];
    w[0] = a;
    w[1] = b;
    return spv_OpCompositeExtract(m, type, comp, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpCompositeInsert(spv_module_t* m, uint32_t type,
                                                         uint32_t object, uint32_t comp,
                                                         const uint32_t* idx, uint32_t n) {
    uint32_t w[8];
    uint32_t i;
    w[0] = object;
    w[1] = comp;
    for (i = 0; i < n && i < 6; i++) {
        w[2 + i] = idx[i];
    }
    return spv_module_result(m, SpvOpCompositeInsert, type, w, 2 + i);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpCompositeInsert1(spv_module_t* m, uint32_t type,
                                                          uint32_t object, uint32_t comp,
                                                          uint32_t a) {
    return spv_OpCompositeInsert(m, type, object, comp, &a, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpVectorShuffle(spv_module_t* m, uint32_t type, uint32_t v1,
                                                       uint32_t v2, const uint32_t* comps,
                                                       uint32_t n) {
    uint32_t w[18];
    uint32_t i;
    w[0] = v1;
    w[1] = v2;
    for (i = 0; i < n && i < 16; i++) {
        w[2 + i] = comps[i];
    }
    return spv_module_result(m, SpvOpVectorShuffle, type, w, 2 + i);
}

/* ---- bits ---- */

static SPIRV_WORDS_INLINE uint32_t spv_OpNot(spv_module_t* m, uint32_t type, uint32_t a) {
    return spv_module_result(m, SpvOpNot, type, &a, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpBitCount(spv_module_t* m, uint32_t type, uint32_t a) {
    return spv_module_result(m, SpvOpBitCount, type, &a, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpBitReverse(spv_module_t* m, uint32_t type, uint32_t a) {
    return spv_module_result(m, SpvOpBitReverse, type, &a, 1);
}

#define SPV_OPS_BINARY(name)                                                                       \
    static SPIRV_WORDS_INLINE uint32_t spv_##name(spv_module_t* m, uint32_t type, uint32_t a,      \
                                                  uint32_t b) {                                    \
        uint32_t w[2];                                                                             \
        w[0] = a;                                                                                  \
        w[1] = b;                                                                                  \
        return spv_module_result(m, Spv##name, type, w, 2);                                        \
    }

#define SPV_OPS_TERNARY(name)                                                                      \
    static SPIRV_WORDS_INLINE uint32_t spv_##name(spv_module_t* m, uint32_t type, uint32_t a,      \
                                                  uint32_t b, uint32_t c) {                        \
        uint32_t w[3];                                                                             \
        w[0] = a;                                                                                  \
        w[1] = b;                                                                                  \
        w[2] = c;                                                                                  \
        return spv_module_result(m, Spv##name, type, w, 3);                                        \
    }

#define SPV_OPS_QUATERNARY(name)                                                                   \
    static SPIRV_WORDS_INLINE uint32_t spv_##name(spv_module_t* m, uint32_t type, uint32_t a,      \
                                                  uint32_t b, uint32_t c, uint32_t d) {            \
        uint32_t w[4];                                                                             \
        w[0] = a;                                                                                  \
        w[1] = b;                                                                                  \
        w[2] = c;                                                                                  \
        w[3] = d;                                                                                  \
        return spv_module_result(m, Spv##name, type, w, 4);                                        \
    }

SPV_OPS_BINARY(OpBitwiseAnd)
SPV_OPS_BINARY(OpBitwiseOr)
SPV_OPS_BINARY(OpBitwiseXor)
SPV_OPS_BINARY(OpShiftLeftLogical)
SPV_OPS_BINARY(OpShiftRightLogical)
SPV_OPS_BINARY(OpShiftRightArithmetic)
SPV_OPS_TERNARY(OpBitFieldSExtract)
SPV_OPS_TERNARY(OpBitFieldUExtract)
SPV_OPS_QUATERNARY(OpBitFieldInsert)
SPV_OPS_BINARY(OpSampledImage)

/* ---- atomics: pointer, scope, semantics[, value] ---- */
SPV_OPS_TERNARY(OpAtomicIIncrement)
SPV_OPS_TERNARY(OpAtomicIDecrement)
SPV_OPS_QUATERNARY(OpAtomicIAdd)
SPV_OPS_QUATERNARY(OpAtomicISub)
SPV_OPS_QUATERNARY(OpAtomicSMin)
SPV_OPS_QUATERNARY(OpAtomicUMin)
SPV_OPS_QUATERNARY(OpAtomicSMax)
SPV_OPS_QUATERNARY(OpAtomicUMax)
SPV_OPS_QUATERNARY(OpAtomicAnd)
SPV_OPS_QUATERNARY(OpAtomicOr)
SPV_OPS_QUATERNARY(OpAtomicXor)
SPV_OPS_QUATERNARY(OpAtomicExchange)

static SPIRV_WORDS_INLINE uint32_t spv_OpAtomicFMin(spv_module_t* m, uint32_t type, uint32_t a,
                                                    uint32_t b, uint32_t c, uint32_t d) {
    uint32_t w[4];
    w[0] = a;
    w[1] = b;
    w[2] = c;
    w[3] = d;
    return spv_module_result(m, SpvOpAtomicFMinEXT, type, w, 4);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpAtomicFMax(spv_module_t* m, uint32_t type, uint32_t a,
                                                    uint32_t b, uint32_t c, uint32_t d) {
    uint32_t w[4];
    w[0] = a;
    w[1] = b;
    w[2] = c;
    w[3] = d;
    return spv_module_result(m, SpvOpAtomicFMaxEXT, type, w, 4);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpAtomicCompareExchange(spv_module_t* m, uint32_t type,
                                                               uint32_t ptr, uint32_t scope,
                                                               uint32_t equal, uint32_t unequal,
                                                               uint32_t value,
                                                               uint32_t comparator) {
    uint32_t w[6];
    w[0] = ptr;
    w[1] = scope;
    w[2] = equal;
    w[3] = unequal;
    w[4] = value;
    w[5] = comparator;
    return spv_module_result(m, SpvOpAtomicCompareExchange, type, w, 6);
}

/* ---- barriers ---- */

static SPIRV_WORDS_INLINE uint32_t spv_OpControlBarrier(spv_module_t* m, uint32_t exec,
                                                        uint32_t mem, uint32_t sem) {
    uint32_t w[3];
    w[0] = exec;
    w[1] = mem;
    w[2] = sem;
    spv_module_op(m, SPV_SEC_CODE, SpvOpControlBarrier, w, 3);
    return 0;
}

static SPIRV_WORDS_INLINE uint32_t spv_OpMemoryBarrier(spv_module_t* m, uint32_t scope,
                                                       uint32_t sem) {
    uint32_t w[2];
    w[0] = scope;
    w[1] = sem;
    spv_module_op(m, SPV_SEC_CODE, SpvOpMemoryBarrier, w, 2);
    return 0;
}

/* ---- groups ---- */
SPV_OPS_BINARY(OpGroupNonUniformAny)
SPV_OPS_BINARY(OpGroupNonUniformBallot)
SPV_OPS_BINARY(OpGroupNonUniformInverseBallot)
SPV_OPS_BINARY(OpGroupNonUniformBroadcastFirst)
SPV_OPS_BINARY(OpGroupNonUniformBallotFindLSB)
SPV_OPS_TERNARY(OpGroupNonUniformBroadcast)
SPV_OPS_TERNARY(OpGroupNonUniformQuadBroadcast)
SPV_OPS_TERNARY(OpGroupNonUniformShuffle)
SPV_OPS_TERNARY(OpGroupNonUniformShuffleXor)

static SPIRV_WORDS_INLINE uint32_t spv_OpReadClockKHR(spv_module_t* m, uint32_t type,
                                                      uint32_t scope) {
    return spv_module_result(m, SpvOpReadClockKHR, type, &scope, 1);
}

/* ---- images, writes ---- */

static SPIRV_WORDS_INLINE uint32_t spv_OpImageWrite(spv_module_t* m, uint32_t image, uint32_t coord,
                                                    uint32_t texel, uint32_t mask,
                                                    const uint32_t* ops, uint32_t n) {
    uint32_t w[12];
    uint32_t k = 3;
    uint32_t i;
    w[0] = image;
    w[1] = coord;
    w[2] = texel;
    if (mask != 0 || n != 0) {
        w[k++] = mask;
        for (i = 0; i < n && k < 12; i++) {
            w[k++] = ops[i];
        }
    }
    spv_module_op(m, SPV_SEC_CODE, SpvOpImageWrite, w, k);
    return 0;
}

/* ---- debug printf: NonSemantic.DebugPrintf's one instruction ---- */
static SPIRV_WORDS_INLINE uint32_t spv_OpDebugPrintf(spv_module_t* m, uint32_t fmt,
                                                     const uint32_t* args, uint32_t n) {
    uint32_t w[20];
    uint32_t i;
    w[0] = spv_module_import(m, "NonSemantic.DebugPrintf");
    w[1] = 1; /* DebugPrintf */
    w[2] = fmt;
    for (i = 0; i < n && i < 17; i++) {
        w[3 + i] = args[i];
    }
    return spv_module_result(m, SpvOpExtInst, spv_TypeVoid(m), w, 3 + i);
}

/* ---- the rest, one result each ---- */

static SPIRV_WORDS_INLINE uint32_t spv_OpSNegate(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[1];
    w[0] = a0;
    return spv_module_result(m, SpvOpSNegate, type, w, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFNegate(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[1];
    w[0] = a0;
    return spv_module_result(m, SpvOpFNegate, type, w, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpIAdd(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpIAdd, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFAdd(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpFAdd, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpISub(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpISub, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFSub(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpFSub, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpIMul(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpIMul, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUMulExtended(spv_module_t* m, uint32_t type, uint32_t a0,
                                                      uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpUMulExtended, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSMulExtended(spv_module_t* m, uint32_t type, uint32_t a0,
                                                      uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpSMulExtended, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFMul(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpFMul, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUDiv(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpUDiv, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSDiv(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpSDiv, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFDiv(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpFDiv, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUMod(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpUMod, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSMod(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpSMod, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpIAddCarry(spv_module_t* m, uint32_t type, uint32_t a0,
                                                   uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpIAddCarry, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpVectorTimesScalar(spv_module_t* m, uint32_t type,
                                                           uint32_t a0, uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpVectorTimesScalar, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpConvertFToU(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[1];
    w[0] = a0;
    return spv_module_result(m, SpvOpConvertFToU, type, w, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpConvertFToS(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[1];
    w[0] = a0;
    return spv_module_result(m, SpvOpConvertFToS, type, w, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpConvertSToF(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[1];
    w[0] = a0;
    return spv_module_result(m, SpvOpConvertSToF, type, w, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpConvertUToF(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[1];
    w[0] = a0;
    return spv_module_result(m, SpvOpConvertUToF, type, w, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpConvertUToPtr(spv_module_t* m, uint32_t type,
                                                       uint32_t a0) {
    uint32_t w[1];
    w[0] = a0;
    return spv_module_result(m, SpvOpConvertUToPtr, type, w, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUConvert(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[1];
    w[0] = a0;
    return spv_module_result(m, SpvOpUConvert, type, w, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSConvert(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[1];
    w[0] = a0;
    return spv_module_result(m, SpvOpSConvert, type, w, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFConvert(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[1];
    w[0] = a0;
    return spv_module_result(m, SpvOpFConvert, type, w, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpBitcast(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[1];
    w[0] = a0;
    return spv_module_result(m, SpvOpBitcast, type, w, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFAbs(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450FAbs;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSAbs(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450SAbs;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpRoundEven(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450RoundEven;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpTrunc(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450Trunc;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFloor(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450Floor;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpCeil(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450Ceil;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFract(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450Fract;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSin(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450Sin;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpCos(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450Cos;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpPow(spv_module_t* m, uint32_t type, uint32_t a0,
                                             uint32_t a1) {
    uint32_t w[4];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450Pow;
    w[2] = a0;
    w[3] = a1;
    return spv_module_result(m, SpvOpExtInst, type, w, 4);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpExp2(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450Exp2;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpLog2(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450Log2;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSqrt(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450Sqrt;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpInverseSqrt(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450InverseSqrt;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpLdexp(spv_module_t* m, uint32_t type, uint32_t a0,
                                               uint32_t a1) {
    uint32_t w[4];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450Ldexp;
    w[2] = a0;
    w[3] = a1;
    return spv_module_result(m, SpvOpExtInst, type, w, 4);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFMin(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[4];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450FMin;
    w[2] = a0;
    w[3] = a1;
    return spv_module_result(m, SpvOpExtInst, type, w, 4);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUMin(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[4];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450UMin;
    w[2] = a0;
    w[3] = a1;
    return spv_module_result(m, SpvOpExtInst, type, w, 4);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSMin(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[4];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450SMin;
    w[2] = a0;
    w[3] = a1;
    return spv_module_result(m, SpvOpExtInst, type, w, 4);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpNMin(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[4];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450NMin;
    w[2] = a0;
    w[3] = a1;
    return spv_module_result(m, SpvOpExtInst, type, w, 4);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFMax(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[4];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450FMax;
    w[2] = a0;
    w[3] = a1;
    return spv_module_result(m, SpvOpExtInst, type, w, 4);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUMax(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[4];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450UMax;
    w[2] = a0;
    w[3] = a1;
    return spv_module_result(m, SpvOpExtInst, type, w, 4);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSMax(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[4];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450SMax;
    w[2] = a0;
    w[3] = a1;
    return spv_module_result(m, SpvOpExtInst, type, w, 4);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpNMax(spv_module_t* m, uint32_t type, uint32_t a0,
                                              uint32_t a1) {
    uint32_t w[4];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450NMax;
    w[2] = a0;
    w[3] = a1;
    return spv_module_result(m, SpvOpExtInst, type, w, 4);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFClamp(spv_module_t* m, uint32_t type, uint32_t a0,
                                                uint32_t a1, uint32_t a2) {
    uint32_t w[5];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450FClamp;
    w[2] = a0;
    w[3] = a1;
    w[4] = a2;
    return spv_module_result(m, SpvOpExtInst, type, w, 5);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpNClamp(spv_module_t* m, uint32_t type, uint32_t a0,
                                                uint32_t a1, uint32_t a2) {
    uint32_t w[5];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450NClamp;
    w[2] = a0;
    w[3] = a1;
    w[4] = a2;
    return spv_module_result(m, SpvOpExtInst, type, w, 5);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUClamp(spv_module_t* m, uint32_t type, uint32_t a0,
                                                uint32_t a1, uint32_t a2) {
    uint32_t w[5];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450UClamp;
    w[2] = a0;
    w[3] = a1;
    w[4] = a2;
    return spv_module_result(m, SpvOpExtInst, type, w, 5);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSClamp(spv_module_t* m, uint32_t type, uint32_t a0,
                                                uint32_t a1, uint32_t a2) {
    uint32_t w[5];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450SClamp;
    w[2] = a0;
    w[3] = a1;
    w[4] = a2;
    return spv_module_result(m, SpvOpExtInst, type, w, 5);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFma(spv_module_t* m, uint32_t type, uint32_t a0,
                                             uint32_t a1, uint32_t a2) {
    uint32_t w[5];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450Fma;
    w[2] = a0;
    w[3] = a1;
    w[4] = a2;
    return spv_module_result(m, SpvOpExtInst, type, w, 5);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFrexpStruct(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450FrexpStruct;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpPackHalf2x16(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450PackHalf2x16;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUnpackHalf2x16(spv_module_t* m, uint32_t type,
                                                        uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450UnpackHalf2x16;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpPackUnorm2x16(spv_module_t* m, uint32_t type,
                                                       uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450PackUnorm2x16;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUnpackUnorm2x16(spv_module_t* m, uint32_t type,
                                                         uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450UnpackUnorm2x16;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpPackSnorm2x16(spv_module_t* m, uint32_t type,
                                                       uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450PackSnorm2x16;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUnpackSnorm2x16(spv_module_t* m, uint32_t type,
                                                         uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450UnpackSnorm2x16;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpPackUnorm4x8(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450PackUnorm4x8;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUnpackUnorm4x8(spv_module_t* m, uint32_t type,
                                                        uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450UnpackUnorm4x8;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpPackSnorm4x8(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450PackSnorm4x8;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUnpackSnorm4x8(spv_module_t* m, uint32_t type,
                                                        uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450UnpackSnorm4x8;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFindILsb(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450FindILsb;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFindSMsb(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450FindSMsb;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFindUMsb(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450FindUMsb;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpInterpolateAtCentroid(spv_module_t* m, uint32_t type,
                                                               uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450InterpolateAtCentroid;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpInterpolateAtSample(spv_module_t* m, uint32_t type,
                                                             uint32_t a0, uint32_t a1) {
    uint32_t w[4];
    w[0] = spv_module_import(m, "GLSL.std.450");
    w[1] = GLSLstd450InterpolateAtSample;
    w[2] = a0;
    w[3] = a1;
    return spv_module_result(m, SpvOpExtInst, type, w, 4);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpCubeFaceIndexAMD(spv_module_t* m, uint32_t type,
                                                          uint32_t a0) {
    uint32_t w[3];
    w[0] = spv_module_import(m, "SPV_AMD_gcn_shader");
    w[1] = AMD_gcn_shaderCubeFaceIndexAMD;
    w[2] = a0;
    return spv_module_result(m, SpvOpExtInst, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpInterpolateAtVertexAMD(spv_module_t* m, uint32_t type,
                                                                uint32_t a0, uint32_t a1) {
    uint32_t w[4];
    w[0] = spv_module_import(m, "SPV_AMD_shader_explicit_vertex_parameter");
    w[1] = AMD_shader_explicit_vertex_parameterInterpolateAtVertexAMD;
    w[2] = a0;
    w[3] = a1;
    return spv_module_result(m, SpvOpExtInst, type, w, 4);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUMin3AMD(spv_module_t* m, uint32_t type, uint32_t a0,
                                                  uint32_t a1, uint32_t a2) {
    uint32_t w[5];
    w[0] = spv_module_import(m, "SPV_AMD_shader_trinary_minmax");
    w[1] = AMD_shader_trinary_minmaxUMin3AMD;
    w[2] = a0;
    w[3] = a1;
    w[4] = a2;
    return spv_module_result(m, SpvOpExtInst, type, w, 5);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSMin3AMD(spv_module_t* m, uint32_t type, uint32_t a0,
                                                  uint32_t a1, uint32_t a2) {
    uint32_t w[5];
    w[0] = spv_module_import(m, "SPV_AMD_shader_trinary_minmax");
    w[1] = AMD_shader_trinary_minmaxSMin3AMD;
    w[2] = a0;
    w[3] = a1;
    w[4] = a2;
    return spv_module_result(m, SpvOpExtInst, type, w, 5);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUMax3AMD(spv_module_t* m, uint32_t type, uint32_t a0,
                                                  uint32_t a1, uint32_t a2) {
    uint32_t w[5];
    w[0] = spv_module_import(m, "SPV_AMD_shader_trinary_minmax");
    w[1] = AMD_shader_trinary_minmaxUMax3AMD;
    w[2] = a0;
    w[3] = a1;
    w[4] = a2;
    return spv_module_result(m, SpvOpExtInst, type, w, 5);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSMax3AMD(spv_module_t* m, uint32_t type, uint32_t a0,
                                                  uint32_t a1, uint32_t a2) {
    uint32_t w[5];
    w[0] = spv_module_import(m, "SPV_AMD_shader_trinary_minmax");
    w[1] = AMD_shader_trinary_minmaxSMax3AMD;
    w[2] = a0;
    w[3] = a1;
    w[4] = a2;
    return spv_module_result(m, SpvOpExtInst, type, w, 5);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUMid3AMD(spv_module_t* m, uint32_t type, uint32_t a0,
                                                  uint32_t a1, uint32_t a2) {
    uint32_t w[5];
    w[0] = spv_module_import(m, "SPV_AMD_shader_trinary_minmax");
    w[1] = AMD_shader_trinary_minmaxUMid3AMD;
    w[2] = a0;
    w[3] = a1;
    w[4] = a2;
    return spv_module_result(m, SpvOpExtInst, type, w, 5);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSMid3AMD(spv_module_t* m, uint32_t type, uint32_t a0,
                                                  uint32_t a1, uint32_t a2) {
    uint32_t w[5];
    w[0] = spv_module_import(m, "SPV_AMD_shader_trinary_minmax");
    w[1] = AMD_shader_trinary_minmaxSMid3AMD;
    w[2] = a0;
    w[3] = a1;
    w[4] = a2;
    return spv_module_result(m, SpvOpExtInst, type, w, 5);
}

/* @mask 0 and no operands: none written */
static SPIRV_WORDS_INLINE uint32_t spv_OpImageSampleImplicitLod(spv_module_t* m, uint32_t type,
                                                                uint32_t a0, uint32_t a1,
                                                                uint32_t mask, const uint32_t* ops,
                                                                uint32_t n) {
    uint32_t w[11];
    uint32_t k = 2;
    uint32_t i;
    w[0] = a0;
    w[1] = a1;
    if (mask != 0 || n != 0) {
        w[k++] = mask;
        for (i = 0; i < n && k < 11; i++) {
            w[k++] = ops[i];
        }
    }
    return spv_module_result(m, SpvOpImageSampleImplicitLod, type, w, k);
}

/* @mask 0 and no operands: none written */
static SPIRV_WORDS_INLINE uint32_t spv_OpImageSampleExplicitLod(spv_module_t* m, uint32_t type,
                                                                uint32_t a0, uint32_t a1,
                                                                uint32_t mask, const uint32_t* ops,
                                                                uint32_t n) {
    uint32_t w[11];
    uint32_t k = 2;
    uint32_t i;
    w[0] = a0;
    w[1] = a1;
    if (mask != 0 || n != 0) {
        w[k++] = mask;
        for (i = 0; i < n && k < 11; i++) {
            w[k++] = ops[i];
        }
    }
    return spv_module_result(m, SpvOpImageSampleExplicitLod, type, w, k);
}

/* @mask 0 and no operands: none written */
static SPIRV_WORDS_INLINE uint32_t spv_OpImageSampleDrefImplicitLod(spv_module_t* m, uint32_t type,
                                                                    uint32_t a0, uint32_t a1,
                                                                    uint32_t a2, uint32_t mask,
                                                                    const uint32_t* ops,
                                                                    uint32_t n) {
    uint32_t w[12];
    uint32_t k = 3;
    uint32_t i;
    w[0] = a0;
    w[1] = a1;
    w[2] = a2;
    if (mask != 0 || n != 0) {
        w[k++] = mask;
        for (i = 0; i < n && k < 12; i++) {
            w[k++] = ops[i];
        }
    }
    return spv_module_result(m, SpvOpImageSampleDrefImplicitLod, type, w, k);
}

/* @mask 0 and no operands: none written */
static SPIRV_WORDS_INLINE uint32_t spv_OpImageSampleDrefExplicitLod(spv_module_t* m, uint32_t type,
                                                                    uint32_t a0, uint32_t a1,
                                                                    uint32_t a2, uint32_t mask,
                                                                    const uint32_t* ops,
                                                                    uint32_t n) {
    uint32_t w[12];
    uint32_t k = 3;
    uint32_t i;
    w[0] = a0;
    w[1] = a1;
    w[2] = a2;
    if (mask != 0 || n != 0) {
        w[k++] = mask;
        for (i = 0; i < n && k < 12; i++) {
            w[k++] = ops[i];
        }
    }
    return spv_module_result(m, SpvOpImageSampleDrefExplicitLod, type, w, k);
}

/* @mask 0 and no operands: none written */
static SPIRV_WORDS_INLINE uint32_t spv_OpImageFetch(spv_module_t* m, uint32_t type, uint32_t a0,
                                                    uint32_t a1, uint32_t mask, const uint32_t* ops,
                                                    uint32_t n) {
    uint32_t w[11];
    uint32_t k = 2;
    uint32_t i;
    w[0] = a0;
    w[1] = a1;
    if (mask != 0 || n != 0) {
        w[k++] = mask;
        for (i = 0; i < n && k < 11; i++) {
            w[k++] = ops[i];
        }
    }
    return spv_module_result(m, SpvOpImageFetch, type, w, k);
}

/* @mask 0 and no operands: none written */
static SPIRV_WORDS_INLINE uint32_t spv_OpImageGather(spv_module_t* m, uint32_t type, uint32_t a0,
                                                     uint32_t a1, uint32_t a2, uint32_t mask,
                                                     const uint32_t* ops, uint32_t n) {
    uint32_t w[12];
    uint32_t k = 3;
    uint32_t i;
    w[0] = a0;
    w[1] = a1;
    w[2] = a2;
    if (mask != 0 || n != 0) {
        w[k++] = mask;
        for (i = 0; i < n && k < 12; i++) {
            w[k++] = ops[i];
        }
    }
    return spv_module_result(m, SpvOpImageGather, type, w, k);
}

/* @mask 0 and no operands: none written */
static SPIRV_WORDS_INLINE uint32_t spv_OpImageDrefGather(spv_module_t* m, uint32_t type,
                                                         uint32_t a0, uint32_t a1, uint32_t a2,
                                                         uint32_t mask, const uint32_t* ops,
                                                         uint32_t n) {
    uint32_t w[12];
    uint32_t k = 3;
    uint32_t i;
    w[0] = a0;
    w[1] = a1;
    w[2] = a2;
    if (mask != 0 || n != 0) {
        w[k++] = mask;
        for (i = 0; i < n && k < 12; i++) {
            w[k++] = ops[i];
        }
    }
    return spv_module_result(m, SpvOpImageDrefGather, type, w, k);
}

/* @mask 0 and no operands: none written */
static SPIRV_WORDS_INLINE uint32_t spv_OpImageRead(spv_module_t* m, uint32_t type, uint32_t a0,
                                                   uint32_t a1, uint32_t mask, const uint32_t* ops,
                                                   uint32_t n) {
    uint32_t w[11];
    uint32_t k = 2;
    uint32_t i;
    w[0] = a0;
    w[1] = a1;
    if (mask != 0 || n != 0) {
        w[k++] = mask;
        for (i = 0; i < n && k < 11; i++) {
            w[k++] = ops[i];
        }
    }
    return spv_module_result(m, SpvOpImageRead, type, w, k);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpImageQuerySizeLod(spv_module_t* m, uint32_t type,
                                                           uint32_t a0, uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpImageQuerySizeLod, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpImageQuerySize(spv_module_t* m, uint32_t type,
                                                        uint32_t a0) {
    uint32_t w[1];
    w[0] = a0;
    return spv_module_result(m, SpvOpImageQuerySize, type, w, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpImageQueryLod(spv_module_t* m, uint32_t type, uint32_t a0,
                                                       uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpImageQueryLod, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpImageQueryLevels(spv_module_t* m, uint32_t type,
                                                          uint32_t a0) {
    uint32_t w[1];
    w[0] = a0;
    return spv_module_result(m, SpvOpImageQueryLevels, type, w, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpImageQuerySamples(spv_module_t* m, uint32_t type,
                                                           uint32_t a0) {
    uint32_t w[1];
    w[0] = a0;
    return spv_module_result(m, SpvOpImageQuerySamples, type, w, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpIsNan(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[1];
    w[0] = a0;
    return spv_module_result(m, SpvOpIsNan, type, w, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpIsInf(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[1];
    w[0] = a0;
    return spv_module_result(m, SpvOpIsInf, type, w, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpLogicalNotEqual(spv_module_t* m, uint32_t type,
                                                         uint32_t a0, uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpLogicalNotEqual, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpLogicalOr(spv_module_t* m, uint32_t type, uint32_t a0,
                                                   uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpLogicalOr, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpLogicalAnd(spv_module_t* m, uint32_t type, uint32_t a0,
                                                    uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpLogicalAnd, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpLogicalNot(spv_module_t* m, uint32_t type, uint32_t a0) {
    uint32_t w[1];
    w[0] = a0;
    return spv_module_result(m, SpvOpLogicalNot, type, w, 1);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSelect(spv_module_t* m, uint32_t type, uint32_t a0,
                                                uint32_t a1, uint32_t a2) {
    uint32_t w[3];
    w[0] = a0;
    w[1] = a1;
    w[2] = a2;
    return spv_module_result(m, SpvOpSelect, type, w, 3);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpIEqual(spv_module_t* m, uint32_t type, uint32_t a0,
                                                uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpIEqual, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpINotEqual(spv_module_t* m, uint32_t type, uint32_t a0,
                                                   uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpINotEqual, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUGreaterThan(spv_module_t* m, uint32_t type, uint32_t a0,
                                                      uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpUGreaterThan, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSGreaterThan(spv_module_t* m, uint32_t type, uint32_t a0,
                                                      uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpSGreaterThan, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpUGreaterThanEqual(spv_module_t* m, uint32_t type,
                                                           uint32_t a0, uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpUGreaterThanEqual, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSGreaterThanEqual(spv_module_t* m, uint32_t type,
                                                           uint32_t a0, uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpSGreaterThanEqual, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpULessThan(spv_module_t* m, uint32_t type, uint32_t a0,
                                                   uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpULessThan, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSLessThan(spv_module_t* m, uint32_t type, uint32_t a0,
                                                   uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpSLessThan, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpULessThanEqual(spv_module_t* m, uint32_t type, uint32_t a0,
                                                        uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpULessThanEqual, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpSLessThanEqual(spv_module_t* m, uint32_t type, uint32_t a0,
                                                        uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpSLessThanEqual, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFOrdEqual(spv_module_t* m, uint32_t type, uint32_t a0,
                                                   uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpFOrdEqual, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFUnordEqual(spv_module_t* m, uint32_t type, uint32_t a0,
                                                     uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpFUnordEqual, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFOrdNotEqual(spv_module_t* m, uint32_t type, uint32_t a0,
                                                      uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpFOrdNotEqual, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFUnordNotEqual(spv_module_t* m, uint32_t type, uint32_t a0,
                                                        uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpFUnordNotEqual, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFOrdLessThan(spv_module_t* m, uint32_t type, uint32_t a0,
                                                      uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpFOrdLessThan, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFUnordLessThan(spv_module_t* m, uint32_t type, uint32_t a0,
                                                        uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpFUnordLessThan, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFOrdGreaterThan(spv_module_t* m, uint32_t type,
                                                         uint32_t a0, uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpFOrdGreaterThan, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFUnordGreaterThan(spv_module_t* m, uint32_t type,
                                                           uint32_t a0, uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpFUnordGreaterThan, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFOrdLessThanEqual(spv_module_t* m, uint32_t type,
                                                           uint32_t a0, uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpFOrdLessThanEqual, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFUnordLessThanEqual(spv_module_t* m, uint32_t type,
                                                             uint32_t a0, uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpFUnordLessThanEqual, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFOrdGreaterThanEqual(spv_module_t* m, uint32_t type,
                                                              uint32_t a0, uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpFOrdGreaterThanEqual, type, w, 2);
}

static SPIRV_WORDS_INLINE uint32_t spv_OpFUnordGreaterThanEqual(spv_module_t* m, uint32_t type,
                                                                uint32_t a0, uint32_t a1) {
    uint32_t w[2];
    w[0] = a0;
    w[1] = a1;
    return spv_module_result(m, SpvOpFUnordGreaterThanEqual, type, w, 2);
}

#ifdef __cplusplus
}
#endif

#endif
