// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <cmrc/cmrc.hpp>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui/imgui_std.h>
#include <queue>

#define MINIMP3_IMPLEMENTATION

#include "common/audio_file.h"
#include "common/io_file.h"
#include "common/logging/formatter.h"
#include "common/logging/log.h"
#include "common/path_util.h"
#include "core/emulator_settings.h"
#include "core/libraries/np/trophy_ui.h"
#include "libretro/audio_mix.h"

CMRC_DECLARE(res);
namespace fs = std::filesystem;
using namespace ImGui;
namespace Libraries::Np::NpTrophy {

std::optional<TrophyUI> current_trophy_ui;

// The trophy sound, as the core's mix plays it: 16-bit stereo at 48 kHz, made once when the
// emulator starts and borrowed by the mix each time a trophy is unlocked.
static std::vector<s16> trophy_sound;

// 16-bit PCM in a RIFF/WAVE file, 1 or 2 channels: where its samples are, read in place.
static bool ParseWav(const u8* data, size_t size, const s16** samples, u64* frames, u32* channels,
                     u32* hz) {
    if (size < 12 || std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) {
        return false;
    }
    const auto le16 = [](const u8* p) { return static_cast<u32>(p[0] | (p[1] << 8)); };
    const auto le32 = [](const u8* p) {
        return static_cast<u32>(p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<u32>(p[3]) << 24));
    };
    bool have_fmt = false;
    for (size_t at = 12; at + 8 <= size;) {
        const u32 chunk = le32(data + at + 4);
        const u8* body = data + at + 8;
        if (chunk > size - at - 8) {
            return false;
        }
        if (std::memcmp(data + at, "fmt ", 4) == 0 && chunk >= 16) {
            if (le16(body) != 1 || le16(body + 14) != 16) {
                return false; // only 16-bit PCM
            }
            *channels = le16(body + 2);
            *hz = le32(body + 4);
            have_fmt = *channels == 1 || *channels == 2;
        } else if (std::memcmp(data + at, "data", 4) == 0 && have_fmt && *hz > 0) {
            *samples = reinterpret_cast<const s16*>(body);
            *frames = chunk / (2 * *channels);
            return true;
        }
        at += 8 + chunk + (chunk & 1);
    }
    return false;
}

static void ResampleTrophySound(const s16* samples, u64 frames, u32 channels, u32 hz) {
    audio_mix_resampler_t r;
    audio_mix_resampler_init(&r, hz, channels);
    trophy_sound.resize(static_cast<size_t>(((frames * 48000) / hz + 1) * 2));
    u64 pos = 0;
    const unsigned out = audio_mix_resample(&r, samples, frames, &pos, trophy_sound.data(),
                                            static_cast<unsigned>(trophy_sound.size() / 2), 1.0f);
    trophy_sound.resize(static_cast<size_t>(out) * 2);
}

void PreloadTrophySound() {
    trophy_sound.clear();
    const auto dir = Common::FS::GetUserPath(Common::FS::PathType::CustomTrophy);
    const auto mp3_path = dir / "trophy.mp3";
    const auto wav_path = dir / "trophy.wav";
    if (fs::exists(mp3_path)) {
        const auto bytes = Common::FS::FileView::Map(mp3_path);
        Common::AudioFile::Pcm pcm;
        if (bytes && Common::AudioFile::DecodeMp3(bytes->data(), bytes->size(), pcm) &&
            (pcm.channels == 1 || pcm.channels == 2)) {
            ResampleTrophySound(pcm.Data(), pcm.Frames(), pcm.channels, pcm.hz);
        } else {
            LOG_ERROR(Lib_NpTrophy, "Could not decode the custom trophy sound {}",
                      fmt::UTF(mp3_path.u8string()));
        }
        return;
    }
    const s16* samples = nullptr;
    u64 frames = 0;
    u32 channels = 0;
    u32 hz = 0;
    if (fs::exists(wav_path)) {
        const auto bytes = Common::FS::FileView::Map(wav_path);
        if (bytes && ParseWav(bytes->data(), bytes->size(), &samples, &frames, &channels, &hz)) {
            ResampleTrophySound(samples, frames, channels, hz);
        } else {
            LOG_ERROR(Lib_NpTrophy, "The custom trophy sound {} is not 16-bit PCM",
                      fmt::UTF(wav_path.u8string()));
        }
        return;
    }
    const auto file = cmrc::res::get_filesystem().open("src/resources/trophy.wav");
    if (ParseWav(reinterpret_cast<const u8*>(file.begin()), file.size(), &samples, &frames,
                 &channels, &hz)) {
        ResampleTrophySound(samples, frames, channels, hz);
    }
}
std::queue<TrophyInfo> trophy_queue;
std::mutex queueMtx;
std::string side = "right";
double trophy_timer;

TrophyUI::TrophyUI(const std::filesystem::path& trophyIconPath, const std::string& trophyName,
                   const std::string_view& rarity)
    : trophy_name(trophyName), trophy_type(rarity) {

    side = emu_settings_get_trophy_notification_side();
    trophy_timer = emu_settings_get_trophy_notification_duration();

    if (std::filesystem::exists(trophyIconPath)) {
        trophy_icon = RefCountedTexture::DecodePngFile(trophyIconPath);
    } else {
        LOG_ERROR(Lib_NpTrophy, "Couldnt load trophy icon at {}",
                  fmt::UTF(trophyIconPath.u8string()));
    }

    std::string pathString = "src/resources/";

    if (trophy_type == "P") {
        pathString += "platinum.png";
    } else if (trophy_type == "G") {
        pathString += "gold.png";
    } else if (trophy_type == "S") {
        pathString += "silver.png";
    } else if (trophy_type == "B") {
        pathString += "bronze.png";
    }

    const auto CustomTrophy_Dir = Common::FS::GetUserPath(Common::FS::PathType::CustomTrophy);
    std::string customPath;

    if (trophy_type == "P" && fs::exists(CustomTrophy_Dir / "platinum.png")) {
        customPath = (CustomTrophy_Dir / "platinum.png").string();
    } else if (trophy_type == "G" && fs::exists(CustomTrophy_Dir / "gold.png")) {
        customPath = (CustomTrophy_Dir / "gold.png").string();
    } else if (trophy_type == "S" && fs::exists(CustomTrophy_Dir / "silver.png")) {
        customPath = (CustomTrophy_Dir / "silver.png").string();
    } else if (trophy_type == "B" && fs::exists(CustomTrophy_Dir / "bronze.png")) {
        customPath = (CustomTrophy_Dir / "bronze.png").string();
    }

    if (!customPath.empty()) {
        trophy_type_icon = RefCountedTexture::DecodePngFile(customPath);
    } else {
        // Embedded in the core: lent, not copied
        auto file = cmrc::res::get_filesystem().open(pathString);
        trophy_type_icon = RefCountedTexture::DecodePngTexture(
            Common::FS::FileView::Borrow({reinterpret_cast<const u8*>(file.begin()), file.size()}));
    }

    AddLayer(this);

    // Louder than the game, as standalone plays it.
    if (!trophy_sound.empty()) {
        audio_mix_play_clip(trophy_sound.data(), static_cast<unsigned>(trophy_sound.size() / 2),
                            emu_settings_get_volume_slider() * 0.01f * 1.2f);
    }
}

TrophyUI::~TrophyUI() {
    Finish();
}

void TrophyUI::Finish() {
    RemoveLayer(this);
}

float fade_opacity = 0.0f;                 // Initial opacity (invisible)
ImVec2 start_pos = ImVec2(1280.0f, 50.0f); // Starts off screen, right
ImVec2 target_pos = ImVec2(0.0f, 50.0f);   // Final position
float animation_duration = 0.5f;           // Animation duration
float elapsed_time = 0.0f;                 // Animation time
float fade_out_duration = 0.5f;            // Final fade duration

void TrophyUI::Draw() {
    const auto& io = GetIO();

    float AdjustWidth = io.DisplaySize.x / 1920;
    float AdjustHeight = io.DisplaySize.y / 1080;
    const ImVec2 window_size{
        std::min(io.DisplaySize.x, (350 * AdjustWidth)),
        std::min(io.DisplaySize.y, (70 * AdjustHeight)),
    };

    elapsed_time += io.DeltaTime;
    float progress = std::min(elapsed_time / animation_duration, 1.0f);

    float final_pos_x, start_x;
    float final_pos_y, start_y;

    if (side == "top") {
        start_x = (io.DisplaySize.x - window_size.x) * 0.5f;
        start_y = -window_size.y;
        final_pos_x = start_x;
        final_pos_y = 20 * AdjustHeight;
    } else if (side == "left") {
        start_x = -window_size.x;
        start_y = 50 * AdjustHeight;
        final_pos_x = 20 * AdjustWidth;
        final_pos_y = start_y;
    } else if (side == "right") {
        start_x = io.DisplaySize.x;
        start_y = 50 * AdjustHeight;
        final_pos_x = io.DisplaySize.x - window_size.x - 20 * AdjustWidth;
        final_pos_y = start_y;
    } else if (side == "bottom") {
        start_x = (io.DisplaySize.x - window_size.x) * 0.5f;
        start_y = io.DisplaySize.y;
        final_pos_x = start_x;
        final_pos_y = io.DisplaySize.y - window_size.y - 20 * AdjustHeight;
    }

    ImVec2 current_pos = ImVec2(start_x + (final_pos_x - start_x) * progress,
                                start_y + (final_pos_y - start_y) * progress);

    trophy_timer -= io.DeltaTime;

    ImGui::SetNextWindowPos(current_pos);

    // If the remaining time of the trophy is less than or equal to 1 second, the fade-out begins.
    if (trophy_timer <= 1.0f) {
        float fade_out_time = 1.0f - (trophy_timer / 1.0f);
        fade_opacity = 1.0f - fade_out_time;
    } else {
        // Fade in , 0 to 1
        fade_opacity = progress;
    }

    fade_opacity = std::max(0.0f, std::min(fade_opacity, 1.0f));

    SetNextWindowSize(window_size);
    SetNextWindowPos(current_pos);
    SetNextWindowCollapsed(false);
    KeepNavHighlight();
    PushStyleVar(ImGuiStyleVar_Alpha, fade_opacity);

    if (Begin("Trophy Window", nullptr,
              ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                  ImGuiWindowFlags_NoInputs)) {

        // Displays the trophy icon
        if (trophy_type_icon) {
            SetCursorPosY((window_size.y * 0.5f) - (25 * AdjustHeight));
            Image(trophy_type_icon.GetTexture().im_id,
                  ImVec2((50 * AdjustWidth), (50 * AdjustHeight)));
            ImGui::SameLine();
        } else {
            // Placeholder
            const auto pos = GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(pos, pos + ImVec2{50.0f * AdjustHeight},
                                                      GetColorU32(ImVec4{0.7f}));
            ImGui::Indent(60);
        }

        // Displays the name of the trophy
        const std::string combinedString = "Trophy earned!\n" + trophy_name;
        const float wrap_width =
            CalcWrapWidthForPos(GetCursorScreenPos(), (window_size.x - (60 * AdjustWidth)));
        SetWindowFontScale(1.2 * AdjustHeight);
        // If trophy name exceeds 1 line
        if (CalcTextSize(trophy_name.c_str()).x > wrap_width) {
            SetCursorPosY(5 * AdjustHeight);
            if (CalcTextSize(trophy_name.c_str()).x > (wrap_width * 2)) {
                SetWindowFontScale(0.95 * AdjustHeight);
            } else {
                SetWindowFontScale(1.1 * AdjustHeight);
            }
        } else {
            const float text_height = ImGui::CalcTextSize(combinedString.c_str()).y;
            SetCursorPosY((window_size.y - text_height) * 0.5);
        }

        if (side == "top" || side == "bottom") {
            float text_width = ImGui::CalcTextSize(trophy_name.c_str()).x;
            float centered_x = (window_size.x - text_width) * 0.5f;
            ImGui::SetCursorPosX(std::max(centered_x, 10.0f * AdjustWidth));
        }

        ImGui::PushTextWrapPos(window_size.x - (60 * AdjustWidth));
        TextWrapped("Trophy earned!\n%s", trophy_name.c_str());
        ImGui::SameLine(window_size.x - (60 * AdjustWidth));

        // Displays the trophy icon
        if (trophy_icon) {
            SetCursorPosY((window_size.y * 0.5f) - (25 * AdjustHeight));
            Image(trophy_icon.GetTexture().im_id, ImVec2((50 * AdjustWidth), (50 * AdjustHeight)));
        } else {
            // Placeholder
            const auto pos = GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(pos, pos + ImVec2{50.0f * AdjustHeight},
                                                      GetColorU32(ImVec4{0.7f}));
        }
    }
    End();

    PopStyleVar();

    if (trophy_timer <= 0) {
        std::lock_guard<std::mutex> lock(queueMtx);
        if (!trophy_queue.empty()) {
            TrophyInfo next_trophy = trophy_queue.front();
            trophy_queue.pop();
            current_trophy_ui.emplace(next_trophy.trophy_icon_path, next_trophy.trophy_name,
                                      next_trophy.trophy_type);
        } else {
            current_trophy_ui.reset();
        }
    }
}

void AddTrophyToQueue(const std::filesystem::path& trophyIconPath, const std::string& trophyName,
                      const std::string_view& rarity) {
    std::lock_guard<std::mutex> lock(queueMtx);

    if (emu_settings_get_trophy_popup_disabled()) {
        return;
    } else if (current_trophy_ui.has_value()) {
        current_trophy_ui.reset();
    }

    TrophyInfo new_trophy;
    new_trophy.trophy_icon_path = trophyIconPath;
    new_trophy.trophy_name = trophyName;
    new_trophy.trophy_type = rarity;
    trophy_queue.push(new_trophy);

    if (!current_trophy_ui.has_value()) {
#ifdef ENABLE_QT_GUI
        BackgroundMusicPlayer::getInstance().stopMusic();
#endif
        // Resetting the animation for the next trophy
        elapsed_time = 0.0f;                // Resetting animation time
        fade_opacity = 0.0f;                // Starts invisible
        start_pos = ImVec2(1280.0f, 50.0f); // Starts off screen, right
        TrophyInfo next_trophy = trophy_queue.front();
        trophy_queue.pop();
        current_trophy_ui.emplace(next_trophy.trophy_icon_path, next_trophy.trophy_name,
                                  next_trophy.trophy_type);
    }
}

} // namespace Libraries::Np::NpTrophy
