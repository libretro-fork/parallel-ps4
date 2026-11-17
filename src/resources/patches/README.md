# Game patches the core carries

The patch files of two repositories, and the core's own, built into the core (`src/common/embedded_resources.h`) and listed per game in the Patch Manager core options (`src/common/patch_catalog.h`). They are the two repositories standalone shadPS4 downloads its patches from.

- `shadPS4/`: the `PATCHES` folder of [shadps4-emu/ps4_cheats](https://github.com/shadps4-emu/ps4_cheats), commit `cf33d5bae8bd82f483c0aed3f09b2d9e94fe6419`.
- `ParaLLEl/`: the core's own patches, in the same format, with one kind of line of its own: `mask_halve` finds a pattern as `mask` does and halves the decimal number of `Size` bytes at `Offset` from it, where it is (`src/common/patch_halve.h`). `Lower Audio Latency` for Bloodborne, Dark Souls III and Sekiro: FromSoftware's sound engine (MagicOrchestra over FMOD Ex) carries its setup as `Key= value` text in the executable and updates once every `MainThreadUpdateTime` microseconds, 33333; the patch halves whatever the value is by then (16666, or 8333 after Bloodborne's 60 FPS++, which writes 16666 itself). It names no `AppVer`: it applies to every version of the game that has the line.
- `GoldHEN/`: the `patches/xml` folder of [illusion0001/PS4-PS5-Game-Patch](https://github.com/illusion0001/PS4-PS5-Game-Patch) (the PS4 patches; `patches/xml_prospero` is the PS5's), commit `65914503123d4e643aa5aa0e8c720de8e99ef7e8`.

The files are as the repositories have them, with two changes: line ends are LF, and `BloodborneNetworkTest`, which has no extension there, is `BloodborneNetworkTest.xml`. Their `isEnabled` attributes are not read: the core options say which patches are on.

To update, copy the folders over again the same way and note the new commits here. The build embeds every `.xml` in the three folders, shadPS4's first, then GoldHEN's, then the core's: for a patch of the same name and game version in both, shadPS4's is the one applied.
