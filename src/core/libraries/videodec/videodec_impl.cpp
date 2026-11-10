// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "videodec_impl.h"

#include "common/alignment.h"
#include "common/logging/log.h"
#include "videodec_error.h"

namespace Libraries::Videodec {

VdecDecoder::VdecDecoder(const OrbisVideodecConfigInfo& pCfgInfoIn,
                         const OrbisVideodecResourceInfo& pRsrcInfoIn) {}

VdecDecoder::~VdecDecoder() = default;

s32 VdecDecoder::Decode(const OrbisVideodecInputData& pInputDataIn,
                        OrbisVideodecFrameBuffer& pFrameBufferInOut,
                        OrbisVideodecPictureInfo& pPictureInfoOut) {
    pPictureInfoOut.isValid = false;

    if (!pInputDataIn.pAuData) {
        return ORBIS_VIDEODEC_ERROR_AU_POINTER;
    }
    if (pInputDataIn.auSize == 0) {
        return ORBIS_VIDEODEC_ERROR_AU_SIZE;
    }
    if (!mDecoder.IsOpen()) {
        return ORBIS_VIDEODEC_ERROR_API_FAIL;
    }

    const PictureMeta meta{
        .pts = pInputDataIn.ptsData,
        .dts = pInputDataIn.dtsData,
        .attached = pInputDataIn.attachedData,
    };
    const int ret =
        mDecoder.Decode(static_cast<const u8*>(pInputDataIn.pAuData), pInputDataIn.auSize, meta);
    if (ret < 0) {
        LOG_ERROR(Lib_Videodec, "Error decoding the access unit: {}", ret);
        return ORBIS_VIDEODEC_ERROR_API_FAIL;
    }
    return ret ? Output(pFrameBufferInOut, pPictureInfoOut) : ORBIS_OK;
}

s32 VdecDecoder::Flush(OrbisVideodecFrameBuffer& pFrameBufferInOut,
                       OrbisVideodecPictureInfo& pPictureInfoOut) {
    pPictureInfoOut.isValid = false;
    // One held picture a call: games flush once per picture they want
    if (!mDecoder.IsOpen() || !mDecoder.Drain()) {
        return ORBIS_OK;
    }
    return Output(pFrameBufferInOut, pPictureInfoOut);
}

s32 VdecDecoder::Reset() {
    mDecoder.Reset();
    return ORBIS_OK;
}

s32 VdecDecoder::Output(OrbisVideodecFrameBuffer& pFrameBufferInOut,
                        OrbisVideodecPictureInfo& pPictureInfoOut) {
    if (!mDecoder.WriteNV12(static_cast<u8*>(pFrameBufferInOut.pFrameBuffer),
                            pFrameBufferInOut.frameBufferSize)) {
        LOG_ERROR(Lib_Videodec, "The {}x{} picture does not fit the frame buffer", mDecoder.Width(),
                  mDecoder.Height());
        return ORBIS_VIDEODEC_ERROR_API_FAIL;
    }

    const auto width = Common::AlignUp<u32>(mDecoder.Width(), 16);
    const auto pitch = mDecoder.Pitch();
    const auto height = mDecoder.AlignedHeight();

    pPictureInfoOut.isValid = true;
    pPictureInfoOut.codecType = 0;
    pPictureInfoOut.frameWidth = width;
    pPictureInfoOut.framePitch = pitch;
    pPictureInfoOut.frameHeight = height;
    pPictureInfoOut.isErrorPic = false;
    pPictureInfoOut.ptsData = mDecoder.Meta().pts;
    pPictureInfoOut.attachedData = mDecoder.Meta().attached;

    pPictureInfoOut.codec.avc.colourPrimaries = mDecoder.ColourPrimaries();
    pPictureInfoOut.codec.avc.frameCropLeftOffset = 0;
    pPictureInfoOut.codec.avc.frameCropRightOffset = pitch - mDecoder.Width();
    pPictureInfoOut.codec.avc.frameCropTopOffset = 0;
    pPictureInfoOut.codec.avc.frameCropBottomOffset = height - mDecoder.Height();
    return ORBIS_OK;
}

} // namespace Libraries::Videodec
