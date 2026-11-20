# FEX (FEXCore) for the aarch64 core

The files of [FEX-Emu/FEX](https://github.com/FEX-Emu/FEX) at f2b679f6028ce1c38875233aecfcf5d3f8ebecec that the core's `ENABLE_FEX_GUEST_CPU=1` build compiles and includes: FEXCore, the parts of `Source/Common` and `Source/Tools/CommonTools` it links, CodeEmitter, FEXHeaderUtils, and the used files of SoftFloat 3e, cephes, rpmalloc, tiny-json, fmt, unordered_dense, range-v3 and cpp-optparse, with their licenses. Nothing else of FEX is here.

- `generated/` holds what FEX's CMake writes at configure time for an aarch64 build: `git_version.h`, the IR tables (`IRDefines.inc`, `IRDefines_Dispatch.inc`) and the config tables (`ConfigValues.inl`, `ConfigOptions.inl`).
- Changes from upstream:
  - rpmalloc's thread heap uses the default TLS model (`TLS_MODEL` is empty), because the frontend loads the core with dlopen and gets no static TLS.
  - No xxhash: the two XXH3 hashes FEXCore takes (`OpcodeDispatcher.h`, `CodeCache.cpp`) are libretro-common's `xxh3_64` with seed 0, which gives the same values (so the code cache's file names stay the same).

The core's `Makefile.common` builds these files (the `FEX*` groups) into one archive that it links.
