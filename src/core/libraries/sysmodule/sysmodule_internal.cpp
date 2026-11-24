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
#include "core/libraries/ngs2/ngs2_lib.h"
#include "core/libraries/rtc/rtc.h"
#include "core/libraries/rudp/rudp.h"
#include "core/libraries/sysmodule/sysmodule_error.h"
#include "core/libraries/sysmodule/sysmodule_internal.h"
#include "core/libraries/system_gesture/system_gesture.h"
#include "core/libraries/ult/ult.h"
#include "core/linker.h"
#include "emulator.h"

namespace Libraries::Kernel {
void PS4_SYSV_ABI sched_yield();
}

namespace Libraries::SysModule {

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

static void RegisterRtc(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("lPEBYdVX0XQ", "libSceRtc", 1, "libSceRtc", sceRtcCheckValid);
    LIB_FUNCTION("fNaZ4DbzHAE", "libSceRtc", 1, "libSceRtc", sceRtcCompareTick);
    LIB_FUNCTION("8Yr143yEnRo", "libSceRtc", 1, "libSceRtc", sceRtcConvertLocalTimeToUtc);
    LIB_FUNCTION("M1TvFst-jrM", "libSceRtc", 1, "libSceRtc", sceRtcConvertUtcToLocalTime);
    LIB_FUNCTION("8SljQx6pDP8", "libSceRtc", 1, "libSceRtc", sceRtcEnd);
    LIB_FUNCTION("eiuobaF-hK4", "libSceRtc", 1, "libSceRtc", sceRtcFormatRFC2822);
    LIB_FUNCTION("AxHBk3eat04", "libSceRtc", 1, "libSceRtc", sceRtcFormatRFC2822LocalTime);
    LIB_FUNCTION("WJ3rqFwymew", "libSceRtc", 1, "libSceRtc", sceRtcFormatRFC3339);
    LIB_FUNCTION("DwuHIlLGW8I", "libSceRtc", 1, "libSceRtc", sceRtcFormatRFC3339LocalTime);
    LIB_FUNCTION("lja0nNPWojg", "libSceRtc", 1, "libSceRtc", sceRtcFormatRFC3339Precise);
    LIB_FUNCTION("tOZ6fwwHZOA", "libSceRtc", 1, "libSceRtc", sceRtcFormatRFC3339PreciseLocalTime);
    LIB_FUNCTION("LN3Zcb72Q0c", "libSceRtc", 1, "libSceRtc", sceRtcGetCurrentAdNetworkTick);
    LIB_FUNCTION("8lfvnRMqwEM", "libSceRtc", 1, "libSceRtc", sceRtcGetCurrentClock);
    LIB_FUNCTION("ZPD1YOKI+Kw", "libSceRtc", 1, "libSceRtc", sceRtcGetCurrentClockLocalTime);
    LIB_FUNCTION("Ot1DE3gif84", "libSceRtc", 1, "libSceRtc", sceRtcGetCurrentDebugNetworkTick);
    LIB_FUNCTION("zO9UL3qIINQ", "libSceRtc", 1, "libSceRtc", sceRtcGetCurrentNetworkTick);
    LIB_FUNCTION("HWxHOdbM-Pg", "libSceRtc", 1, "libSceRtc", sceRtcGetCurrentRawNetworkTick);
    LIB_FUNCTION("18B2NS1y9UU", "libSceRtc", 1, "libSceRtc", sceRtcGetCurrentTick);
    LIB_FUNCTION("CyIK-i4XdgQ", "libSceRtc", 1, "libSceRtc", sceRtcGetDayOfWeek);
    LIB_FUNCTION("3O7Ln8AqJ1o", "libSceRtc", 1, "libSceRtc", sceRtcGetDaysInMonth);
    LIB_FUNCTION("E7AR4o7Ny7E", "libSceRtc", 1, "libSceRtc", sceRtcGetDosTime);
    LIB_FUNCTION("8w-H19ip48I", "libSceRtc", 1, "libSceRtc", sceRtcGetTick);
    LIB_FUNCTION("jMNwqYr4R-k", "libSceRtc", 1, "libSceRtc", sceRtcGetTickResolution);
    LIB_FUNCTION("BtqmpTRXHgk", "libSceRtc", 1, "libSceRtc", sceRtcGetTime_t);
    LIB_FUNCTION("jfRO0uTjtzA", "libSceRtc", 1, "libSceRtc", sceRtcGetWin32FileTime);
    LIB_FUNCTION("LlodCMDbk3o", "libSceRtc", 1, "libSceRtc", sceRtcInit);
    LIB_FUNCTION("Ug8pCwQvh0c", "libSceRtc", 1, "libSceRtc", sceRtcIsLeapYear);
    LIB_FUNCTION("NxEI1KByvCI", "libSceRtc", 1, "libSceRtc", sceRtcParseDateTime);
    LIB_FUNCTION("99bMGglFW3I", "libSceRtc", 1, "libSceRtc", sceRtcParseRFC3339);
    LIB_FUNCTION("fFLgmNUpChg", "libSceRtc", 1, "libSceRtc", sceRtcSetConf);
    LIB_FUNCTION("sV2tK+yOhBU", "libSceRtc", 1, "libSceRtc", sceRtcSetCurrentAdNetworkTick);
    LIB_FUNCTION("VLDUPKmw5L8", "libSceRtc", 1, "libSceRtc", sceRtcSetCurrentDebugNetworkTick);
    LIB_FUNCTION("qhDBtIo+auw", "libSceRtc", 1, "libSceRtc", sceRtcSetCurrentNetworkTick);
    LIB_FUNCTION("d4fHLCGmY80", "libSceRtc", 1, "libSceRtc", sceRtcSetCurrentTick);
    LIB_FUNCTION("aYPCd1cChyg", "libSceRtc", 1, "libSceRtc", sceRtcSetDosTime);
    LIB_FUNCTION("ueega6v3GUw", "libSceRtc", 1, "libSceRtc", sceRtcSetTick);
    LIB_FUNCTION("bDEVVP4bTjQ", "libSceRtc", 1, "libSceRtc", sceRtcSetTime_t);
    LIB_FUNCTION("n5JiAJXsbcs", "libSceRtc", 1, "libSceRtc", sceRtcSetWin32FileTime);
    LIB_FUNCTION("NR1J0N7L2xY", "libSceRtc", 1, "libSceRtc", sceRtcTickAddDays);
    LIB_FUNCTION("MDc5cd8HfCA", "libSceRtc", 1, "libSceRtc", sceRtcTickAddHours);
    LIB_FUNCTION("XPIiw58C+GM", "libSceRtc", 1, "libSceRtc", sceRtcTickAddMicroseconds);
    LIB_FUNCTION("mn-tf4QiFzk", "libSceRtc", 1, "libSceRtc", sceRtcTickAddMinutes);
    LIB_FUNCTION("CL6y9q-XbuQ", "libSceRtc", 1, "libSceRtc", sceRtcTickAddMonths);
    LIB_FUNCTION("07O525HgICs", "libSceRtc", 1, "libSceRtc", sceRtcTickAddSeconds);
    LIB_FUNCTION("AqVMssr52Rc", "libSceRtc", 1, "libSceRtc", sceRtcTickAddTicks);
    LIB_FUNCTION("gI4t194c2W8", "libSceRtc", 1, "libSceRtc", sceRtcTickAddWeeks);
    LIB_FUNCTION("-5y2uJ62qS8", "libSceRtc", 1, "libSceRtc", sceRtcTickAddYears);
}

static void RegisterNgs2(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("3pCNbVM11UA", "libSceNgs2", 1, "libSceNgs2", sceNgs2CalcWaveformBlock);
    LIB_FUNCTION("6qN1zaEZuN0", "libSceNgs2", 1, "libSceNgs2", sceNgs2CustomRackGetModuleInfo);
    LIB_FUNCTION("Kg1MA5j7KFk", "libSceNgs2", 1, "libSceNgs2", sceNgs2FftInit);
    LIB_FUNCTION("D8eCqBxSojA", "libSceNgs2", 1, "libSceNgs2", sceNgs2FftProcess);
    LIB_FUNCTION("-YNfTO6KOMY", "libSceNgs2", 1, "libSceNgs2", sceNgs2FftQuerySize);
    LIB_FUNCTION("eF8yRCC6W64", "libSceNgs2", 1, "libSceNgs2", sceNgs2GeomApply);
    LIB_FUNCTION("1WsleK-MTkE", "libSceNgs2", 1, "libSceNgs2", sceNgs2GeomCalcListener);
    LIB_FUNCTION("7Lcfo8SmpsU", "libSceNgs2", 1, "libSceNgs2", sceNgs2GeomResetListenerParam);
    LIB_FUNCTION("0lbbayqDNoE", "libSceNgs2", 1, "libSceNgs2", sceNgs2GeomResetSourceParam);
    LIB_FUNCTION("ekGJmmoc8j4", "libSceNgs2", 1, "libSceNgs2", sceNgs2GetWaveformFrameInfo);
    LIB_FUNCTION("BcoPfWfpvVI", "libSceNgs2", 1, "libSceNgs2", sceNgs2JobSchedulerResetOption);
    LIB_FUNCTION("EEemGEQCjO8", "libSceNgs2", 1, "libSceNgs2", sceNgs2ModuleArrayEnumItems);
    LIB_FUNCTION("TaoNtmMKkXQ", "libSceNgs2", 1, "libSceNgs2", sceNgs2ModuleEnumConfigs);
    LIB_FUNCTION("ve6bZi+1sYQ", "libSceNgs2", 1, "libSceNgs2", sceNgs2ModuleQueueEnumItems);
    LIB_FUNCTION("gbMKV+8Enuo", "libSceNgs2", 1, "libSceNgs2", sceNgs2PanGetVolumeMatrix);
    LIB_FUNCTION("xa8oL9dmXkM", "libSceNgs2", 1, "libSceNgs2", sceNgs2PanInit);
    LIB_FUNCTION("hyVLT2VlOYk", "libSceNgs2", 1, "libSceNgs2", sceNgs2ParseWaveformData);
    LIB_FUNCTION("iprCTXPVWMI", "libSceNgs2", 1, "libSceNgs2", sceNgs2ParseWaveformFile);
    LIB_FUNCTION("t9T0QM17Kvo", "libSceNgs2", 1, "libSceNgs2", sceNgs2ParseWaveformUser);
    LIB_FUNCTION("cLV4aiT9JpA", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackCreate);
    LIB_FUNCTION("U546k6orxQo", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackCreateWithAllocator);
    LIB_FUNCTION("lCqD7oycmIM", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackDestroy);
    LIB_FUNCTION("M4LYATRhRUE", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackGetInfo);
    LIB_FUNCTION("Mn4XNDg03XY", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackGetUserData);
    LIB_FUNCTION("MwmHz8pAdAo", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackGetVoiceHandle);
    LIB_FUNCTION("MzTa7VLjogY", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackLock);
    LIB_FUNCTION("0eFLVCfWVds", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackQueryBufferSize);
    LIB_FUNCTION("TZqb8E-j3dY", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackQueryInfo);
    LIB_FUNCTION("MI2VmBx2RbM", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackRunCommands);
    LIB_FUNCTION("JNTMIaBIbV4", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackSetUserData);
    LIB_FUNCTION("++YZ7P9e87U", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackUnlock);
    LIB_FUNCTION("uBIN24Tv2MI", "libSceNgs2", 1, "libSceNgs2", sceNgs2ReportRegisterHandler);
    LIB_FUNCTION("nPzb7Ly-VjE", "libSceNgs2", 1, "libSceNgs2", sceNgs2ReportUnregisterHandler);
    LIB_FUNCTION("koBbCMvOKWw", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemCreate);
    LIB_FUNCTION("mPYgU4oYpuY", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemCreateWithAllocator);
    LIB_FUNCTION("u-WrYDaJA3k", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemDestroy);
    LIB_FUNCTION("vubFP0T6MP0", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemEnumHandles);
    LIB_FUNCTION("U-+7HsswcIs", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemEnumRackHandles);
    LIB_FUNCTION("vU7TQ62pItw", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemGetInfo);
    LIB_FUNCTION("4lFaRxd-aLs", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemGetUserData);
    LIB_FUNCTION("gThZqM5PYlQ", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemLock);
    LIB_FUNCTION("pgFAiLR5qT4", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemQueryBufferSize);
    LIB_FUNCTION("3oIK7y7O4k0", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemQueryInfo);
    LIB_FUNCTION("i0VnXM-C9fc", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemRender);
    LIB_FUNCTION("AQkj7C0f3PY", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemResetOption);
    LIB_FUNCTION("gXiormHoZZ4", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemRunCommands);
    LIB_FUNCTION("l4Q2dWEH6UM", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemSetGrainSamples);
    LIB_FUNCTION("Wdlx0ZFTV9s", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemSetLoudThreshold);
    LIB_FUNCTION("-tbc2SxQD60", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemSetSampleRate);
    LIB_FUNCTION("GZB2v0XnG0k", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemSetUserData);
    LIB_FUNCTION("JXRC5n0RQls", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemUnlock);
    LIB_FUNCTION("sU2St3agdjg", "libSceNgs2", 1, "libSceNgs2", sceNgs2StreamCreate);
    LIB_FUNCTION("I+RLwaauggA", "libSceNgs2", 1, "libSceNgs2", sceNgs2StreamCreateWithAllocator);
    LIB_FUNCTION("bfoMXnTRtwE", "libSceNgs2", 1, "libSceNgs2", sceNgs2StreamDestroy);
    LIB_FUNCTION("dxulc33msHM", "libSceNgs2", 1, "libSceNgs2", sceNgs2StreamQueryBufferSize);
    LIB_FUNCTION("rfw6ufRsmow", "libSceNgs2", 1, "libSceNgs2", sceNgs2StreamQueryInfo);
    LIB_FUNCTION("q+2W8YdK0F8", "libSceNgs2", 1, "libSceNgs2", sceNgs2StreamResetOption);
    LIB_FUNCTION("qQHCi9pjDps", "libSceNgs2", 1, "libSceNgs2", sceNgs2StreamRunCommands);
    LIB_FUNCTION("uu94irFOGpA", "libSceNgs2", 1, "libSceNgs2", sceNgs2VoiceControl);
    LIB_FUNCTION("jjBVvPN9964", "libSceNgs2", 1, "libSceNgs2", sceNgs2VoiceGetMatrixInfo);
    LIB_FUNCTION("W-Z8wWMBnhk", "libSceNgs2", 1, "libSceNgs2", sceNgs2VoiceGetOwner);
    LIB_FUNCTION("WCayTgob7-o", "libSceNgs2", 1, "libSceNgs2", sceNgs2VoiceGetPortInfo);
    LIB_FUNCTION("-TOuuAQ-buE", "libSceNgs2", 1, "libSceNgs2", sceNgs2VoiceGetState);
    LIB_FUNCTION("rEh728kXk3w", "libSceNgs2", 1, "libSceNgs2", sceNgs2VoiceGetStateFlags);
    LIB_FUNCTION("9eic4AmjGVI", "libSceNgs2", 1, "libSceNgs2", sceNgs2VoiceQueryInfo);
    LIB_FUNCTION("AbYvTOZ8Pts", "libSceNgs2", 1, "libSceNgs2", sceNgs2VoiceRunCommands);
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

// The system modules that may be LLE, and the HLE each falls back on when it is not there
static constexpr auto ModulesToLoad =
    std::to_array<Core::SysModules>({{"libSceNgs2.sprx", &RegisterNgs2},
                                     {"libSceUlt.sprx", &RegisterUlt},
                                     {"libSceAvPlayer.sprx", &Libraries::AvPlayer::RegisterLib},
                                     {"libSceAvPlayerStreaming.sprx", nullptr},
                                     {"libSceRtc.sprx", &RegisterRtc},
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

} // namespace Libraries::SysModule

extern "C" s32 sysmodule_hle_find(const char* mod_name, bool* has_hle) {
    using Libraries::SysModule::ModulesToLoad;
    for (std::size_t i = 0; i < ModulesToLoad.size(); i++) {
        if (std::string_view{mod_name} == ModulesToLoad[i].module_name) {
            *has_hle = ModulesToLoad[i].callback != nullptr;
            return static_cast<s32>(i);
        }
    }
    *has_hle = false;
    return -1;
}

extern "C" void sysmodule_hle_register(s32 index) {
    auto* linker = Common::Singleton<Core::Linker>::Instance();
    Libraries::SysModule::ModulesToLoad[index].callback(&linker->GetHLESymbols());
    // When loading HLEs, we need to relocate imports
    // This ensures later module loads can see our HLE functions.
    linker->RelocateAllImports();
}
