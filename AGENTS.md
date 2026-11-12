# Rules for coding agents

These apply to anyone, human or agent, working in this repository (Claude Code reads them through `CLAUDE.md`). They came out of testing the libretro cores with their testers, and they are the rules of the other parallel cores (parallel-gx1, parallel-gx2, parallel-ps2, parallel-n64, parallel-dc, parallel-xbox, parallel-xbox360) in so far as they apply to shadPS4. Read the section that covers the code you touch before touching it.

The order of what matters: accuracy first (the same guest-visible bytes, order and timing as standalone shadPS4), then a game that never hitches, then no work done that the hardware did not need done.

## Ground rules

- **Never compile a pipeline on the draw path.** After a game has started, nothing on the GPU thread (the Liverpool command processor and the rasterizer it drives), the game's threads, the presenter or anything `retro_run` waits on translates a shader, creates a shader module or a pipeline, or waits for one. This outranks every other renderer change. See "Shader compilation and stutter".
- **No allocation mid-game.** From the first frame until the game is closed, nothing per frame, per draw, per guest call, per audio or input poll allocates or frees. See "No allocations during play".
- **No locks, no sleep-polling.** Threads hand work over through atomics, SPSC/MPSC rings and eventcount park/wake. A mutex added to fix a race is the wrong fix: give the state one owner. A loop that sleeps a few milliseconds to look again is a defect. See "Ownership and threads".
- **Zero-copy wherever a copy has no consumer.** Every change looks for copies to remove in the code it touches. See "Zero-copy and borrowed lends".
- **Every fix ships with its test**, wired into CI, failing on the old code and passing on the fix.
- **libretro-common, not shims.** Where the core needs a facility libretro-common has (atomics, rings, eventcounts, threads, filestream/VFS, hashing, compression, archives, data transfer), it uses libretro-common's, not a hand-rolled one and not an OS-specific call. Its files sit in the tree as a plain sync of RetroArch's `libretro-common/`: no core-local edits or private APIs. What the core needs that it lacks goes into RetroArch's libretro-common first, and the core syncs it.

## Branches and history

- The `libretro` branch is never deleted, and nothing that rewrites its history is run on it: no force-push, no rebase, no amending or squashing of commits that are already pushed. It is the fork's only long-lived branch. A mistake is corrected by a follow-up commit.
- New features and experimental changes go on their own branch, and testers test builds of that branch. When testing is done, the related commits are squashed and merged into `libretro`, so the main branch does not collect commits that were superseded midway.
- A fix that users of the `libretro` builds need before the branch is merged is cherry-picked into `libretro`, in a way that does not break the later merge of the branch.
- No pull request is opened without the developer's explicit permission for that pull request.

## Merging upstream shadPS4

This repository builds the libretro core and nothing else. The standalone's parts - its entry point, packaging files, tests, documents, upstream's CI and its contributor files - were deleted on purpose, and must not come back with a merge. The paths are listed in `.upstream-excluded`.

- Upstream is shadps4-emu/shadPS4, branch `main` (remote `upstream`). Only full merges of it, no cherry-picked upstream commits.
- Right after `git merge` of upstream, before resolving anything else, run:
  `git rm -r -q --ignore-unmatch --pathspec-from-file=.upstream-excluded`
  This settles the modify/delete conflicts in favour of the deletion, and it also removes files upstream newly added under those paths, which git would otherwise bring in without any conflict.
- Never resolve a conflict on one of those paths by restoring the file.
- When upstream changes a build file around one of the removed parts, keep them removed and take the rest of the change.
- When something new is deleted for the same reason, add its path to `.upstream-excluded` in the same commit.
- This `AGENTS.md`, `README.md` and `CHANGELOG.md` replace or add to upstream's own. When a merge conflicts on them, keep these.
- After a merge, `CHANGELOG.md` names the upstream commit merged.
- There is no `LIBRETRO` define or `ENABLE_LIBRETRO` option: the core is the only thing built. When a merge brings upstream code for the standalone's window, presentation, audio devices or packaging, keep the core's behaviour and drop the standalone's, without adding a guard to choose between them. Keep such changes small, so merges stay easy.
- Work the rules below demand of the emulator itself - removing draw-path compiles, allocations, locks, sleeps and copies - is not limited to libretro-only code: it changes upstream's files where it has to. Keep it merge-friendly: new mechanisms in new files, narrow hooks in upstream's, and no reformatting of code the change does not need to touch. A merge that conflicts with such work keeps the work and takes the rest of upstream's change.

## These files

- Only the developer writes to or deletes `AGENTS.md` and `CLAUDE.md`. An agent does not change them on its own; it proposes the change to the developer instead.

## Changelog and README

- `CHANGELOG.md` is updated in the same commit as the change it describes: what changed for someone using the core, newest first. Not a copy of the commit message.
- A change on a feature branch gets its entry on that branch, under "Unreleased"; it reaches `libretro` with the squash merge.
- `README.md` is kept true in the same way: when a change makes a sentence in it wrong (a limit lifted, an option added, a file moved), fix the sentence in that commit.
- A result is written as what was measured or tried, on which machine, and what was not: "tried on one machine", "not yet run by anyone". Never as fixed when only a software device ran it (see "Testing").

## How the core is built

- The emulator runs in a process of its own, started by the core (`libretro/libretro_core.cpp` in the frontend's process, `libretro/host.cpp` in the emulator's). Everything that crosses between them goes through the shared memory of `libretro/shared_state.h`. Do not move emulator code into the frontend's process: the emulator needs its address range to itself and can only be stopped by ending its process.
- A change to the layout of `SharedState` bumps `SHARED_VERSION`.
- The shared memory is the one place the two processes meet, so it is the place to get ownership right: every field has one writer, is published with release and read with acquire, and a ring in it is SPSC with its producer and consumer named in a comment. The frame and audio paths through it are borrowed in place (the frontend is handed the frame where it lies in the shared memory, not a copy of it), with the slot held until the frontend is done with it.
- Builds are CMake. A commit that adds, removes or renames a source updates the CMake files and anything else that lists sources (`.gitlab-ci.yml`, `.ci/` scripts, `libretro/embed_host.cmake`) in the same commit, and builds the core to completion on every lane it touches.

## Code

- Keep comments true. When a change makes a comment describe behaviour that no longer exists, fix or remove the comment in the same commit.
- In the emulator's own files follow upstream's style and its `src/.clang-format`, and comment only what the libretro core changes and why. The core's own files in `libretro/` are commented as the other cores are: short comments that say why.
- **A new source file is a `.c` file in C89 that MSVC compiles, and its test is C89 too.** No new `.cpp`, and no new C++ header with code in it: a new library of the PS4's, a decoder, a mixer, a helper is written in C, and the few lines that hand it to upstream's C++ (a `LIB_FUNCTION` list, a call through the game's calling convention, a log line through `src/common/c_bridge.h`) go into the existing C++ file that uses it. The only reason for a new C++ file is code that cannot be written without upstream's C++ types, and the commit message says which.
- **New self-contained code is C89 that MSVC compiles**, with narrow typed C++ glue where it meets upstream's C++: new modules, ring and pool helpers, file-format readers, CI checkers and test harnesses. Declarations at the top of a block, `/* */` comments, no designated initializers, compound literals, VLAs or `//` in a C unit. A header a C unit includes compiles as C and as C++. Inside upstream's C++ files, a change writes C89-shaped code (a struct and functions, a switch on a kind, an explicit init and free; no new classes, lambdas, exceptions or STL), and its codegen is checked to be faster than the C++ form, or at worst neutral.
- **Stack frames stay small**: 4 KiB at most in the core's own code, 2 KiB in libretro-common.
- **Validate first, mutate second.** Reject bad offsets, counts and overflowing ranges before touching a destination. An empty span is zero bytes, never one, and never a pointer formed past the end.
- Look at the big picture, not just the function being changed. Resetting the content and closing it release the game's resources through the same path: a reset is an unload and a load (here, the emulator's process ended and a new one started).
- When you find inefficiencies in code you touch, fix them, even if the gain is small. Do the safe parts of an optimization first rather than setting the whole of it aside because part of it is risky.
- Stop and join threads the way upstream shadPS4 does in its own shutdown, not with methods made up for the libretro port. Ad-hoc ones are what cause shutdown and reset bugs.
- No game-specific hacks, as upstream asks: a fix addresses the emulation's behaviour, not one title.
- Every core option has to be connected to something. Do not add an option the core does not read, and remove one that turns out to do nothing. Option defaults follow the standalone's defaults unless there is a reason, written down beside the option. An option off costs nothing over the code without it.
- Option texts never mention other emulators or cores.
- A setting the frontend's user should reach is a core option, set on top of the emulator's configuration when a game starts; the emulator's own config files stay the place for everything else.
- No runtime logging added to measure something. Timing and profiling go in standalone tools or an opt-in build; no per-frame, per-draw or per-compile log line in a release build (the `LOG_INFO` on every compile in `vk_pipeline_cache.cpp` is one to take off the draw path, not a model).

## Shader compilation and stutter

The target is the console's own behaviour: a game never waits for a shader. The first play of a game, with no shader cache, pipeline cache or driver cache on disk, has to be as smooth as playing it with all of them full: no compile is ever seen in gameplay. Shader compilation gets better with every commit, and never worse.

PSO here means a complete Vulkan pipeline. Guest GCN code, translated IR, SPIR-V words, a `VkShaderModule`, a driver cache blob and a ready `VkPipeline` are different milestones: a warmed shader alone does not make a first draw free of a hitch.

### Where the core stands

The rule is not met. These draw paths still compile, and each one is a bug to remove, most-played first. They are debts, not precedent:

1. `PipelineCache::GetGraphicsPipeline` and `GetComputePipeline` (`src/video_core/renderer_vulkan/vk_pipeline_cache.cpp`), called from the rasterizer's draws and dispatches, build an unseen key on the spot: GCN translation (`Shader::TranslateProgram`), SPIR-V emission (`EmitSPIRV`), `vkCreateShaderModule` (`CompileSPV`) and `vkCreateGraphicsPipelines`/`vkCreateComputePipelines`, with the draw waiting.
2. The `GraphicsPipeline` constructor (`vk_graphics_pipeline.cpp`) creates the tessellation and fragment stage modules inline, inside that same miss.
3. `TileManager::GetTilingPipeline` (`texture_cache/tile_manager.cpp`) builds a detiling or tiling pipeline the first time a format meets it, and `BlitHelper` (`texture_cache/blit_helper.cpp`) builds its MSAA copy and colour-to-depth pipelines the first time a key meets it. These are utility pipelines with a known, finite key space: build them all before the first frame. The fault buffer, FSR and post-process pipelines are built when their owners are made; keep them there.

`PipelineCache::WarmUp` and the serialized pipeline data (`vk_pipeline_serialization.cpp`, `cache_storage.cpp`) help a warm run. They do nothing for the cold one, which is the case that counts.

### Invariants

1. **Nothing new compiles on the draw path.** After the game has started, no shader translation, module, pipeline or library is created on the GPU thread, the game's threads or anything `retro_run` waits on, and nothing there waits for one. A code path that adds one is a bug, even when it runs only the first time a state is seen. Moving an existing one off the draw path is the direction of every change here. This covers every pipeline the core owns: draws, dispatches, tessellation, tiling and detiling, blits, clears, the host passes.
2. **Every draw has a correct pipeline ready.** A draw whose specialized pipeline is not built yet uses one that renders the same result: a general pipeline that reads what was baked from push constants, uniforms or dynamic state, or graphics pipeline libraries built ahead and fast-linked (fast-linking parts that were all built ahead is the only pipeline call the draw path may make). Never a skipped draw, a placeholder, a stale frame or a guessed approximation. Before a key loses a field to dynamic state, show that the dynamic version renders the same. Where no correct ready path exists for a case yet, that case stays a listed violation; it is not hidden by dropping work.
3. **A miss is a state machine.** Unseen, queued, translated, module ready, pipeline queued, pipeline landed. The draw takes the best ready step. There is no synchronous step: a path that still builds on a miss is a listed violation, counted as such, and the count only falls.
4. **Workers never hold up a draw.** Compile queues are bounded, and admission counts results not yet drained as well as work not yet done. A full queue defers speculative work and the draw goes ahead with its ready pipeline; a compile is never run inline to make room. Finished work is adopted on the thread that owns the cache, published with release/acquire and checked against its key and generation. A worker result that lost the race is destroyed, not swapped in. Raising queue sizes or worker counts because utilisation looks low is not a fix.
5. **The cache is never what removes a stutter.** Invariants 1 and 2 hold with the Shader Cache option off and with nothing on disk. With it on:
   - nothing reads or writes the disk on the GPU thread, the game's threads or anything `retro_run` waits on;
   - what is new collects in memory and is written by a worker, or at shutdown;
   - cache files live under `system/shadPS4/`;
   - a corrupt, rejected or mismatched file is a recoverable miss: start from an empty cache, never crash, never reuse wrong code.
6. **Cache the right representation.** Recorded keys and translated SPIR-V are portable recipes and survive a driver update; a `VkPipelineCache` blob is tied to its device, driver and version and is never reused on the strength of a portable key. A list written at the end of a short session merges with the old one rather than replacing it.
7. **Stable identity.** Keys are compared byte for byte: zero the padding before hashing; clear the fields a pipeline does not read; audit every field through key construction (`RefreshGraphicsKey`, `RefreshComputeKey`), pipeline creation and dynamic-state emission before removing one, as omitting state a pipeline consumes is a rendering bug; include the shader recompiler's profile and options (fast shader math among them), the SPIR-V target and the host configuration; never key on a pointer or a handle. A hash match needs an exact-key compare. Changing what a key or a cached entry means, even when its size stays the same, bumps the cache version in the same commit.
8. **Zero-copy in the shader path.** Each shader's words are owned once and handed over without a copy: `RegisterShaderBinary` and module creation take what the emitter made, not a clone. Cache files are mapped, or read in place through filestream/VFS, and validated slices go straight to the driver (`vkCreatePipelineCache` initial data). No slurps, no byte-by-byte copies, no wrapper allocated only to be discarded. Shader IR and SPIR-V are built in fixed or reused storage (the recompiler's `pools` are the model), with no formatted temporaries and no heap allocation during play.
9. **A faster compile that runs slower is a regression.** Emitted SPIR-V words and instructions are tracked with the compile counts. A report that a shader got slower on the GPU is reproduced, the emitted code compared, and the result pinned by a test.

### Draw-side overhead

- **Compare in the destination.** Uniform and push-constant blocks compare and update in their packed form; no second copy of the values exists to compare against.
- **Reuse immutable snapshots.** An unchanged constant block reuses its staged copy within the same staging generation; a reset or a reallocation invalidates it. Never overwrite a snapshot an in-flight draw can read.
- **Descriptors.** Reuse a set when its contents are unchanged and move dynamic offsets; write a fresh one when anything changed. A set an in-flight command buffer recorded is never patched in place.
- **Clear and draw are separate states.** A clear never reuses a draw pipeline or inherits its dynamic state, nor the other way round.

### The gate: every commit, measured

A commit that touches `src/shader_recompiler`, `src/video_core`, the pipeline keys, the caches or the file I/O behind them reports, in its delivery and not its commit message:

- pipelines and shader modules built on the draw path after start, by kind: must not rise, target 0;
- draws or dispatches that waited for a compile: must not rise, target 0;
- distinct pipelines and modules built for the same corpus;
- emitted SPIR-V words and instructions;
- heap allocations during play attributed to the renderer;
- pictures identical to the parent's, with no validation errors.

Measured in four cache states kept apart deliberately: cold (no emulator caches, the driver's identified), recipes only, everything warm, and warm in-process. Compare each case with its parent on the same machine, and keep a pinned last-known-good reference: nothing promotes a newer result to the reference automatically, and no threshold is loosened without the developer's say. A missing device, a skipped suite, an empty corpus or a timeout is unverified, never a pass. Cold must match warm: the cold run's p99 and max frame time stay within the warm run's spread, and a gap is a stutter to fix. The corpus in CI is synthetic or homebrew; no game assets, extracted shaders or dumps in the repository or in CI artifacts.

## No allocations during play

As with compiles, a game never waits for memory. Once a game runs, nothing is allocated mid-game: not on the GPU thread, not on the game's threads, not on the audio path, not on the compile workers, not in `retro_run`.

- **What counts.** Heap allocation of any kind (`malloc`, `new`, `make_unique`, a container that grows or rehashes - `try_emplace` into the pipeline maps among them - a `std::string` or `std::vector` built and thrown away, `fmt::format`), and the GPU's: `vkAllocateMemory`, `vkCreateBuffer`/`vkCreateImage`, views, descriptor pools and sets, command pools made for one draw, pass or frame. Freeing during play is the same work and counts too. "Only the first time it grows" is the same stutter.
- **Sized before the first frame.** Storage play needs is made when the emulator starts, a game loads or a setting changes, at a bound taken from the most that can be live at once: the guest's address space, the driver's real VRAM budget (`VK_EXT_memory_budget`), the hardware's own limits. Play takes from pools, rings, arenas and free lists and gives back. A thread that starts at load makes what it will use then.
- **A full pool is handled, not grown.** Evict, recycle or defer to the next frame; never allocate on the spot, never drop a draw. A pool that grows on exhaustion still allocates.
- **Moving an allocation to another thread does not remove it**, and neither does an oversized reservation chosen to hide one. A permanent reservation is a trade-off to state, not a free win.
- **Measured.** Allocations during play are counted where they happen, with first-use and steady-state counts kept apart. Every place that still allocates is a listed debt; the list only shrinks, and a commit that touches such code removes its allocation. Known to allocate during play today: the texture and buffer caches' growth and garbage collection, the pipeline maps and the shader binaries kept per compile, the tiling and blit pipeline maps, and the audio stream list in `libretro/host.cpp`.

## Zero-copy and borrowed lends

Every change looks for copies to remove in the code it touches, and every delivery says what it found and removed. A copy that stays has a reason a reader can see.

- **One owner, the rest borrow.** Data is owned once. Everything else takes a pointer and a length for a bounded time. The borrow ends before the owner frees or reuses the memory, and a borrowed slice outlives every consumer, including the driver call it is passed to, the GPU submission that reads it and the frontend that is shown it.
- **The borrow contract.** Acquire a valid view and its reference; settle its producer; consume it within its lifetime; release it after the last synchronous read or the fence of the last asynchronous one. A consumer that stores the pointer, or a guest that may write the memory meanwhile, needs a snapshot or a proven copy-on-write. A pointer alone is not a zero-copy design: a lease, a pinned slot, a generation-tagged handle or a submission index makes the lifetime explicit.
- **Producers write where the data ends up.** A detiler or converter writes into the memory the GPU reads, the emulator's frame into the image or slot the frontend is handed, a PKG block into the game's read buffer, the audio mix into the shared ring. No intermediate buffer, no container returned by value to be copied somewhere else, no pop-to-scratch stage.
- **No CPU-GPU round trips without a CPU consumer.** A frame read back from the GPU into system memory to be uploaded again by the frontend is a round trip: the picture is handed over on the GPU (libretro/gpu_frames.cpp), and the readback path is only for a frontend device the images cannot be shared with. Readbacks the guest really needs keep their completion waits; everything a GPU-only consumer reads stays on the GPU. Defaults change only on the developer's say.
- **No slurps.** Files are mapped, or read in place through libretro-common's filestream/VFS. A file read whole into a fresh allocation, to be parsed or compared, is a bug when it could be mapped or streamed. A parser borrows the mapped bytes for as long as its tree lives. A PKG run in place is read a block at a time into its destination, as it is today; keep it so.
- **Copies that must stay say why**, with a comment naming the reason: guest memory the GPU reads later is snapshotted as the game's threads go on writing it; data an API hands over only for the length of a call; a buffer that keeps a failed or cancelled read from changing its destination. A bounded snapshot reused while unchanged is often faster than fencing every guest write: make it small, do not make it go away.
- **Say exactly what "zero-copy" removed.** No CPU pixel copy, no CPU/GPU round trip, a borrowed GPU image and shared memory are different claims. Name the duplicate that went and the work that stays (the GPU may still copy, the driver may still stage).
- **Write the final representation once.** Detiling, format conversion, decryption, decompression and endian swaps are work the emulation needs: fuse them into the write to the final destination rather than adding a stage.
- **Reuse storage with a defensible bound.** Reset counts rather than clear payloads; do not clear bytes about to be overwritten; define what happens when the pool is exhausted.
- **Three ledgers for a resource.** Content (which producer wrote the newest version, covering which bytes), lifetime (which owner, command buffer, submission or frontend ticket can still reach it) and cost (bytes, passes, driver calls, allocations, waits). Draw the graph of an expensive transition, remove the edges whose consumer can use an existing version, and only then schedule what remains better.

## Speed of light

Measure against what the work needs and what the hardware can do, not against the previous build. Every byte copied, allocation made, layer crossed and wait taken is a cost with a reason or a defect.

- **The cost model** is bytes moved, cache lines touched, instructions, allocations, driver calls and waits, together with the guest progress they bought. A copy removed on one thread and added on another, or on the GPU, is not removed.
- **Ownership, representation and granularity before instructions.** Fewer shared writers, fewer temporary forms of the same data and fewer redundant driver interactions first; instruction tuning last.
- **Delete by reachability.** Code nothing in the core reaches (the standalone's UI, updater, Discord, and its input and audio backends where the frontend replaces them) costs initialisation, binary size and maintenance. Trace the configurations, callbacks and failure paths that could reach it, then delete it, and add its path to `.upstream-excluded` if it is a whole file.
- **Inspect what the compiler made** where it can undo an optimisation (a `malloc` plus `memset` becoming `calloc`, a hidden TLS call, a copy loop turned back into `memcpy`, a register spill in a hot loop).
- **Acceptance.** Exact output and preserved lifetimes first. Then the measured benefit. A smaller copy count, a C rewrite or a microbenchmark win alone does not show smoother play.
- **Stop conditions.** Keep the copy if the consumer outlives the borrow. Keep the barrier if its proof is missing. Keep the general shader if the equivalence is incomplete. Keep the fallback when the new method loses.

## Ownership and threads

- **Change ownership, not the lock.** One thread owns a structure's mutation; others send it requests and read immutable publications. SPSC where there is one producer and one consumer, MPSC where there really are several. New code uses libretro-common's `retro_atomic`, rings and eventcount, not `std::mutex`, `std::condition_variable`, OS critical sections or `Interlocked*`; a lock only where there is truly no alternative, said so in a comment.
- **Nothing blocks the frontend's thread.** `retro_run` never waits on the emulator's process, a worker or the GPU: it takes the newest finished frame and the audio that is there, and hands them over. Nothing on the GPU thread waits on a worker either.
- **Publication is a lifetime contract.** Fill every field, then release-publish; acquire before reading. Use the weakest ordering that is correct, and say why in a comment. A job published to a worker is self-contained: never a pointer to a stack temporary or a stack create-info, never moved after its interior pointers are set. Completion is the last access to a request: the caller may reuse its storage at once.
- **Parking has two checks.** Check the predicate, prepare the eventcount wait, check again, then commit or cancel. A one-check sleep loses wakeups. Parking an idle worker is fine; busy-spinning one is not.
- **No sleep-polling, no wall clock deciding progress.** A thread waits for work on an eventcount (across the two processes, on a futex or OS wait on the shared word), not in a `sleep_for` loop. A wait that has to be bounded (closing content, the frontend's device appearing) wakes when signalled and has a timeout only as its guard, never as its pace. No time slices, deadlines or frame budgets decide how much work a thread does.
- **Name progress precisely.** An atomic fast path is not lock-freedom. A CAS loop waiting for an owner-held bit is a lock. Finite polling before parking is a latency policy, and needs numbers that show it pays.
- **Shutdown and failure paths are part of the design.** Stop the producers, cancel queued work, finish active work, drain the results, release the references, and only then destroy device objects. Join workers even when the feature they serve was off.
- **Thread placement is conservative.** The game's threads, the GPU thread and the audio path come first; compile workers and other auxiliary parallelism take what is left. No worker per logical CPU: GPU drivers, the OS and the frontend run threads of their own.

Not there yet, in the core's own glue. These are debts, not precedent:

1. `libretro/host.cpp` polls with `sleep_for`: live options every 20 ms, the audio mix every 2 ms, the pads every 2 ms, the frontend's GPU in 10 ms steps for up to five seconds, a free frame image every 1 ms.
2. `libretro/host.cpp` guards its audio stream list with `audio_mutex`.
3. `StopEmulator` in `libretro/libretro_core.cpp` waits for the emulator's process to end in 10 ms `usleep` steps; it should wait on the process itself (a pidfd, `WaitForSingleObject`) with the same time limit.

## Audio, video and pacing

- **Audio:** each `retro_run` hands the frontend exactly one frame's worth (sample rate / declared fps, with the fraction carried), never "whatever is queued" and never a fixed chunk size. Several streams (audio ports, Audio3D, the music player) are mixed, not appended one after another, and each is taken in its own format and sample rate. The game's audio threads wait once about 64 ms is queued; no bigger buffer on the core's side. The core does no resampling that is not part of the console's own audio; the frontend resamples. The mix is written into the shared ring in place, with no intermediate copy.
- **Timing goes by real rates.** The core declares the guest's refresh rate (the VBlank Frequency option); the frontend paces display and audio, and the core does not sleep to pace itself. Nothing is tuned to feel like 60 Hz at other rates: higher refresh rates are smoother.
- **Geometry:** the size the frame is handed over at goes to the frontend: base geometry at load, SET_GEOMETRY when it changes, SET_SYSTEM_AV_INFO when it would exceed the max. The max has to cover the highest output resolution, and no more than that: a frontend sizes its shader passes by it.
- **Rendering design:** zero-copy, no CPU-GPU round trips, the smallest exact formats, buffers and caches sized from the driver's real VRAM budget and never paged, no CPU waits on the GPU except true readbacks (rings and polled fences instead), tile-GPU friendly passes (load/store don't-care where the content is dead, no needless clears, no mid-pass readback), fewer state changes and draws. Measure stutter as max and 99th percentile frame time with GPU timestamps, not averages.
- **A game's progress is the measure.** A faster `retro_run` that presents an old frame again, skips guest work or slows the game's time is not faster. Count new pictures and the game's progress alongside frame times.

## Quantization

Look for it in every change, in audio, video, textures, input and any conversion: precision lost by the core's own handling, not by the console's. What the console itself does to the guest's data (a texture format's bits, the library output a game reads back) is emulation and is matched exactly; everything the core adds on the way to the frontend loses nothing it does not have to.

- **Round, never cut.** A float or wider value made narrower is rounded to the nearest step (halves away from zero, or to even) and clamped. A plain cast or an integer divide cuts towards zero or down: the step around zero gets twice as wide, everything leans one way, and a centred value lands off centre (a stick at rest read as 127 instead of 128).
- **Quantize once, at the end.** Keep full precision (float, or integers with headroom) through every stage and narrow once, where the format is fixed: the frontend's 16-bit audio, its 8-bit picture. A source rounded on its own and again after it is mixed, a level applied to already narrowed samples, a picture taken to 8 bits and filtered again, each adds an error the last step cannot take back.
- **One scale per format.** A 16-bit sample read as float is divided by 32768 and written back multiplied by 32768. Two scales on one path (32767 one way, 32768 the other) are a level change and a lopsided clip.
- **Never narrower than the source.** A game that renders or outputs 10 bits, or a float sound, keeps that depth up to the frontend where the frontend can take it; a fallback to a narrower format is a fallback, said so in a comment.
- **Wider integers from narrower ones by repeating the bits** (`x * 0x101`, `(x << 3) | (x >> 2)`), not by shifting alone: a shift alone never reaches full scale.
- **Dither is a decision**, made where a narrowing is audible or visible (gradients, quiet audio) and said so; it is not left to chance and not added to digital silence.
- **Tests show the rounding:** a value just over and just under half a step, both signs, the two ends, and a sum of two sources that each would round to nothing.

Not there yet. These are debts, not precedent:

1. HDR Output (core option, off by default) is not yet run on an HDR display. With it on and a frontend that takes HDR10, games are told the display shows HDR, a game's own PQ passes through the output pass, and any other picture and ImGui's overlays are encoded to PQ there (`host_spirv_post_process_frag`, `host_spirv_imgui_pq_frag`), SDR white at the frontend's paper white. A game is told the frontend's peak and black level (`sceSystemServiceGetHdrToneMapLuminance`, from `max_nits` and `min_nits`); the full-frame peak is given the peak too, since the frontend does not report one.
2. `ngs2_core.c` writes a game's 16-bit output buffers at a scale of 32767, cut towards zero. That output is read by the game, so what Sony's library does there decides it, and it is not known.
3. A FLAC with no seek table, seeked backwards, makes rflac's state again (its only way back to the start), so that seek allocates.
4. libretro-common's FLAC decoder takes a 24-bit stream to 16 bits by shifting, which rounds down, not to the nearest; a fix goes to RetroArch's libretro-common first.

## Testing

- The emulator runs the game's code on an x86-64 CPU. On ARM (aarch64) the core is the emulator built for ARM with FEXCore inside it running the game's x86-64 code (the older box64 core can still be built); a change can be tried there, but say which build it was tried on.
- The core logs, at start, its version and the commit it was built from. Ask testers for the frontend's log and the emulator's own (`system/shadPS4/log/shadps4.log`, then `shad_log.txt` from the point a game is loaded), and check which commit a log came from before drawing conclusions from it.
- Compare with standalone shadPS4 at the same upstream version before calling something a core bug; when standalone fails the same way, it is upstream's.
- Do the measuring and testing yourself rather than asking testers for numbers. Batch what needs a tester's hands into as few runs as possible, and say up front how many there are.
- A GPU fix tried only on a software device (lavapipe, llvmpipe) or under wine is unverified on real hardware until someone runs it there; say so, and check the other paths it reaches for regressions before delivering. For a live run use the latest RetroArch nightly or RetroArch built from source, never an old distro package.
- **Every fix ships with its test** in the same commit, wired into `.github/workflows/libretro.yml` (or a workflow beside it), shown to fail on the old code and pass on the new. Prefer plain C89/C99 tests where that is faster in CI; keep a scripted test where the C one would be slower once its compile is counted.
- **A flaky test is fixed, not rerun; a slow one is made faster.** Say so at once when a test or CI run is slow, stalls or is flaky, instead of waiting on it.
- **Do not break the existing tests and checks.** Run the ones that cover what a change touches before delivering it.
- Sanitizers (AddressSanitizer, UndefinedBehaviorSanitizer, LeakSanitizer) run on the core's own code and on the tests that reach what changed; a new leak, use-after-free or overflow blocks the change.
- CI builds the releases. A change is still built and run before it is delivered: Linux x86-64 with clang, the Windows cross build (`.ci/build-windows-cross.sh`) for a change that reaches Windows code, aarch64 for a change that reaches the ARM build. Match the build's job count to the CPUs the machine has.

## Measuring honestly

- **Clean runs.** Time on an idle machine, with nothing else building and no other emulator running, alternating parent and candidate (ABBA) with reverse-order repeats. Timing runs take no screenshots, picture hashes, readbacks or GPU query polling; picture checks are separate runs. Report medians, p95, p99 and max with frame numbers, never a single run and never a best of several.
- **Record the controls.** Revisions, driver, GPU, OS, the extensions actually enabled, compiler flags, cache state and the core options go with every result. Use private copies of `system/shadPS4/`; never empty a user's caches or replace an installed core to make a cold run.
- **Explain hitches with counters.** Don't diagnose a hitch by how it looks. A long frame is a compile only when the compile counters put one at that frame; readbacks, the game's own CPU work, PKG reads and page faults look the same. Time driver calls with CPU time, completed GPU work with retired timestamps.
- **Counters cost nothing in release builds.** They exist only in an opt-in build: one atomic add where the event happens, no logging, no wall clocks, no extra work.
- **Equal fixtures.** Instrument both sides the same way and read the generated code before believing a small difference. Use a monotonic high-resolution clock.
- **Keep work and time apart.** Fewer compiles, fewer bytes copied and fewer allocations are work results. They are not frame-time results until timing shows it, and a mixed timing result stays mixed.

## Delivering changes

- Patches are delivered as mailbox files made by `git format-patch` (`0001-*.patch`, with the full commit message), never a bare diff or an archive. Before cutting them, fetch the current `libretro` and rebase onto it, then apply them with `git am` to a clean checkout of that commit and check the resulting tree matches.
- Every commit and patch is authored `LibretroAdmin <libretroadmin@users.noreply.github.com>`, with no `Co-Authored-By` or other agent attribution trailers.
- Deliver each patch as soon as it is ready and has passed its gates, not batched at the end. When the developer says a set is merged, the next delivery holds only the new patches.
- Commands given to the developer are bare and runnable: HTTPS remotes, no placeholder variables, no invented paths in front of file names.
- Core `.info` file changes go to the libretro-super repository (`dist/info/parallel_ps4_libretro.info`) as well as the copy in `libretro/`.

## Libretro pitfalls already hit in the other cores

Each of these was a bug in at least one of the other cores (cemu, dolphin, flycast, pcsx2, rpcs3, vita3k, xenia, xemu). Check new code against them before asking testers.

- Files: everything the core writes goes under `system/shadPS4/` (or saves/); nothing next to the frontend's executable.
- No window: the standalone's UI has no window in a core. Every path that reaches for the window or its UI thread needs a headless branch that answers the way the user most likely would.
- Unload: never end the frontend's process. Closing content ends the emulator's process, with a time limit, and never waits without one. In the frontend's process, never wait without a timeout on a fence, event or thread the frontend has to drive, and no `vkQueueWaitIdle`/`vkDeviceWaitIdle` while RetroArch holds its queue lock.
- Libraries that pin themselves (statically linked OpenSSL and the like) keep the core loaded after `dlclose`; build them so they do not.
- No static TLS in what the frontend loads with `dlopen`.
- Features work out of the box on every platform the core ships for, with no user setup beyond the documented system files.
- Nothing optional is load-bearing: a function used everywhere is never inside a block that one platform compiles out.
- No downloads at run time; data files ship with the core.
