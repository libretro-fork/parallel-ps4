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
            {"libSceVideoOut.sprx", Libraries::VideoOut::RegisterLib},
            {"libSceUserService.sprx", Libraries::UserService::RegisterLib},
            {"libSceSystemService.sprx", Libraries::SystemService::RegisterLib},
            {"libScePad.sprx", RegisterPad},
            {"libSceCommonDialog.sprx", RegisterCommonDialog},
            {"libSceMsgDialog.sprx", Libraries::MsgDialog::RegisterLib},
            {"libSceAudioOut.sprx", Libraries::AudioOut::RegisterLib},
            {"libSceAudioIn.sprx", Libraries::AudioIn::RegisterLib},
            {"libSceAudio3d.sprx", RegisterAudio3d},
            {"libSceHttp.sprx", Libraries::Http::RegisterLib},
            {"libSceHttp2.sprx", Libraries::Http2::RegisterLib},
            {"libSceNet.sprx", Libraries::Net::RegisterLib},
            {"libSceNetCtl.sprx", Libraries::NetCtl::RegisterLib},
            {"libSceSaveData.sprx", Libraries::SaveData::RegisterLib},
            {"libSceSaveDataDialog.sprx", Libraries::SaveData::Dialog::RegisterLib},
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
            {"libSceUsbd.sprx", Libraries::Usbd::RegisterLib},
            {"libSceUsbStorage.sprx", Libraries::UsbStorage::RegisterLib},
            {"libSceMusicPlayerService.sprx", RegisterMusicPlayerService},
            {"libSceAjm.sprx", Libraries::Ajm::RegisterLib},
            {"libSceErrorDialog.sprx", Libraries::ErrorDialog::RegisterLib},
            {"libSceImeDialog.sprx", Libraries::ImeDialog::RegisterLib},
            {"libSceVdecsw.sprx", Libraries::Vdecsw::RegisterLib},
            {"libSceVideodec.sprx", Libraries::Videodec::RegisterLib},
            {"libSceVideodec2.sprx", Libraries::Videodec2::RegisterLib},
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
            {"libSceFiber.sprx", Libraries::Fiber::RegisterLib},
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
