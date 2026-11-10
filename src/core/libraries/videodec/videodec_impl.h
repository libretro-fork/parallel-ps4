// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "video_utils.h"
#include "videodec.h"

namespace Libraries::Videodec {

class VdecDecoder {
public:
    VdecDecoder(const OrbisVideodecConfigInfo& pCfgInfoIn,
                const OrbisVideodecResourceInfo& pRsrcInfoIn);
    ~VdecDecoder();
    s32 Decode(const OrbisVideodecInputData& pInputDataIn,
               OrbisVideodecFrameBuffer& pFrameBufferInOut,
               OrbisVideodecPictureInfo& pPictureInfoOut);
    s32 Flush(OrbisVideodecFrameBuffer& pFrameBufferInOut,
              OrbisVideodecPictureInfo& pPictureInfoOut);
    s32 Reset();

private:
    s32 Output(OrbisVideodecFrameBuffer& pFrameBufferInOut,
               OrbisVideodecPictureInfo& pPictureInfoOut);

    SwDecoder mDecoder{false};
};

} // namespace Libraries::Videodec