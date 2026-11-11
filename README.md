# shadPS4 libretro core

A [libretro](https://www.libretro.com/) core of [shadPS4](https://shadps4.net), the PlayStation 4 emulator, for RetroArch and other libretro frontends.

The emulator is upstream's: this repository follows [shadps4-emu/shadPS4](https://github.com/shadps4-emu/shadPS4) and adds the libretro core in `libretro/`. It holds the core only; upstream's standalone entry point, packaging, tests, documents and CI are not part of it (the paths are listed in `.upstream-excluded`). The version the core reports is upstream's; the core is currently on upstream shadPS4 0.20.1 WIP, commit [5dad169](https://github.com/shadps4-emu/shadPS4/commit/5dad16928a8a2cd546d3e75acf51071ec0c9510d) of 15 October 2026. What changed in the core is in [CHANGELOG.md](CHANGELOG.md).

The core is experimental: it is at the stage of first tests, and games may not start at all.

## Downloads

Builds for Windows, Linux (x86_64) and Linux on ARM (aarch64) are on the [Releases](https://github.com/WizzardSK/shadps4-libretro/releases) page, replaced by every full CI build of the release branch.

- **Windows:** `parallel_ps4_libretro.dll`, Windows 10 or newer.
- **Linux:** `parallel_ps4_libretro.so`, glibc 2.39 or newer.

Put the core into RetroArch's `cores` folder and `parallel_ps4_libretro.info` into its `info` folder. The core was called `shadps4_libretro` before; its folder in `system/` is still `shadPS4/`, so what is there is kept.

On Windows and on Linux x86_64 the emulator runs the game's code directly on the host CPU and has no recompiler, so it needs a 64-bit x86 CPU, with AVX2.

- **Linux on ARM (aarch64), experimental:** `parallel_ps4_libretro.so` for an ARM frontend is the emulator built for ARM, with [FEX](https://github.com/FEX-Emu/FEX)'s CPU core (FEXCore) inside it running the game's x86-64 code; nothing else is needed. The port is the one of [Bachata S4](https://github.com/zenithblue-oss/shadps4-arm64), taken over with fixes for a core that is loaded into a frontend. It needs glibc, a GPU with Vulkan 1.3 and a 48-bit address space. Tried on one machine (Snapdragon 7c, Adreno 618 with Turnip): Sonic Mania plays at full speed and Horizon Chase Turbo at about 22 frames a second, held back by the GPU. Where the emulator's own `libSceAvPlayer` or `libSceFont` would call back into the game, dumped modules are needed on ARM, as those calls are not run through FEX yet. Not on Android. The earlier ARM core, which carried box64 and the x86-64 build of the emulator and translated all of it, can still be built (`box64core` in the CI workflow); it is not in the releases.

## Video

The emulator renders with Vulkan and needs a GPU with Vulkan 1.3. A GPU without sparse buffers (Intel Gen9, such as HD Graphics 615) works too, with the GPU's buffers taking more memory.

The finished frames are handed to RetroArch on the GPU: RetroArch draws the emulator's own images, at the size set by the core option Resolution, and nothing is copied. The core asks RetroArch for a Vulkan context, so RetroArch uses its `vulkan` video driver (switching to it where it allows cores to switch drivers), and the emulator uses the GPU RetroArch is on, where there are several. Where the images cannot be shared with RetroArch's device (another GPU or driver, or a driver that cannot import memory), each frame is read back into system memory and uploaded instead, and the log says why; with a frontend that gives no Vulkan context the frames go to it through system memory.

Resolution is also the size the game renders at, where it is below the game's own (most PS4 games render at 1920x1080): at 960x540 such a game renders a quarter of the pixels, which is much lighter on a slow GPU (Horizon Chase Turbo on an Adreno 618: 22 frames a second at Default, 58 at 960x540). It is the core's own: the emulator makes the render targets of the game's frame size (and their depth buffers) that much smaller on the GPU and draws into them scaled; other targets, shadows and effects among them, stay as they are. Effects that read those targets at their full size can come out wrong, and some games may break; Default is then the fix. Above the game's own resolution the game renders at its own and the picture is scaled up. Default hands over the game's own size, as soon as the game has set it up; until then (the first moments) the picture is 1920x1080.

## Content

A dumped PS4 game as a folder; load its `eboot.bin`.

A fake-signed PKG (fPKG) of a game can be loaded as it is. The update and additional content are PKGs too, put beside the game's: of the PKGs in the game's folder with its title ID, the update with the highest version is laid over the game, and each additional content is offered to it. Load the game's PKG, not the update's.

How the PKG is run is the core option System > PKG Content:

- **Run from PKG** (the default): the game runs straight out of the PKG. Its files are decrypted and unpacked as the game reads them, and nothing is written to the disk.
- **Install:** the game is extracted into `system/shadPS4/games/<title ID>/`, its update into `games/<title ID>-UPDATE/` and each additional content into `addcont/<title ID>/<entitlement label>/`, and the game is run from there. This happens the first time; after that the installed game is started at once, and a PKG that was replaced by another (a new update) is installed again. It reads faster where the drive or the machine is slow, but takes the room of the whole game.

The core carries no keys. PKGs are opened with two RSA keys that go into `system/shadPS4/pkg_keys.txt`, one `name = hex` a line, big-endian:

```
derived_key3_modulus = ...
derived_key3_private_exponent = ...
fake_modulus = ...
fake_private_exponent = ...
```

`derived_key3_public_exponent` and `fake_public_exponent` can be given too; left out they are 65537. These are the keys of upstream shadPS4's `src/core/crypto/keys.h` before its PKG support was removed (its `PkgDerivedKey3Keyset` and `FakeKeyset`). Without the file, a PKG is not started and a message names the file. PKGs signed for retail consoles cannot be opened.

## System modules and fonts

The PS4 has no BIOS file, and the core starts without one. What takes its place are system modules and fonts dumped from a console's firmware, and some games do not start without them:

- **System modules** (`.sprx` files) go into `system/shadPS4/sys_modules/`.
- **Fonts** go into `system/shadPS4/fonts/font/` and `system/shadPS4/fonts/font2/`, the console's two font folders. Without them some games show no text or are unstable.

The core creates these folders the first time a game is started. They have to be dumped from your own console; the core does not download them.

The emulator has its own replacement for some modules and uses it when the dumped file is missing: `libSceLibcInternal`, `libSceNgs2`, `libSceAvPlayer`, `libSceRtc`, `libSceJpegEnc`, `libScePngDec`, `libScePngEnc`, `libSceFont`, `libSceFontFt`, `libSceRudp`, `libSceSystemGesture`. The dumped module is still the better choice where there is one.

For the others there is no replacement, and a game that calls into one that is missing usually stops: `libSceUlt`, `libSceJpegDec`, `libSceJson`, `libSceJson2`, `libSceCesCs`, `libSceFreeTypeOt`, `libSceFreeTypeOl`, `libSceFreeTypeOptOl`, `libSceAvPlayerStreaming`, `libSceAt9Enc`, `libSceAudiodec` and its `libSceAudiodecCpu*` modules, `libSceBeisobmf`, `libSceBemp2sys`, `libSceWkFontConfig`, `libScePsmKitSystem`, `libSceDepth`, `libScePadTracker`, `libSceMoveTracker`, `libSceXml`.

When a game stops at start, look in `system/shadPS4/log/shad_log.txt` for these lines. They name what is missing:

```
No HLE available for libSceJpegDec.sprx module
Can't Load libSceLibcInternal.sprx switching to HLE
No dumped system fonts, expect missing text or instability
```

## Files

Everything the core keeps is in `system/shadPS4/`, laid out as standalone shadPS4's user folder, so what is set up there can be copied over: settings, save data, the shader cache, the folders for dumped system modules and fonts, and the emulator's logs in `log/`: `shadps4.log` covers the start, and from the point a game is loaded the log is `shad_log.txt` (or a file named after the game's ID with the core option Separate Log Files).

## Controls

RetroArch's ports 1 to 4 are the console's controllers, as many as the core option Controllers says (one by default): buttons, both sticks, analog triggers and rumble. Select is the touchpad button. RetroArch's Controls menu shows the buttons under their PS4 names, for remapping.

## Core options

- **System:** PS4 Pro Mode, Console Language, Devkit Console Mode, Additional Memory, Confirm Button, Connected to Network, PKG Content, Controllers, Show Splash Screen, Exact CPU Flags (only in the box64 build of the ARM core)
- **Video:** Resolution, VBlank Frequency, GPU Readbacks, Direct Memory Access, Readback Linear Images, Copy GPU Buffers, Shader Cache, Show FPS Counter
- **Audio:** Volume
- **Trophies:** Trophy Notifications, Trophy Notification Position, Trophy Notification Duration
- **Logging:** Logging, Separate Log Files, Log Sync

They are the settings of standalone shadPS4's settings dialog that apply to a core, with its defaults. Volume, Show FPS Counter and the three trophy options take effect at once, while the game runs; the others when the next game is started, or after Reset. The one default that differs is Copy GPU Buffers, on in the core: its picture reaches RetroArch a frame or two late, which makes the fault that setting gets around a regular one. Settings without a core option are in the emulator's config files under `system/shadPS4/`; a per-game setting there wins over the core option. The option texts are translated through libretro's [Crowdin project](https://crowdin.com/project/libretro).

There are no save states: the emulator has none.

## How it works

The core does not run the emulator inside RetroArch's process. It starts itself again in a second process and trades with it through shared memory: the picture and the sound come out, the pads and the core options go in. On Windows the second process is `rundll32.exe` loading the core; on Linux it is a small launcher carried inside the core and run from memory.

That is because the emulator needs the address range the game was built for to itself, and cannot stop a running game other than by ending its process. This way closing content ends the emulator and leaves RetroArch running, another game can be loaded afterwards, and a game that crashes does not take RetroArch with it.

## Building

Linux, with upstream's build dependencies (the list is in `.github/workflows/libretro.yml`), from a checkout with its submodules:

```
git clone --recursive https://github.com/WizzardSK/shadps4-libretro.git
cd shadps4-libretro
cmake -B build -DENABLE_UPDATER=OFF \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build --parallel
```

Or with the static Makefile, which needs no CMake (`nproc` jobs; the objects go under `obj/`):

```
make -j$(nproc) CC=clang CXX=clang++
```

On Windows, the same in an MSYS2 UCRT64, MINGW64 or CLANG64 shell, with `make -j$(nproc)` (the shell's own gcc or clang); a cross build from Linux needs only `CC` and `CXX` set to a MinGW-w64 compiler.

It takes `DEBUG=1`, `LTO=-flto`, `SANITIZER=address`, and for an aarch64 build `LIBRETRO_X86_64_PARTS=<dir>` (box64 and the x86-64 build) or `ENABLE_FEX_GUEST_CPU=1` with `FEXCORE_GUEST_CPU_SOURCE_DIR` and `FEXCORE_GUEST_CPU_BUILD_DIR`, as the CMake options of the same names. The source lists in `Makefile.common` follow `CMakeLists.txt`.

The CI workflow `.github/workflows/libretro.yml` builds both platforms. `.gitlab-ci.yml` is the recipe for libretro's buildbot, and `.github/workflows/gitlab-preflight.yml` replays it.

## License

GPL-2.0-or-later, as shadPS4. See [LICENSE](LICENSE). The ARM core's FEXCore and the code taken from Bachata S4 are under the MIT license.
