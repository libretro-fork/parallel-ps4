// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/arch.h"
#include "common/elf_info.h"
#include "common/io_file.h"
#include "common/path_util.h"
#include "common/singleton.h"
#include "core/emulator_settings.h"
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
#include "core/libraries/keyboard/keyboard.h"
#include "core/libraries/libc_internal/libc_internal.h"
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
#include "core/libraries/zlib/zlib_sce.h"
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

static void RegisterAudio3d(Core::Loader::SymbolsResolver* sym) {
    // The frontend plays the sound: Audio3D goes out through AudioOut, never to a device of
    // its own.
    Libraries::Audio3d::RegisterLib(sym);
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
            {"libScePad.sprx", Libraries::Pad::RegisterLib},
            {"libSceCommonDialog.sprx", Libraries::CommonDialog::RegisterLib},
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
            {"libSceSysmodule.sprx", Libraries::SysModule::RegisterLib},
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
            {"libSceAppContent.sprx", Libraries::AppContent::RegisterLib},
            {"libScePlayGo.sprx", Libraries::PlayGo::RegisterLib},
            {"libScePlayGoDialog.sprx", Libraries::PlayGo::Dialog::RegisterLib},
            {"libSceRandom.sprx", RegisterRandom},
            {"libSceUsbd.sprx", Libraries::Usbd::RegisterLib},
            {"libSceUsbStorage.sprx", Libraries::UsbStorage::RegisterLib},
            {"libSceMusicPlayerService.sprx", Libraries::MusicPlayerService::RegisterLib},
            {"libSceAjm.sprx", Libraries::Ajm::RegisterLib},
            {"libSceErrorDialog.sprx", Libraries::ErrorDialog::RegisterLib},
            {"libSceImeDialog.sprx", Libraries::ImeDialog::RegisterLib},
            {"libSceVdecsw.sprx", Libraries::Vdecsw::RegisterLib},
            {"libSceVideodec.sprx", Libraries::Videodec::RegisterLib},
            {"libSceVideodec2.sprx", Libraries::Videodec2::RegisterLib},
            {"libSceIme.sprx", Libraries::Ime::RegisterLib},
            {"libSceGameLiveStreaming.sprx", Libraries::GameLiveStreaming::RegisterLib},
            {"libSceSharePlay.sprx", RegisterSharePlay},
            {"libSceRemoteplay.sprx", Libraries::Remoteplay::RegisterLib},
            {"libSceRazorCpu.sprx", Libraries::RazorCpu::RegisterLib},
            {"libSceMove.sprx", RegisterMove},
            {"libSceMouse.sprx", RegisterMouse},
            {"libSceKeyboard.sprx", RegisterKeyboard},
            {"libSceWebBrowserDialog.sprx", Libraries::WebBrowserDialog::RegisterLib},
            {"libSceZlib.sprx", Libraries::Zlib::RegisterLib},
            {"libSceHmd.sprx", Libraries::Hmd::RegisterLib},
            {"libSceHmdSetupDialog.sprx", Libraries::HmdSetupDialog::RegisterLib},
            {"libSceDiscMap.sprx", RegisterDiscMap},
            {"ulobjmgr.sprx", RegisterUlobjmgr},
            {"libSceSigninDialog.sprx", RegisterSigninDialog},
            {"libSceCamera.sprx", Libraries::Camera::RegisterLib},
            {"libSceCompanionHttpd.sprx", Libraries::CompanionHttpd::RegisterLib},
            {"libSceCompanionUtil.sprx", Libraries::CompanionUtil::RegisterLib},
            {"libSceVoice.sprx", Libraries::Voice::RegisterLib},
            {"libSceVrTracker.sprx", Libraries::VrTracker::RegisterLib},
            {"libSceContentExport.sprx", RegisterContentExport},
            {"libSceVideoRecording.sprx", RegisterVideoRecording},
            {"libSceInvitationDialog.sprx", Libraries::InvitationDialog::RegisterLib},
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
