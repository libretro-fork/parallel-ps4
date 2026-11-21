// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/elf_info.h"
#include "common/io_file.h"
#include "common/logging/log.h"
#include "common/path_util.h"
#include "core/emulator_settings.h"
#include "core/file_sys/fs.h"
#include "core/guest_call.h"
#include "core/libraries/audiodec/audiodec.h"
#include "core/libraries/avplayer/avplayer.h"
#include "core/libraries/disc_map/disc_map.h"
#include "core/libraries/font/font.h"
#include "core/libraries/font/fontft.h"
#include "core/libraries/jpeg/jpegdec.h"
#include "core/libraries/jpeg/jpegenc.h"
#include "core/libraries/kernel/kernel.h"
#include "core/libraries/kernel/orbis_error.h"
#include "core/libraries/kernel/threads.h"
#include "core/libraries/kernel/threads/pthread.h"
#include "core/libraries/libc_internal/libc_internal.h"
#include "core/libraries/libpng/pngdec.h"
#include "core/libraries/libpng/pngenc.h"
#include "core/libraries/libs.h"
#include "core/libraries/ngs2/ngs2.h"
#include "core/libraries/rtc/rtc.h"
#include "core/libraries/rudp/rudp.h"
#include "core/libraries/sysmodule/sysmodule_error.h"
#include "core/libraries/sysmodule/sysmodule_internal.h"
#include "core/libraries/sysmodule/sysmodule_table.h"
#include "core/libraries/system_gesture/system_gesture.h"
#include "core/libraries/ult/ult.h"
#include "core/linker.h"
#include "emulator.h"

namespace Libraries::Kernel {
void PS4_SYSV_ABI sched_yield();
}

namespace Libraries::SysModule {

bool g_need_scelibc = false, g_need_fios2 = false;

// libSceUlt is C (ult.c): the game's threads it needs, and its functions given to the game

static int UltThreadCreate(void** thread, void* arg, const char* name) {
    Kernel::PthreadT created{};
    if (Kernel::posix_pthread_create_name_np(&created, nullptr, ult_run_ulthread, arg, name) != 0) {
        return 0;
    }
    *thread = created;
    return 1;
}

static void UltThreadJoin(void* thread) {
    Kernel::posix_pthread_join(static_cast<Kernel::PthreadT>(thread), nullptr);
}

static void UltThreadExit() {
    Kernel::posix_pthread_exit(nullptr);
}

static void UltYield() {
    Kernel::sched_yield();
}

static thread_local void* ult_self = nullptr;

static void* UltSelfGet() {
    return ult_self;
}

static void UltSelfSet(void* value) {
    ult_self = value;
}

static s32 UltCallEntry(ult_entry entry, u64 arg) {
    return Core::CallGuest(reinterpret_cast<s32 PS4_SYSV_ABI (*)(u64)>(entry), arg);
}

static void RegisterJpegDec(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("uNAUmANZMEw", "libSceJpegDec", 1, "libSceJpegDec", sceJpegDecQueryMemorySize);
    LIB_FUNCTION("JPh3Zgg0Zwc", "libSceJpegDec", 1, "libSceJpegDec", sceJpegDecCreate);
    LIB_FUNCTION("Hwh11+m5KoI", "libSceJpegDec", 1, "libSceJpegDec", sceJpegDecDelete);
    LIB_FUNCTION("1kzQRoWEgSA", "libSceJpegDec", 1, "libSceJpegDec", sceJpegDecDecode);
    LIB_FUNCTION("LSinoSQH790", "libSceJpegDec", 1, "libSceJpegDec", sceJpegDecParseHeader);
    LIB_FUNCTION("919MhccOiII", "libSceJpegDec_jvm", 1, "libSceJpegDec",
                 sceJpegDecDecodeWithInputControl);
}

static void RegisterJpegEnc(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("K+rocojkr-I", "libSceJpegEnc", 1, "libSceJpegEnc", sceJpegEncCreate);
    LIB_FUNCTION("j1LyMdaM+C0", "libSceJpegEnc", 1, "libSceJpegEnc", sceJpegEncDelete);
    LIB_FUNCTION("QbrU0cUghEM", "libSceJpegEnc", 1, "libSceJpegEnc", sceJpegEncEncode);
    LIB_FUNCTION("o6ZgXfFdWXQ", "libSceJpegEnc", 1, "libSceJpegEnc", sceJpegEncQueryMemorySize);
}

static void RegisterSystemGesture(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("1MMK0W-kMgA", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureAppendTouchRecognizer);
    LIB_FUNCTION("j4yXIA2jJ68", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureClose);
    LIB_FUNCTION("FWF8zkhr854", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureCreateTouchRecognizer);
    LIB_FUNCTION("3QYCmMlOlCY", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureFinalizePrimitiveTouchRecognizer);
    LIB_FUNCTION("KAeP0+cQPVU", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureGetPrimitiveTouchEventByIndex);
    LIB_FUNCTION("yBaQ0h9m1NM", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureGetPrimitiveTouchEventByPrimitiveID);
    LIB_FUNCTION("L8YmemOeSNY", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureGetPrimitiveTouchEvents);
    LIB_FUNCTION("JhwByySf9FY", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureGetPrimitiveTouchEventsCount);
    LIB_FUNCTION("lpsXm7tzeoc", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureGetTouchEventByEventID);
    LIB_FUNCTION("TSKvgSz5ChU", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureGetTouchEventByIndex);
    LIB_FUNCTION("fLTseA7XiWY", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureGetTouchEvents);
    LIB_FUNCTION("h8uongcBNVs", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureGetTouchEventsCount);
    LIB_FUNCTION("0KrW5eMnrwY", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureGetTouchRecognizerInformation);
    LIB_FUNCTION("3pcAvmwKCvM", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureInitializePrimitiveTouchRecognizer);
    LIB_FUNCTION("qpo-mEOwje0", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureOpen);
    LIB_FUNCTION("ELvBVG-LKT0", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureRemoveTouchRecognizer);
    LIB_FUNCTION("o11J529VaAE", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureResetPrimitiveTouchRecognizer);
    LIB_FUNCTION("oBuH3zFWYIg", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureResetTouchRecognizer);
    LIB_FUNCTION("wPJGwI2RM2I", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureUpdateAllTouchRecognizer);
    LIB_FUNCTION("GgFMb22sbbI", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureUpdatePrimitiveTouchRecognizer);
    LIB_FUNCTION("j4h82CQWENo", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureUpdateTouchRecognizer);
    LIB_FUNCTION("4WOA1eTx3V8", "libSceSystemGesture", 1, "libSceSystemGesture",
                 sceSystemGestureUpdateTouchRecognizerRectangle);
}

static void RegisterPngDec(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("m0uW+8pFyaw", "libScePngDec", 1, "libScePngDec", scePngDecCreate);
    LIB_FUNCTION("WC216DD3El4", "libScePngDec", 1, "libScePngDec", scePngDecDecode);
    LIB_FUNCTION("cJ--1xAbj-I", "libScePngDec", 1, "libScePngDec", scePngDecDecodeWithInputControl);
    LIB_FUNCTION("QbD+eENEwo8", "libScePngDec", 1, "libScePngDec", scePngDecDelete);
    LIB_FUNCTION("U6h4e5JRPaQ", "libScePngDec", 1, "libScePngDec", scePngDecParseHeader);
    LIB_FUNCTION("-6srIGbLTIU", "libScePngDec", 1, "libScePngDec", scePngDecQueryMemorySize);
    LIB_FUNCTION("cJ--1xAbj-I", "libScePngDec_jvm", 1, "libScePngDec",
                 scePngDecDecodeWithInputControl);
}

static void RegisterPngEnc(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("7aGTPfrqT9s", "libScePngEnc", 1, "libScePngEnc", scePngEncCreate);
    LIB_FUNCTION("RUrWdwTWZy8", "libScePngEnc", 1, "libScePngEnc", scePngEncDelete);
    LIB_FUNCTION("xgDjJKpcyHo", "libScePngEnc", 1, "libScePngEnc", scePngEncEncode);
    LIB_FUNCTION("9030RnBDoh4", "libScePngEnc", 1, "libScePngEnc", scePngEncQueryMemorySize);
}

static void RegisterRudp(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("uQiK7fjU6y8", "libSceRudp", 1, "libSceRudp", sceRudpAccept);
    LIB_FUNCTION("J-6d0WTjzMc", "libSceRudp", 1, "libSceRudp", sceRudpActivate);
    LIB_FUNCTION("l4SLBpKUDK4", "libSceRudp", 1, "libSceRudp", sceRudpBind);
    LIB_FUNCTION("CAbbX6BuQZ0", "libSceRudp", 1, "libSceRudp", sceRudpCreateContext);
    LIB_FUNCTION("6PBNpsgyaxw", "libSceRudp", 1, "libSceRudp", sceRudpEnableInternalIOThread);
    LIB_FUNCTION("fJ51weR1WAI", "libSceRudp", 1, "libSceRudp", sceRudpEnableInternalIOThread2);
    LIB_FUNCTION("3hBvwqEwqj8", "libSceRudp", 1, "libSceRudp", sceRudpEnd);
    LIB_FUNCTION("Ms0cLK8sTtE", "libSceRudp", 1, "libSceRudp", sceRudpFlush);
    LIB_FUNCTION("wIJsiqY+BMk", "libSceRudp", 1, "libSceRudp", sceRudpGetContextStatus);
    LIB_FUNCTION("2G7-vVz9SIg", "libSceRudp", 1, "libSceRudp", sceRudpGetLocalInfo);
    LIB_FUNCTION("vfrL8gPlm2Y", "libSceRudp", 1, "libSceRudp", sceRudpGetMaxSegmentSize);
    LIB_FUNCTION("Px0miD2LuW0", "libSceRudp", 1, "libSceRudp", sceRudpGetNumberOfPacketsToRead);
    LIB_FUNCTION("mCQIhSmCP6o", "libSceRudp", 1, "libSceRudp", sceRudpGetOption);
    LIB_FUNCTION("Qignjmfgha0", "libSceRudp", 1, "libSceRudp", sceRudpGetRemoteInfo);
    LIB_FUNCTION("sAZqO2+5Qqo", "libSceRudp", 1, "libSceRudp", sceRudpGetSizeReadable);
    LIB_FUNCTION("fRc1ahQppR4", "libSceRudp", 1, "libSceRudp", sceRudpGetSizeWritable);
    LIB_FUNCTION("i3STzxuwPx0", "libSceRudp", 1, "libSceRudp", sceRudpGetStatus);
    LIB_FUNCTION("amuBfI-AQc4", "libSceRudp", 1, "libSceRudp", sceRudpInit);
    LIB_FUNCTION("szEVu+edXV4", "libSceRudp", 1, "libSceRudp", sceRudpInitiate);
    LIB_FUNCTION("tYVWcWDnctE", "libSceRudp", 1, "libSceRudp", sceRudpListen);
    LIB_FUNCTION("+BJ9svDmjYs", "libSceRudp", 1, "libSceRudp", sceRudpNetFlush);
    LIB_FUNCTION("vPzJldDSxXc", "libSceRudp", 1, "libSceRudp", sceRudpNetReceived);
    LIB_FUNCTION("yzeXuww-UWg", "libSceRudp", 1, "libSceRudp", sceRudpPollCancel);
    LIB_FUNCTION("haMpc7TFx0A", "libSceRudp", 1, "libSceRudp", sceRudpPollControl);
    LIB_FUNCTION("MVbmLASjn5M", "libSceRudp", 1, "libSceRudp", sceRudpPollCreate);
    LIB_FUNCTION("LjwbHpEeW0A", "libSceRudp", 1, "libSceRudp", sceRudpPollDestroy);
    LIB_FUNCTION("M6ggviwXpLs", "libSceRudp", 1, "libSceRudp", sceRudpPollWait);
    LIB_FUNCTION("9U9m1YH0ScQ", "libSceRudp", 1, "libSceRudp", sceRudpProcessEvents);
    LIB_FUNCTION("rZqWV3eXgOA", "libSceRudp", 1, "libSceRudp", sceRudpRead);
    LIB_FUNCTION("SUEVes8gvmw", "libSceRudp", 1, "libSceRudp", sceRudpSetEventHandler);
    LIB_FUNCTION("beAsSTVWVPQ", "libSceRudp", 1, "libSceRudp", sceRudpSetMaxSegmentSize);
    LIB_FUNCTION("0yzYdZf0IwE", "libSceRudp", 1, "libSceRudp", sceRudpSetOption);
    LIB_FUNCTION("OMYRTU0uc4w", "libSceRudp", 1, "libSceRudp", sceRudpTerminate);
    LIB_FUNCTION("KaPL3fbTLCA", "libSceRudp", 1, "libSceRudp", sceRudpWrite);
}

static void RegisterUlt(Core::Loader::SymbolsResolver* sym) {
    static const ult_host host = {UltThreadCreate, UltThreadJoin, UltThreadExit, UltYield,
                                  UltSelfGet,      UltSelfSet,    UltCallEntry};
    ult_set_host(&host);

    LIB_FUNCTION("hZIg1EWGsHM", "libSceUlt", 1, "libSceUlt", sceUltInitialize);
    LIB_FUNCTION("d-kSG2fLrvI", "libSceUlt", 1, "libSceUlt", sceUltFinalize);
    LIB_FUNCTION("LuLTRt0rfTw", "libSceUlt", 1, "libSceUlt",
                 _sceUltOptParamInitialize); // _sceUltWaitingQueueResourcePoolOptParamInitialize
    LIB_FUNCTION("WIWV1Qd7PFU", "libSceUlt", 1, "libSceUlt",
                 sceUltWaitingQueueResourcePoolGetWorkAreaSize);
    LIB_FUNCTION("YiHujOG9vXY", "libSceUlt", 1, "libSceUlt", _sceUltWaitingQueueResourcePoolCreate);
    LIB_FUNCTION("or55417wcDk", "libSceUlt", 1, "libSceUlt", sceUltWaitingQueueResourcePoolDestroy);
    LIB_FUNCTION("6gYjd50q0CE", "libSceUlt", 1, "libSceUlt",
                 _sceUltOptParamInitialize); // _sceUltQueueDataResourcePoolOptParamInitialize
    LIB_FUNCTION("evj9YPkS8s4", "libSceUlt", 1, "libSceUlt",
                 sceUltQueueDataResourcePoolGetWorkAreaSize);
    LIB_FUNCTION("TFHm6-N6vks", "libSceUlt", 1, "libSceUlt", _sceUltQueueDataResourcePoolCreate);
    LIB_FUNCTION("dh11uAUWNyM", "libSceUlt", 1, "libSceUlt", sceUltQueueDataResourcePoolDestroy);
    LIB_FUNCTION("TkASc9I-xX0", "libSceUlt", 1, "libSceUlt",
                 _sceUltOptParamInitialize); // _sceUltQueueOptParamInitialize
    LIB_FUNCTION("9Y5keOvb6ok", "libSceUlt", 1, "libSceUlt", _sceUltQueueCreate);
    LIB_FUNCTION("PP9nZxpSKLY", "libSceUlt", 1, "libSceUlt", sceUltQueueDestroy);
    LIB_FUNCTION("dUwpX3e5NDE", "libSceUlt", 1, "libSceUlt", sceUltQueuePush);
    LIB_FUNCTION("6Mc2Xs7pI1I", "libSceUlt", 1, "libSceUlt", sceUltQueueTryPush);
    LIB_FUNCTION("RVSq2tsm2yw", "libSceUlt", 1, "libSceUlt", sceUltQueuePop);
    LIB_FUNCTION("uZz3ci7XYqc", "libSceUlt", 1, "libSceUlt", sceUltQueueTryPop);
    LIB_FUNCTION("V2u3WLrwh64", "libSceUlt", 1, "libSceUlt",
                 _sceUltUlthreadRuntimeOptParamInitialize);
    LIB_FUNCTION("grs2pbc2awM", "libSceUlt", 1, "libSceUlt", sceUltUlthreadRuntimeGetWorkAreaSize);
    LIB_FUNCTION("jw9FkZBXo-g", "libSceUlt", 1, "libSceUlt", _sceUltUlthreadRuntimeCreate);
    LIB_FUNCTION("-gxcs521SvA", "libSceUlt", 1, "libSceUlt", sceUltUlthreadRuntimeDestroy);
    LIB_FUNCTION("vIuen2qlRmo", "libSceUlt", 1, "libSceUlt",
                 _sceUltOptParamInitialize); // _sceUltUlthreadOptParamInitialize
    LIB_FUNCTION("znI3q8S7KQ4", "libSceUlt", 1, "libSceUlt", _sceUltUlthreadCreate);
    LIB_FUNCTION("gCeAI57LGgI", "libSceUlt", 1, "libSceUlt", sceUltUlthreadJoin);
    LIB_FUNCTION("DsW+3FTXL0Q", "libSceUlt", 1, "libSceUlt", sceUltUlthreadTryJoin);
    LIB_FUNCTION("kF--s3xZjco", "libSceUlt", 1, "libSceUlt", sceUltUlthreadExit);
    LIB_FUNCTION("HFd-lpjGxJA", "libSceUlt", 1, "libSceUlt", sceUltUlthreadYield);
    LIB_FUNCTION("xOQ9oACobA4", "libSceUlt", 1, "libSceUlt", sceUltUlthreadGetSelf);
    LIB_FUNCTION("1+8t9aHLiz8", "libSceUlt", 1, "libSceUlt",
                 _sceUltOptParamInitialize); // _sceUltMutexOptParamInitialize
    LIB_FUNCTION("mmt8Sa6tL6c", "libSceUlt", 1, "libSceUlt", _sceUltMutexCreate);
    LIB_FUNCTION("jW+HnafeS3Y", "libSceUlt", 1, "libSceUlt", sceUltMutexDestroy);
    LIB_FUNCTION("8hEGkR1pfr8", "libSceUlt", 1, "libSceUlt", sceUltMutexLock);
    LIB_FUNCTION("jOsUG0BJI-Y", "libSceUlt", 1, "libSceUlt", sceUltMutexTryLock);
    LIB_FUNCTION("h0XebKiMBtk", "libSceUlt", 1, "libSceUlt", sceUltMutexUnlock);
    LIB_FUNCTION("NPRRPNKDBN0", "libSceUlt", 1, "libSceUlt",
                 _sceUltOptParamInitialize); // _sceUltSemaphoreOptParamInitialize
    LIB_FUNCTION("h5QlIYj+Ro8", "libSceUlt", 1, "libSceUlt", _sceUltSemaphoreCreate);
    LIB_FUNCTION("izXyehpoZGo", "libSceUlt", 1, "libSceUlt", sceUltSemaphoreDestroy);
    LIB_FUNCTION("QAH1ofI97vU", "libSceUlt", 1, "libSceUlt", sceUltSemaphoreAcquire);
    LIB_FUNCTION("HA1Ldbi3lPY", "libSceUlt", 1, "libSceUlt", sceUltSemaphoreTryAcquire);
    LIB_FUNCTION("lbtk5X1mecw", "libSceUlt", 1, "libSceUlt", sceUltSemaphoreRelease);
    LIB_FUNCTION("RVmEia0vXMI", "libSceUlt", 1, "libSceUlt",
                 _sceUltOptParamInitialize); // _sceUltConditionVariableOptParamInitialize
    LIB_FUNCTION("jnKaHGkrxZ4", "libSceUlt", 1, "libSceUlt", _sceUltConditionVariableCreate);
    LIB_FUNCTION("xrmmI832R4U", "libSceUlt", 1, "libSceUlt", sceUltConditionVariableDestroy);
    LIB_FUNCTION("5xGAHCxA8M0", "libSceUlt", 1, "libSceUlt", sceUltConditionVariableWait);
    LIB_FUNCTION("JTw1cAVkuc0", "libSceUlt", 1, "libSceUlt", sceUltConditionVariableSignal);
    LIB_FUNCTION("byiceqcMvV0", "libSceUlt", 1, "libSceUlt", sceUltConditionVariableSignalAll);
}

// libSceAudiodec is C (audiodec.c): its functions are given to the game here
static void RegisterAudiodec(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("VjhsmxpcezI", "libSceAudiodec", 1, "libSceAudiodec", sceAudiodecInitLibrary);
    LIB_FUNCTION("h5jSB2QIDV0", "libSceAudiodec", 1, "libSceAudiodec", sceAudiodecTermLibrary);
    LIB_FUNCTION("O3f1sLMWRvs", "libSceAudiodec", 1, "libSceAudiodec", sceAudiodecCreateDecoder);
    LIB_FUNCTION("Tp+ZEy69mLk", "libSceAudiodec", 1, "libSceAudiodec", sceAudiodecDeleteDecoder);
    LIB_FUNCTION("6Vf9WTLDoss", "libSceAudiodec", 1, "libSceAudiodec", sceAudiodecClearContext);
    LIB_FUNCTION("KHXHMDLkILw", "libSceAudiodec", 1, "libSceAudiodec", sceAudiodecDecode);
}

s32 getModuleHandle(s32 id, s32* handle) {
    if (id == 0) {
        return ORBIS_SYSMODULE_INVALID_ID;
    }
    for (OrbisSysmoduleModuleInternal mod : g_modules_array) {
        if (mod.id != id) {
            continue;
        }
        if (mod.is_loaded < 1) {
            return ORBIS_SYSMODULE_NOT_LOADED;
        }
        if (handle != nullptr) {
            *handle = mod.handle;
        }
        return ORBIS_OK;
    }
    return ORBIS_SYSMODULE_INVALID_ID;
}

bool shouldHideName(const char* module_name) {
    for (u64 i = 0; i < g_num_modules; i++) {
        OrbisSysmoduleModuleInternal mod = g_modules_array[i];
        if ((mod.flags & OrbisSysmoduleModuleInternalFlags::IsGame) == 0) {
            continue;
        }
        u64 name_length = std::strlen(mod.name);
        char name_copy[0x100];
        std::strncpy(name_copy, mod.name, sizeof(name_copy));
        // Module table stores names without extensions, so check with .prx appended to the name.
        std::strncpy(&name_copy[name_length], ".prx", 4);
        s32 result = std::strncmp(module_name, name_copy, sizeof(name_copy));
        if (result == 0) {
            return true;
        }

        // libSceFios2 and libc are checked as both sprx or prx modules.
        if (i == 3) {
            result = std::strncmp(module_name, "libSceFios2.sprx", sizeof(name_copy));
        } else if (i == 4) {
            result = std::strncmp(module_name, "libc.sprx", sizeof(name_copy));
        }

        if (result == 0) {
            return true;
        }
    }
    return false;
}

bool isDebugModule(s32 id) {
    for (OrbisSysmoduleModuleInternal mod : g_modules_array) {
        if (mod.id == id && (mod.flags & OrbisSysmoduleModuleInternalFlags::IsDebug) != 0) {
            return true;
        }
    }
    return false;
}

u32 validateModuleId(s32 id) {
    if ((id & 0x7fffffff) == 0) {
        return ORBIS_SYSMODULE_INVALID_ID;
    }

    s32 sdk_ver = 0;
    ASSERT_MSG(!Kernel::sceKernelGetCompiledSdkVersion(&sdk_ver),
               "Failed to retrieve compiled SDK version");

    // libSceGameCustomDialog isn't loadable on SDK >= 7.50
    if (id == 0xb8 && sdk_ver >= Common::ElfInfo::FW_750) {
        return ORBIS_SYSMODULE_INVALID_ID;
    }

    // libSceNpSnsFacebookDialog isn't loadable on SDK >= 7.00
    if (id == 0xb0 && sdk_ver >= Common::ElfInfo::FW_700) {
        return ORBIS_SYSMODULE_INVALID_ID;
    }

    // libSceJson isn't loadable on SDK >= 3.00
    if (id == 0x80 && sdk_ver >= Common::ElfInfo::FW_300) {
        return ORBIS_SYSMODULE_INVALID_ID;
    }

    // Cannot load debug modules on retail hardware.
    if (isDebugModule(id) && !emu_settings_get_dev_kit_mode()) {
        return ORBIS_SYSMODULE_INVALID_ID;
    }

    return ORBIS_OK;
}

s32 loadModuleInternal(s32 index, s32 argc, const void* argv, s32* res_out) {
    auto* linker = Common::Singleton<Core::Linker>::Instance();
    auto* game_info = Common::Singleton<Common::ElfInfo>::Instance();

    // If the module is already loaded, increment is_loaded and return ORBIS_OK.
    OrbisSysmoduleModuleInternal& mod = g_modules_array[index];
    if (mod.is_loaded > 0) {
        mod.is_loaded++;
        return ORBIS_OK;
    }

    s32 start_result = 0;
    // Most of the logic the actual module has here is to get the correct location of this module.
    // Since we only care about a small subset of LLEs, we can simplify this logic.
    if ((mod.flags & OrbisSysmoduleModuleInternalFlags::IsGame) != 0) {
        std::string guest_path = std::string("/app0/sce_module/").append(mod.name);
        guest_path.append(".prx");
        s32 result = linker->LoadAndStartModule(guest_path, argc, argv, &start_result);
        // If the module is missing, the library prints a very helpful message for developers.
        // We'll just log an error.
        if (result < 0) {
            LOG_ERROR(Lib_SysModule, "Failed to load game library %s", (guest_path).c_str());
            return result;
        } else {
            // On success, the library validates module params and the module SDK version.
            // We don't store the information this uses, so skip the proper checks.
            mod.handle = result;
            mod.is_loaded++;
        }
    } else {
        // This is not a game library. We'll need to perform some checks,
        // but we don't need to perform the path resolution logic the actual library has.
        std::string mod_name = std::string(mod.name);

        // libSceGnmDriver case
        if (index == 0xd && emu_settings_get_dev_kit_mode()) {
            // There are some other checks involved here that I am not familiar with.
            // Since we're not exactly running libSceGnmDriver LLE, this shouldn't matter too much.
            mod_name.append("_padebug");
        }

        // libSceSsl2 case
        if (index == 0x27 && false /*needs legacy ssl*/) {
            // Replaces module name with libSceSsl (index 0x15)
            mod_name.clear();
            mod_name.append(g_modules_array[0x15].name);
        }

        // libSceVrTracker case
        if (index == 0xb3 && emu_settings_get_dev_kit_mode()) {
            mod_name.append("_debug");
        }

        if ((mod.flags & OrbisSysmoduleModuleInternalFlags::IsNeo) == 0 &&
            (mod.flags & OrbisSysmoduleModuleInternalFlags::IsNeoMode) != 0 &&
            Kernel::sceKernelIsNeoMode() == 1) {
            // PS4 Pro running in enhanced mode
            mod_name.append("ForNeoMode");
        } else if ((mod.flags & OrbisSysmoduleModuleInternalFlags::IsNeo) != 0 &&
                   emu_settings_get_neo_mode()) {
            // PS4 Pro running in base mode
            mod_name.append("ForNeo");
        }

        // Append .sprx extension.
        mod_name.append(".sprx");

        // Now we need to check if the requested library is allowed to LLE.
        // First, we allow all modules from game-specific sys_modules
        const auto& sys_module_path = Common::FS::PathFromCString(emu_settings_get_sys_modules_dir);
        const char* serial = game_info->GameSerial();
        if (serial[0]) {
            const auto& game_specific_module_path = sys_module_path / serial / mod_name;
            if (Common::FS::Exists(game_specific_module_path)) {
                // The requested module is present in the game-specific sys_modules, load it.
                LOG_INFO(Loader, "Loading %s from game serial file %s", (mod_name).c_str(), serial);
                s32 handle = linker->LoadAndStartModule(game_specific_module_path, argc, argv,
                                                        &start_result);
                ASSERT_MSG(handle >= 0, "Failed to load module %s", (mod_name).c_str());
                mod.handle = handle;
                mod.is_loaded++;
                if (res_out != nullptr) {
                    *res_out = start_result;
                }
                return ORBIS_OK;
            }
        }

        // We need to check a few things here.
        // First, check if this is a module we allow LLE for.
        static s32 stub_handle = 100;
        constexpr auto ModulesToLoad = std::to_array<Core::SysModules>(
            {{"libSceNgs2.sprx", &Libraries::Ngs2::RegisterLib},
             {"libSceUlt.sprx", &RegisterUlt},
             {"libSceAvPlayer.sprx", &Libraries::AvPlayer::RegisterLib},
             {"libSceAvPlayerStreaming.sprx", nullptr},
             {"libSceRtc.sprx", &Libraries::Rtc::RegisterLib},
             {"libSceJpegDec.sprx", &RegisterJpegDec},
             {"libSceJpegEnc.sprx", &RegisterJpegEnc},
             {"libScePngDec.sprx", &RegisterPngDec},
             {"libScePngEnc.sprx", &RegisterPngEnc},
             {"libSceJson.sprx", nullptr},
             {"libSceJson2.sprx", nullptr},
             {"libSceCesCs.sprx", nullptr},
             {"libSceAt9Enc.sprx", nullptr},
             {"libSceAudiodec.sprx", &RegisterAudiodec},
             {"libSceAudiodecCpu.sprx", nullptr},
             {"libSceAudiodecCpuDdp.sprx", nullptr},
             {"libSceAudiodecCpuM4aac.sprx", nullptr},
             {"libSceAudiodecCpuDtsHdLbr.sprx", nullptr},
             {"libSceAudiodecCpuHevag.sprx", nullptr},
             {"libSceFont.sprx", &Libraries::Font::RegisterLib},
             {"libSceFontFt.sprx", &Libraries::FontFt::RegisterLib},
             {"libSceFreeTypeOt.sprx", nullptr},
             {"libSceFreeTypeOl.sprx", nullptr},
             {"libSceFreeTypeOptOl.sprx", nullptr},
             {"libSceBeisobmf.sprx", nullptr},
             {"libSceBemp2sys.sprx", nullptr},
             {"libSceRudp.sprx", &RegisterRudp},
             {"libSceWkFontConfig.sprx", nullptr},
             {"libScePsmKitSystem.sprx", nullptr},
             {"libSceDepth.sprx", nullptr},
             {"libScePadTracker.sprx", nullptr},
             {"libSceMoveTracker.sprx", nullptr},
             {"libSceSystemGesture.sprx", &RegisterSystemGesture},
             {"libSceXml.sprx", nullptr}});

        // Iterate through the allowed array
        const auto it = std::ranges::find_if(
            ModulesToLoad, [&](Core::SysModules module) { return mod_name == module.module_name; });
        if (it == ModulesToLoad.end()) {
            // Not an allowed LLE, stub success without warning.
            mod.is_loaded++;
            // Some internal checks rely on a handle, stub a valid one.
            mod.handle = stub_handle++;
            if (res_out != nullptr) {
                *res_out = ORBIS_OK;
            }
            return ORBIS_OK;
        }

        // Allowed module, check if it exists
        const auto& module_path = sys_module_path / mod_name;
        if (Common::FS::Exists(module_path)) {
            LOG_INFO(Loader, "Loading %s", (mod_name).c_str());
            s32 handle = linker->LoadAndStartModule(module_path, argc, argv, &start_result);
            ASSERT_MSG(handle >= 0, "Failed to load module %s", (mod_name).c_str());
            mod.handle = handle;
        } else {
            // Allowed LLE that isn't present, log message
            auto& [name, init_func] = *it;
            if (init_func) {
                LOG_INFO(Loader, "Can't Load %s switching to HLE", (mod_name).c_str());
                init_func(&linker->GetHLESymbols());

                // When loading HLEs, we need to relocate imports
                // This ensures later module loads can see our HLE functions.
                linker->RelocateAllImports();
            } else {
                LOG_INFO(Loader, "No HLE available for %s module", (mod_name).c_str());
            }
            mod.handle = stub_handle++;
        }

        // Mark module as loaded.
        mod.is_loaded++;
    }

    // Only successful loads will reach here
    if (res_out != nullptr) {
        *res_out = start_result;
    }

    return ORBIS_OK;
}

s32 loadModule(s32 id, s32 argc, const void* argv, s32* res_out) {
    // Retrieve the module to load from the table
    OrbisSysmoduleModuleInternal requested_module{};
    for (OrbisSysmoduleModuleInternal mod : g_modules_array) {
        if (mod.id == id) {
            requested_module = mod;
            break;
        }
    }
    if (requested_module.id != id || requested_module.id == 0) {
        return ORBIS_SYSMODULE_INVALID_ID;
    }

    // Every module has a pointer to an array of indexes to modules that need loading.
    if (requested_module.to_load == nullptr) {
        // Seems like ORBIS_SYSMODULE_LOCK_FAILED is a generic internal error.
        return ORBIS_SYSMODULE_LOCK_FAILED;
    }

    LOG_INFO(Lib_SysModule, "Loading %s", requested_module.name);

    // Loop through every module that requires loading, in reverse order
    for (s64 i = requested_module.num_to_load - 1; i >= 0; i--) {
        // Modules flagged as debug modules only load for devkits
        u32 mod_index = requested_module.to_load[i];
        if ((!emu_settings_get_dev_kit_mode() &&
             g_modules_array[mod_index].flags & OrbisSysmoduleModuleInternalFlags::IsDebug) != 0) {
            continue;
        }

        // Arguments and result should only be applied to the requested module
        // Dependencies don't receive these values.
        s32 result = 0;
        if (i != 0) {
            result = loadModuleInternal(mod_index, 0, nullptr, nullptr);
        } else {
            result = loadModuleInternal(mod_index, argc, argv, res_out);
        }

        // If loading any module fails, abort there.
        if (result != ORBIS_OK) {
            return result;
        }
    }
    return ORBIS_OK;
}

s32 unloadModule(s32 id, s32 argc, const void* argv, s32* res_out, bool is_internal) {
    OrbisSysmoduleModuleInternal mod{};
    for (s32 i = 0; i < g_modules_array.size(); i++) {
        mod = g_modules_array[i];
        if (mod.id != id) {
            continue;
        }

        // Skips checking libSceDiscMap
        if (i == 0x22) {
            continue;
        }

        // If the module is loaded once, and is part of the second preload list,
        // then return OK and do nothing.
        for (s32 index : g_preload_list_2) {
            if (index == i && mod.is_loaded == 1) {
                return ORBIS_OK;
            }
        }

        // Found the correct module.
        break;
    }

    // If we failed to locate the module, return invalid id.
    if (mod.id != id || mod.id == 0) {
        return ORBIS_SYSMODULE_INVALID_ID;
    }

    // If the module has no dependencies, then return an internal error.
    if (mod.num_to_load == 0 || mod.to_load == nullptr) {
        return ORBIS_SYSMODULE_LOCK_FAILED;
    }

    // Unload the module and it's dependencies
    for (s64 i = 0; i < mod.num_to_load; i++) {
        OrbisSysmoduleModuleInternal dep_mod = g_modules_array[mod.to_load[i]];
        // If this is a debug module and we're not emulating a devkit, skip it.
        if ((dep_mod.flags & OrbisSysmoduleModuleInternalFlags::IsDebug) != 0 &&
            !emu_settings_get_dev_kit_mode()) {
            continue;
        }

        // If the module to unload is marked as unloaded, then return not loaded
        if (dep_mod.is_loaded == 0) {
            return ORBIS_SYSMODULE_NOT_LOADED;
        }

        // By this point, all necessary checks are performed, decrement the load count.
        dep_mod.is_loaded--;

        // Normally, this is where the real library would actually unload the module,
        // through a call to sceKernelStopUnloadModule.
        // As we don't implement module unloading, this behavior is skipped.

        // Stub success during requested module unload.
        if (i == 0 && res_out != nullptr) {
            *res_out = ORBIS_OK;
        }
    }
    return ORBIS_OK;
}

s32 preloadModulesForLibkernel() {
    // For now, default to loading g_preload_list_3.
    // As far as I can tell, g_preload_list_1 seems to be some sort of list with libs
    // that games don't typically use, and g_preload_list_2 is just a reorganized version of 3.
    s32 sdk_ver = 0;
    ASSERT_MSG(Kernel::sceKernelGetCompiledSdkVersion(&sdk_ver) == 0,
               "Failed to get compiled SDK version");
    for (u32 module_index : g_preload_list_3) {
        // As per usual, these are arrays of indexes for g_modules_array
        // libSceDbg, libScePerf, libSceMat, and libSceRazorCpu_debug.
        // These are skipped unless this console is a devkit.
        if ((module_index == 0x12 || module_index == 0x1e || module_index == 0x24 ||
             module_index == 0x26) &&
            !emu_settings_get_dev_kit_mode()) {
            continue;
        }

        // libSceDiscMap case, skipped on newer SDK versions.
        if (module_index == 0x22 && sdk_ver >= Common::ElfInfo::FW_200) {
            continue;
        }

        // libSceDbgAssist is skipped on non-testkit consoles.
        // For now, stub check to non-devkit.
        if (module_index == 0x23 && !emu_settings_get_dev_kit_mode()) {
            continue;
        }

        // libSceRazorCpu, skipped for old non-devkit consoles.
        if (module_index == 0x25 && sdk_ver < Common::ElfInfo::FW_450 &&
            !emu_settings_get_dev_kit_mode()) {
            continue;
        }

        // libSceHttp2, skipped for SDK versions below 7.00.
        if (module_index == 0x28 && sdk_ver < Common::ElfInfo::FW_700) {
            continue;
        }

        // libSceNpWebApi2 and libSceNpGameIntent, skipped for SDK versions below 7.50
        if ((module_index == 0x29 || module_index == 0x2a) && sdk_ver < Common::ElfInfo::FW_750) {
            continue;
        }

        if (module_index == 1 || module_index == 2) {
            // libkernel and libSceLibcInternal aren't directly loaded here.
            // All we do for those is increment is_loaded
            g_modules_array[module_index].is_loaded++;
            continue;
        }

        // Load the actual module
        s32 result = loadModuleInternal(module_index, 0, nullptr, nullptr);
        if (result != ORBIS_OK) {
            // On real hardware, module preloading must succeed or the game will abort.
            // To enable users to test homebrew easier, we'll log a critical error instead.
            if (g_modules_array[module_index].name == std::string("libc")) {
                ASSERT_MSG(!g_need_scelibc,
                           "libc.prx cannot be loaded, but the guest attempted to use it. "
                           "This usually indicates a corrupted dump.");
            } else if (g_modules_array[module_index].name == std::string("libSceFios2")) {
                ASSERT_MSG(!g_need_fios2,
                           "libSceFios2.prx cannot be loaded, but the guest attempted to use it. "
                           "This usually indicates a corrupted dump.");
            } else {
                LOG_CRITICAL(Lib_SysModule, "Failed to preload %s, expect crashes",
                             g_modules_array[module_index].name);
            }
        }
    }
    return ORBIS_OK;
}

} // namespace Libraries::SysModule
