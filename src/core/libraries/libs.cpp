// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/arch.h"
#include "common/elf_info.h"
#include "common/io_file.h"
#include "common/path_util.h"
#include "common/singleton.h"
#include "core/emulator_settings.h"
#include "core/guest_call.h"
#include "core/guest_call_c.h"
#include "core/libraries/ajm/ajm.h"
#include "core/libraries/app_content/app_content.h"
#include "core/libraries/audio/audioin.h"
#include "core/libraries/audio/audioout.h"
#include "core/libraries/audio3d/audio3d.h"
#include "core/libraries/avplayer/avplayer.h"
#include "core/libraries/camera/camera.h"
#include "core/libraries/companion/companion_httpd.h"
#include "core/libraries/companion/companion_util.h"
#include "core/libraries/content_export/content_export.h"
#include "core/libraries/disc_map/disc_map.h"
#include "core/libraries/fiber/fiber.h"
#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
#include "core/guest_cpu/hle_call_adapter.h"
#include "core/guest_cpu/hle_frame_c.h"
#include "core/linker.h"
#endif
#include "core/libraries/game_live_streaming/gamelivestreaming.h"
#include "core/libraries/gnmdriver/gnmdriver.h"
#include "core/libraries/hmd/hmd.h"
#include "core/libraries/hmd/hmd_setup_dialog.h"
#include "core/libraries/ime/error_dialog.h"
#include "core/libraries/ime/ime.h"
#include "core/libraries/ime/ime_dialog.h"
#include "core/libraries/invitation_dialog/invitation_dialog.h"
#include "core/libraries/kernel/kernel.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/kernel/process.h"
#include "core/libraries/keyboard/keyboard.h"
#include "core/libraries/libc_internal/libc_internal.h"
#include "core/libraries/libc_internal/libc_internal_io.h"
#include "core/libraries/libc_internal/libc_internal_math.h"
#include "core/libraries/libc_internal/libc_internal_memory.h"
#include "core/libraries/libc_internal/libc_internal_str.h"
#include "core/libraries/libc_internal/libc_internal_threads.h"
#include "core/libraries/libs.h"
#include "core/libraries/mouse/mouse.h"
#include "core/libraries/move/move.h"
#include "core/libraries/music/music_player_service.h"
#include "core/libraries/net/net.h"
#include "core/libraries/network/http.h"
#include "core/libraries/network/http2.h"
#include "core/libraries/network/netctl.h"
#include "core/libraries/network/ssl.h"
#include "core/libraries/network/ssl2.h"
#include "core/libraries/np/np_auth/np_auth.h"
#include "core/libraries/np/np_commerce/np_commerce.h"
#include "core/libraries/np/np_common.h"
#include "core/libraries/np/np_manager.h"
#include "core/libraries/np/np_matching2/np_matching2.h"
#include "core/libraries/np/np_partner.h"
#include "core/libraries/np/np_party.h"
#include "core/libraries/np/np_profile_dialog/np_profile_dialog.h"
#include "core/libraries/np/np_score/np_score.h"
#include "core/libraries/np/np_signaling/np_signaling.h"
#include "core/libraries/np/np_sns_facebook_dialog.h"
#include "core/libraries/np/np_trophy.h"
#include "core/libraries/np/np_tus/np_tus.h"
#include "core/libraries/np/np_utility/np_utility.h"
#include "core/libraries/np/np_web_api/np_web_api.h"
#include "core/libraries/np/np_web_api2/np_web_api2.h"
#include "core/libraries/pad/pad.h"
#include "core/libraries/playgo/playgo.h"
#include "core/libraries/playgo/playgo_dialog.h"
#include "core/libraries/random/random.h"
#include "core/libraries/razor_cpu/razor_cpu.h"
#include "core/libraries/remote_play/remoteplay.h"
#include "core/libraries/rudp/rudp.h"
#include "core/libraries/save_data/dialog/savedatadialog.h"
#include "core/libraries/save_data/savedata.h"
#include "core/libraries/screenshot/screenshot.h"
#include "core/libraries/share_play/shareplay.h"
#include "core/libraries/signin_dialog/signindialog.h"
#include "core/libraries/sysmodule/sysmodule.h"
#include "core/libraries/system/commondialog.h"
#include "core/libraries/system/msgdialog.h"
#include "core/libraries/system/systemservice.h"
#include "core/libraries/system/userservice.h"
#include "core/libraries/ulobjmgr/ulobjmgr.h"
#include "core/libraries/usbd/usbd.h"
#include "core/libraries/usbd/usbstorage.h"
#include "core/libraries/video_recording/video_recording.h"
#include "core/libraries/videodec/vdecsw.h"
#include "core/libraries/videodec/videodec.h"
#include "core/libraries/videodec/videodec2.h"
#include "core/libraries/videoout/video_out.h"
#include "core/libraries/voice/voice.h"
#include "core/libraries/vr_tracker/vr_tracker.h"
#include "core/libraries/web_browser_dialog/webbrowserdialog.h"
#include "core/libraries/zlib/zlib.h"
#include "emulator.h"

#include <array>

void LinkSymbolImpl(Core::Loader::SymbolsResolver* sym, char const* nid, char const* lib,
                    u16 libversion, char const* mod, u64 symbol,
                    Core::Loader::SymbolType sym_type) {
    Core::Loader::SymbolResolver sr{};
    sr.name = nid;
    sr.library = lib;
    sr.library_version = libversion;
    sr.module = mod;
    sr.type = sym_type;
    sym->AddSymbol(sr, symbol);
}

#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
void LinkFunctionImpl(Core::Loader::SymbolsResolver* sym, char const* nid, char const* lib,
                      u16 libversion, char const* mod,
                      std::shared_ptr<Core::GuestCpu::HleCallAdapter> adapter) {
    Core::Loader::SymbolResolver sr{};
    sr.name = nid;
    sr.library = lib;
    sr.library_version = libversion;
    sr.module = mod;
    sr.type = Core::Loader::SymbolType::Function;
    sym->AddFunction(sr, std::move(adapter));
}
#endif

// Core::CallGuest for the code written in C (core/guest_call_c.h)
extern "C" u64 core_call_guest4(const void* function, u64 a0, u64 a1, u64 a2, u64 a3) {
    using Function = u64 PS4_SYSV_ABI (*)(u64, u64, u64, u64);
    return Core::CallGuest(reinterpret_cast<Function>(const_cast<void*>(function)), a0, a1, a2, a3);
}

namespace Libraries {

// The libraries written in C: their functions, registered for the game
static void RegisterRandom(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("PI7jIZj4pcE", "libSceRandom", 1, "libSceRandom", sceRandomGetRandomNumber);
}

static void RegisterVideoRecording(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("Fc8qxlKINYQ", "libSceVideoRecording", 1, "libSceVideoRecording",
                 sceVideoRecordingSetInfo);
}

static void RegisterDiscMap(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("fl1eoDnwQ4s", "libSceDiscMap", 1, "libSceDiscMap", sceDiscMapGetPackageSize);
    LIB_FUNCTION("lbQKqsERhtE", "libSceDiscMap", 1, "libSceDiscMap", sceDiscMapIsRequestOnHDD);
    LIB_FUNCTION("fJgP+wqifno", "libSceDiscMap", 1, "libSceDiscMap", DiscMap_Func_7C980FFB0AA27E7A);
    LIB_FUNCTION("ioKMruft1ek", "libSceDiscMap", 1, "libSceDiscMap", DiscMap_Func_8A828CAEE7EDD5E9);
    LIB_FUNCTION("5+vOlukvkfg", "libSceDiscMap", 1, "libSceDiscMap", DiscMap_Func_E7EBCE96E92F91F8);
}

static void RegisterUlobjmgr(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("BG26hBGiNlw", "ulobjmgr", 1, "ulobjmgr", _sceUlobjmgrRegisterObject);
    LIB_FUNCTION("Smf+fUNblPc", "ulobjmgr", 1, "ulobjmgr", _sceUlobjmgrUnregisterObject);
    LIB_FUNCTION("HZ9Q2c+4BU4", "ulobjmgr", 1, "ulobjmgr", Ulobjmgr_Func_1D9F50D9CFB8054E);
    LIB_FUNCTION("SweJO7t3pkk", "ulobjmgr", 1, "ulobjmgr", Ulobjmgr_Func_4B07893BBB77A649);
}

static void RegisterContentExport(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("FzEWeYnAFlI", "libSceContentExport", 1, "libSceContentExport",
                 sceContentExportInit);
    LIB_FUNCTION("0GnN4QCgIfs", "libSceContentExport", 1, "libSceContentExport",
                 sceContentExportInit2);
    LIB_FUNCTION("+KDWny9Y-6k", "libSceContentExport", 1, "libSceContentExport",
                 sceContentExportTerm);
}

static void RegisterSigninDialog(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("mlYGfmqE3fQ", "libSceSigninDialog", 1, "libSceSigninDialog",
                 sceSigninDialogInitialize);
    LIB_FUNCTION("JlpJVoRWv7U", "libSceSigninDialog", 1, "libSceSigninDialog", sceSigninDialogOpen);
    LIB_FUNCTION("2m077aeC+PA", "libSceSigninDialog", 1, "libSceSigninDialog",
                 sceSigninDialogGetStatus);
    LIB_FUNCTION("Bw31liTFT3A", "libSceSigninDialog", 1, "libSceSigninDialog",
                 sceSigninDialogUpdateStatus);
    LIB_FUNCTION("nqG7rqnYw1U", "libSceSigninDialog", 1, "libSceSigninDialog",
                 sceSigninDialogGetResult);
    LIB_FUNCTION("M3OkENHcyiU", "libSceSigninDialog", 1, "libSceSigninDialog",
                 sceSigninDialogClose);
    LIB_FUNCTION("LXlmS6PvJdU", "libSceSigninDialog", 1, "libSceSigninDialog",
                 sceSigninDialogTerminate);
}

static void RegisterScreenShot(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("AS45QoYHjc4", "libSceScreenShot", 1, "libSceScreenShot", ScreenShot_dummy);
    LIB_FUNCTION("JuMLLmmvRgk", "libSceScreenShot", 1, "libSceScreenShot", sceScreenShotCapture);
    LIB_FUNCTION("tIYf0W5VTi8", "libSceScreenShot", 1, "libSceScreenShot", sceScreenShotDisable);
    LIB_FUNCTION("ysfza71rm9M", "libSceScreenShot", 1, "libSceScreenShot",
                 sceScreenShotDisableNotification);
    LIB_FUNCTION("2xxUtuC-RzE", "libSceScreenShot", 1, "libSceScreenShot", sceScreenShotEnable);
    LIB_FUNCTION("BDUaqlVdSAY", "libSceScreenShot", 1, "libSceScreenShot",
                 sceScreenShotEnableNotification);
    LIB_FUNCTION("hNmK4SdhPT0", "libSceScreenShot", 1, "libSceScreenShot", sceScreenShotGetAppInfo);
    LIB_FUNCTION("VlAQIgXa2R0", "libSceScreenShot", 1, "libSceScreenShot",
                 sceScreenShotGetDrcParam);
    LIB_FUNCTION("-SV-oTNGFQk", "libSceScreenShot", 1, "libSceScreenShot", sceScreenShotIsDisabled);
    LIB_FUNCTION("ICNJ-1POs84", "libSceScreenShot", 1, "libSceScreenShot",
                 sceScreenShotIsVshScreenCaptureDisabled);
    LIB_FUNCTION("ahHhOf+QNkQ", "libSceScreenShot", 1, "libSceScreenShot",
                 sceScreenShotSetOverlayImage);
    LIB_FUNCTION("73WQ4Jj0nJI", "libSceScreenShot", 1, "libSceScreenShot",
                 sceScreenShotSetOverlayImageWithOrigin);
    LIB_FUNCTION("G7KlmIYFIZc", "libSceScreenShot", 1, "libSceScreenShot", sceScreenShotSetParam);
    LIB_FUNCTION("itlWFWV3Tzc", "libSceScreenShotDrc", 1, "libSceScreenShot",
                 sceScreenShotSetDrcParam);
}

static void RegisterMouse(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("cAnT0Rw-IwU", "libSceMouse", 1, "libSceMouse", sceMouseClose);
    LIB_FUNCTION("Ymyy1HSSJLQ", "libSceMouse", 1, "libSceMouse", sceMouseConnectPort);
    LIB_FUNCTION("BRXOoXQtb+k", "libSceMouse", 1, "libSceMouse", sceMouseDebugGetDeviceId);
    LIB_FUNCTION("WiGKINCZWkc", "libSceMouse", 1, "libSceMouse", sceMouseDeviceOpen);
    LIB_FUNCTION("eDQTFHbgeTU", "libSceMouse", 1, "libSceMouse", sceMouseDisconnectDevice);
    LIB_FUNCTION("jJP1vYMEPd4", "libSceMouse", 1, "libSceMouse", sceMouseDisconnectPort);
    LIB_FUNCTION("QA9Qupz3Zjw", "libSceMouse", 1, "libSceMouse", sceMouseGetDeviceInfo);
    LIB_FUNCTION("Qs0wWulgl7U", "libSceMouse", 1, "libSceMouse", sceMouseInit);
    LIB_FUNCTION("1FeceR5YhAo", "libSceMouse", 1, "libSceMouse", sceMouseMbusInit);
    LIB_FUNCTION("RaqxZIf6DvE", "libSceMouse", 1, "libSceMouse", sceMouseOpen);
    LIB_FUNCTION("x8qnXqh-tiM", "libSceMouse", 1, "libSceMouse", sceMouseRead);
    LIB_FUNCTION("crkFfp-cmFo", "libSceMouse", 1, "libSceMouse", sceMouseSetHandType);
    LIB_FUNCTION("ghLUU2Z5Lcg", "libSceMouse", 1, "libSceMouse", sceMouseSetPointerSpeed);
    LIB_FUNCTION("6aANndpS0Wo", "libSceMouse", 1, "libSceMouse", sceMouseSetProcessPrivilege);
}

static void RegisterMove(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("j1ITE-EoJmE", "libSceMove", 1, "libSceMove", sceMoveInit);
    LIB_FUNCTION("HzC60MfjJxU", "libSceMove", 1, "libSceMove", sceMoveOpen);
    LIB_FUNCTION("GWXTyxs4QbE", "libSceMove", 1, "libSceMove", sceMoveGetDeviceInfo);
    LIB_FUNCTION("ttU+JOhShl4", "libSceMove", 1, "libSceMove", sceMoveReadStateLatest);
    LIB_FUNCTION("f2bcpK6kJfg", "libSceMove", 1, "libSceMove", sceMoveReadStateRecent);
    LIB_FUNCTION("y5h7f8H1Jnk", "libSceMove", 1, "libSceMove", sceMoveGetExtensionPortInfo);
    LIB_FUNCTION("IFQwtT2CeY0", "libSceMove", 1, "libSceMove", sceMoveSetVibration);
    LIB_FUNCTION("T8KYHPs1JE8", "libSceMove", 1, "libSceMove", sceMoveSetLightSphere);
    LIB_FUNCTION("zuxWAg3HAac", "libSceMove", 1, "libSceMove", sceMoveResetLightSphere);
    LIB_FUNCTION("XX6wlxpHyeo", "libSceMove", 1, "libSceMove", sceMoveClose);
    LIB_FUNCTION("tsZi60H4ypY", "libSceMove", 1, "libSceMove", sceMoveTerm);
}

static void RegisterKeyboard(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("0LWei+c7RNc", "libSceKeyboard", 1, "libSceKeyboard", sceKeyboardClose);
    LIB_FUNCTION("cKyUWdEKBME", "libSceKeyboard", 1, "libSceKeyboard", sceKeyboardConnectPort);
    LIB_FUNCTION("qkHOAYtCFxg", "libSceKeyboard", 1, "libSceKeyboard", sceKeyboardDebugGetDeviceId);
    LIB_FUNCTION("0lAFLqZonXI", "libSceKeyboard", 1, "libSceKeyboard", sceKeyboardDeviceOpen);
    LIB_FUNCTION("DhTavziiZ80", "libSceKeyboard", 1, "libSceKeyboard", sceKeyboardDisconnectDevice);
    LIB_FUNCTION("Hdm6rO0xlnw", "libSceKeyboard", 1, "libSceKeyboard", sceKeyboardDisconnectPort);
    LIB_FUNCTION("XNArvUsT18s", "libSceKeyboard", 1, "libSceKeyboard", sceKeyboardGetConnection);
    LIB_FUNCTION("84mvEJFMYK4", "libSceKeyboard", 1, "libSceKeyboard", sceKeyboardGetDeviceInfo);
    LIB_FUNCTION("yO9JwdRhtSA", "libSceKeyboard", 1, "libSceKeyboard", sceKeyboardGetKey2Char);
    LIB_FUNCTION("wadT3QBCGY0", "libSceKeyboard", 1, "libSceKeyboard", sceKeyboardInit);
    LIB_FUNCTION("6zKouhabgjM", "libSceKeyboard", 1, "libSceKeyboard", sceKeyboardMbusInit);
    LIB_FUNCTION("HJ+KnEHcaxI", "libSceKeyboard", 1, "libSceKeyboard", sceKeyboardOpen);
    LIB_FUNCTION("xybbGMCr738", "libSceKeyboard", 1, "libSceKeyboard", sceKeyboardRead);
    LIB_FUNCTION("6HpE68bzX6M", "libSceKeyboard", 1, "libSceKeyboard", sceKeyboardReadState);
    LIB_FUNCTION("jqRcNtZmLuE", "libSceKeyboard", 1, "libSceKeyboard", sceKeyboardSetProcessFocus);
    LIB_FUNCTION("K1A7IJp5xYc", "libSceKeyboard", 1, "libSceKeyboard",
                 sceKeyboardSetProcessPrivilege);
    LIB_FUNCTION("l76jptHfizM", "libSceKeyboard", 1, "libSceKeyboard",
                 Keyboard_Func_97BEA3A6D1DF8B33);
    LIB_FUNCTION("FU9dBtKaM90", "libSceDbgKeyboard", 1, "libSceKeyboard", sceDbgKeyboardClose);
    LIB_FUNCTION("61ykjnH3AR4", "libSceDbgKeyboard", 1, "libSceKeyboard",
                 sceDbgKeyboardGetKey2Char);
    LIB_FUNCTION("bRuUeIPXFDM", "libSceDbgKeyboard", 1, "libSceKeyboard", sceDbgKeyboardInit);
    LIB_FUNCTION("TESmX5D-i54", "libSceDbgKeyboard", 1, "libSceKeyboard", sceDbgKeyboardOpen);
    LIB_FUNCTION("vKbf07G7KPE", "libSceDbgKeyboard", 1, "libSceKeyboard", sceDbgKeyboardRead);
    LIB_FUNCTION("HJMS5pu4dvY", "libSceDbgKeyboard", 1, "libSceKeyboard", sceDbgKeyboardReadState);
}

static void RegisterSharePlay(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("ggnCfalLU-8", "libSceSharePlay", 1, "libSceSharePlay", sceSharePlayCrashDaemon);
    LIB_FUNCTION("OOrLKB0bSDs", "libSceSharePlay", 1, "libSceSharePlay",
                 sceSharePlayGetCurrentConnectionInfo);
    LIB_FUNCTION("+MCXJlWdi+s", "libSceSharePlay", 1, "libSceSharePlay",
                 sceSharePlayGetCurrentConnectionInfoA);
    LIB_FUNCTION("vUMkWXQff3w", "libSceSharePlay", 1, "libSceSharePlay",
                 sceSharePlayGetCurrentInfo);
    LIB_FUNCTION("Md7Mdkr8LBc", "libSceSharePlay", 1, "libSceSharePlay", sceSharePlayGetEvent);
    LIB_FUNCTION("isruqthpYcw", "libSceSharePlay", 1, "libSceSharePlay", sceSharePlayInitialize);
    LIB_FUNCTION("9zwJpai7jGc", "libSceSharePlay", 1, "libSceSharePlay",
                 sceSharePlayNotifyDialogOpen);
    LIB_FUNCTION("VUW2V9cUTP4", "libSceSharePlay", 1, "libSceSharePlay",
                 sceSharePlayNotifyForceCloseForCdlg);
    LIB_FUNCTION("XL0WwUJoQPg", "libSceSharePlay", 1, "libSceSharePlay",
                 sceSharePlayNotifyOpenQuickMenu);
    LIB_FUNCTION("6-1fKaa5HlY", "libSceSharePlay", 1, "libSceSharePlay",
                 sceSharePlayResumeScreenForCdlg);
    LIB_FUNCTION("U28jAuLHj6c", "libSceSharePlay", 1, "libSceSharePlay", sceSharePlayServerLock);
    LIB_FUNCTION("3Oaux9ITEtY", "libSceSharePlay", 1, "libSceSharePlay", sceSharePlayServerUnLock);
    LIB_FUNCTION("QZy+KmyqKPU", "libSceSharePlay", 1, "libSceSharePlay", sceSharePlaySetMode);
    LIB_FUNCTION("co2NCj--pnc", "libSceSharePlay", 1, "libSceSharePlay",
                 sceSharePlaySetProhibition);
    LIB_FUNCTION("KADsbjNCgPo", "libSceSharePlay", 1, "libSceSharePlay",
                 sceSharePlaySetProhibitionModeWithAppId);
    LIB_FUNCTION("-F6NddfUsa4", "libSceSharePlay", 1, "libSceSharePlay", sceSharePlayStartStandby);
    LIB_FUNCTION("rWVNHNnEx6g", "libSceSharePlay", 1, "libSceSharePlay",
                 sceSharePlayStartStreaming);
    LIB_FUNCTION("zEDkUWLVwFI", "libSceSharePlay", 1, "libSceSharePlay", sceSharePlayStopStandby);
    LIB_FUNCTION("aGlema+JxUU", "libSceSharePlay", 1, "libSceSharePlay", sceSharePlayStopStreaming);
    LIB_FUNCTION("UaLjloJinow", "libSceSharePlay", 1, "libSceSharePlay", sceSharePlayTerminate);
    LIB_FUNCTION("LpPA6mprZ8Q", "libSceSharePlay", 1, "libSceSharePlay",
                 SharePlay_Func_2E93C0EA6A6B67C4);
    LIB_FUNCTION("wcI2co2I4Xc", "libSceSharePlay", 1, "libSceSharePlay",
                 SharePlay_Func_C1C236728D88E177);
    LIB_FUNCTION("6egMR0eB8RU", "libSceSharePlay", 1, "libSceSharePlay",
                 SharePlay_Func_E9E80C474781F115);
    LIB_FUNCTION("891hmdoV7UQ", "libSceSharePlay", 1, "libSceSharePlay",
                 SharePlay_Func_F3DD6199DA15ED44);
    LIB_FUNCTION("OOrLKB0bSDs", "libSceSharePlayCompat", 1, "libSceSharePlay",
                 sceSharePlayGetCurrentConnectionInfo);
}

static void RegisterCompanionHttpd(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("8pWltDG7h6A", "libSceCompanionHttpd", 1, "libSceCompanionHttpd",
                 sceCompanionHttpdAddHeader);
    LIB_FUNCTION("B-QBMeFdNgY", "libSceCompanionHttpd", 1, "libSceCompanionHttpd",
                 sceCompanionHttpdGet2ndScreenStatus);
    LIB_FUNCTION("Vku4big+IYM", "libSceCompanionHttpd", 1, "libSceCompanionHttpd",
                 sceCompanionHttpdGetEvent);
    LIB_FUNCTION("0SySxcuVNG0", "libSceCompanionHttpd", 1, "libSceCompanionHttpd",
                 sceCompanionHttpdGetUserId);
    LIB_FUNCTION("ykNpWs3ktLY", "libSceCompanionHttpd", 1, "libSceCompanionHttpd",
                 sceCompanionHttpdInitialize);
    LIB_FUNCTION("OA6FbORefbo", "libSceCompanionHttpd", 1, "libSceCompanionHttpd",
                 sceCompanionHttpdInitialize2);
    LIB_FUNCTION("r-2-a0c7Kfc", "libSceCompanionHttpd", 1, "libSceCompanionHttpd",
                 sceCompanionHttpdOptParamInitialize);
    LIB_FUNCTION("fHNmij7kAUM", "libSceCompanionHttpd", 1, "libSceCompanionHttpd",
                 sceCompanionHttpdRegisterRequestBodyReceptionCallback);
    LIB_FUNCTION("OaWw+IVEdbI", "libSceCompanionHttpd", 1, "libSceCompanionHttpd",
                 sceCompanionHttpdRegisterRequestCallback);
    LIB_FUNCTION("-0c9TCTwnGs", "libSceCompanionHttpd", 1, "libSceCompanionHttpd",
                 sceCompanionHttpdRegisterRequestCallback2);
    LIB_FUNCTION("h3OvVxzX4qM", "libSceCompanionHttpd", 1, "libSceCompanionHttpd",
                 sceCompanionHttpdSetBody);
    LIB_FUNCTION("w7oz0AWHpT4", "libSceCompanionHttpd", 1, "libSceCompanionHttpd",
                 sceCompanionHttpdSetStatus);
    LIB_FUNCTION("k7F0FcDM-Xc", "libSceCompanionHttpd", 1, "libSceCompanionHttpd",
                 sceCompanionHttpdStart);
    LIB_FUNCTION("0SCgzfVQHpo", "libSceCompanionHttpd", 1, "libSceCompanionHttpd",
                 sceCompanionHttpdStop);
    LIB_FUNCTION("+-du9tWgE9s", "libSceCompanionHttpd", 1, "libSceCompanionHttpd",
                 sceCompanionHttpdTerminate);
    LIB_FUNCTION("ZSHiUfYK+QI", "libSceCompanionHttpd", 1, "libSceCompanionHttpd",
                 sceCompanionHttpdUnregisterRequestBodyReceptionCallback);
    LIB_FUNCTION("xweOi2QT-BE", "libSceCompanionHttpd", 1, "libSceCompanionHttpd",
                 sceCompanionHttpdUnregisterRequestCallback);
}

static void RegisterCompanionUtil(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("cE5Msy11WhU", "libSceCompanionUtil", 1, "libSceCompanionUtil",
                 sceCompanionUtilGetEvent);
    LIB_FUNCTION("MaVrz79mT5o", "libSceCompanionUtil", 1, "libSceCompanionUtil",
                 sceCompanionUtilGetRemoteOskEvent);
    LIB_FUNCTION("xb1xlIhf0QY", "libSceCompanionUtil", 1, "libSceCompanionUtil",
                 sceCompanionUtilInitialize);
    LIB_FUNCTION("IPN-FRSrafk", "libSceCompanionUtil", 1, "libSceCompanionUtil",
                 sceCompanionUtilOptParamInitialize);
    LIB_FUNCTION("H1fYQd5lFAI", "libSceCompanionUtil", 1, "libSceCompanionUtil",
                 sceCompanionUtilTerminate);
}

static void RegisterGameLiveStreaming(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("caqgDl+V9qA", "libSceGameLiveStreaming_debug", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingStartDebugBroadcast);
    LIB_FUNCTION("0i8Lrllxwow", "libSceGameLiveStreaming_debug", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingStopDebugBroadcast);
    LIB_FUNCTION("NqkTzemliC0", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingApplySocialFeedbackMessageFilter);
    LIB_FUNCTION("PC4jq87+YQI", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingCheckCallback);
    LIB_FUNCTION("FcHBfHjFXkA", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingClearPresetSocialFeedbackCommands);
    LIB_FUNCTION("lZ2Sd0uEvpo", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingClearSocialFeedbackMessages);
    LIB_FUNCTION("6c2zGtThFww", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingClearSpoilerTag);
    LIB_FUNCTION("dWM80AX39o4", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingEnableLiveStreaming);
    LIB_FUNCTION("wBOQWjbWMfU", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingEnableSocialFeedback);
    LIB_FUNCTION("aRSQNqbats4", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingGetCurrentBroadcastScreenLayout);
    LIB_FUNCTION("CoPMx369EqM", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingGetCurrentStatus);
    LIB_FUNCTION("lK8dLBNp9OE", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingGetCurrentStatus2);
    LIB_FUNCTION("OIIm19xu+NM", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingGetProgramInfo);
    LIB_FUNCTION("PMx7N4WqNdo", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingGetSocialFeedbackMessages);
    LIB_FUNCTION("yeQKjHETi40", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingGetSocialFeedbackMessagesCount);
    LIB_FUNCTION("kvYEw2lBndk", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingInitialize);
    LIB_FUNCTION("ysWfX5PPbfc", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingLaunchLiveViewer);
    LIB_FUNCTION("cvRCb7DTAig", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingLaunchLiveViewerA);
    LIB_FUNCTION("K0QxEbD7q+c", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingPermitLiveStreaming);
    LIB_FUNCTION("-EHnU68gExU", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingPermitServerSideRecording);
    LIB_FUNCTION("hggKhPySVgI", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingPostSocialMessage);
    LIB_FUNCTION("nFP8qT9YXbo", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingRegisterCallback);
    LIB_FUNCTION("b5RaMD2J0So", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingScreenCloseSeparateMode);
    LIB_FUNCTION("hBdd8n6kuvE", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingScreenConfigureSeparateMode);
    LIB_FUNCTION("uhCmn81s-mU", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingScreenInitialize);
    LIB_FUNCTION("fo5B8RUaBxQ", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingScreenInitializeSeparateModeParameter);
    LIB_FUNCTION("iorzW0pKOiA", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingScreenOpenSeparateMode);
    LIB_FUNCTION("gDSvt78H3Oo", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingScreenSetMode);
    LIB_FUNCTION("HE93dr-5rx4", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingScreenTerminate);
    LIB_FUNCTION("3PSiwAzFISE", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingSetCameraFrameSetting);
    LIB_FUNCTION("TwuUzTKKeek", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingSetDefaultServiceProviderPermission);
    LIB_FUNCTION("Gw6S4oqlY7E", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingSetGuardAreas);
    LIB_FUNCTION("QmQYwQ7OTJI", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingSetInvitationSessionId);
    LIB_FUNCTION("Sb5bAXyUt5c", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingSetLinkCommentPreset);
    LIB_FUNCTION("q-kxuaF7URU", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingSetMaxBitrate);
    LIB_FUNCTION("hUY-mSOyGL0", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingSetMetadata);
    LIB_FUNCTION("ycodiP2I0xo", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingSetPresetSocialFeedbackCommands);
    LIB_FUNCTION("x6deXUpQbBo", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingSetPresetSocialFeedbackCommandsDescription);
    LIB_FUNCTION("mCoz3k3zPmA", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingSetServiceProviderPermission);
    LIB_FUNCTION("ZuX+zzz2DkA", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingSetSpoilerTag);
    LIB_FUNCTION("MLvYI86FFAo", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingSetStandbyScreenResource);
    LIB_FUNCTION("y0KkAydy9xE", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingStartGenerateStandbyScreenResource);
    LIB_FUNCTION("Y1WxX7dPMCw", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingStartSocialFeedbackMessageFiltering);
    LIB_FUNCTION("D7dg5QJ4FlE", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingStopGenerateStandbyScreenResource);
    LIB_FUNCTION("bYuGUBuIsaY", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingStopSocialFeedbackMessageFiltering);
    LIB_FUNCTION("9yK6Fk8mKOQ", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingTerminate);
    LIB_FUNCTION("5XHaH3kL+bA", "libSceGameLiveStreaming", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingUnregisterCallback);
    LIB_FUNCTION("caqgDl+V9qA", "libSceGameLiveStreaming_direct_streaming", 1,
                 "libSceGameLiveStreaming", sceGameLiveStreamingStartDebugBroadcast);
    LIB_FUNCTION("0i8Lrllxwow", "libSceGameLiveStreaming_direct_streaming", 1,
                 "libSceGameLiveStreaming", sceGameLiveStreamingStopDebugBroadcast);
    LIB_FUNCTION("CoPMx369EqM", "libSceGameLiveStreamingCompat", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingGetCurrentStatus);
    LIB_FUNCTION("ysWfX5PPbfc", "libSceGameLiveStreamingCompat", 1, "libSceGameLiveStreaming",
                 sceGameLiveStreamingLaunchLiveViewer);
}

static void RegisterRazorCpu(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("JFzLJBlYIJE", "libSceRazorCpu", 1, "libSceRazorCpu",
                 sceRazorCpuBeginLogicalFileAccess);
    LIB_FUNCTION("SfRTRZ1Sh+U", "libSceRazorCpu", 1, "libSceRazorCpu",
                 sceRazorCpuDisableFiberUserMarkers);
    LIB_FUNCTION("gVioM9cbiDs", "libSceRazorCpu", 1, "libSceRazorCpu",
                 sceRazorCpuEndLogicalFileAccess);
    LIB_FUNCTION("G90IIOtgFQ0", "libSceRazorCpu", 1, "libSceRazorCpu",
                 sceRazorCpuFiberLogNameChange);
    LIB_FUNCTION("PAytDtFGpqY", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuFiberSwitch);
    LIB_FUNCTION("sPhrQD31ClM", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuFlushOccurred);
    LIB_FUNCTION("B782NptkGUc", "libSceRazorCpu", 1, "libSceRazorCpu",
                 sceRazorCpuGetDataTagStorageSize);
    LIB_FUNCTION("EH9Au2RlSrE", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuGpuMarkerSync);
    LIB_FUNCTION("A7oRMdaOJP8", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuInitDataTags);
    LIB_FUNCTION("NFwh-J-BrI0", "libSceRazorCpu", 1, "libSceRazorCpu",
                 sceRazorCpuInitializeGpuMarkerContext);
    LIB_FUNCTION("ElNyedXaa4o", "libSceRazorCpu", 1, "libSceRazorCpu",
                 sceRazorCpuInitializeInternal);
    LIB_FUNCTION("EboejOQvLL4", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuIsCapturing);
    LIB_FUNCTION("dnEdyY4+klQ", "libSceRazorCpu", 1, "libSceRazorCpu",
                 sceRazorCpuJobManagerDispatch);
    LIB_FUNCTION("KP+TBWGHlgs", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuJobManagerJob);
    LIB_FUNCTION("9FowWFMEIM8", "libSceRazorCpu", 1, "libSceRazorCpu",
                 sceRazorCpuJobManagerSequence);
    LIB_FUNCTION("XCuZoBSVFG8", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuNamedSync);
    LIB_FUNCTION("njGikRrxkC0", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuPlotValue);
    LIB_FUNCTION("YpkGsMXP3ew", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuPopMarker);
    LIB_FUNCTION("zw+celG7zSI", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuPushMarker);
    LIB_FUNCTION("uZrOwuNJX-M", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuPushMarkerStatic);
    LIB_FUNCTION("D0yUjM33QqU", "libSceRazorCpu", 1, "libSceRazorCpu",
                 sceRazorCpuResizeTaggedBuffer);
    LIB_FUNCTION("jqYWaTfgZs0", "libSceRazorCpu", 1, "libSceRazorCpu",
                 sceRazorCpuSetPopMarkerCallback);
    LIB_FUNCTION("DJsHcEb94n0", "libSceRazorCpu", 1, "libSceRazorCpu",
                 sceRazorCpuSetPushMarkerCallback);
    LIB_FUNCTION("EZtqozPTS4M", "libSceRazorCpu", 1, "libSceRazorCpu",
                 sceRazorCpuSetPushMarkerStaticCallback);
    LIB_FUNCTION("emklx7RK-LY", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuShutdownDataTags);
    LIB_FUNCTION("TIytAjYeaik", "libSceRazorCpu", 1, "libSceRazorCpu",
                 sceRazorCpuStartCaptureInternal);
    LIB_FUNCTION("jWpkVWdMrsM", "libSceRazorCpu", 1, "libSceRazorCpu",
                 sceRazorCpuStopCaptureInternal);
    LIB_FUNCTION("Ax7NjOzctIM", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuSync);
    LIB_FUNCTION("we3oTKSPSTw", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuTagArray);
    LIB_FUNCTION("vyjdThnQfQQ", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuTagBuffer);
    LIB_FUNCTION("0yNHPIkVTmw", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuUnTagBuffer);
    LIB_FUNCTION("Crha9LvwvJM", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuWorkloadRunBegin);
    LIB_FUNCTION("q1GxBfGHO0s", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuWorkloadRunEnd);
    LIB_FUNCTION("6rUvx-6QmYc", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuWorkloadSubmit);
    LIB_FUNCTION("G3brhegfyNg", "libSceRazorCpu", 1, "libSceRazorCpu", sceRazorCpuWriteBookmark);
}

static void RegisterRemoteplay(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("xQeIryTX7dY", "libSceRemoteplay", 1, "libSceRemoteplay", sceRemoteplayApprove);
    LIB_FUNCTION("IYZ+Mu+8tPo", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayChangeEnterKey);
    LIB_FUNCTION("ZYUsJtcAnqA", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayClearAllRegistData);
    LIB_FUNCTION("cCheyCbF7qw", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayClearConnectHistory);
    LIB_FUNCTION("tPYT-kGbZh8", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayConfirmDeviceRegist);
    LIB_FUNCTION("6Lg4BNleJWc", "libSceRemoteplay", 1, "libSceRemoteplay", sceRemoteplayDisconnect);
    LIB_FUNCTION("j98LdSGy4eY", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayGeneratePinCode);
    LIB_FUNCTION("L+cL-M-DP3w", "libSceRemoteplay", 1, "libSceRemoteplay", sceRemoteplayGetApMode);
    LIB_FUNCTION("g4K51cY+PEw", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayGetConnectHistory);
    LIB_FUNCTION("g3PNjYKWqnQ", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayGetConnectionStatus);
    LIB_FUNCTION("3eBNV9A0BUM", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayGetConnectUserId);
    LIB_FUNCTION("ufesWMVX6iU", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayGetMbusDeviceInfo);
    LIB_FUNCTION("DxU4JGh4S2k", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayGetOperationStatus);
    LIB_FUNCTION("n5OxFJEvPlc", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayGetRemoteplayStatus);
    LIB_FUNCTION("Cekhs6LSHC0", "libSceRemoteplay", 1, "libSceRemoteplay", sceRemoteplayGetRpMode);
    LIB_FUNCTION("ig1ocbR7Ptw", "libSceRemoteplay", 1, "libSceRemoteplay", sceRemoteplayImeClose);
    LIB_FUNCTION("gV9-8cJPM3I", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayImeFilterResult);
    LIB_FUNCTION("cMk57DZXe6c", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayImeGetEvent);
    LIB_FUNCTION("-gwkQpOCl68", "libSceRemoteplay", 1, "libSceRemoteplay", sceRemoteplayImeNotify);
    LIB_FUNCTION("58v9tSlRxc8", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayImeNotifyEventResult);
    LIB_FUNCTION("C3r2zT5ebMg", "libSceRemoteplay", 1, "libSceRemoteplay", sceRemoteplayImeOpen);
    LIB_FUNCTION("oB730zwoz0s", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayImeSetCaret);
    LIB_FUNCTION("rOTg1Nljp8w", "libSceRemoteplay", 1, "libSceRemoteplay", sceRemoteplayImeSetText);
    LIB_FUNCTION("k1SwgkMSOM8", "libSceRemoteplay", 1, "libSceRemoteplay", sceRemoteplayInitialize);
    LIB_FUNCTION("R8RZC1ZIkzU", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayIsRemoteOskReady);
    LIB_FUNCTION("uYhiELUtLgA", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayIsRemotePlaying);
    LIB_FUNCTION("d-BBSEq1nfc", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayNotifyMbusDeviceRegistComplete);
    LIB_FUNCTION("Yytq7NE38R8", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayNotifyNpPushWakeup);
    LIB_FUNCTION("Wg-w8xjMZA4", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayNotifyPinCodeError);
    LIB_FUNCTION("yheulqylKwI", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayNotifyUserDelete);
    LIB_FUNCTION("t5ZvUiZ1hpE", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayPrintAllRegistData);
    LIB_FUNCTION("mrNh78tBpmg", "libSceRemoteplay", 1, "libSceRemoteplay", sceRemoteplayProhibit);
    LIB_FUNCTION("7QLrixwVHcU", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayProhibitStreaming);
    LIB_FUNCTION("-ThIlThsN80", "libSceRemoteplay", 1, "libSceRemoteplay", sceRemoteplayServerLock);
    LIB_FUNCTION("0Z-Pm5rZJOI", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplayServerUnLock);
    LIB_FUNCTION("xSrhtSLIjOc", "libSceRemoteplay", 1, "libSceRemoteplay", sceRemoteplaySetApMode);
    LIB_FUNCTION("5-2agAeaE+c", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplaySetLogLevel);
    LIB_FUNCTION("Rf0XMVR7xPw", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplaySetProhibition);
    LIB_FUNCTION("n4l3FTZtNQM", "libSceRemoteplay", 1, "libSceRemoteplay",
                 sceRemoteplaySetProhibitionForVsh);
    LIB_FUNCTION("-BPcEQ1w8xc", "libSceRemoteplay", 1, "libSceRemoteplay", sceRemoteplaySetRpMode);
    LIB_FUNCTION("BOwybKVa3Do", "libSceRemoteplay", 1, "libSceRemoteplay", sceRemoteplayTerminate);
    LIB_FUNCTION("HV7jZe1frbM", "libSceRemoteplay", 1, "libSceRemoteplay",
                 Remoteplay_Func_1D5EE365ED5FADB3);
}

static void RegisterCamera(Core::Loader::SymbolsResolver* sym) {
    // The firmware the game was built for: some of the library's checks depend on it
    kernel_get_compiled_sdk_version(&g_firmware_version);

    LIB_FUNCTION("QhjrPkRPUZQ", "libSceCamera", 1, "libSceCamera", sceCameraAccGetData);
    LIB_FUNCTION("UFonL7xopFM", "libSceCamera", 1, "libSceCamera", sceCameraAudioClose);
    LIB_FUNCTION("fkZE7Hup2ro", "libSceCamera", 1, "libSceCamera", sceCameraAudioGetData);
    LIB_FUNCTION("hftC5A1C8OQ", "libSceCamera", 1, "libSceCamera", sceCameraAudioGetData2);
    LIB_FUNCTION("DhqqFiBU+6g", "libSceCamera", 1, "libSceCamera", sceCameraAudioOpen);
    LIB_FUNCTION("wyU98EXAYxU", "libSceCamera", 1, "libSceCamera", sceCameraAudioReset);
    LIB_FUNCTION("Y0pCDajzkVQ", "libSceCamera", 1, "libSceCamera", sceCameraChangeAppModuleState);
    LIB_FUNCTION("OMS9LlcrvBo", "libSceCamera", 1, "libSceCamera", sceCameraClose);
    LIB_FUNCTION("ztqH5qNTpTk", "libSceCamera", 1, "libSceCamera", sceCameraCloseByHandle);
    LIB_FUNCTION("nBH6i2s4Glc", "libSceCamera", 1, "libSceCamera", sceCameraDeviceOpen);
    LIB_FUNCTION("0btIPD5hg5A", "libSceCamera", 1, "libSceCamera", sceCameraGetAttribute);
    LIB_FUNCTION("oEi6vM-3E2c", "libSceCamera", 1, "libSceCamera", sceCameraGetAutoExposureGain);
    LIB_FUNCTION("qTPRMh4eY60", "libSceCamera", 1, "libSceCamera", sceCameraGetAutoWhiteBalance);
    LIB_FUNCTION("hHA1frlMxYE", "libSceCamera", 1, "libSceCamera", sceCameraGetCalibData);
    LIB_FUNCTION("5Oie5RArfWs", "libSceCamera", 1, "libSceCamera", sceCameraGetCalibDataFromDevice);
    LIB_FUNCTION("RHYJ7GKOSMg", "libSceCamera", 1, "libSceCamera", sceCameraGetCalibrationData);
    LIB_FUNCTION("ZaqmGEtYuL0", "libSceCamera", 1, "libSceCamera", sceCameraGetConfig);
    LIB_FUNCTION("a5xFueMZIMs", "libSceCamera", 1, "libSceCamera", sceCameraGetContrast);
    LIB_FUNCTION("tslCukqFE+E", "libSceCamera", 1, "libSceCamera",
                 sceCameraGetDefectivePixelCancellation);
    LIB_FUNCTION("DSOLCrc3Kh8", "libSceCamera", 1, "libSceCamera", sceCameraGetDeviceConfig);
    LIB_FUNCTION("n+rFeP1XXyM", "libSceCamera", 1, "libSceCamera",
                 sceCameraGetDeviceConfigWithoutHandle);
    LIB_FUNCTION("jTJCdyv9GLU", "libSceCamera", 1, "libSceCamera", sceCameraGetDeviceID);
    LIB_FUNCTION("-H3UwGQvNZI", "libSceCamera", 1, "libSceCamera", sceCameraGetDeviceIDWithoutOpen);
    LIB_FUNCTION("WZpxnSAM-ds", "libSceCamera", 1, "libSceCamera", sceCameraGetDeviceInfo);
    LIB_FUNCTION("ObIste7hqdk", "libSceCamera", 1, "libSceCamera", sceCameraGetExposureGain);
    LIB_FUNCTION("mxgMmR+1Kr0", "libSceCamera", 1, "libSceCamera", sceCameraGetFrameData);
    LIB_FUNCTION("WVox2rwGuSc", "libSceCamera", 1, "libSceCamera", sceCameraGetGamma);
    LIB_FUNCTION("zrIUDKZx0iE", "libSceCamera", 1, "libSceCamera", sceCameraGetHue);
    LIB_FUNCTION("XqYRHc4aw3w", "libSceCamera", 1, "libSceCamera", sceCameraGetLensCorrection);
    LIB_FUNCTION("B260o9pSzM8", "libSceCamera", 1, "libSceCamera", sceCameraGetMmapConnectedCount);
    LIB_FUNCTION("ULxbwqiYYuU", "libSceCamera", 1, "libSceCamera", sceCameraGetProductInfo);
    LIB_FUNCTION("olojYZKYiYs", "libSceCamera", 1, "libSceCamera", sceCameraGetRegister);
    LIB_FUNCTION("hawKak+Auw4", "libSceCamera", 1, "libSceCamera", sceCameraGetRegistryInfo);
    LIB_FUNCTION("RTDOsWWqdME", "libSceCamera", 1, "libSceCamera", sceCameraGetSaturation);
    LIB_FUNCTION("c6Fp9M1EXXc", "libSceCamera", 1, "libSceCamera", sceCameraGetSharpness);
    LIB_FUNCTION("IAz2HgZQWzE", "libSceCamera", 1, "libSceCamera", sceCameraGetVrCaptureInfo);
    LIB_FUNCTION("HX5524E5tMY", "libSceCamera", 1, "libSceCamera", sceCameraGetWhiteBalance);
    LIB_FUNCTION("0wnf2a60FqI", "libSceCamera", 1, "libSceCamera",
                 sceCameraInitializeRegistryCalibData);
    LIB_FUNCTION("p6n3Npi3YY4", "libSceCamera", 1, "libSceCamera", sceCameraIsAttached);
    LIB_FUNCTION("wQfd7kfRZvo", "libSceCamera", 1, "libSceCamera", sceCameraIsConfigChangeDone);
    LIB_FUNCTION("U3BVwQl2R5Q", "libSceCamera", 1, "libSceCamera", sceCameraIsValidFrameData);
    LIB_FUNCTION("BHn83xrF92E", "libSceCamera", 1, "libSceCamera", sceCameraOpen);
    LIB_FUNCTION("eTywOSWsEiI", "libSceCamera", 1, "libSceCamera", sceCameraOpenByModuleId);
    LIB_FUNCTION("py8p6kZcHmA", "libSceCamera", 1, "libSceCamera", sceCameraRemoveAppModuleFocus);
    LIB_FUNCTION("j5isFVIlZLk", "libSceCamera", 1, "libSceCamera", sceCameraSetAppModuleFocus);
    LIB_FUNCTION("doPlf33ab-U", "libSceCamera", 1, "libSceCamera", sceCameraSetAttribute);
    LIB_FUNCTION("96F7zp1Xo+k", "libSceCamera", 1, "libSceCamera", sceCameraSetAttributeInternal);
    LIB_FUNCTION("yfSdswDaElo", "libSceCamera", 1, "libSceCamera", sceCameraSetAutoExposureGain);
    LIB_FUNCTION("zIKL4kZleuc", "libSceCamera", 1, "libSceCamera", sceCameraSetAutoWhiteBalance);
    LIB_FUNCTION("LEMk5cTHKEA", "libSceCamera", 1, "libSceCamera", sceCameraSetCalibData);
    LIB_FUNCTION("VQ+5kAqsE2Q", "libSceCamera", 1, "libSceCamera", sceCameraSetConfig);
    LIB_FUNCTION("9+SNhbctk64", "libSceCamera", 1, "libSceCamera", sceCameraSetConfigInternal);
    LIB_FUNCTION("3i5MEzrC1pg", "libSceCamera", 1, "libSceCamera", sceCameraSetContrast);
    LIB_FUNCTION("vejouEusC7g", "libSceCamera", 1, "libSceCamera", sceCameraSetDebugStop);
    LIB_FUNCTION("jMv40y2A23g", "libSceCamera", 1, "libSceCamera",
                 sceCameraSetDefectivePixelCancellation);
    LIB_FUNCTION("vER3cIMBHqI", "libSceCamera", 1, "libSceCamera",
                 sceCameraSetDefectivePixelCancellationInternal);
    LIB_FUNCTION("wgBMXJJA6K4", "libSceCamera", 1, "libSceCamera", sceCameraSetExposureGain);
    LIB_FUNCTION("jeTpU0MqKU0", "libSceCamera", 1, "libSceCamera", sceCameraSetForceActivate);
    LIB_FUNCTION("lhEIsHzB8r4", "libSceCamera", 1, "libSceCamera", sceCameraSetGamma);
    LIB_FUNCTION("QI8GVJUy2ZY", "libSceCamera", 1, "libSceCamera", sceCameraSetHue);
    LIB_FUNCTION("K7W7H4ZRwbc", "libSceCamera", 1, "libSceCamera", sceCameraSetLensCorrection);
    LIB_FUNCTION("eHa3vhGu2rQ", "libSceCamera", 1, "libSceCamera",
                 sceCameraSetLensCorrectionInternal);
    LIB_FUNCTION("lS0tM6n+Q5E", "libSceCamera", 1, "libSceCamera", sceCameraSetProcessFocus);
    LIB_FUNCTION("NVITuK83Z7o", "libSceCamera", 1, "libSceCamera",
                 sceCameraSetProcessFocusByHandle);
    LIB_FUNCTION("8MjO05qk5hA", "libSceCamera", 1, "libSceCamera", sceCameraSetRegister);
    LIB_FUNCTION("bSKEi2PzzXI", "libSceCamera", 1, "libSceCamera", sceCameraSetSaturation);
    LIB_FUNCTION("P-7MVfzvpsM", "libSceCamera", 1, "libSceCamera", sceCameraSetSharpness);
    LIB_FUNCTION("3VJOpzKoIeM", "libSceCamera", 1, "libSceCamera", sceCameraSetTrackerMode);
    LIB_FUNCTION("nnR7KAIDPv8", "libSceCamera", 1, "libSceCamera", sceCameraSetUacModeInternal);
    LIB_FUNCTION("wpeyFwJ+UEI", "libSceCamera", 1, "libSceCamera", sceCameraSetVideoSync);
    LIB_FUNCTION("8WtmqmE4edw", "libSceCamera", 1, "libSceCamera", sceCameraSetVideoSyncInternal);
    LIB_FUNCTION("k3zPIcgFNv0", "libSceCamera", 1, "libSceCamera", sceCameraSetWhiteBalance);
    LIB_FUNCTION("9EpRYMy7rHU", "libSceCamera", 1, "libSceCamera", sceCameraStart);
    LIB_FUNCTION("cLxF1QtHch0", "libSceCamera", 1, "libSceCamera", sceCameraStartByHandle);
    LIB_FUNCTION("2G2C0nmd++M", "libSceCamera", 1, "libSceCamera", sceCameraStop);
    LIB_FUNCTION("+X1Kgnn3bzg", "libSceCamera", 1, "libSceCamera", sceCameraStopByHandle);
}

static void RegisterVrTracker(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("24kDA+A0Ox0", "libSceVrTrackerFourDeviceAllowed", 1, "libSceVrTracker",
                 sceVrTrackerRegisterDevice2);
    LIB_FUNCTION("5IFOAYv-62g", "libSceVrTracker", 1, "libSceVrTracker", sceVrTrackerCpuProcess);
    LIB_FUNCTION("zvyKP0Z3UvU", "libSceVrTracker", 1, "libSceVrTracker",
                 sceVrTrackerGetPlayAreaWarningInfo);
    LIB_FUNCTION("76OBvrrQXUc", "libSceVrTracker", 1, "libSceVrTracker", sceVrTrackerGetResult);
    LIB_FUNCTION("XoeWzXlrnMw", "libSceVrTracker", 1, "libSceVrTracker", sceVrTrackerGetTime);
    LIB_FUNCTION("TVegDMLaBB8", "libSceVrTracker", 1, "libSceVrTracker", sceVrTrackerGpuSubmit);
    LIB_FUNCTION("gkGuO9dd57M", "libSceVrTracker", 1, "libSceVrTracker", sceVrTrackerGpuWait);
    LIB_FUNCTION("ARhgpXvwoR0", "libSceVrTracker", 1, "libSceVrTracker",
                 sceVrTrackerGpuWaitAndCpuProcess);
    LIB_FUNCTION("QkRl7pART9M", "libSceVrTracker", 1, "libSceVrTracker", sceVrTrackerInit);
    LIB_FUNCTION("VItTwN8DmS8", "libSceVrTracker", 1, "libSceVrTracker",
                 sceVrTrackerNotifyEndOfCpuProcess);
    LIB_FUNCTION("K7yhYrsIBPc", "libSceVrTracker", 1, "libSceVrTracker", sceVrTrackerQueryMemory);
    LIB_FUNCTION("EUCaQtXXYNI", "libSceVrTracker", 1, "libSceVrTracker", sceVrTrackerRecalibrate);
    LIB_FUNCTION("sIh8GwcevaQ", "libSceVrTracker", 1, "libSceVrTracker",
                 sceVrTrackerRegisterDevice);
    LIB_FUNCTION("ufexf4aNiwg", "libSceVrTracker", 1, "libSceVrTracker",
                 sceVrTrackerRegisterDeviceInternal);
    LIB_FUNCTION("CtWUbFgmq+I", "libSceVrTracker", 1, "libSceVrTracker", sceVrTrackerResetAll);
    LIB_FUNCTION("E0P0sN-wy+4", "libSceVrTracker", 1, "libSceVrTracker",
                 sceVrTrackerResetOrientationRelative);
    LIB_FUNCTION("bDGZVTwwZ1A", "libSceVrTracker", 1, "libSceVrTracker",
                 sceVrTrackerSaveInternalBuffers);
    LIB_FUNCTION("qBjnR0HtMYI", "libSceVrTracker", 1, "libSceVrTracker",
                 sceVrTrackerSetDurationUntilStatusNotTracking);
    LIB_FUNCTION("NhPkY3V8E+8", "libSceVrTracker", 1, "libSceVrTracker",
                 sceVrTrackerSetExtendedMode);
    LIB_FUNCTION("vpsLLotiSUg", "libSceVrTracker", 1, "libSceVrTracker",
                 sceVrTrackerSetLEDBrightness);
    LIB_FUNCTION("lgWSHQ8p4i4", "libSceVrTracker", 1, "libSceVrTracker",
                 sceVrTrackerSetRestingMode);
    LIB_FUNCTION("IBv4P3q1pQ0", "libSceVrTracker", 1, "libSceVrTracker", sceVrTrackerTerm);
    LIB_FUNCTION("Q8skQqEwn5c", "libSceVrTracker", 1, "libSceVrTracker",
                 sceVrTrackerUnregisterDevice);
    LIB_FUNCTION("9fvHMUbsom4", "libSceVrTracker", 1, "libSceVrTracker",
                 sceVrTrackerUpdateMotionSensorData);
    LIB_FUNCTION("D6TJSfjTAk4", "libSceVrTracker", 1, "libSceVrTracker",
                 VrTracker_Func_0FA4C949F8D3024E);
    LIB_FUNCTION("KFxq-AnEL34", "libSceVrTracker", 1, "libSceVrTracker",
                 VrTracker_Func_285C6AFC09C42F7E);
    LIB_FUNCTION("mmzbIQNmT4o", "libSceVrTracker", 1, "libSceVrTracker",
                 VrTracker_Func_9A6CDB2103664F8A);
    LIB_FUNCTION("tNJrfYsY3wY", "libSceVrTracker", 1, "libSceVrTracker",
                 VrTracker_Func_B4D26B7D8B18DF06);
    LIB_FUNCTION("jGqEkPy0iLU", "libSceVrTrackerDeviceRejection", 1, "libSceVrTracker",
                 sceVrTrackerSetDeviceRejection);
    LIB_FUNCTION("ERmwvjmfN+c", "libSceVrTrackerGpuTest", 1, "libSceVrTracker",
                 VrTracker_Func_1119B0BE399F37E7);
    LIB_FUNCTION("SSi0OBa8RA0", "libSceVrTrackerGpuTest", 1, "libSceVrTracker",
                 VrTracker_Func_4928B43816BC440D);
    LIB_FUNCTION("hj7zLvyw+pw", "libSceVrTrackerGpuTest", 1, "libSceVrTracker",
                 VrTracker_Func_863EF32EFCB0FA9C);
    LIB_FUNCTION("5ucmy8hcSPk", "libSceVrTrackerGpuTest", 1, "libSceVrTracker",
                 VrTracker_Func_E6E726CBC85C48F9);
    LIB_FUNCTION("9kB+RsZt84M", "libSceVrTrackerGpuTest", 1, "libSceVrTracker",
                 VrTracker_Func_F6407E46C66DF383);
    LIB_FUNCTION("sBkAqyF5Gns", "libSceVrTrackerLiveCapture", 1, "libSceVrTracker",
                 sceVrTrackerCpuPopMarker);
    LIB_FUNCTION("rvCywCbc7Pk", "libSceVrTrackerLiveCapture", 1, "libSceVrTracker",
                 sceVrTrackerCpuPushMarker);
    LIB_FUNCTION("lm6T1Ur6JRk", "libSceVrTrackerLiveCapture", 1, "libSceVrTracker",
                 sceVrTrackerGetLiveCaptureId);
    LIB_FUNCTION("qa1+CeXKDPc", "libSceVrTrackerLiveCapture", 1, "libSceVrTracker",
                 sceVrTrackerStartLiveCapture);
    LIB_FUNCTION("3YCwwpHkHIg", "libSceVrTrackerLiveCapture", 1, "libSceVrTracker",
                 sceVrTrackerStopLiveCapture);
}

static void RegisterCommonDialog(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("2RdicdHhtGA", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_ZN3sce16CommonDialogUtil12getSelfAppIdEv);
    LIB_FUNCTION("I+tdxsCap08", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_ZN3sce16CommonDialogUtil6Client11closeModuleEv);
    LIB_FUNCTION("v4+gzuTkv6k", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_ZN3sce16CommonDialogUtil6Client11updateStateEv);
    LIB_FUNCTION("CwCzG0nnLg8", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_ZN3sce16CommonDialogUtil6Client15launchCmnDialogEv);
    LIB_FUNCTION("Ib1SMmbr07k", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_ZN3sce16CommonDialogUtil6ClientD0Ev);
    LIB_FUNCTION("6TIMpGvsrC4", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_ZN3sce16CommonDialogUtil6ClientD1Ev);
    LIB_FUNCTION("+UyKxWAnqIU", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_ZN3sce16CommonDialogUtil6ClientD2Ev);
    LIB_FUNCTION("bUCx72-9f0g", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_ZNK3sce16CommonDialogUtil6Client10isCloseReqEv);
    LIB_FUNCTION("xZtXq554Lbg", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_ZNK3sce16CommonDialogUtil6Client13getFinishDataEPvm);
    LIB_FUNCTION("C-EZ3PkhibQ", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_ZNK3sce16CommonDialogUtil6Client14getClientStateEv);
    LIB_FUNCTION("70niEKUAnZ0", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_ZNK3sce16CommonDialogUtil6Client19isInitializedStatusEv);
    LIB_FUNCTION("mdJgdwoM0Mo", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_ZNK3sce16CommonDialogUtil6Client8getAppIdEv);
    LIB_FUNCTION("87GekE1nowg", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_ZNK3sce16CommonDialogUtil6Client8isFinishEv);
    LIB_FUNCTION("6ljeTSi+fjs", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_ZNK3sce16CommonDialogUtil6Client9getResultEv);
    LIB_FUNCTION("W2MzrWix2mM", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_ZTVN3sce16CommonDialogUtil6ClientE);
    LIB_FUNCTION("uoUpLGNkygk", "libSceCommonDialog", 1, "libSceCommonDialog",
                 sceCommonDialogInitialize);
    LIB_FUNCTION("BQ3tey0JmQM", "libSceCommonDialog", 1, "libSceCommonDialog",
                 sceCommonDialogIsUsed);
    LIB_FUNCTION("D-V35OhFeIM", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_Func_0FF577E4E8457883);
    LIB_FUNCTION("QXFsLON5QWw", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_Func_41716C2CE379416C);
    LIB_FUNCTION("SDpCfY9uB0g", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_Func_483A427D8F6E0748);
    LIB_FUNCTION("aUS4PgJye98", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_Func_6944B83E02727BDF);
    LIB_FUNCTION("afLdI6i0lQw", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_Func_69F2DD23A8B4950C);
    LIB_FUNCTION("mVRnPerBcK0", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_Func_9954673DEAC170AD);
    LIB_FUNCTION("p9TTq4bLdFU", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_Func_A7D4D3AB86CB7455);
    LIB_FUNCTION("reTFEla4NQw", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_Func_ADE4C51256B8350C);
    LIB_FUNCTION("txNJzxX6yrA", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_Func_B71349CF15FACAB0);
    LIB_FUNCTION("yxjgDvqUbGQ", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_Func_CB18E00EFA946C64);
    LIB_FUNCTION("8q7icGBWIrA", "libSceCommonDialog", 1, "libSceCommonDialog",
                 CommonDialog_Func_F2AEE270605622B0);
}

static void RegisterPlayGoDialog(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("fbigNQiZpm0", "libScePlayGoDialog", 1, "libScePlayGoDialog",
                 scePlayGoDialogClose);
    LIB_FUNCTION("wx9TDplJKB4", "libScePlayGoDialog", 1, "libScePlayGoDialog",
                 scePlayGoDialogGetResult);
    LIB_FUNCTION("NOAMxY2EGS0", "libScePlayGoDialog", 1, "libScePlayGoDialog",
                 scePlayGoDialogGetStatus);
    LIB_FUNCTION("fECamTJKpsM", "libScePlayGoDialog", 1, "libScePlayGoDialog",
                 scePlayGoDialogInitialize);
    LIB_FUNCTION("kHd72ukqbxw", "libScePlayGoDialog", 1, "libScePlayGoDialog", scePlayGoDialogOpen);
    LIB_FUNCTION("okgIGdr5Iz0", "libScePlayGoDialog", 1, "libScePlayGoDialog",
                 scePlayGoDialogTerminate);
    LIB_FUNCTION("Yb60K7BST48", "libScePlayGoDialog", 1, "libScePlayGoDialog",
                 scePlayGoDialogUpdateStatus);
}

static void RegisterWebBrowserDialog(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("PSK+Eik919Q", "libSceWebBrowserDialog", 1, "libSceWebBrowserDialog",
                 sceWebBrowserDialogClose);
    LIB_FUNCTION("Wit4LjeoeX4", "libSceWebBrowserDialog", 1, "libSceWebBrowserDialog",
                 sceWebBrowserDialogGetEvent);
    LIB_FUNCTION("vCaW0fgVQmc", "libSceWebBrowserDialog", 1, "libSceWebBrowserDialog",
                 sceWebBrowserDialogGetResult);
    LIB_FUNCTION("CFTG6a8TjOU", "libSceWebBrowserDialog", 1, "libSceWebBrowserDialog",
                 sceWebBrowserDialogGetStatus);
    LIB_FUNCTION("jqb7HntFQFc", "libSceWebBrowserDialog", 1, "libSceWebBrowserDialog",
                 sceWebBrowserDialogInitialize);
    LIB_FUNCTION("uYELOMVnmNQ", "libSceWebBrowserDialog", 1, "libSceWebBrowserDialog",
                 sceWebBrowserDialogNavigate);
    LIB_FUNCTION("FraP7debcdg", "libSceWebBrowserDialog", 1, "libSceWebBrowserDialog",
                 sceWebBrowserDialogOpen);
    LIB_FUNCTION("O7dIZQrwVFY", "libSceWebBrowserDialog", 1, "libSceWebBrowserDialog",
                 sceWebBrowserDialogOpenForPredeterminedContent);
    LIB_FUNCTION("Cya+jvTtPqg", "libSceWebBrowserDialog", 1, "libSceWebBrowserDialog",
                 sceWebBrowserDialogResetCookie);
    LIB_FUNCTION("TZnDVkP91Rg", "libSceWebBrowserDialog", 1, "libSceWebBrowserDialog",
                 sceWebBrowserDialogSetCookie);
    LIB_FUNCTION("RLhKBOoNyXY", "libSceWebBrowserDialog", 1, "libSceWebBrowserDialog",
                 sceWebBrowserDialogSetZoom);
    LIB_FUNCTION("ocHtyBwHfys", "libSceWebBrowserDialog", 1, "libSceWebBrowserDialog",
                 sceWebBrowserDialogTerminate);
    LIB_FUNCTION("h1dR-t5ISgg", "libSceWebBrowserDialog", 1, "libSceWebBrowserDialog",
                 sceWebBrowserDialogUpdateStatus);
    LIB_FUNCTION("8r4EJ3FiX4w", "libSceWebBrowserDialogLimited", 1, "libSceWebBrowserDialog",
                 WebBrowserDialog_Func_F2BE042771625F8C);
}

static void RegisterInvitationDialog(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("WWtCL5lzi7Y", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogClose);
    LIB_FUNCTION("8XKR6wa64iQ", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogGetResult);
    LIB_FUNCTION("WuuUhuKOxwQ", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogGetResultA);
    LIB_FUNCTION("EiF92YDNHRA", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogGetStatus);
    LIB_FUNCTION("XvA5KS56wcs", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogInitialize);
    LIB_FUNCTION("0zU0G+wiVLA", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogOpen);
    LIB_FUNCTION("sAxbHhAWMXM", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogOpenA);
    LIB_FUNCTION("B6HVJtDYxEE", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogTerminate);
    LIB_FUNCTION("9+g9iOq+7kg", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogUpdateStatus);
    LIB_FUNCTION("8XKR6wa64iQ", "libSceInvitationDialogCompat", 1, "libSceInvitationDialog",
                 sceInvitationDialogGetResult);
    LIB_FUNCTION("0zU0G+wiVLA", "libSceInvitationDialogCompat", 1, "libSceInvitationDialog",
                 sceInvitationDialogOpen);
}

namespace LibcInternal {

// The HLE libSceLibcInternal, which the linker registers when the game has none of its own
// (libc_internal_*.c)
void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("H8ya2H00jbI", "libSceLibcInternal", 1, "libSceLibcInternal", internal_sin);
    LIB_FUNCTION("Q4rRL34CEeE", "libSceLibcInternal", 1, "libSceLibcInternal", internal_sinf);
    LIB_FUNCTION("2WE3BTYVwKM", "libSceLibcInternal", 1, "libSceLibcInternal", internal_cos);
    LIB_FUNCTION("-P6FNMzk2Kc", "libSceLibcInternal", 1, "libSceLibcInternal", internal_cosf);
    LIB_FUNCTION("jMB7EFyu30Y", "libSceLibcInternal", 1, "libSceLibcInternal", internal_sincos);
    LIB_FUNCTION("pztV4AF18iI", "libSceLibcInternal", 1, "libSceLibcInternal", internal_sincosf);
    LIB_FUNCTION("T7uyNqP7vQA", "libSceLibcInternal", 1, "libSceLibcInternal", internal_tan);
    LIB_FUNCTION("ZE6RNL+eLbk", "libSceLibcInternal", 1, "libSceLibcInternal", internal_tanf);
    LIB_FUNCTION("7Ly52zaL44Q", "libSceLibcInternal", 1, "libSceLibcInternal", internal_asin);
    LIB_FUNCTION("GZWjF-YIFFk", "libSceLibcInternal", 1, "libSceLibcInternal", internal_asinf);
    LIB_FUNCTION("JBcgYuW8lPU", "libSceLibcInternal", 1, "libSceLibcInternal", internal_acos);
    LIB_FUNCTION("QI-x0SL8jhw", "libSceLibcInternal", 1, "libSceLibcInternal", internal_acosf);
    LIB_FUNCTION("OXmauLdQ8kY", "libSceLibcInternal", 1, "libSceLibcInternal", internal_atan);
    LIB_FUNCTION("weDug8QD-lE", "libSceLibcInternal", 1, "libSceLibcInternal", internal_atanf);
    LIB_FUNCTION("HUbZmOnT-Dg", "libSceLibcInternal", 1, "libSceLibcInternal", internal_atan2);
    LIB_FUNCTION("EH-x713A99c", "libSceLibcInternal", 1, "libSceLibcInternal", internal_atan2f);
    LIB_FUNCTION("NVadfnzQhHQ", "libSceLibcInternal", 1, "libSceLibcInternal", internal_exp);
    LIB_FUNCTION("8zsu04XNsZ4", "libSceLibcInternal", 1, "libSceLibcInternal", internal_expf);
    LIB_FUNCTION("dnaeGXbjP6E", "libSceLibcInternal", 1, "libSceLibcInternal", internal_exp2);
    LIB_FUNCTION("wuAQt-j+p4o", "libSceLibcInternal", 1, "libSceLibcInternal", internal_exp2f);
    LIB_FUNCTION("9LCjpWyQ5Zc", "libSceLibcInternal", 1, "libSceLibcInternal", internal_pow);
    LIB_FUNCTION("1D0H2KNjshE", "libSceLibcInternal", 1, "libSceLibcInternal", internal_powf);
    LIB_FUNCTION("rtV7-jWC6Yg", "libSceLibcInternal", 1, "libSceLibcInternal", internal_log);
    LIB_FUNCTION("RQXLbdT2lc4", "libSceLibcInternal", 1, "libSceLibcInternal", internal_logf);
    LIB_FUNCTION("WuMbPBKN1TU", "libSceLibcInternal", 1, "libSceLibcInternal", internal_log10);
    LIB_FUNCTION("lhpd6Wk6ccs", "libSceLibcInternal", 1, "libSceLibcInternal", internal_log10f);
    LIB_FUNCTION("5Xa2ACNECdo", "libSceLibcInternal", 1, "libSceLibcInternal", internal_strcpy_s);
    LIB_FUNCTION("K+gcnFFJKVc", "libSceLibcInternal", 1, "libSceLibcInternal", internal_strcat_s);
    LIB_FUNCTION("Ovb2dSJOAuE", "libSceLibcInternal", 1, "libSceLibcInternal", internal_strcmp);
    LIB_FUNCTION("aesyjrHVWy4", "libSceLibcInternal", 1, "libSceLibcInternal", internal_strncmp);
    LIB_FUNCTION("j4ViWNHEgww", "libSceLibcInternal", 1, "libSceLibcInternal", internal_strlen);
    LIB_FUNCTION("6sJWiWSRuqk", "libSceLibcInternal", 1, "libSceLibcInternal", internal_strncpy);
    LIB_FUNCTION("YNzNkJzYqEg", "libSceLibcInternal", 1, "libSceLibcInternal", internal_strncpy_s);
    LIB_FUNCTION("Ls4tzzhimqQ", "libSceLibcInternal", 1, "libSceLibcInternal", internal_strcat);
    LIB_FUNCTION("ob5xAW4ln-0", "libSceLibcInternal", 1, "libSceLibcInternal", internal_strchr);
    LIB_FUNCTION("NFLs+dRJGNg", "libSceLibcInternal", 1, "libSceLibcInternal", internal_memcpy_s);
    LIB_FUNCTION("Q3VBxCXhUHs", "libSceLibcInternal", 1, "libSceLibcInternal", internal_memcpy);
    LIB_FUNCTION("+P6FRGH4LfA", "libSceLibcInternal", 1, "libSceLibcInternal", internal_memmove);
    LIB_FUNCTION("8zTFvBIAIN8", "libSceLibcInternal", 1, "libSceLibcInternal", internal_memset);
    LIB_FUNCTION("DfivPArhucg", "libSceLibcInternal", 1, "libSceLibcInternal", internal_memcmp);
    LIB_FUNCTION("NWtTN10cJzE", "libSceLibcInternalExt", 1, "libSceLibcInternal",
                 sceLibcHeapGetTraceInfo);
    LIB_FUNCTION("eLdDw6l0-bU", "libSceLibcInternal", 1, "libSceLibcInternal", internal_snprintf);
    LIB_FUNCTION("xGT4Mc55ViQ", "libSceLibcInternal", 1, "libSceLibcInternal", internal__Fofind);
    LIB_FUNCTION("dREVnZkAKRE", "libSceLibcInternal", 1, "libSceLibcInternal", internal__Foprep);
    LIB_FUNCTION("sQL8D-jio7U", "libSceLibcInternal", 1, "libSceLibcInternal", internal__Fopen);
    LIB_FUNCTION("A+Y3xfrWLLo", "libSceLibcInternal", 1, "libSceLibcInternal", internal__Fspos);
    LIB_FUNCTION("Ss3108pBuZY", "libSceLibcInternal", 1, "libSceLibcInternal", internal__Nnl);
    LIB_FUNCTION("9s3P+LCvWP8", "libSceLibcInternal", 1, "libSceLibcInternal", internal__Frprep);
    LIB_FUNCTION("jVDuvE3s5Bs", "libSceLibcInternal", 1, "libSceLibcInternal", internal__Fofree);
    LIB_FUNCTION("vZkmJmvqueY", "libSceLibcInternal", 1, "libSceLibcInternal",
                 internal__Lockfilelock);
    LIB_FUNCTION("0x7rx8TKy2Y", "libSceLibcInternal", 1, "libSceLibcInternal",
                 internal__Unlockfilelock);
    LIB_FUNCTION("xeYO4u7uyJ0", "libSceLibcInternal", 1, "libSceLibcInternal", internal_fopen);
    LIB_FUNCTION("rQFVBXp-Cxg", "libSceLibcInternal", 1, "libSceLibcInternal", internal_fseek);
    LIB_FUNCTION("lbB+UlZqVG0", "libSceLibcInternal", 1, "libSceLibcInternal", internal_fread);
    LIB_FUNCTION("MUjC4lbHrK4", "libSceLibcInternal", 1, "libSceLibcInternal", internal_fflush);
    LIB_FUNCTION("uodLYyUip20", "libSceLibcInternal", 1, "libSceLibcInternal", internal_fclose);
    LIB_FUNCTION("z7STeF6abuU", "libSceLibcInternal", 1, "libSceLibcInternal", internal__Mtxinit);
    LIB_FUNCTION("pE4Ot3CffW0", "libSceLibcInternal", 1, "libSceLibcInternal", internal__Mtxlock);
    LIB_FUNCTION("cMwgSSmpE5o", "libSceLibcInternal", 1, "libSceLibcInternal", internal__Mtxunlock);
    LIB_FUNCTION("LaPaA6mYA38", "libSceLibcInternal", 1, "libSceLibcInternal", internal__Mtxdst);
}

} // namespace LibcInternal

static void RegisterAppContent(Core::Loader::SymbolsResolver* sym) {
    app_content_library_init();

    LIB_FUNCTION("AS45QoYHjc4", "libSceAppContent", 1, "libSceAppContentUtil", AppContent_Z5dummyv);
    LIB_FUNCTION("ZiATpP9gEkA", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentAddcontDelete);
    LIB_FUNCTION("7gxh+5QubhY", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentAddcontEnqueueDownload);
    LIB_FUNCTION("TVM-aYIsG9k", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentAddcontEnqueueDownloadSp);
    LIB_FUNCTION("VANhIWcqYak", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentAddcontMount);
    LIB_FUNCTION("D3H+cjfzzFY", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentAddcontShrink);
    LIB_FUNCTION("3rHWaV-1KC4", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentAddcontUnmount);
    LIB_FUNCTION("99b82IKXpH4", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentAppParamGetInt);
    LIB_FUNCTION("+OlXCu8qxUk", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentAppParamGetString);
    LIB_FUNCTION("gpGZDB4ZlrI", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentDownload0Expand);
    LIB_FUNCTION("S5eMvWnbbXg", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentDownload0Shrink);
    LIB_FUNCTION("B5gVeVurdUA", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentDownload1Expand);
    LIB_FUNCTION("kUeYucqnb7o", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentDownload1Shrink);
    LIB_FUNCTION("CN7EbEV7MFU", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentDownloadDataFormat);
    LIB_FUNCTION("Gl6w5i0JokY", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentDownloadDataGetAvailableSpaceKb);
    LIB_FUNCTION("5bvvbUSiFs4", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentGetAddcontDownloadProgress);
    LIB_FUNCTION("m47juOmH0VE", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentGetAddcontInfo);
    LIB_FUNCTION("xnd8BJzAxmk", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentGetAddcontInfoList);
    LIB_FUNCTION("XTWR0UXvcgs", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentGetEntitlementKey);
    LIB_FUNCTION("74-1x3lyZK8", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentGetRegion);
    LIB_FUNCTION("R9lA82OraNs", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentInitialize);
    LIB_FUNCTION("bVtF7v2uqT0", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentRequestPatchInstall);
    LIB_FUNCTION("9Gq5rOkWzNU", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentSmallSharedDataFormat);
    LIB_FUNCTION("xhb-r8etmAA", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentSmallSharedDataGetAvailableSpaceKb);
    LIB_FUNCTION("QuApZnMo9MM", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentSmallSharedDataMount);
    LIB_FUNCTION("EqMtBHWu-5M", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentSmallSharedDataUnmount);
    LIB_FUNCTION("a5N7lAG0y2Q", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentTemporaryDataFormat);
    LIB_FUNCTION("SaKib2Ug0yI", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentTemporaryDataGetAvailableSpaceKb);
    LIB_FUNCTION("7bOLX66Iz-U", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentTemporaryDataMount);
    LIB_FUNCTION("buYbeLOGWmA", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentTemporaryDataMount2);
    LIB_FUNCTION("bcolXMmp6qQ", "libSceAppContent", 1, "libSceAppContentUtil",
                 sceAppContentTemporaryDataUnmount);
    LIB_FUNCTION("xmhnAoxN3Wk", "libSceAppContentPft", 1, "libSceAppContent",
                 sceAppContentGetPftFlag);
    LIB_FUNCTION("xZo2-418Wdo", "libSceAppContentBundle", 1, "libSceAppContent",
                 AppContent_Func_C59A36FF8D7C59DA);
    LIB_FUNCTION("kJmjt81mXKQ", "libSceAppContentIro", 1, "libSceAppContent",
                 sceAppContentAddcontEnqueueDownloadByEntitlementId);
    LIB_FUNCTION("efX3lrPwdKA", "libSceAppContentIro", 1, "libSceAppContent",
                 sceAppContentAddcontMountByEntitlementId);
    LIB_FUNCTION("z9hgjLd1SGA", "libSceAppContentIro", 1, "libSceAppContent",
                 sceAppContentGetAddcontInfoByEntitlementId);
    LIB_FUNCTION("3wUaDTGmjcQ", "libSceAppContentIro", 1, "libSceAppContent",
                 sceAppContentGetAddcontInfoListByIroTag);
    LIB_FUNCTION("TCqT7kPuGx0", "libSceAppContentSc", 1, "libSceAppContent",
                 sceAppContentGetDownloadedStoreCountry);
}

static void RegisterPad(Core::Loader::SymbolsResolver* sym) {
    pad_library_init();

    LIB_FUNCTION("6ncge5+l5Qs", "libScePad", 1, "libScePad", scePadClose);
    LIB_FUNCTION("kazv1NzSB8c", "libScePad", 1, "libScePad", scePadConnectPort);
    LIB_FUNCTION("AcslpN1jHR8", "libScePad", 1, "libScePad",
                 scePadDeviceClassGetExtendedInformation);
    LIB_FUNCTION("IHPqcbc0zCA", "libScePad", 1, "libScePad", scePadDeviceClassParseData);
    LIB_FUNCTION("d7bXuEBycDI", "libScePad", 1, "libScePad", scePadDeviceOpen);
    LIB_FUNCTION("0aziJjRZxqQ", "libScePad", 1, "libScePad", scePadDisableVibration);
    LIB_FUNCTION("pnZXireDoeI", "libScePad", 1, "libScePad", scePadDisconnectDevice);
    LIB_FUNCTION("9ez71nWSvD0", "libScePad", 1, "libScePad", scePadDisconnectPort);
    LIB_FUNCTION("77ooWxGOIVs", "libScePad", 1, "libScePad", scePadEnableAutoDetect);
    LIB_FUNCTION("+cE4Jx431wc", "libScePad", 1, "libScePad", scePadEnableExtensionPort);
    LIB_FUNCTION("E1KEw5XMGQQ", "libScePad", 1, "libScePad", scePadEnableSpecificDeviceClass);
    LIB_FUNCTION("DD-KiRLBqkQ", "libScePad", 1, "libScePad", scePadEnableUsbConnection);
    LIB_FUNCTION("Q66U8FdrMaw", "libScePad", 1, "libScePad", scePadGetBluetoothAddress);
    LIB_FUNCTION("qtasqbvwgV4", "libScePad", 1, "libScePad", scePadGetCapability);
    LIB_FUNCTION("gjP9-KQzoUk", "libScePad", 1, "libScePad", scePadGetControllerInformation);
    LIB_FUNCTION("Uq6LgTJEmQs", "libScePad", 1, "libScePad", scePadGetDataInternal);
    LIB_FUNCTION("hDgisSGkOgw", "libScePad", 1, "libScePad", scePadGetDeviceId);
    LIB_FUNCTION("4rS5zG7RFaM", "libScePad", 1, "libScePad", scePadGetDeviceInfo);
    LIB_FUNCTION("hGbf2QTBmqc", "libScePad", 1, "libScePad", scePadGetExtControllerInformation);
    LIB_FUNCTION("1DmZjZAuzEM", "libScePad", 1, "libScePad", scePadGetExtensionUnitInfo);
    LIB_FUNCTION("PZSoY8j0Pko", "libScePad", 1, "libScePad", scePadGetFeatureReport);
    LIB_FUNCTION("u1GRHp+oWoY", "libScePad", 1, "libScePad", scePadGetHandle);
    LIB_FUNCTION("kiA9bZhbnAg", "libScePad", 1, "libScePad", scePadGetIdleCount);
    LIB_FUNCTION("1Odcw19nADw", "libScePad", 1, "libScePad", scePadGetInfo);
    LIB_FUNCTION("4x5Im8pr0-4", "libScePad", 1, "libScePad", scePadGetInfoByPortType);
    LIB_FUNCTION("vegw8qax5MI", "libScePad", 1, "libScePad", scePadGetLicenseControllerInformation);
    LIB_FUNCTION("WPIB7zBWxVE", "libScePad", 1, "libScePad", scePadGetMotionSensorPosition);
    LIB_FUNCTION("k4+nDV9vbT0", "libScePad", 1, "libScePad", scePadGetMotionTimerUnit);
    LIB_FUNCTION("do-JDWX+zRs", "libScePad", 1, "libScePad", scePadGetSphereRadius);
    LIB_FUNCTION("QuOaoOcSOw0", "libScePad", 1, "libScePad", scePadGetVersionInfo);
    LIB_FUNCTION("hv1luiJrqQM", "libScePad", 1, "libScePad", scePadInit);
    LIB_FUNCTION("bi0WNvZ1nug", "libScePad", 1, "libScePad", scePadIsBlasterConnected);
    LIB_FUNCTION("mEC+xJKyIjQ", "libScePad", 1, "libScePad", scePadIsDS4Connected);
    LIB_FUNCTION("d2Qk-i8wGak", "libScePad", 1, "libScePad",
                 scePadIsLightBarBaseBrightnessControllable);
    LIB_FUNCTION("4y9RNPSBsqg", "libScePad", 1, "libScePad", scePadIsMoveConnected);
    LIB_FUNCTION("9e56uLgk5y0", "libScePad", 1, "libScePad", scePadIsMoveReproductionModel);
    LIB_FUNCTION("pFTi-yOrVeQ", "libScePad", 1, "libScePad", scePadIsValidHandle);
    LIB_FUNCTION("CfwUlQtCFi4", "libScePad", 1, "libScePad", scePadMbusInit);
    LIB_FUNCTION("s7CvzS+9ZIs", "libScePad", 1, "libScePad", scePadMbusTerm);
    LIB_FUNCTION("xk0AcarP3V4", "libScePad", 1, "libScePad", scePadOpen);
    LIB_FUNCTION("WFIiSfXGUq8", "libScePad", 1, "libScePad", scePadOpenExt);
    LIB_FUNCTION("71E9e6n+2R8", "libScePad", 1, "libScePad", scePadOpenExt2);
    LIB_FUNCTION("DrUu8cPrje8", "libScePad", 1, "libScePad", scePadOutputReport);
    LIB_FUNCTION("q1cHNfGycLI", "libScePad", 1, "libScePad", scePadRead);
    LIB_FUNCTION("fm1r2vv5+OU", "libScePad", 1, "libScePad", scePadReadBlasterForTracker);
    LIB_FUNCTION("QjwkT2Ycmew", "libScePad", 1, "libScePad", scePadReadExt);
    LIB_FUNCTION("2NhkFTRnXHk", "libScePad", 1, "libScePad", scePadReadForTracker);
    LIB_FUNCTION("3u4M8ck9vJM", "libScePad", 1, "libScePad", scePadReadHistory);
    LIB_FUNCTION("YndgXqQVV7c", "libScePad", 1, "libScePad", scePadReadState);
    LIB_FUNCTION("5Wf4q349s+Q", "libScePad", 1, "libScePad", scePadReadStateExt);
    LIB_FUNCTION("DscD1i9HX1w", "libScePad", 1, "libScePad", scePadResetLightBar);
    LIB_FUNCTION("+4c9xRLmiXQ", "libScePad", 1, "libScePad", scePadResetLightBarAll);
    LIB_FUNCTION("+Yp6+orqf1M", "libScePad", 1, "libScePad", scePadResetLightBarAllByPortType);
    LIB_FUNCTION("rIZnR6eSpvk", "libScePad", 1, "libScePad", scePadResetOrientation);
    LIB_FUNCTION("jbAqAvLEP4A", "libScePad", 1, "libScePad", scePadResetOrientationForTracker);
    LIB_FUNCTION("KLmYx9ij2h0", "libScePad", 1, "libScePad",
                 scePadSetAngularVelocityBiasCorrectionState);
    LIB_FUNCTION("r44mAxdSG+U", "libScePad", 1, "libScePad", scePadSetAngularVelocityDeadbandState);
    LIB_FUNCTION("ew647HuKi2Y", "libScePad", 1, "libScePad", scePadSetAutoPowerOffCount);
    LIB_FUNCTION("MbTt1EHYCTg", "libScePad", 1, "libScePad", scePadSetButtonRemappingInfo);
    LIB_FUNCTION("MLA06oNfF+4", "libScePad", 1, "libScePad", scePadSetConnection);
    LIB_FUNCTION("bsbHFI0bl5s", "libScePad", 1, "libScePad", scePadSetExtensionReport);
    LIB_FUNCTION("xqgVCEflEDY", "libScePad", 1, "libScePad", scePadSetFeatureReport);
    LIB_FUNCTION("lrjFx4xWnY8", "libScePad", 1, "libScePad", scePadSetForceIntercepted);
    LIB_FUNCTION("RR4novUEENY", "libScePad", 1, "libScePad", scePadSetLightBar);
    LIB_FUNCTION("dhQXEvmrVNQ", "libScePad", 1, "libScePad", scePadSetLightBarBaseBrightness);
    LIB_FUNCTION("etaQhgPHDRY", "libScePad", 1, "libScePad", scePadSetLightBarBlinking);
    LIB_FUNCTION("iHuOWdvQVpg", "libScePad", 1, "libScePad", scePadSetLightBarForTracker);
    LIB_FUNCTION("o-6Y99a8dKU", "libScePad", 1, "libScePad", scePadSetLoginUserNumber);
    LIB_FUNCTION("clVvL4ZDntw", "libScePad", 1, "libScePad", scePadSetMotionSensorState);
    LIB_FUNCTION("flYYxek1wy8", "libScePad", 1, "libScePad", scePadSetProcessFocus);
    LIB_FUNCTION("DmBx8K+jDWw", "libScePad", 1, "libScePad", scePadSetProcessPrivilege);
    LIB_FUNCTION("FbxEpTRDou8", "libScePad", 1, "libScePad",
                 scePadSetProcessPrivilegeOfButtonRemapping);
    LIB_FUNCTION("yah8Bk4TcYY", "libScePad", 1, "libScePad", scePadSetShareButtonMaskForRemotePlay);
    LIB_FUNCTION("vDLMoJLde8I", "libScePad", 1, "libScePad", scePadSetTiltCorrectionState);
    LIB_FUNCTION("z+GEemoTxOo", "libScePad", 1, "libScePad", scePadSetUserColor);
    LIB_FUNCTION("yFVnOdGxvZY", "libScePad", 1, "libScePad", scePadSetVibration);
    LIB_FUNCTION("8BOObG94-tc", "libScePad", 1, "libScePad", scePadSetVibrationForce);
    LIB_FUNCTION("--jrY4SHfm8", "libScePad", 1, "libScePad", scePadSetVrTrackingMode);
    LIB_FUNCTION("zFJ35q3RVnY", "libScePad", 1, "libScePad", scePadShareOutputData);
    LIB_FUNCTION("80XdmVYsNPA", "libScePad", 1, "libScePad", scePadStartRecording);
    LIB_FUNCTION("gAHvg6JPIic", "libScePad", 1, "libScePad", scePadStopRecording);
    LIB_FUNCTION("Oi7FzRWFr0Y", "libScePad", 1, "libScePad", scePadSwitchConnection);
    LIB_FUNCTION("0MB5x-ieRGI", "libScePad", 1, "libScePad", scePadVertualDeviceAddDevice);
    LIB_FUNCTION("N7tpsjWQ87s", "libScePad", 1, "libScePad", scePadVirtualDeviceAddDevice);
    LIB_FUNCTION("PFec14-UhEQ", "libScePad", 1, "libScePad", scePadVirtualDeviceDeleteDevice);
    LIB_FUNCTION("pjPCronWdxI", "libScePad", 1, "libScePad",
                 scePadVirtualDeviceDisableButtonRemapping);
    LIB_FUNCTION("LKXfw7VJYqg", "libScePad", 1, "libScePad", scePadVirtualDeviceGetRemoteSetting);
    LIB_FUNCTION("IWOyO5jKuZg", "libScePad", 1, "libScePad", scePadVirtualDeviceInsertData);
    LIB_FUNCTION("KY0hSB+Uyfo", "libScePad", 1, "libScePad", Pad_Func_298D21481F94C9FA);
    LIB_FUNCTION("UeUUvNOgXKU", "libScePad", 1, "libScePad", Pad_Func_51E514BCD3A05CA5);
    LIB_FUNCTION("ickjfjk9okM", "libScePad", 1, "libScePad", Pad_Func_89C9237E393DA243);
    LIB_FUNCTION("7xA+hFtvBCA", "libScePad", 1, "libScePad", Pad_Func_EF103E845B6F0420);
}

static void RegisterSysmodule(Core::Loader::SymbolsResolver* sym) {
    sysmodule_library_init();

    LIB_FUNCTION("D8cuU4d72xM", "libSceSysmodule", 1, "libSceSysmodule",
                 sceSysmoduleGetModuleHandleInternal);
    LIB_FUNCTION("4fU5yvOkVG4", "libSceSysmodule", 1, "libSceSysmodule",
                 sceSysmoduleGetModuleInfoForUnwind);
    LIB_FUNCTION("ctfO7dQ7geg", "libSceSysmodule", 1, "libSceSysmodule",
                 sceSysmoduleIsCalledFromSysModule);
    LIB_FUNCTION("no6T3EfiS3E", "libSceSysmodule", 1, "libSceSysmodule",
                 sceSysmoduleIsCameraPreloaded);
    LIB_FUNCTION("fMP5NHUOaMk", "libSceSysmodule", 1, "libSceSysmodule", sceSysmoduleIsLoaded);
    LIB_FUNCTION("ynFKQ5bfGks", "libSceSysmodule", 1, "libSceSysmodule",
                 sceSysmoduleIsLoadedInternal);
    LIB_FUNCTION("g8cM39EUZ6o", "libSceSysmodule", 1, "libSceSysmodule", sceSysmoduleLoadModule);
    LIB_FUNCTION("CU8m+Qs+HN4", "libSceSysmodule", 1, "libSceSysmodule",
                 sceSysmoduleLoadModuleByNameInternal);
    LIB_FUNCTION("39iV5E1HoCk", "libSceSysmodule", 1, "libSceSysmodule",
                 sceSysmoduleLoadModuleInternal);
    LIB_FUNCTION("hHrGoGoNf+s", "libSceSysmodule", 1, "libSceSysmodule",
                 sceSysmoduleLoadModuleInternalWithArg);
    LIB_FUNCTION("lZ6RvVl0vo0", "libSceSysmodule", 1, "libSceSysmodule",
                 sceSysmoduleMapLibcForLibkernel);
    LIB_FUNCTION("DOO+zuW1lrE", "libSceSysmodule", 1, "libSceSysmodule",
                 sceSysmodulePreloadModuleForLibkernel);
    LIB_FUNCTION("eR2bZFAAU0Q", "libSceSysmodule", 1, "libSceSysmodule", sceSysmoduleUnloadModule);
    LIB_FUNCTION("vpTHmA6Knvg", "libSceSysmodule", 1, "libSceSysmodule",
                 sceSysmoduleUnloadModuleByNameInternal);
    LIB_FUNCTION("vXZhrtJxkGc", "libSceSysmodule", 1, "libSceSysmodule",
                 sceSysmoduleUnloadModuleInternal);
    LIB_FUNCTION("aKa6YfBKZs4", "libSceSysmodule", 1, "libSceSysmodule",
                 sceSysmoduleUnloadModuleInternalWithArg);
}

static void RegisterMusicPlayerService(Core::Loader::SymbolsResolver* sym) {
    music_player_library_init();

    LIB_FUNCTION("WypjBw-l+pE", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceInitialize);
    LIB_FUNCTION("xT55MQDY9lg", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceInitialize2);
    LIB_FUNCTION("sY8beqtqGv0", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceInitialize3);
    LIB_FUNCTION("uZXjTVsS-hU", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceTerminate);
    LIB_FUNCTION("JlbWOi1Webo", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceSetUsbStorageDeviceInfo);
    LIB_FUNCTION("-JE2KLnsg9I", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceRemoveAllData);
    LIB_FUNCTION("4cuKd4S83xw", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceSetTrackList);
    LIB_FUNCTION("iawJpYIQM7s", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceSetTrackList2);
    LIB_FUNCTION("KblnYfZ5AGM", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServicePlay);
    LIB_FUNCTION("kMb+qpZTQ18", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServicePlayStartByTime);
    LIB_FUNCTION("7Ayp3diNYnk", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceStop);
    LIB_FUNCTION("fIii6-0Adxc", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServicePause);
    LIB_FUNCTION("DLXiDjC1D1A", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceUnpause);
    LIB_FUNCTION("M76dl-GnCs8", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceNextTrack);
    LIB_FUNCTION("NuJT-nmTqrI", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServicePreviousTrack);
    LIB_FUNCTION("5LhFvqzrcug", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceSeek);
    LIB_FUNCTION("tMgpmzMA4Zc", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceSetAudioVolume);
    LIB_FUNCTION("3KChwrxsVPg", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceGetAudioVolume);
    LIB_FUNCTION("DMvAm4HFKg0", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceSetShuffle);
    LIB_FUNCTION("+uCwAp6KYtQ", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceSetRepeatMode);
    LIB_FUNCTION("TVe6T-UKcoc", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceGetPlayStatusExtension);
    LIB_FUNCTION("CNsRsR5a+qc", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceGetCurrentPlayStatus);
    LIB_FUNCTION("Py0XtCoi5IU", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceReceiveEvent);
    LIB_FUNCTION("1+KpQYgv1zk", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceGetPlaybackResults);
    LIB_FUNCTION("wHKiDjNLQwI", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceGetTrackInfo);
    LIB_FUNCTION("AbiIaBA50I0", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceBeginTransaction);
    LIB_FUNCTION("7SOVShNUDXo", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceEndTransaction);
    LIB_FUNCTION("mW3EHwAVHPA", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceCreateTrackList);
    LIB_FUNCTION("EF1lpApBn4s", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceRemoveTrackList);
    LIB_FUNCTION("R1SLegxzCGU", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceInsertTrackEntry);
    LIB_FUNCTION("QEIJRZErmxQ", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceRemoveTrackEntry);
    LIB_FUNCTION("qYbmAxRGuq8", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceGetTrackListVersion);
}

static void RegisterHmd(Core::Loader::SymbolsResolver* sym) {
    hmd_library_init();
    LIB_FUNCTION("6biw1XHTSqQ", "libSceHmd", 1, "libSceHmd", sceHmdClose);
    LIB_FUNCTION("BWY-qKM5hxE", "libSceHmd", 1, "libSceHmd", sceHmdGet2DEyeOffset);
    LIB_FUNCTION("Yx+CuF11D3Q", "libSceHmd", 1, "libSceHmd", sceHmdGetAssyError);
    LIB_FUNCTION("thDt9upZlp8", "libSceHmd", 1, "libSceHmd", sceHmdGetDeviceInformation);
    LIB_FUNCTION("1pxQfif1rkE", "libSceHmd", 1, "libSceHmd", sceHmdGetDeviceInformationByHandle);
    LIB_FUNCTION("NPQwYFqi0bs", "libSceHmd", 1, "libSceHmd", sceHmdGetFieldOfView);
    LIB_FUNCTION("rU3HK9Q0r8o", "libSceHmd", 1, "libSceHmd", sceHmdGetInertialSensorData);
    LIB_FUNCTION("K4KnH0QkT2c", "libSceHmd", 1, "libSceHmd", sceHmdInitialize);
    LIB_FUNCTION("s-J66ar9g50", "libSceHmd", 1, "libSceHmd", sceHmdInitialize315);
    LIB_FUNCTION("riPQfAdebHk", "libSceHmd", 1, "libSceHmd", sceHmdInternal3dAudioClose);
    LIB_FUNCTION("wHnZU1qtiqw", "libSceHmd", 1, "libSceHmd", sceHmdInternal3dAudioOpen);
    LIB_FUNCTION("NuEjeN8WCBA", "libSceHmd", 1, "libSceHmd", sceHmdInternal3dAudioSendData);
    LIB_FUNCTION("QasPTUPWVZE", "libSceHmd", 1, "libSceHmd", sceHmdInternalAnotherScreenClose);
    LIB_FUNCTION("Wr5KVtyVDG0", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalAnotherScreenGetAudioStatus);
    LIB_FUNCTION("whRxl6Hhrzg", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalAnotherScreenGetFadeState);
    LIB_FUNCTION("w8BEUsIYn8w", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalAnotherScreenGetVideoStatus);
    LIB_FUNCTION("0cQDAbkOt2A", "libSceHmd", 1, "libSceHmd", sceHmdInternalAnotherScreenOpen);
    LIB_FUNCTION("Asczi8gw1NM", "libSceHmd", 1, "libSceHmd", sceHmdInternalAnotherScreenSendAudio);
    LIB_FUNCTION("6+v7m1vwE+0", "libSceHmd", 1, "libSceHmd", sceHmdInternalAnotherScreenSendVideo);
    LIB_FUNCTION("E0BLvy57IiQ", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalAnotherScreenSetFadeAndSwitch);
    LIB_FUNCTION("UTqrWB+1+SU", "libSceHmd", 1, "libSceHmd", sceHmdInternalBindDeviceWithUserId);
    LIB_FUNCTION("ego1YdqNGpI", "libSceHmd", 1, "libSceHmd", sceHmdInternalCheckDeviceModelMk3);
    LIB_FUNCTION("WR7XsLdjcqQ", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalCheckS3dPassModeAvailable);
    LIB_FUNCTION("eMI1Hq+NEwY", "libSceHmd", 1, "libSceHmd", sceHmdInternalCrashReportCancel);
    LIB_FUNCTION("dI3StPLQlMM", "libSceHmd", 1, "libSceHmd", sceHmdInternalCrashReportClose);
    LIB_FUNCTION("lqPT-Bf1s4I", "libSceHmd", 1, "libSceHmd", sceHmdInternalCrashReportOpen);
    LIB_FUNCTION("QxhJs6zHUmU", "libSceHmd", 1, "libSceHmd", sceHmdInternalCrashReportReadData);
    LIB_FUNCTION("A2jWOLPzHHE", "libSceHmd", 1, "libSceHmd", sceHmdInternalCrashReportReadDataSize);
    LIB_FUNCTION("E9scVxt0DNg", "libSceHmd", 1, "libSceHmd", sceHmdInternalCreateSharedMemory);
    LIB_FUNCTION("6RclvsKxr3I", "libSceHmd", 1, "libSceHmd", sceHmdInternalDfuCheckAfterPvt);
    LIB_FUNCTION("cE99PJR6b8w", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalDfuCheckPartialUpdateAvailable);
    LIB_FUNCTION("SuE90Qscg0s", "libSceHmd", 1, "libSceHmd", sceHmdInternalDfuClose);
    LIB_FUNCTION("5f-6lp7L5cY", "libSceHmd", 1, "libSceHmd", sceHmdInternalDfuGetStatus);
    LIB_FUNCTION("dv2RqD7ZBd4", "libSceHmd", 1, "libSceHmd", sceHmdInternalDfuOpen);
    LIB_FUNCTION("pN0HjRU86Jo", "libSceHmd", 1, "libSceHmd", sceHmdInternalDfuReset);
    LIB_FUNCTION("mdc++HCXSsQ", "libSceHmd", 1, "libSceHmd", sceHmdInternalDfuSend);
    LIB_FUNCTION("gjyqnphjGZE", "libSceHmd", 1, "libSceHmd", sceHmdInternalDfuSendSize);
    LIB_FUNCTION("bl4MkWNLxKs", "libSceHmd", 1, "libSceHmd", sceHmdInternalDfuSetMode);
    LIB_FUNCTION("a1LmvXhZ6TM", "libSceHmd", 1, "libSceHmd", sceHmdInternalDfuStart);
    LIB_FUNCTION("+UzzSnc0z9A", "libSceHmd", 1, "libSceHmd", sceHmdInternalEventInitialize);
    LIB_FUNCTION("uQc9P8Hrr6U", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetBrightness);
    LIB_FUNCTION("nK1g+MwMV10", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetCrashDumpInfo);
    LIB_FUNCTION("L5WZgOTw41Y", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetDebugMode);
    LIB_FUNCTION("3w8SkMfCHY0", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalGetDebugSocialScreenMode);
    LIB_FUNCTION("1Xmb76MHXug", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetDebugTextMode);
    LIB_FUNCTION("S0ITgPRkfUg", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetDefaultLedData);
    LIB_FUNCTION("mxjolbeBa78", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetDemoMode);
    LIB_FUNCTION("RFIi20Wp9j0", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetDeviceInformation);
    LIB_FUNCTION("P04LQJQZ43Y", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalGetDeviceInformationByHandle);
    LIB_FUNCTION("PPCqsD8B5uM", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetDeviceStatus);
    LIB_FUNCTION("-u82z1UhOq4", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetEyeStatus);
    LIB_FUNCTION("iINSFzCIaB8", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetHmuOpticalParam);
    LIB_FUNCTION("Csuvq2MMXHU", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalGetHmuPowerStatusForDebug);
    LIB_FUNCTION("UhFPniZvm8U", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetHmuSerialNumber);
    LIB_FUNCTION("aTg7K0466r8", "libSceHmd", 1, "libSceHmd", Hmd_Func_69383B2B4E3AEABF);
    LIB_FUNCTION("9exeDpk7JU8", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetIPD);
    LIB_FUNCTION("yNtYRsxZ6-A", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalGetIpdSettingEnableForSystemService);
    LIB_FUNCTION("EKn+IFVsz0M", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetPuBuildNumber);
    LIB_FUNCTION("AxQ6HtktYfQ", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetPuPositionParam);
    LIB_FUNCTION("ynKv9QCSbto", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetPuRevision);
    LIB_FUNCTION("3jcyx7XOm7A", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetPUSerialNumber);
    LIB_FUNCTION("+PDyXnclP5w", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetPUVersion);
    LIB_FUNCTION("67q17ERGBuw", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetRequiredPUPVersion);
    LIB_FUNCTION("uGyN1CkvwYU", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetStatusReport);
    LIB_FUNCTION("p9lSvZujLuo", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetTv4kCapability);
    LIB_FUNCTION("-Z+-9u98m9o", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetVirtualDisplayDepth);
    LIB_FUNCTION("df+b0FQnnVQ", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetVirtualDisplayHeight);
    LIB_FUNCTION("i6yROd9ygJs", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetVirtualDisplaySize);
    LIB_FUNCTION("Aajiktl6JXU", "libSceHmd", 1, "libSceHmd", sceHmdInternalGetVr2dData);
    LIB_FUNCTION("GwFVF2KkIT4", "libSceHmd", 1, "libSceHmd", sceHmdInternalIsCommonDlgMiniAppVr2d);
    LIB_FUNCTION("LWQpWHOSUvk", "libSceHmd", 1, "libSceHmd", sceHmdInternalIsCommonDlgVr2d);
    LIB_FUNCTION("YiIVBPLxmfE", "libSceHmd", 1, "libSceHmd", sceHmdInternalIsGameVr2d);
    LIB_FUNCTION("LMlWs+oKHTg", "libSceHmd", 1, "libSceHmd", sceHmdInternalIsMiniAppVr2d);
    LIB_FUNCTION("nBv4CKUGX0Y", "libSceHmd", 1, "libSceHmd", sceHmdInternalMapSharedMemory);
    LIB_FUNCTION("4hTD8I3CyAk", "libSceHmd", 1, "libSceHmd", sceHmdInternalMirroringModeSetAspect);
    LIB_FUNCTION("EJwPtSSZykY", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalMirroringModeSetAspectDebug);
    LIB_FUNCTION("r7f7M5q3snU", "libSceHmd", 1, "libSceHmd", sceHmdInternalMmapGetCount);
    LIB_FUNCTION("gCjTEtEsOOw", "libSceHmd", 1, "libSceHmd", sceHmdInternalMmapGetModeId);
    LIB_FUNCTION("HAr740Mt9Hs", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalMmapGetSensorCalibrationData);
    LIB_FUNCTION("1PNiQR-7L6k", "libSceHmd", 1, "libSceHmd", sceHmdInternalMmapIsConnect);
    LIB_FUNCTION("9-jaAXUNG-A", "libSceHmd", 1, "libSceHmd", sceHmdInternalPushVr2dData);
    LIB_FUNCTION("1gkbLH5+kxU", "libSceHmd", 1, "libSceHmd", sceHmdInternalRegisterEventCallback);
    LIB_FUNCTION("6kHBllapJas", "libSceHmd", 1, "libSceHmd", sceHmdInternalResetInertialSensor);
    LIB_FUNCTION("k1W6RPkd0mc", "libSceHmd", 1, "libSceHmd", sceHmdInternalResetLedForVrTracker);
    LIB_FUNCTION("dp1wu22jSGc", "libSceHmd", 1, "libSceHmd", sceHmdInternalResetLedForVsh);
    LIB_FUNCTION("d2TeoKeqM5U", "libSceHmd", 1, "libSceHmd", sceHmdInternalSeparateModeClose);
    LIB_FUNCTION("WxsnAsjPF7Q", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalSeparateModeGetAudioStatus);
    LIB_FUNCTION("eOOeG9SpEuc", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalSeparateModeGetVideoStatus);
    LIB_FUNCTION("gA4Xnn+NSGk", "libSceHmd", 1, "libSceHmd", sceHmdInternalSeparateModeOpen);
    LIB_FUNCTION("stQ7AsondmE", "libSceHmd", 1, "libSceHmd", sceHmdInternalSeparateModeSendAudio);
    LIB_FUNCTION("jfnS-OoDayM", "libSceHmd", 1, "libSceHmd", sceHmdInternalSeparateModeSendVideo);
    LIB_FUNCTION("roHN4ml+tB8", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetBrightness);
    LIB_FUNCTION("0z2qLqedQH0", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetCrashReportCommand);
    LIB_FUNCTION("xhx5rVZEpnw", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetDebugGpo);
    LIB_FUNCTION("e7laRxRGCHc", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetDebugMode);
    LIB_FUNCTION("CRyJ7Q-ap3g", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalSetDebugSocialScreenMode);
    LIB_FUNCTION("dG4XPW4juU4", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetDebugTextMode);
    LIB_FUNCTION("rAXmGoO-VmE", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetDefaultLedData);
    LIB_FUNCTION("lu9I7jnUvWQ", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetDemoMode);
    LIB_FUNCTION("hyATMTuQSoQ", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetDeviceConnection);
    LIB_FUNCTION("c4mSi64bXUw", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetForcedCrash);
    LIB_FUNCTION("U9kPT4g1mFE", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetHmuPowerControl);
    LIB_FUNCTION("dX-MVpXIPwQ", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalSetHmuPowerControlForDebug);
    LIB_FUNCTION("4KIjvAf8PCA", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetIPD);
    LIB_FUNCTION("NbxTfUKO184", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalSetIpdSettingEnableForSystemService);
    LIB_FUNCTION("NnRKjf+hxW4", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetLedOn);
    LIB_FUNCTION("4AP0X9qGhqw", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetM2LedBrightness);
    LIB_FUNCTION("Mzzz2HPWM+8", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetM2LedOn);
    LIB_FUNCTION("LkBkse9Pit0", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetPortConnection);
    LIB_FUNCTION("v243mvYg0Y0", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetPortStatus);
    LIB_FUNCTION("EwXvkZpo9Go", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetS3dPassMode);
    LIB_FUNCTION("g3DKNOy1tYw", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetSidetone);
    LIB_FUNCTION("mjMsl838XM8", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetUserType);
    LIB_FUNCTION("8IS0KLkDNQY", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetVirtualDisplayDepth);
    LIB_FUNCTION("afhK5KcJOJY", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetVirtualDisplayHeight);
    LIB_FUNCTION("+zPvzIiB+BU", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetVirtualDisplaySize);
    LIB_FUNCTION("9z8Lc64NF1c", "libSceHmd", 1, "libSceHmd", sceHmdInternalSetVRMode);
    LIB_FUNCTION("s5EqYh5kbwM", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalSocialScreenGetFadeState);
    LIB_FUNCTION("a1LMFZtK9b0", "libSceHmd", 1, "libSceHmd",
                 sceHmdInternalSocialScreenSetFadeAndSwitch);
    LIB_FUNCTION("-6FjKlMA+Yc", "libSceHmd", 1, "libSceHmd", sceHmdInternalSocialScreenSetOutput);
    LIB_FUNCTION("d2g5Ij7EUzo", "libSceHmd", 1, "libSceHmd", sceHmdOpen);
    LIB_FUNCTION("z-RMILqP6tE", "libSceHmd", 1, "libSceHmd", sceHmdTerminate);
    LIB_FUNCTION("IC0NGmh-zS8", "libSceHmd", 1, "libSceHmd", Hmd_Func_202D0D1A687FCD2F);
    LIB_FUNCTION("NY2-gYo9ihI", "libSceHmd", 1, "libSceHmd", Hmd_Func_358DBF818A3D8A12);
    LIB_FUNCTION("XMutp2-o9A4", "libSceHmd", 1, "libSceHmd", Hmd_Func_5CCBADA76FE8F40E);
    LIB_FUNCTION("Y9QDFn3AjPA", "libSceHmd", 1, "libSceHmd", Hmd_Func_63D403167DC08CF0);
    LIB_FUNCTION("eRVgwy9PbWg", "libSceHmd", 1, "libSceHmd", Hmd_Func_791560C32F4F6D68);
    LIB_FUNCTION("fJVZYeqFttM", "libSceHmd", 1, "libSceHmd", Hmd_Func_7C955961EA85B6D3);
    LIB_FUNCTION("mVIneDkja6c", "libSceHmd", 1, "libSceHmd", Hmd_Func_9952277839236BA7);
    LIB_FUNCTION("miduc55U7q8", "libSceHmd", 1, "libSceHmd", Hmd_Func_9A276E739E54EEAF);
    LIB_FUNCTION("nlAZlOKJy+c", "libSceHmd", 1, "libSceHmd", Hmd_Func_9E501994E289CBE7);
    LIB_FUNCTION("ox9NqLO9LhI", "libSceHmd", 1, "libSceHmd", Hmd_Func_A31F4DA8B3BD2E12);
    LIB_FUNCTION("qS18I6w2SZM", "libSceHmd", 1, "libSceHmd", Hmd_Func_A92D7C23AC364993);
    LIB_FUNCTION("rczCXLh2-b4", "libSceHmd", 1, "libSceHmd", Hmd_Func_ADCCC25CB876FDBE);
    LIB_FUNCTION("sWZSZB-mnw4", "libSceHmd", 1, "libSceHmd", Hmd_Func_B16652641FE69F0E);
    LIB_FUNCTION("-Bk71lPyry4", "libSceHmd", 1, "libSceHmd", Hmd_Func_FC193BD653F2AF2E);
    LIB_FUNCTION("-y4OUwFf4jE", "libSceHmd", 1, "libSceHmd", Hmd_Func_FF2E0E53015FE231);
    LIB_FUNCTION("gEokC+OGI8g", "libSceHmdDistortion", 1, "libSceHmd",
                 sceHmdDistortionGet2dVrCommand);
    LIB_FUNCTION("za4xJfzCBcM", "libSceHmd", 1, "libSceHmd", sceHmdDistortionGet2dVrCommand);
    LIB_FUNCTION("ER2ar8yUmbk", "libSceHmdDistortion", 1, "libSceHmd",
                 sceHmdDistortionGetCompoundEyeCorrectionCommand);
    LIB_FUNCTION("HT8qWOTOGmo", "libSceHmdDistortion", 1, "libSceHmd",
                 sceHmdDistortionGetCorrectionCommand);
    LIB_FUNCTION("grCYks4m8Jw", "libSceHmd", 1, "libSceHmd", sceHmdDistortionGetCorrectionCommand);
    LIB_FUNCTION("Vkkhy8RFIuk", "libSceHmdDistortion", 1, "libSceHmd",
                 sceHmdDistortionGetWideNearCorrectionCommand);
    LIB_FUNCTION("goi5ASvH-V8", "libSceHmd", 1, "libSceHmd",
                 sceHmdDistortionGetWideNearCorrectionCommand);
    LIB_FUNCTION("1cS7W5J-v3k", "libSceHmdDistortion", 1, "libSceHmd",
                 sceHmdDistortionGetWorkMemoryAlign);
    LIB_FUNCTION("8Ick-e6cDVY", "libSceHmd", 1, "libSceHmd", sceHmdDistortionGetWorkMemoryAlign);
    LIB_FUNCTION("36xDKk+Hw7o", "libSceHmdDistortion", 1, "libSceHmd",
                 sceHmdDistortionGetWorkMemorySize);
    LIB_FUNCTION("D5JfdpJKvXk", "libSceHmd", 1, "libSceHmd", sceHmdDistortionGetWorkMemorySize);
    LIB_FUNCTION("ao8NZ+FRYJE", "libSceHmdDistortion", 1, "libSceHmd", sceHmdDistortionInitialize);
    LIB_FUNCTION("8A4T5ahi790", "libSceHmdDistortion", 1, "libSceHmd",
                 sceHmdDistortionSetOutputMinColor);
    LIB_FUNCTION("mP2ZcYmDg-o", "libSceHmd", 1, "libSceHmd", sceHmdGetDistortionParams);
    LIB_FUNCTION("smQw6nT8PcA", "libSceHmdDistortion", 1, "libSceHmd", Hmd_Func_B26430EA74FC3DC0);
    LIB_FUNCTION("thTykLZ-tZs", "libSceHmd", 1, "libSceHmd", Hmd_Func_B614F290B67FB59B);
    LIB_FUNCTION("8gH1aLgty5I", "libsceHmdReprojectionMultilayer", 1, "libSceHmd",
                 sceHmdReprojectionStartMultilayer);
    LIB_FUNCTION("NTIbBpSH9ik", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionAddDisplayBuffer);
    LIB_FUNCTION("94+Ggm38KCg", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionClearUserEventEnd);
    LIB_FUNCTION("mdyFbaJj66M", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionClearUserEventStart);
    LIB_FUNCTION("MdV0akauNow", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionDebugGetLastInfo);
    LIB_FUNCTION("ymiwVjPB5+k", "libSceHmd", 1, "libSceHmd",
                 sceHmdReprojectionDebugGetLastInfoMultilayer);
    LIB_FUNCTION("ZrV5YIqD09I", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionFinalize);
    LIB_FUNCTION("utHD2Ab-Ixo", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionFinalizeCapture);
    LIB_FUNCTION("OuygGEWkins", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionInitialize);
    LIB_FUNCTION("BTrQnC6fcAk", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionInitializeCapture);
    LIB_FUNCTION("TkcANcGM0s8", "libSceHmd", 1, "libSceHmd",
                 sceHmdReprojectionQueryGarlicBuffAlign);
    LIB_FUNCTION("z0KtN1vqF2E", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionQueryGarlicBuffSize);
    LIB_FUNCTION("IWybWbR-xvA", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionQueryOnionBuffAlign);
    LIB_FUNCTION("kLUAkN6a1e8", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionQueryOnionBuffSize);
    LIB_FUNCTION("6CRWGc-evO4", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionSetCallback);
    LIB_FUNCTION("E+dPfjeQLHI", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionSetDisplayBuffers);
    LIB_FUNCTION("LjdLRysHU6Y", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionSetOutputMinColor);
    LIB_FUNCTION("knyIhlkpLgE", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionSetUserEventEnd);
    LIB_FUNCTION("7as0CjXW1B8", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionSetUserEventStart);
    LIB_FUNCTION("dntZTJ7meIU", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionStart);
    LIB_FUNCTION("q3e8+nEguyE", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionStart2dVr);
    LIB_FUNCTION("RrvyU1pjb9A", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionStartCapture);
    LIB_FUNCTION("XZ5QUzb4ae0", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionStartLiveCapture);
    LIB_FUNCTION("8gH1aLgty5I", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionStartMultilayer);
    LIB_FUNCTION("gqAG7JYeE7A", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionStartMultilayer2);
    LIB_FUNCTION("3JyuejcNhC0", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionStartWideNear);
    LIB_FUNCTION("mKa8scOc4-k", "libSceHmd", 1, "libSceHmd",
                 sceHmdReprojectionStartWideNearWithOverlay);
    LIB_FUNCTION("kcldQ7zLYQQ", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionStartWithOverlay);
    LIB_FUNCTION("vzMEkwBQciM", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionStop);
    LIB_FUNCTION("F7Sndm5teWw", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionStopCapture);
    LIB_FUNCTION("PAa6cUL5bR4", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionStopLiveCapture);
    LIB_FUNCTION("0wnZViigP9o", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionUnsetCallback);
    LIB_FUNCTION("iGNNpDDjcwo", "libSceHmd", 1, "libSceHmd", sceHmdReprojectionUnsetDisplayBuffers);
    LIB_FUNCTION("oxoDINgOrZk", "libSceHmd", 1, "libSceHmd", Hmd_Func_A31A0320D80EAD99);
    LIB_FUNCTION("uab6BzXsfkk", "libSceHmd", 1, "libSceHmd", Hmd_Func_B9A6FA0735EC7E49);
}

static void RegisterHmdSetupDialog(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("nmHzU4Gh0xs", "libSceHmdSetupDialog", 1, "libSceHmdSetupDialog",
                 sceHmdSetupDialogClose);
    LIB_FUNCTION("6lVRHMV5LY0", "libSceHmdSetupDialog", 1, "libSceHmdSetupDialog",
                 sceHmdSetupDialogGetResult);
    LIB_FUNCTION("J9eBpW1udl4", "libSceHmdSetupDialog", 1, "libSceHmdSetupDialog",
                 sceHmdSetupDialogGetStatus);
    LIB_FUNCTION("NB1Y2kA2jCY", "libSceHmdSetupDialog", 1, "libSceHmdSetupDialog",
                 sceHmdSetupDialogInitialize);
    LIB_FUNCTION("NNgiV4T+akU", "libSceHmdSetupDialog", 1, "libSceHmdSetupDialog",
                 sceHmdSetupDialogOpen);
    LIB_FUNCTION("+z4OJmFreZc", "libSceHmdSetupDialog", 1, "libSceHmdSetupDialog",
                 sceHmdSetupDialogTerminate);
    LIB_FUNCTION("Ud7j3+RDIBg", "libSceHmdSetupDialog", 1, "libSceHmdSetupDialog",
                 sceHmdSetupDialogUpdateStatus);
}

#if defined(ARCH_X86_64) || defined(SHADPS4_ENABLE_FEX_GUEST_CPU)
static void RegisterFiber(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("hVYD7Ou2pCQ", "libSceFiber", 1, "libSceFiber", sceFiberInitialize);
    LIB_FUNCTION("7+OJIpko9RY", "libSceFiber", 1, "libSceFiber",
                 sceFiberInitializeImpl); // _sceFiberInitializeWithInternalOptionImpl
    LIB_FUNCTION("asjUJJ+aa8s", "libSceFiber", 1, "libSceFiber", sceFiberOptParamInitialize);
    LIB_FUNCTION("JeNX5F-NzQU", "libSceFiber", 1, "libSceFiber", sceFiberFinalize);

    LIB_FUNCTION("a0LLrZWac0M", "libSceFiber", 1, "libSceFiber", sceFiberRun);
    LIB_FUNCTION("PFT2S-tJ7Uk", "libSceFiber", 1, "libSceFiber", sceFiberSwitch);
    LIB_FUNCTION("p+zLIOg27zU", "libSceFiber", 1, "libSceFiber", sceFiberGetSelf);
    LIB_FUNCTION("B0ZX2hx9DMw", "libSceFiber", 1, "libSceFiber", sceFiberReturnToThread);

    LIB_FUNCTION("avfGJ94g36Q", "libSceFiber", 1, "libSceFiber",
                 sceFiberRunImpl); // _sceFiberAttachContextAndRun
    LIB_FUNCTION("ZqhZFuzKT6U", "libSceFiber", 1, "libSceFiber",
                 sceFiberSwitchImpl); // _sceFiberAttachContextAndSwitch

    LIB_FUNCTION("uq2Y5BFz0PE", "libSceFiber", 1, "libSceFiber", sceFiberGetInfo);
    LIB_FUNCTION("Lcqty+QNWFc", "libSceFiber", 1, "libSceFiber", sceFiberStartContextSizeCheck);
    LIB_FUNCTION("Kj4nXMpnM8Y", "libSceFiber", 1, "libSceFiber", sceFiberStopContextSizeCheck);
    LIB_FUNCTION("JzyT91ucGDc", "libSceFiber", 1, "libSceFiber", sceFiberRename);

    LIB_FUNCTION("0dy4JtMUcMQ", "libSceFiber", 1, "libSceFiber",
                 sceFiberGetThreadFramePointerAddress);
}

#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
// A fiber entry's return address: the veneer of fiber_entry_returned, made once
extern "C" u64 core_hle_fiber_exit_address(void) {
    static const u64 address = Common::Singleton<Core::Linker>::Instance()->AllocateHleVeneer(
        Core::GuestCpu::MakeHleCallAdapter(&fiber_entry_returned), "libSceFiber#entry_returned");
    return address;
}
#endif
#endif

// The driver stopped (its present thread joined) when the program's statics are, as the
// driver itself was when it was a static of video_out.cpp
static struct VideoOutTeardown {
    bool started = false;
    ~VideoOutTeardown() {
        if (started) {
            video_out_library_deinit();
        }
    }
} s_video_out_teardown;

static void RegisterSaveData(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("dQ2GohUHXzk", "libSceSaveData", 1, "libSceSaveData", sceSaveDataAbort);
    LIB_FUNCTION("z1JA8-iJt3k", "libSceSaveData", 1, "libSceSaveData", sceSaveDataBackup);
    LIB_FUNCTION("kLJQ3XioYiU", "libSceSaveData", 1, "libSceSaveData", sceSaveDataBindPsnAccount);
    LIB_FUNCTION("hHHCPRqA3+g", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataBindPsnAccountForSystemBackup);
    LIB_FUNCTION("ykwIZfVD08s", "libSceSaveData", 1, "libSceSaveData", sceSaveDataChangeDatabase);
    LIB_FUNCTION("G0hFeOdRCUs", "libSceSaveData", 1, "libSceSaveData", sceSaveDataChangeInternal);
    LIB_FUNCTION("RQOqDbk3bSU", "libSceSaveData", 1, "libSceSaveData", sceSaveDataCheckBackupData);
    LIB_FUNCTION("rYvLW1z2poM", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataCheckBackupDataForCdlg);
    LIB_FUNCTION("v1TrX+3ZB10", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataCheckBackupDataInternal);
    LIB_FUNCTION("-eczr5e4dsI", "libSceSaveData", 1, "libSceSaveData", sceSaveDataCheckCloudData);
    LIB_FUNCTION("4OPOZxfVkHA", "libSceSaveData", 1, "libSceSaveData", sceSaveDataCheckIpmiIfSize);
    LIB_FUNCTION("1i0rfc+mfa8", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataCheckSaveDataBroken);
    LIB_FUNCTION("p6A1adyQi3E", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataCheckSaveDataVersion);
    LIB_FUNCTION("S49B+I96kpk", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataCheckSaveDataVersionLatest);
    LIB_FUNCTION("Wz-4JZfeO9g", "libSceSaveData", 1, "libSceSaveData", sceSaveDataClearProgress);
    LIB_FUNCTION("YbCO38BOOl4", "libSceSaveData", 1, "libSceSaveData", sceSaveDataCopy5);
    LIB_FUNCTION("kbIIP9aXK9A", "libSceSaveData", 1, "libSceSaveData", sceSaveDataCreateUploadData);
    LIB_FUNCTION("gW6G4HxBBXA", "libSceSaveData", 1, "libSceSaveData", sceSaveDataDebug);
    LIB_FUNCTION("bYCnxLexU7M", "libSceSaveData", 1, "libSceSaveData", sceSaveDataDebugCleanMount);
    LIB_FUNCTION("hVDqYB8+jkk", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataDebugCompiledSdkVersion);
    LIB_FUNCTION("K9gXXlrVLNI", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataDebugCreateSaveDataRoot);
    LIB_FUNCTION("5yHFvMwZX2o", "libSceSaveData", 1, "libSceSaveData", sceSaveDataDebugGetThreadId);
    LIB_FUNCTION("UGTldPVEdB4", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataDebugRemoveSaveDataRoot);
    LIB_FUNCTION("AYBQmnRplrg", "libSceSaveData", 1, "libSceSaveData", sceSaveDataDebugTarget);
    LIB_FUNCTION("S1GkePI17zQ", "libSceSaveData", 1, "libSceSaveData", sceSaveDataDelete);
    LIB_FUNCTION("SQWusLoK8Pw", "libSceSaveData", 1, "libSceSaveData", sceSaveDataDelete5);
    LIB_FUNCTION("pJrlpCgR8h4", "libSceSaveData", 1, "libSceSaveData", sceSaveDataDeleteAllUser);
    LIB_FUNCTION("fU43mJUgKcM", "libSceSaveData", 1, "libSceSaveData", sceSaveDataDeleteCloudData);
    LIB_FUNCTION("uZqc4JpFdeY", "libSceSaveData", 1, "libSceSaveData", sceSaveDataDeleteUser);
    LIB_FUNCTION("dyIhnXq-0SM", "libSceSaveData", 1, "libSceSaveData", sceSaveDataDirNameSearch);
    LIB_FUNCTION("xJ5NFWC3m+k", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataDirNameSearchInternal);
    LIB_FUNCTION("h1nP9EYv3uc", "libSceSaveData", 1, "libSceSaveData", sceSaveDataDownload);
    LIB_FUNCTION("A1ThglSGUwA", "libSceSaveData", 1, "libSceSaveData", sceSaveDataGetAllSize);
    LIB_FUNCTION("KuXcrMAQIMQ", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataGetAppLaunchedUser);
    LIB_FUNCTION("itZ46iH14Vs", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataGetAutoUploadConditions);
    LIB_FUNCTION("PL20kjAXZZ4", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataGetAutoUploadRequestInfo);
    LIB_FUNCTION("G12foE0S77E", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataGetAutoUploadSetting);
    LIB_FUNCTION("PzDtD6eBXIM", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataGetBoundPsnAccountCount);
    LIB_FUNCTION("tu0SDPl+h88", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataGetClientThreadPriority);
    LIB_FUNCTION("6lZYZqQPfkY", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataGetCloudQuotaInfo);
    LIB_FUNCTION("CWlBd2Ay1M4", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataGetDataBaseFilePath);
    LIB_FUNCTION("eBSSNIG6hMk", "libSceSaveData", 1, "libSceSaveData", sceSaveDataGetEventInfo);
    LIB_FUNCTION("j8xKtiFj0SY", "libSceSaveData", 1, "libSceSaveData", sceSaveDataGetEventResult);
    LIB_FUNCTION("UMpxor4AlKQ", "libSceSaveData", 1, "libSceSaveData", sceSaveDataGetFormat);
    LIB_FUNCTION("pc4guaUPVqA", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataGetMountedSaveDataCount);
    LIB_FUNCTION("65VH0Qaaz6s", "libSceSaveData", 1, "libSceSaveData", sceSaveDataGetMountInfo);
    LIB_FUNCTION("XgvSuIdnMlw", "libSceSaveData", 1, "libSceSaveData", sceSaveDataGetParam);
    LIB_FUNCTION("ANmSWUiyyGQ", "libSceSaveData", 1, "libSceSaveData", sceSaveDataGetProgress);
    LIB_FUNCTION("SN7rTPHS+Cg", "libSceSaveData", 1, "libSceSaveData", sceSaveDataGetSaveDataCount);
    LIB_FUNCTION("7Bt5pBC-Aco", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataGetSaveDataMemory);
    LIB_FUNCTION("QwOO7vegnV8", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataGetSaveDataMemory2);
    LIB_FUNCTION("+bRDRotfj0Y", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataGetSaveDataRootDir);
    LIB_FUNCTION("3luF0xq0DkQ", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataGetSaveDataRootPath);
    LIB_FUNCTION("DwAvlQGvf1o", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataGetSaveDataRootUsbPath);
    LIB_FUNCTION("kb24-4DLyNo", "libSceSaveData", 1, "libSceSaveData", sceSaveDataGetSavePoint);
    LIB_FUNCTION("OYmnApJ9q+U", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataGetUpdatedDataCount);
    LIB_FUNCTION("ZkZhskCPXFw", "libSceSaveData", 1, "libSceSaveData", sceSaveDataInitialize);
    LIB_FUNCTION("l1NmDeDpNGU", "libSceSaveData", 1, "libSceSaveData", sceSaveDataInitialize2);
    LIB_FUNCTION("TywrFKCoLGY", "libSceSaveData", 1, "libSceSaveData", sceSaveDataInitialize3);
    LIB_FUNCTION("g9uwUI3BlQ8", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataInitializeForCdlg);
    LIB_FUNCTION("voAQW45oKuo", "libSceSaveData", 1, "libSceSaveData", sceSaveDataIsDeletingUsbDb);
    LIB_FUNCTION("ieP6jP138Qo", "libSceSaveData", 1, "libSceSaveData", sceSaveDataIsMounted);
    LIB_FUNCTION("cGjO3wM3V28", "libSceSaveData", 1, "libSceSaveData", sceSaveDataLoadIcon);
    LIB_FUNCTION("32HQAQdwM2o", "libSceSaveData", 1, "libSceSaveData", sceSaveDataMount);
    LIB_FUNCTION("0z45PIH+SNI", "libSceSaveData", 1, "libSceSaveData", sceSaveDataMount2);
    LIB_FUNCTION("xz0YMi6BfNk", "libSceSaveData", 1, "libSceSaveData", sceSaveDataMount5);
    LIB_FUNCTION("msCER7Iibm8", "libSceSaveData", 1, "libSceSaveData", sceSaveDataMountInternal);
    LIB_FUNCTION("-XYmdxjOqyA", "libSceSaveData", 1, "libSceSaveData", sceSaveDataMountSys);
    LIB_FUNCTION("uNu7j3pL2mQ", "libSceSaveData", 1, "libSceSaveData", sceSaveDataPromote5);
    LIB_FUNCTION("SgIY-XYA2Xg", "libSceSaveData", 1, "libSceSaveData", sceSaveDataRebuildDatabase);
    LIB_FUNCTION("hsKd5c21sQc", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataRegisterEventCallback);
    LIB_FUNCTION("lU9YRFsgwSU", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataRestoreBackupData);
    LIB_FUNCTION("HuToUt1GQ8w", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataRestoreBackupDataForCdlg);
    LIB_FUNCTION("aoZKKNjlq3Y", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataRestoreLoadSaveDataMemory);
    LIB_FUNCTION("c88Yy54Mx0w", "libSceSaveData", 1, "libSceSaveData", sceSaveDataSaveIcon);
    LIB_FUNCTION("0VFHv-Fa4w8", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataSetAutoUploadSetting);
    LIB_FUNCTION("52pL2GKkdjA", "libSceSaveData", 1, "libSceSaveData", sceSaveDataSetEventInfo);
    LIB_FUNCTION("85zul--eGXs", "libSceSaveData", 1, "libSceSaveData", sceSaveDataSetParam);
    LIB_FUNCTION("v3vg2+cooYw", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataSetSaveDataLibraryUser);
    LIB_FUNCTION("h3YURzXGSVQ", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataSetSaveDataMemory);
    LIB_FUNCTION("cduy9v4YmT4", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataSetSaveDataMemory2);
    LIB_FUNCTION("v7AAAMo0Lz4", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataSetupSaveDataMemory);
    LIB_FUNCTION("oQySEUfgXRA", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataSetupSaveDataMemory2);
    LIB_FUNCTION("zMgXM79jRhw", "libSceSaveData", 1, "libSceSaveData", sceSaveDataShutdownStart);
    LIB_FUNCTION("+orZm32HB1s", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataSupportedFakeBrokenStatus);
    LIB_FUNCTION("LMSQUTxmGVg", "libSceSaveData", 1, "libSceSaveData", sceSaveDataSyncCloudList);
    LIB_FUNCTION("wiT9jeC7xPw", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataSyncSaveDataMemory);
    LIB_FUNCTION("yKDy8S5yLA0", "libSceSaveData", 1, "libSceSaveData", sceSaveDataTerminate);
    LIB_FUNCTION("WAzWTZm1H+I", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataTransferringMount);
    LIB_FUNCTION("BMR4F-Uek3E", "libSceSaveData", 1, "libSceSaveData", sceSaveDataUmount);
    LIB_FUNCTION("2-8NWLS8QSA", "libSceSaveData", 1, "libSceSaveData", sceSaveDataUmountSys);
    LIB_FUNCTION("VwadwBBBJ80", "libSceSaveData", 1, "libSceSaveData", sceSaveDataUmountWithBackup);
    LIB_FUNCTION("v-AK1AxQhS0", "libSceSaveData", 1, "libSceSaveData",
                 sceSaveDataUnregisterEventCallback);
    LIB_FUNCTION("COwz3WBj+5s", "libSceSaveData", 1, "libSceSaveData", sceSaveDataUpload);
    LIB_FUNCTION("AuTE0gFxZCI", "libSceSaveData", 1, "libSceSaveData",
                 SaveData_Func_02E4C4D201716422);
}

static void RegisterSaveDataDialog(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("fH46Lag88XY", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogClose);
    LIB_FUNCTION("yEiJ-qqr6Cg", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogGetResult);
    LIB_FUNCTION("ERKzksauAJA", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogGetStatus);
    LIB_FUNCTION("s9e3+YpRnzw", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogInitialize);
    LIB_FUNCTION("en7gNVnh878", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogIsReadyToDisplay);
    LIB_FUNCTION("4tPhsP6FpDI", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogOpen);
    LIB_FUNCTION("V-uEeFKARJU", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogProgressBarInc);
    LIB_FUNCTION("hay1CfTmLyA", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogProgressBarSetValue);
    LIB_FUNCTION("YuH2FA7azqQ", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogTerminate);
    LIB_FUNCTION("KK3Bdg1RWK0", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogUpdateStatus);
}

static void RegisterAjm(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("NVDXiUesSbA", "libSceAjm", 1, "libSceAjm", sceAjmBatchCancel);
    LIB_FUNCTION("WfAiBW8Wcek", "libSceAjm", 1, "libSceAjm", sceAjmBatchErrorDump);
    LIB_FUNCTION("dmDybN--Fn8", "libSceAjm", 1, "libSceAjm", sceAjmBatchJobControlBufferRa);
    LIB_FUNCTION("stlghnic3Jc", "libSceAjm", 1, "libSceAjm", sceAjmBatchJobInlineBuffer);
    LIB_FUNCTION("ElslOCpOIns", "libSceAjm", 1, "libSceAjm", sceAjmBatchJobRunBufferRa);
    LIB_FUNCTION("7jdAXK+2fMo", "libSceAjm", 1, "libSceAjm", sceAjmBatchJobRunSplitBufferRa);
    LIB_FUNCTION("fFFkk0xfGWs", "libSceAjm", 1, "libSceAjm", sceAjmBatchStartBuffer);
    LIB_FUNCTION("-qLsfDAywIY", "libSceAjm", 1, "libSceAjm", sceAjmBatchWait);
    LIB_FUNCTION("1t3ixYNXyuc", "libSceAjm", 1, "libSceAjm", sceAjmDecAt9ParseConfigData);
    LIB_FUNCTION("eDFeTyi+G3Y", "libSceAjm", 1, "libSceAjm", sceAjmDecMp3ParseFrame);
    LIB_FUNCTION("MHur6qCsUus", "libSceAjm", 1, "libSceAjm", sceAjmFinalize);
    LIB_FUNCTION("dl+4eHSzUu4", "libSceAjm", 1, "libSceAjm", sceAjmInitialize);
    LIB_FUNCTION("diXjQNiMu-s", "libSceAjm", 1, "libSceAjm", sceAjmInstanceCodecType);
    LIB_FUNCTION("AxoDrINp4J8", "libSceAjm", 1, "libSceAjm", sceAjmInstanceCreate);
    LIB_FUNCTION("RbLbuKv8zho", "libSceAjm", 1, "libSceAjm", sceAjmInstanceDestroy);
    LIB_FUNCTION("YDFR0dDVGAg", "libSceAjm", 1, "libSceAjm", sceAjmInstanceExtend);
    LIB_FUNCTION("rgLjmfdXocI", "libSceAjm", 1, "libSceAjm", sceAjmInstanceSwitch);
    LIB_FUNCTION("bkRHEYG6lEM", "libSceAjm", 1, "libSceAjm", sceAjmMemoryRegister);
    LIB_FUNCTION("pIpGiaYkHkM", "libSceAjm", 1, "libSceAjm", sceAjmMemoryUnregister);
    LIB_FUNCTION("Q3dyFuwGn64", "libSceAjm", 1, "libSceAjm", sceAjmModuleRegister);
    LIB_FUNCTION("Wi7DtlLV+KI", "libSceAjm", 1, "libSceAjm", sceAjmModuleUnregister);
    LIB_FUNCTION("AxhcqVv5AYU", "libSceAjm", 1, "libSceAjm", sceAjmStrError);
}

static void RegisterAudioOut(Core::Loader::SymbolsResolver* sym) {
    audio_out_library_init();
    LIB_FUNCTION("cx2dYFbzIAg", "libSceAudioOutDeviceService", 1, "libSceAudioOut",
                 sceAudioOutDeviceIdOpen);
    LIB_FUNCTION("tKumjQSzhys", "libSceAudioDeviceControl", 1, "libSceAudioOut",
                 sceAudioDeviceControlGet);
    LIB_FUNCTION("5ChfcHOf3SM", "libSceAudioDeviceControl", 1, "libSceAudioOut",
                 sceAudioDeviceControlSet);
    LIB_FUNCTION("Iz9X7ISldhs", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutA3dControl);
    LIB_FUNCTION("9RVIoocOVAo", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutA3dExit);
    LIB_FUNCTION("n7KgxE8rOuE", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutA3dInit);
    LIB_FUNCTION("WBAO6-n0-4M", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutAttachToApplicationByPid);
    LIB_FUNCTION("O3FM2WXIJaI", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutChangeAppModuleState);
    LIB_FUNCTION("s1--uE9mBFw", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutClose);
    LIB_FUNCTION("ol4LbeTG8mc", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutDetachFromApplicationByPid);
    LIB_FUNCTION("r1V9IFEE+Ts", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutExConfigureOutputMode);
    LIB_FUNCTION("wZakRQsWGos", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutExGetSystemInfo);
    LIB_FUNCTION("xjjhT5uw08o", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutExPtClose);
    LIB_FUNCTION("DsST7TNsyfo", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutExPtGetLastOutputTime);
    LIB_FUNCTION("4UlW3CSuCa4", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutExPtOpen);
    LIB_FUNCTION("Xcj8VTtnZw0", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutExSystemInfoIsSupportedAudioOutExMode);
    LIB_FUNCTION("I3Fwcmkg5Po", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutGetFocusEnablePid);
    LIB_FUNCTION("Y3lXfCFEWFY", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutGetHandleStatusInfo);
    LIB_FUNCTION("-00OAutAw+c", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutGetInfo);
    LIB_FUNCTION("RqmKxBqB8B4", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutGetInfoOpenNum);
    LIB_FUNCTION("Ptlts326pds", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutGetLastOutputTime);
    LIB_FUNCTION("GrQ9s4IrNaQ", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutGetPortState);
    LIB_FUNCTION("c7mVozxJkPU", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutGetSimulatedBusUsableStatusByBusType);
    LIB_FUNCTION("pWmS7LajYlo", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutGetSimulatedHandleStatusInfo);
    LIB_FUNCTION("oPLghhAWgMM", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutGetSimulatedHandleStatusInfo2);
    LIB_FUNCTION("5+r7JYHpkXg", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutGetSparkVss);
    LIB_FUNCTION("R5hemoKKID8", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutGetSystemState);
    LIB_FUNCTION("JfEPXVxhFqA", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutInit);
    LIB_FUNCTION("n16Kdoxnvl0", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutInitIpmiGetSession);
    LIB_FUNCTION("r+qKw+ueD+Q", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutMasteringGetState);
    LIB_FUNCTION("xX4RLegarbg", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutMasteringInit);
    LIB_FUNCTION("4055yaUg3EY", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutMasteringSetParam);
    LIB_FUNCTION("RVWtUgoif5o", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutMasteringTerm);
    LIB_FUNCTION("-LXhcGARw3k", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutMbusInit);
    LIB_FUNCTION("ekNvsT22rsY", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutOpen);
    LIB_FUNCTION("qLpSK75lXI4", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutOpenEx);
    LIB_FUNCTION("QOQtbeDqsT4", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutOutput);
    LIB_FUNCTION("w3PdaSTSwGE", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutOutputs);
    LIB_FUNCTION("MapHTgeogbk", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutPtClose);
    LIB_FUNCTION("YZaq+UKbriQ", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutPtGetLastOutputTime);
    LIB_FUNCTION("xyT8IUCL3CI", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutPtOpen);
    LIB_FUNCTION("o4OLQQqqA90", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutSetConnections);
    LIB_FUNCTION("QHq2ylFOZ0k", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutSetConnectionsForUser);
    LIB_FUNCTION("r9KGqGpwTpg", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutSetDevConnection);
    LIB_FUNCTION("08MKi2E-RcE", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutSetHeadphoneOutMode);
    LIB_FUNCTION("18IVGrIQDU4", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutSetJediJackVolume);
    LIB_FUNCTION("h0o+D4YYr1k", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutSetJediSpkVolume);
    LIB_FUNCTION("KI9cl22to7E", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutSetMainOutput);
    LIB_FUNCTION("wVwPU50pS1c", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutSetMixLevelPadSpk);
    LIB_FUNCTION("eeRsbeGYe20", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutSetMorpheusParam);
    LIB_FUNCTION("IZrItPnflBM", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutSetMorpheusWorkingMode);
    LIB_FUNCTION("Gy0ReOgXW00", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutSetPortConnections);
    LIB_FUNCTION("oRBFflIrCg0", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutSetPortStatuses);
    LIB_FUNCTION("ae-IVPMSWjU", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutSetRecMode);
    LIB_FUNCTION("d3WL2uPE1eE", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutSetSparkParam);
    LIB_FUNCTION("X7Cfsiujm8Y", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutSetUsbVolume);
    LIB_FUNCTION("b+uAV89IlxE", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutSetVolume);
    LIB_FUNCTION("rho9DH-0ehs", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutSetVolumeDown);
    LIB_FUNCTION("I91P0HAPpjw", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutStartAuxBroadcast);
    LIB_FUNCTION("uo+eoPzdQ-s", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutStartSharePlay);
    LIB_FUNCTION("AImiaYFrKdc", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutStopAuxBroadcast);
    LIB_FUNCTION("teCyKKZPjME", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutStopSharePlay);
    LIB_FUNCTION("95bdtHdNUic", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutSuspendResume);
    LIB_FUNCTION("oRJZnXxok-M", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutSysConfigureOutputMode);
    LIB_FUNCTION("Tf9-yOJwF-A", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutSysGetHdmiMonitorInfo);
    LIB_FUNCTION("y2-hP-KoTMI", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutSysGetSystemInfo);
    LIB_FUNCTION("YV+bnMvMfYg", "libSceAudioOut", 1, "libSceAudioOut",
                 sceAudioOutSysHdmiMonitorInfoIsSupportedAudioOutMode);
    LIB_FUNCTION("JEHhANREcLs", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutSystemControlGet);
    LIB_FUNCTION("9CHWVv6r3Dg", "libSceAudioOut", 1, "libSceAudioOut", sceAudioOutSystemControlSet);
    LIB_FUNCTION("Mt7JB3lOyJk", "libSceAudioOutSparkControl", 1, "libSceAudioOut",
                 sceAudioOutSparkControlSetEqCoef);
    LIB_FUNCTION("7UsdDOEvjlk", "libSceDbgAudioOut", 1, "libSceAudioOut",
                 sceAudioOutSetSystemDebugState);
}

static void RegisterAudioIn(Core::Loader::SymbolsResolver* sym) {
    audio_in_library_init();
    LIB_FUNCTION("IQtWgnrw6v8", "libSceAudioIn", 1, "libSceAudioIn",
                 sceAudioInChangeAppModuleState);
    LIB_FUNCTION("Jh6WbHhnI68", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInClose);
    LIB_FUNCTION("8mtcsG-Qp5E", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInCountPorts);
    LIB_FUNCTION("5qRVfxOmbno", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInDeviceHqOpen);
    LIB_FUNCTION("gUNabrUkZNg", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInDeviceIdHqOpen);
    LIB_FUNCTION("X-AQLtdxQOo", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInDeviceIdOpen);
    LIB_FUNCTION("VoX9InuwwTg", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInDeviceOpen);
    LIB_FUNCTION("48-miagyJ2I", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInDeviceOpenEx);
    LIB_FUNCTION("kFKJ3MVcDuo", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInExtClose);
    LIB_FUNCTION("mhAfefP9m2g", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInExtCtrl);
    LIB_FUNCTION("KpBKoHKVKEc", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInExtInput);
    LIB_FUNCTION("YZ+3seW7CyY", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInExtOpen);
    LIB_FUNCTION("FVGWf8JaHOE", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInExtSetAecMode);
    LIB_FUNCTION("S-rDUfQk9sg", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInGetGain);
    LIB_FUNCTION("NJam1-F7lNY", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInGetHandleStatusInfo);
    LIB_FUNCTION("3shKmTrTw6c", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInGetRerouteCount);
    LIB_FUNCTION("BohEAQ7DlUE", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInGetSilentState);
    LIB_FUNCTION("nya-R5gDYhM", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInHqOpen);
    LIB_FUNCTION("CTh72m+IYbU", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInHqOpenEx);
    LIB_FUNCTION("SxQprgjttKE", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInInit);
    LIB_FUNCTION("LozEOU8+anM", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInInput);
    LIB_FUNCTION("rmgXsZ-2Tyk", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInInputs);
    LIB_FUNCTION("6QP1MzdFWhs", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInIsSharedDevice);
    LIB_FUNCTION("5NE8Sjc7VC8", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInOpen);
    LIB_FUNCTION("+DY07NwJb0s", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInOpenEx);
    LIB_FUNCTION("vYFsze1SqU8", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInSetAllMute);
    LIB_FUNCTION("vyh-T6sMqnw", "libSceAudioIn", 1, "libSceAudioIn",
                 sceAudioInSetCompressorPreGain);
    LIB_FUNCTION("YeBSNVAELe4", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInSetConnections);
    LIB_FUNCTION("thLNHvkWSeg", "libSceAudioIn", 1, "libSceAudioIn",
                 sceAudioInSetConnectionsForUser);
    LIB_FUNCTION("rcgv2ciDrtc", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInSetDevConnection);
    LIB_FUNCTION("iN3KqF-8R-w", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInSetFocusForUser);
    LIB_FUNCTION("VAzfxqDwbQ0", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInSetMode);
    LIB_FUNCTION("CwBFvAlOv7k", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInSetMode2);
    LIB_FUNCTION("tQpOPpYwv7o", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInSetPortConnections);
    LIB_FUNCTION("NUWqWguYcNQ", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInSetPortStatuses);
    LIB_FUNCTION("U0ivfdKFZbA", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInSetSparkParam);
    LIB_FUNCTION("hWMCAPpqzDo", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInSetSparkSideTone);
    LIB_FUNCTION("nqXpw3MaN50", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInSetUsbGain);
    LIB_FUNCTION("arJp991xk5k", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInSetUserMute);
    LIB_FUNCTION("DVTn+iMSpBM", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInVmicCreate);
    LIB_FUNCTION("3ULZGIl+Acc", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInVmicDestroy);
    LIB_FUNCTION("4kHw99LUG3A", "libSceAudioIn", 1, "libSceAudioIn", sceAudioInVmicWrite);
}

static void RegisterVideoOut(Core::Loader::SymbolsResolver* sym) {
    video_out_library_init(emu_settings_get_internal_screen_width(),
                           emu_settings_get_internal_screen_height());
    s_video_out_teardown.started = true;

    LIB_FUNCTION("SbU3dwp80lQ", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutGetFlipStatus);
    LIB_FUNCTION("U46NwOiJpys", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutSubmitFlip);
    LIB_FUNCTION("w3BY+tAEiQY", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutRegisterBuffers);
    LIB_FUNCTION("HXzjK9yI30k", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutAddFlipEvent);
    LIB_FUNCTION("Xru92wHJRmg", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutAddVblankEvent);
    LIB_FUNCTION("CBiu4mCE1DA", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutSetFlipRate);
    LIB_FUNCTION("i6-sR91Wt-4", "libSceVideoOut", 1, "libSceVideoOut",
                 sceVideoOutSetBufferAttribute);
    LIB_FUNCTION("6kPnj51T62Y", "libSceVideoOut", 1, "libSceVideoOut",
                 sceVideoOutGetResolutionStatus);
    LIB_FUNCTION("8XGijEoThE0", "libSceVideoOut", 1, "libSceVideoOut",
                 sceVideoOutGetResolutionStatus); // sceVideoOutSysGetResolutionStatus
    LIB_FUNCTION("Ek+VR4lcJQI", "libSceVideoOut", 1, "libSceVideoOut",
                 sceVideoOutAddVblankEvent); // sceVideoOutSysAddVblankEvent
    LIB_FUNCTION("Up36PTk687E", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutOpen);
    LIB_FUNCTION("zgXifHT9ErY", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutIsFlipPending);
    LIB_FUNCTION("N5KDtkIjjJ4", "libSceVideoOut", 1, "libSceVideoOut",
                 sceVideoOutUnregisterBuffers);
    LIB_FUNCTION("OcQybQejHEY", "libSceVideoOut", 1, "libSceVideoOut",
                 sceVideoOutGetBufferLabelAddress);
    LIB_FUNCTION("uquVH4-Du78", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutClose);
    LIB_FUNCTION("1FZBKy8HeNU", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutGetVblankStatus);
    LIB_FUNCTION("d1AjT2uZJn0", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutGetVblankStatus);
    LIB_FUNCTION("kGVLc3htQE8", "libSceVideoOut", 1, "libSceVideoOut",
                 sceVideoOutGetDeviceCapabilityInfo);
    LIB_FUNCTION("j6RaAUlaLv0", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutWaitVblank);
    LIB_FUNCTION("U2JJtSqNKZI", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutGetEventId);
    LIB_FUNCTION("rWUTcKdkUzQ", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutGetEventData);
    LIB_FUNCTION("Mt4QHHkxkOc", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutGetEventCount);
    LIB_FUNCTION("DYhhWbJSeRg", "libSceVideoOut", 1, "libSceVideoOut",
                 sceVideoOutColorSettingsSetGamma);
    LIB_FUNCTION("pv9CI5VC+R0", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutAdjustColor);
    LIB_FUNCTION("-Ozn0F1AFRg", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutDeleteFlipEvent);
    LIB_FUNCTION("oNOQn3knW6s", "libSceVideoOut", 1, "libSceVideoOut",
                 sceVideoOutDeleteVblankEvent);
    LIB_FUNCTION("pjkDsgxli6c", "libSceVideoOut", 1, "libSceVideoOut", sceVideoOutModeSetAny_);
    LIB_FUNCTION("N1bEoJ4SRw4", "libSceVideoOut", 1, "libSceVideoOut",
                 sceVideoOutConfigureOutputMode_);
    LIB_FUNCTION("MTxxrOCeSig", "libSceVideoOut", 1, "libSceVideoOut",
                 sceVideoOutSetWindowModeMargins);
    LIB_FUNCTION("IOdgHlCGU-k", "libSceVideoOut", 1, "libSceVideoOut",
                 sceVideoOutSubmitChangeBufferAttribute);
}

static void RegisterUsbStorage(Core::Loader::SymbolsResolver* sym) {
    usb_storage_library_init();
    LIB_FUNCTION("BDDZwF5kuTc", "libSceUsbStorage", 1, "libSceUsbStorage", sceUsbStorageInit);
    LIB_FUNCTION("Wp8zHTocS5E", "libSceUsbStorage", 1, "libSceUsbStorage", sceUsbStorageTerm);
    LIB_FUNCTION("vFkdkzJgSpw", "libSceUsbStorage", 1, "libSceUsbStorage",
                 sceUsbStorageRegisterCallback);
    LIB_FUNCTION("+Ib-MHNUf80", "libSceUsbStorage", 1, "libSceUsbStorage",
                 sceUsbStorageUnregisterCallback);
    LIB_FUNCTION("IDYJZSeBgDs", "libSceUsbStorage", 1, "libSceUsbStorage", sceUsbStorageRequestMap);
    LIB_FUNCTION("rx7EcAS2ARk", "libSceUsbStorage", 1, "libSceUsbStorage",
                 sceUsbStorageRequestMapWSB);
    LIB_FUNCTION("rx7EcAS2ARk", "libSceUsbStorageAux", 1, "libSceUsbStorage",
                 sceUsbStorageRequestMapWSB);
    LIB_FUNCTION("fl3roYs7F9U", "libSceUsbStorage", 1, "libSceUsbStorage",
                 sceUsbStorageRequestUnmap);
    LIB_FUNCTION("mryrNITeYvI", "libSceUsbStorage", 1, "libSceUsbStorage",
                 sceUsbStorageGetDeviceList);
    LIB_FUNCTION("-GvBqz54ssU", "libSceUsbStorage", 1, "libSceUsbStorage",
                 sceUsbStorageGetDeviceInfo);
    LIB_FUNCTION("tO8DvyElInw", "libSceUsbStorage", 1, "libSceUsbStorage", sceUsbStorageIsExist);
    LIB_FUNCTION("0rG6xtn7N5Q", "libSceUsbStorage", 1, "libSceUsbStorage",
                 sceUsbStorageRegisterCallbackForMapAvailable);
    LIB_FUNCTION("C3ETNYXsht4", "libSceUsbStorage", 1, "libSceUsbStorage",
                 sceUsbStorageUnregisterCallbackForMapAvailable);
    LIB_FUNCTION("8mpZuu7xfbM", "libSceUsbStorage", 1, "libSceUsbStorage",
                 sceUsbStorageRegisterCallbackForMapUnavailable);
    LIB_FUNCTION("4YMBk1lfUm0", "libSceUsbStorage", 1, "libSceUsbStorage",
                 sceUsbStorageUnregisterCallbackForMapUnavailable);
    LIB_FUNCTION("2LrlpFmBTC8", "libSceUsbStorage", 1, "libSceUsbStorage",
                 sceUsbStorageGetdentsOpen);
    LIB_FUNCTION("2LrlpFmBTC8", "libSceUsbStorageAux", 1, "libSceUsbStorage",
                 sceUsbStorageGetdentsOpen);
    LIB_FUNCTION("OeJdPEmLYX4", "libSceUsbStorage", 1, "libSceUsbStorage",
                 sceUsbStorageGetdentsRead);
    LIB_FUNCTION("OeJdPEmLYX4", "libSceUsbStorageAux", 1, "libSceUsbStorage",
                 sceUsbStorageGetdentsRead);
    LIB_FUNCTION("w1mZOJCxvhQ", "libSceUsbStorage", 1, "libSceUsbStorage",
                 sceUsbStorageGetdentsClose);
    LIB_FUNCTION("w1mZOJCxvhQ", "libSceUsbStorageAux", 1, "libSceUsbStorage",
                 sceUsbStorageGetdentsClose);
}

static void RegisterUsbd(Core::Loader::SymbolsResolver* sym) {
    usbd_library_init();
    LIB_FUNCTION("0ktE1PhzGFU", "libSceUsbd", 1, "libSceUsbd", sceUsbdAllocTransfer);
    LIB_FUNCTION("BKMEGvfCPyU", "libSceUsbd", 1, "libSceUsbd", sceUsbdAttachKernelDriver);
    LIB_FUNCTION("fotb7DzeHYw", "libSceUsbd", 1, "libSceUsbd", sceUsbdBulkTransfer);
    LIB_FUNCTION("-KNh1VFIzlM", "libSceUsbd", 1, "libSceUsbd", sceUsbdCancelTransfer);
    LIB_FUNCTION("MlW6deWfPp0", "libSceUsbd", 1, "libSceUsbd", sceUsbdCheckConnected);
    LIB_FUNCTION("AE+mHBHneyk", "libSceUsbd", 1, "libSceUsbd", sceUsbdClaimInterface);
    LIB_FUNCTION("3tPPMo4QRdY", "libSceUsbd", 1, "libSceUsbd", sceUsbdClearHalt);
    LIB_FUNCTION("HarYYlaFGJY", "libSceUsbd", 1, "libSceUsbd", sceUsbdClose);
    LIB_FUNCTION("RRKFcKQ1Ka4", "libSceUsbd", 1, "libSceUsbd", sceUsbdControlTransfer);
    LIB_FUNCTION("XUWtxI31YEY", "libSceUsbd", 1, "libSceUsbd", sceUsbdControlTransferGetData);
    LIB_FUNCTION("SEdQo8CFmus", "libSceUsbd", 1, "libSceUsbd", sceUsbdControlTransferGetSetup);
    LIB_FUNCTION("Y5go+ha6eDs", "libSceUsbd", 1, "libSceUsbd", sceUsbdDetachKernelDriver);
    LIB_FUNCTION("Vw8Hg1CN028", "libSceUsbd", 1, "libSceUsbd", sceUsbdEventHandlerActive);
    LIB_FUNCTION("e7gp1xhu6RI", "libSceUsbd", 1, "libSceUsbd", sceUsbdEventHandlingOk);
    LIB_FUNCTION("Fq6+0Fm55xU", "libSceUsbd", 1, "libSceUsbd", sceUsbdExit);
    LIB_FUNCTION("oHCade-0qQ0", "libSceUsbd", 1, "libSceUsbd", sceUsbdFillBulkTransfer);
    LIB_FUNCTION("8KrqbaaPkE0", "libSceUsbd", 1, "libSceUsbd", sceUsbdFillControlSetup);
    LIB_FUNCTION("7VGfMerK6m0", "libSceUsbd", 1, "libSceUsbd", sceUsbdFillControlTransfer);
    LIB_FUNCTION("t3J5pXxhJlI", "libSceUsbd", 1, "libSceUsbd", sceUsbdFillInterruptTransfer);
    LIB_FUNCTION("xqmkjHCEOSY", "libSceUsbd", 1, "libSceUsbd", sceUsbdFillIsoTransfer);
    LIB_FUNCTION("Hvd3S--n25w", "libSceUsbd", 1, "libSceUsbd", sceUsbdFreeConfigDescriptor);
    LIB_FUNCTION("EQ6SCLMqzkM", "libSceUsbd", 1, "libSceUsbd", sceUsbdFreeDeviceList);
    LIB_FUNCTION("-sgi7EeLSO8", "libSceUsbd", 1, "libSceUsbd", sceUsbdFreeTransfer);
    LIB_FUNCTION("S1o1C6yOt5g", "libSceUsbd", 1, "libSceUsbd", sceUsbdGetActiveConfigDescriptor);
    LIB_FUNCTION("t7WE9mb1TB8", "libSceUsbd", 1, "libSceUsbd", sceUsbdGetBusNumber);
    LIB_FUNCTION("Dkm5qe8j3XE", "libSceUsbd", 1, "libSceUsbd", sceUsbdGetConfigDescriptor);
    LIB_FUNCTION("GQsAVJuy8gM", "libSceUsbd", 1, "libSceUsbd", sceUsbdGetConfigDescriptorByValue);
    LIB_FUNCTION("L7FoTZp3bZs", "libSceUsbd", 1, "libSceUsbd", sceUsbdGetConfiguration);
    LIB_FUNCTION("-JBoEtvTxvA", "libSceUsbd", 1, "libSceUsbd", sceUsbdGetDescriptor);
    LIB_FUNCTION("rsl9KQ-agyA", "libSceUsbd", 1, "libSceUsbd", sceUsbdGetDevice);
    LIB_FUNCTION("GjlCrU4GcIY", "libSceUsbd", 1, "libSceUsbd", sceUsbdGetDeviceAddress);
    LIB_FUNCTION("bhomgbiQgeo", "libSceUsbd", 1, "libSceUsbd", sceUsbdGetDeviceDescriptor);
    LIB_FUNCTION("8qB9Ar4P5nc", "libSceUsbd", 1, "libSceUsbd", sceUsbdGetDeviceList);
    LIB_FUNCTION("e1UWb8cWPJM", "libSceUsbd", 1, "libSceUsbd", sceUsbdGetDeviceSpeed);
    LIB_FUNCTION("vokkJ0aDf54", "libSceUsbd", 1, "libSceUsbd", sceUsbdGetIsoPacketBuffer);
    LIB_FUNCTION("nuIRlpbxauM", "libSceUsbd", 1, "libSceUsbd", sceUsbdGetMaxIsoPacketSize);
    LIB_FUNCTION("YJ0cMAlLuxQ", "libSceUsbd", 1, "libSceUsbd", sceUsbdGetMaxPacketSize);
    LIB_FUNCTION("g2oYm1DitDg", "libSceUsbd", 1, "libSceUsbd", sceUsbdGetStringDescriptor);
    LIB_FUNCTION("t4gUfGsjk+g", "libSceUsbd", 1, "libSceUsbd", sceUsbdGetStringDescriptorAscii);
    LIB_FUNCTION("EkqGLxWC-S0", "libSceUsbd", 1, "libSceUsbd", sceUsbdHandleEvents);
    LIB_FUNCTION("rt-WeUGibfg", "libSceUsbd", 1, "libSceUsbd", sceUsbdHandleEventsLocked);
    LIB_FUNCTION("+wU6CGuZcWk", "libSceUsbd", 1, "libSceUsbd", sceUsbdHandleEventsTimeout);
    LIB_FUNCTION("TOhg7P6kTH4", "libSceUsbd", 1, "libSceUsbd", sceUsbdInit);
    LIB_FUNCTION("rxi1nCOKWc8", "libSceUsbd", 1, "libSceUsbd", sceUsbdInterruptTransfer);
    LIB_FUNCTION("RLf56F-WjKQ", "libSceUsbd", 1, "libSceUsbd", sceUsbdKernelDriverActive);
    LIB_FUNCTION("u9yKks02-rA", "libSceUsbd", 1, "libSceUsbd", sceUsbdLockEvents);
    LIB_FUNCTION("AeGaY8JrAV4", "libSceUsbd", 1, "libSceUsbd", sceUsbdLockEventWaiters);
    LIB_FUNCTION("VJ6oMq-Di2U", "libSceUsbd", 1, "libSceUsbd", sceUsbdOpen);
    LIB_FUNCTION("vrQXYRo1Gwk", "libSceUsbd", 1, "libSceUsbd", sceUsbdOpenDeviceWithVidPid);
    LIB_FUNCTION("U1t1SoJvV-A", "libSceUsbd", 1, "libSceUsbd", sceUsbdRefDevice);
    LIB_FUNCTION("REfUTmTchMw", "libSceUsbd", 1, "libSceUsbd", sceUsbdReleaseInterface);
    LIB_FUNCTION("hvMn0QJXj5g", "libSceUsbd", 1, "libSceUsbd", sceUsbdResetDevice);
    LIB_FUNCTION("FhU9oYrbXoA", "libSceUsbd", 1, "libSceUsbd", sceUsbdSetConfiguration);
    LIB_FUNCTION("DVCQW9o+ki0", "libSceUsbd", 1, "libSceUsbd", sceUsbdSetInterfaceAltSetting);
    LIB_FUNCTION("dJxro8Nzcjk", "libSceUsbd", 1, "libSceUsbd", sceUsbdSetIsoPacketLengths);
    LIB_FUNCTION("L0EHgZZNVas", "libSceUsbd", 1, "libSceUsbd", sceUsbdSubmitTransfer);
    LIB_FUNCTION("TcXVGc-LPbQ", "libSceUsbd", 1, "libSceUsbd", sceUsbdTryLockEvents);
    LIB_FUNCTION("RA2D9rFH-Uw", "libSceUsbd", 1, "libSceUsbd", sceUsbdUnlockEvents);
    LIB_FUNCTION("1DkGvUQYFKI", "libSceUsbd", 1, "libSceUsbd", sceUsbdUnlockEventWaiters);
    LIB_FUNCTION("OULgIo1zAsA", "libSceUsbd", 1, "libSceUsbd", sceUsbdUnrefDevice);
    LIB_FUNCTION("ys2e9VRBPrY", "libSceUsbd", 1, "libSceUsbd", sceUsbdWaitForEvent);
    LIB_FUNCTION("ZfbvM+OP-1A", "libSceUsbd", 1, "libSceUsbd", Func_65F6EF33E38FFF50);
    LIB_FUNCTION("l-BWutkKrec", "libSceUsbd", 1, "libSceUsbd", Func_97F056BAD90AADE7);
    LIB_FUNCTION("xVEEozs1smQ", "libSceUsbd", 1, "libSceUsbd", Func_C55104A33B35B264);
    LIB_FUNCTION("1WtDBgcgseA", "libSceUsbd", 1, "libSceUsbd", Func_D56B43060720B1E0);
}

static void RegisterVdecsw(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("hIgrg5h4V6s", "libSceVdecsw", 1, "libSceVdecsw", sceVdecswAllocateComputeQueue);
    LIB_FUNCTION("+L5ArV1tPGA", "libSceVdecsw", 1, "libSceVdecsw", sceVdecswCreateDecoder);
    LIB_FUNCTION("ecUtPX+dBYk", "libSceVdecsw", 1, "libSceVdecsw", sceVdecswDeleteDecoder);
    LIB_FUNCTION("5Y6nZqIZvBg", "libSceVdecsw", 1, "libSceVdecsw", sceVdecswFinalizeDecodeSequence);
    LIB_FUNCTION("ihNT-uuEAr4", "libSceVdecsw", 1, "libSceVdecsw", sceVdecswGetAvcPictureInfo);
    LIB_FUNCTION("PzF+L5zXoyg", "libSceVdecsw", 1, "libSceVdecsw", sceVdecswGetHevcPictureInfo);
    LIB_FUNCTION("FzECy3Wxxas", "libSceVdecsw", 1, "libSceVdecsw", sceVdecswGetPictureInfo);
    LIB_FUNCTION("0moTubWCsTM", "libSceVdecsw", 1, "libSceVdecsw", sceVdecswQueryComputeMemoryInfo);
    LIB_FUNCTION("A+2M7EivuOU", "libSceVdecsw", 1, "libSceVdecsw", sceVdecswQueryDecoderMemoryInfo);
    LIB_FUNCTION("fX-zOOefbbs", "libSceVdecsw", 1, "libSceVdecsw", sceVdecswReleaseComputeQueue);
    LIB_FUNCTION("veb-YBrOqo0", "libSceVdecsw", 1, "libSceVdecsw", sceVdecswResetDecoder);
    LIB_FUNCTION("aqMiF0AgUYI", "libSceVdecsw", 1, "libSceVdecsw", sceVdecswSetDecodeInput);
    LIB_FUNCTION("rgtMCOpyBSc", "libSceVdecsw", 1, "libSceVdecsw", sceVdecswSetDecodeOutput);
    LIB_FUNCTION("AAMM-Q1X0g0", "libSceVdecsw", 1, "libSceVdecsw", sceVdecswSyncDecodeInput);
    LIB_FUNCTION("tWiSgXov8GM", "libSceVdecsw", 1, "libSceVdecsw", sceVdecswSyncDecodeOutput);
    LIB_FUNCTION("l4sQYy5wPkc", "libSceVdecsw", 1, "libSceVdecsw", sceVdecswTrySyncDecodeInput);
    LIB_FUNCTION("kMBw37oH8nI", "libSceVdecsw", 1, "libSceVdecsw", sceVdecswTrySyncDecodeOutput);
}

static void RegisterVideodec(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("qkgRiwHyheU", "libSceVideodec", 1, "libSceVideodec", sceVideodecCreateDecoder);
    LIB_FUNCTION("q0W5GJMovMs", "libSceVideodec", 1, "libSceVideodec", sceVideodecDecode);
    LIB_FUNCTION("U0kpGF1cl90", "libSceVideodec", 1, "libSceVideodec", sceVideodecDeleteDecoder);
    LIB_FUNCTION("jeigLlKdp5I", "libSceVideodec", 1, "libSceVideodec", sceVideodecFlush);
    LIB_FUNCTION("kg+lH0V61hM", "libSceVideodec", 1, "libSceVideodec", sceVideodecMapMemory);
    LIB_FUNCTION("leCAscipfFY", "libSceVideodec", 1, "libSceVideodec",
                 sceVideodecQueryResourceInfo);
    LIB_FUNCTION("f8AgDv-1X8A", "libSceVideodec", 1, "libSceVideodec", sceVideodecReset);
}

static void RegisterVideodec2(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("RnDibcGCPKw", "libSceVideodec2", 1, "libSceVideodec2",
                 sceVideodec2QueryComputeMemoryInfo);
    LIB_FUNCTION("eD+X2SmxUt4", "libSceVideodec2", 1, "libSceVideodec2",
                 sceVideodec2AllocateComputeQueue);
    LIB_FUNCTION("UvtA3FAiF4Y", "libSceVideodec2", 1, "libSceVideodec2",
                 sceVideodec2ReleaseComputeQueue);
    LIB_FUNCTION("qqMCwlULR+E", "libSceVideodec2", 1, "libSceVideodec2",
                 sceVideodec2QueryDecoderMemoryInfo);
    LIB_FUNCTION("CNNRoRYd8XI", "libSceVideodec2", 1, "libSceVideodec2", sceVideodec2CreateDecoder);
    LIB_FUNCTION("jwImxXRGSKA", "libSceVideodec2", 1, "libSceVideodec2", sceVideodec2DeleteDecoder);
    LIB_FUNCTION("852F5+q6+iM", "libSceVideodec2", 1, "libSceVideodec2", sceVideodec2Decode);
    LIB_FUNCTION("l1hXwscLuCY", "libSceVideodec2", 1, "libSceVideodec2", sceVideodec2Flush);
    LIB_FUNCTION("wJXikG6QFN8", "libSceVideodec2", 1, "libSceVideodec2", sceVideodec2Reset);
    LIB_FUNCTION("NtXRa3dRzU0", "libSceVideodec2", 1, "libSceVideodec2",
                 sceVideodec2GetPictureInfo);
    LIB_FUNCTION("kjrLbcyhEiw", "libSceVideodec2", 1, "libSceVideodec2",
                 sceVideodec2GetAvcPictureInfo);
    LIB_FUNCTION("7M+1UFqWOAI", "libSceVideodec2", 1, "libSceVideodec2",
                 sceVideodec2GetHevcPictureInfo);
}

static void RegisterPlayGo(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("uEqMfMITvEI", "libSceDbgPlayGo", 1, "libScePlayGo", sceDbgPlayGoRequestNextChunk);
    LIB_FUNCTION("vU+FqrH+pEY", "libSceDbgPlayGo", 1, "libScePlayGo", sceDbgPlayGoSnapshot);
    LIB_FUNCTION("Uco1I0dlDi8", "libScePlayGo", 1, "libScePlayGo", scePlayGoClose);
    LIB_FUNCTION("73fF1MFU8hA", "libScePlayGo", 1, "libScePlayGo", scePlayGoGetChunkId);
    LIB_FUNCTION("v6EZ-YWRdMs", "libScePlayGo", 1, "libScePlayGo", scePlayGoGetEta);
    LIB_FUNCTION("rvBSfTimejE", "libScePlayGo", 1, "libScePlayGo", scePlayGoGetInstallSpeed);
    LIB_FUNCTION("3OMbYZBaa50", "libScePlayGo", 1, "libScePlayGo", scePlayGoGetLanguageMask);
    LIB_FUNCTION("uWIYLFkkwqk", "libScePlayGo", 1, "libScePlayGo", scePlayGoGetLocus);
    LIB_FUNCTION("-RJWNMK3fC8", "libScePlayGo", 1, "libScePlayGo", scePlayGoGetProgress);
    LIB_FUNCTION("Nn7zKwnA5q0", "libScePlayGo", 1, "libScePlayGo", scePlayGoGetToDoList);
    LIB_FUNCTION("ts6GlZOKRrE", "libScePlayGo", 1, "libScePlayGo", scePlayGoInitialize);
    LIB_FUNCTION("M1Gma1ocrGE", "libScePlayGo", 1, "libScePlayGo", scePlayGoOpen);
    LIB_FUNCTION("-Q1-u1a7p0g", "libScePlayGo", 1, "libScePlayGo", scePlayGoPrefetch);
    LIB_FUNCTION("4AAcTU9R3XM", "libScePlayGo", 1, "libScePlayGo", scePlayGoSetInstallSpeed);
    LIB_FUNCTION("LosLlHOpNqQ", "libScePlayGo", 1, "libScePlayGo", scePlayGoSetLanguageMask);
    LIB_FUNCTION("gUPGiOQ1tmQ", "libScePlayGo", 1, "libScePlayGo", scePlayGoSetToDoList);
    LIB_FUNCTION("MPe0EeBGM-E", "libScePlayGo", 1, "libScePlayGo", scePlayGoTerminate);
}

static void RegisterZlib(Core::Loader::SymbolsResolver* sym) {
    zlib_library_init();

    LIB_FUNCTION("m1YErdIXCp4", "libSceZlib", 1, "libSceZlib", sceZlibInitialize);
    LIB_FUNCTION("6na+Sa-B83w", "libSceZlib", 1, "libSceZlib", sceZlibFinalize);
    LIB_FUNCTION("TLar1HULv1Q", "libSceZlib", 1, "libSceZlib", sceZlibInflate);
    LIB_FUNCTION("uB8VlDD4e0s", "libSceZlib", 1, "libSceZlib", sceZlibWaitForDone);
    LIB_FUNCTION("2eDcGHC0YaM", "libSceZlib", 1, "libSceZlib", sceZlibGetResult);
}

static void RegisterVoice(Core::Loader::SymbolsResolver* sym) {
    voice_library_init();

    LIB_FUNCTION("oV9GAdJ23Gw", "libSceVoice", 1, "libSceVoice", sceVoiceConnectIPortToOPort);
    LIB_FUNCTION("nXpje5yNpaE", "libSceVoice", 1, "libSceVoice", sceVoiceCreatePort);
    LIB_FUNCTION("b7kJI+nx2hg", "libSceVoice", 1, "libSceVoice", sceVoiceDeletePort);
    LIB_FUNCTION("ajVj3QG2um4", "libSceVoice", 1, "libSceVoice", sceVoiceDisconnectIPortFromOPort);
    LIB_FUNCTION("Oo0S5PH7FIQ", "libSceVoice", 1, "libSceVoice", sceVoiceEnd);
    LIB_FUNCTION("cJLufzou6bc", "libSceVoice", 1, "libSceVoice", sceVoiceGetBitRate);
    LIB_FUNCTION("Pc4z1QjForU", "libSceVoice", 1, "libSceVoice", sceVoiceGetMuteFlag);
    LIB_FUNCTION("elcxZTEfHZM", "libSceVoice", 1, "libSceVoice", sceVoiceGetPortAttr);
    LIB_FUNCTION("CrLqDwWLoXM", "libSceVoice", 1, "libSceVoice", sceVoiceGetPortInfo);
    LIB_FUNCTION("Z6QV6j7igvE", "libSceVoice", 1, "libSceVoice", sceVoiceGetResourceInfo);
    LIB_FUNCTION("jjkCjneOYSs", "libSceVoice", 1, "libSceVoice", sceVoiceGetVolume);
    LIB_FUNCTION("9TrhuGzberQ", "libSceVoice", 1, "libSceVoice", sceVoiceInit);
    LIB_FUNCTION("IPHvnM5+g04", "libSceVoice", 1, "libSceVoice", sceVoiceInitHQ);
    LIB_FUNCTION("x0slGBQW+wY", "libSceVoice", 1, "libSceVoice", sceVoicePausePort);
    LIB_FUNCTION("Dinob0yMRl8", "libSceVoice", 1, "libSceVoice", sceVoicePausePortAll);
    LIB_FUNCTION("cQ6DGsQEjV4", "libSceVoice", 1, "libSceVoice", sceVoiceReadFromOPort);
    LIB_FUNCTION("udAxvCePkUs", "libSceVoice", 1, "libSceVoice", sceVoiceResetPort);
    LIB_FUNCTION("gAgN+HkiEzY", "libSceVoice", 1, "libSceVoice", sceVoiceResumePort);
    LIB_FUNCTION("jbkJFmOZ9U0", "libSceVoice", 1, "libSceVoice", sceVoiceResumePortAll);
    LIB_FUNCTION("TexwmOHQsDg", "libSceVoice", 1, "libSceVoice", sceVoiceSetBitRate);
    LIB_FUNCTION("gwUynkEgNFY", "libSceVoice", 1, "libSceVoice", sceVoiceSetMuteFlag);
    LIB_FUNCTION("oUha0S-Ij9Q", "libSceVoice", 1, "libSceVoice", sceVoiceSetMuteFlagAll);
    LIB_FUNCTION("clyKUyi3RYU", "libSceVoice", 1, "libSceVoice", sceVoiceSetThreadsParams);
    LIB_FUNCTION("QBFoAIjJoXQ", "libSceVoice", 1, "libSceVoice", sceVoiceSetVolume);
    LIB_FUNCTION("54phPH2LZls", "libSceVoice", 1, "libSceVoice", sceVoiceStart);
    LIB_FUNCTION("Ao2YNSA7-Qo", "libSceVoice", 1, "libSceVoice", sceVoiceStop);
    LIB_FUNCTION("jSZNP7xJrcw", "libSceVoice", 1, "libSceVoice", sceVoiceUpdatePort);
    LIB_FUNCTION("hg9T73LlRiU", "libSceVoice", 1, "libSceVoice", sceVoiceVADAdjustment);
    LIB_FUNCTION("wFeAxEeEi-8", "libSceVoice", 1, "libSceVoice", sceVoiceVADSetVersion);
    LIB_FUNCTION("YeJl6yDlhW0", "libSceVoice", 1, "libSceVoice", sceVoiceWriteToIPort);
}

static void RegisterAudio3d(Core::Loader::SymbolsResolver* sym) {
    // The frontend plays the sound: Audio3D goes out through AudioOut, never to a device of
    // its own.
    LIB_FUNCTION("pZlOm1aF3aA", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dAudioOutClose);
    LIB_FUNCTION("ucEsi62soTo", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dAudioOutOpen);
    LIB_FUNCTION("7NYEzJ9SJbM", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dAudioOutOutput);
    LIB_FUNCTION("HbxYY27lK6E", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dAudioOutOutputs);
    LIB_FUNCTION("9tEwE0GV0qo", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dBedWrite);
    LIB_FUNCTION("xH4Q9UILL3o", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dBedWrite2);
    LIB_FUNCTION("lvWMW6vEqFU", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dCreateSpeakerArray);
    LIB_FUNCTION("8hm6YdoQgwg", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dDeleteSpeakerArray);
    LIB_FUNCTION("Im+jOoa5WAI", "libSceAudio3d", 1, "libSceAudio3d",
                 sceAudio3dGetDefaultOpenParameters);
    LIB_FUNCTION("kEqqyDkmgdI", "libSceAudio3d", 1, "libSceAudio3d",
                 sceAudio3dGetSpeakerArrayMemorySize);
    LIB_FUNCTION("-R1DukFq7Dk", "libSceAudio3d", 1, "libSceAudio3d",
                 sceAudio3dGetSpeakerArrayMixCoefficients);
    LIB_FUNCTION("-Re+pCWvwjQ", "libSceAudio3d", 1, "libSceAudio3d",
                 sceAudio3dGetSpeakerArrayMixCoefficients2);
    LIB_FUNCTION("UmCvjSmuZIw", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dInitialize);
    LIB_FUNCTION("jO2tec4dJ2M", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dObjectReserve);
    LIB_FUNCTION("V1FBFpNIAzk", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dObjectSetAttribute);
    LIB_FUNCTION("4uyHN9q4ZeU", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dObjectSetAttributes);
    LIB_FUNCTION("1HXxo-+1qCw", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dObjectUnreserve);
    LIB_FUNCTION("lw0qrdSjZt8", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortAdvance);
    LIB_FUNCTION("OyVqOeVNtSk", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortClose);
    LIB_FUNCTION("UHFOgVNz0kk", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortCreate);
    LIB_FUNCTION("Mw9mRQtWepY", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortDestroy);
    LIB_FUNCTION("ZOGrxWLgQzE", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortFlush);
    LIB_FUNCTION("uJ0VhGcxCTQ", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortFreeState);
    LIB_FUNCTION("9ZA23Ia46Po", "libSceAudio3d", 1, "libSceAudio3d",
                 sceAudio3dPortGetAttributesSupported);
    LIB_FUNCTION("SEggctIeTcI", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortGetList);
    LIB_FUNCTION("flPcUaXVXcw", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortGetParameters);
    LIB_FUNCTION("YaaDbDwKpFM", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortGetQueueLevel);
    LIB_FUNCTION("CKHlRW2E9dA", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortGetState);
    LIB_FUNCTION("iRX6GJs9tvE", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortGetStatus);
    LIB_FUNCTION("XeDDK0xJWQA", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortOpen);
    LIB_FUNCTION("VEVhZ9qd4ZY", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortPush);
    LIB_FUNCTION("-pzYDZozm+M", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortQueryDebug);
    LIB_FUNCTION("Yq9bfUQ0uJg", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortSetAttribute);
    LIB_FUNCTION("QfNXBrKZeI0", "libSceAudio3d", 1, "libSceAudio3d",
                 sceAudio3dReportRegisterHandler);
    LIB_FUNCTION("psv2gbihC1A", "libSceAudio3d", 1, "libSceAudio3d",
                 sceAudio3dReportUnregisterHandler);
    LIB_FUNCTION("yEYXcbAGK14", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dSetGpuRenderer);
    LIB_FUNCTION("Aacl5qkRU6U", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dStrError);
    LIB_FUNCTION("WW1TS2iz5yc", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dTerminate);
}

void InitHLELibs(Core::Loader::SymbolsResolver* sym) {
    LOG_INFO(Lib_Kernel, "Initializing HLE libraries");

    auto* game_info = Common::Singleton<Common::ElfInfo>::Instance();
    const auto& sys_module_path = Common::FS::PathFromCString(emu_settings_get_sys_modules_dir);
    const char* serial = game_info->GameSerial();
    const auto& game_specific_modules_path = sys_module_path / (serial[0] ? serial : "no_serial");
    {
        constexpr auto ModulesToLoad = std::to_array<Core::SysModules>({
            {"libkernel.sprx", Libraries::Kernel::RegisterLib},
            {"libSceGnmDriver.sprx", Libraries::GnmDriver::RegisterLib},
            {"libSceVideoOut.sprx", RegisterVideoOut},
            {"libSceUserService.sprx", Libraries::UserService::RegisterLib},
            {"libSceSystemService.sprx", Libraries::SystemService::RegisterLib},
            {"libScePad.sprx", RegisterPad},
            {"libSceCommonDialog.sprx", RegisterCommonDialog},
            {"libSceMsgDialog.sprx", Libraries::MsgDialog::RegisterLib},
            {"libSceAudioOut.sprx", RegisterAudioOut},
            {"libSceAudioIn.sprx", RegisterAudioIn},
            {"libSceAudio3d.sprx", RegisterAudio3d},
            {"libSceHttp.sprx", Libraries::Http::RegisterLib},
            {"libSceHttp2.sprx", Libraries::Http2::RegisterLib},
            {"libSceNet.sprx", Libraries::Net::RegisterLib},
            {"libSceNetCtl.sprx", Libraries::NetCtl::RegisterLib},
            {"libSceSaveData.sprx", RegisterSaveData},
            {"libSceSaveDataDialog.sprx", RegisterSaveDataDialog},
            {"libSceSsl2.sprx", Libraries::Ssl2::RegisterLib},
            {"libSceSysmodule.sprx", RegisterSysmodule},
            {"libSceNpCommerce.sprx", Libraries::Np::NpCommerce::RegisterLib},
            {"libSceNpCommon.sprx", Libraries::Np::NpCommon::RegisterLib},
            {"libSceNpManager.sprx", Libraries::Np::NpManager::RegisterLib},
            {"libSceNpMatching2.sprx", Libraries::Np::NpMatching2::RegisterLib},
            {"libSceNpSignaling.sprx", Libraries::Np::NpSignaling::RegisterLib},
            {"libSceNpScore.sprx", Libraries::Np::NpScore::RegisterLib},
            {"libSceNpTrophy.sprx", Libraries::Np::NpTrophy::RegisterLib},
            {"libSceNpWebApi.sprx", Libraries::Np::NpWebApi::RegisterLib},
            {"libSceNpWebApi2.sprx", Libraries::Np::NpWebApi2::RegisterLib},
            {"libSceNpProfileDialog.sprx", Libraries::Np::NpProfileDialog::RegisterLib},
            {"libSceNpSnsFacebookDialog.sprx", Libraries::Np::NpSnsFacebookDialog::RegisterLib},
            {"libSceNpAuth.sprx", Libraries::Np::NpAuth::RegisterLib},
            {"libSceNpParty.sprx", Libraries::Np::NpParty::RegisterLib},
            {"libSceNpPartner001.sprx", Libraries::Np::NpPartner::RegisterLib},
            {"libSceNpTus.sprx", Libraries::Np::NpTus::RegisterLib},
            {"libSceScreenShot.sprx", RegisterScreenShot},
            {"libSceAppContent.sprx", RegisterAppContent},
            {"libScePlayGo.sprx", RegisterPlayGo},
            {"libScePlayGoDialog.sprx", RegisterPlayGoDialog},
            {"libSceRandom.sprx", RegisterRandom},
            {"libSceUsbd.sprx", RegisterUsbd},
            {"libSceUsbStorage.sprx", RegisterUsbStorage},
            {"libSceMusicPlayerService.sprx", RegisterMusicPlayerService},
            {"libSceAjm.sprx", RegisterAjm},
            {"libSceErrorDialog.sprx", Libraries::ErrorDialog::RegisterLib},
            {"libSceImeDialog.sprx", Libraries::ImeDialog::RegisterLib},
            {"libSceVdecsw.sprx", RegisterVdecsw},
            {"libSceVideodec.sprx", RegisterVideodec},
            {"libSceVideodec2.sprx", RegisterVideodec2},
            {"libSceIme.sprx", Libraries::Ime::RegisterLib},
            {"libSceGameLiveStreaming.sprx", RegisterGameLiveStreaming},
            {"libSceSharePlay.sprx", RegisterSharePlay},
            {"libSceRemoteplay.sprx", RegisterRemoteplay},
            {"libSceRazorCpu.sprx", RegisterRazorCpu},
            {"libSceMove.sprx", RegisterMove},
            {"libSceMouse.sprx", RegisterMouse},
            {"libSceKeyboard.sprx", RegisterKeyboard},
            {"libSceWebBrowserDialog.sprx", RegisterWebBrowserDialog},
            {"libSceZlib.sprx", RegisterZlib},
            {"libSceHmd.sprx", RegisterHmd},
            {"libSceHmdSetupDialog.sprx", RegisterHmdSetupDialog},
            {"libSceDiscMap.sprx", RegisterDiscMap},
            {"ulobjmgr.sprx", RegisterUlobjmgr},
            {"libSceSigninDialog.sprx", RegisterSigninDialog},
            {"libSceCamera.sprx", RegisterCamera},
            {"libSceCompanionHttpd.sprx", RegisterCompanionHttpd},
            {"libSceCompanionUtil.sprx", RegisterCompanionUtil},
            {"libSceVoice.sprx", RegisterVoice},
            {"libSceVrTracker.sprx", RegisterVrTracker},
            {"libSceContentExport.sprx", RegisterContentExport},
            {"libSceVideoRecording.sprx", RegisterVideoRecording},
            {"libSceInvitationDialog.sprx", RegisterInvitationDialog},
            {"libSceNpUtility.sprx", Libraries::Np::NpUtility::RegisterLib},
#if defined(ARCH_X86_64) || defined(SHADPS4_ENABLE_FEX_GUEST_CPU)
            {"libSceFiber.sprx", RegisterFiber},
#endif
            // Loading libSceSsl is locked behind a title workaround that currently applies to
            // nothing.
            // {"libSceSsl.sprx", Libraries::Ssl::RegisterLib},
        });

        for (auto mod : ModulesToLoad) {
            if (mod.module_name == "libSceGnmDriver.sprx") {
                auto lle_file = Kernel::sceKernelIsNeoMode() ? "libSceGnmDriverForNeoMode.sprx"
                                                             : mod.module_name;
                if (Common::FS::Exists(game_specific_modules_path / lle_file)) {
                    LOG_WARNING(Loader, "%.*s LLEd", static_cast<int>((lle_file).size()),
                                (lle_file).data());
                } else {
                    mod.callback(sym);
                }
            } else if (Common::FS::Exists(game_specific_modules_path / mod.module_name)) {
                LOG_WARNING(Loader, "%.*s LLEd", static_cast<int>((mod.module_name).size()),
                            (mod.module_name).data());
            } else {
                mod.callback(sym);
            }
        }
    }
}

} // namespace Libraries
