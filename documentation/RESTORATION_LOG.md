# FreeStyle restoration work log

This is the chronological engineering journal for the first playable reconstruction.
It records successful work, unsuccessful attempts, corrections and remaining uncertainty.
Dates use Europe/Paris time (UTC+02:00 on 2026-09-08).

## 2026-09-08 — Retrospective record of work before this journal was requested

The entries below were reconstructed from the command/tool history. Their order is
known; individual execution times were not recorded, so none are invented here.

### 1. Repository and reference inspection — successful

- Read the existing README, extractor CMake configuration, KLX format documentation,
  asset inventory, original distribution and ignored analysis workspace.
- The repository already contained extracted **assets**, not a decompiled demo C++
  application. It also contained a useful unpacked process image:
  `analysis/klx/freestyle.memory.bin` (720,896 bytes), strings and the previous
  Unicorn-based original-decoder validation script.
- Noted a pre-existing user edit in `README.md`; it must be preserved.
- Located 11 version-1 LWSC scenes, 64 LWOB meshes, 72 JPEG images, the MOA files,
  the script and the user-provided `BGM/Mush.xm`.
- Confirmed CMake, MSVC 19.41 / Visual Studio 2022, FFmpeg, Python 3.12,
  Pillow, NumPy, Capstone and pefile are available.
- No applicable `AGENTS.md` was found. No sub-agents were used.
- First PowerShell command emitted a profile execution-policy error. Subsequent
  commands use `-NoProfile` (`login: false`); no policy was changed.
- One `rg` command passed a Windows wildcard as a literal pathname and failed.
  Corrected searches to `rg ... demo-assets -g '*.lws'`.

### 2. nxNG source inspection — successful, scope clarified

- Retrieved `https://github.com/astrofra/preservation-nxng-engine`, commit
  `2c0dfc51f8bdd26c75dd3217a35e7afb729dbb9b`.
- The upstream restoration loads static mesh geometry and renders an orbiting
  OpenGL view. It does **not** yet replay LWS object motion, parenting, camera
  motion, envelopes, music or the Freestyle timeline.
- Read original `Cmotion.cpp`, `Cenvelop.cpp`, `nX_Entity.cpp`, `Nx_scene.cpp`,
  `nX_LWloader.cpp`, the rasterizers, lighting and particle implementations.
- Decision: preserve the complete upstream source in `vendor/nxng`; adapt its
  portable LWOB reader and port the relevant original motion/math behavior into
  `src/`. Do not attempt to compile the obsolete DirectDraw/Direct3D 7 shell.
- The player is derived from the GPL-3.0 nxNG source. The existing extractor's
  Unlicense is a separate license; dependency licenses are retained.

### 3. Video remux and first visual inventory — successful

- FFprobe identified separate VP9 video and Opus audio streams.
- Actual video dimensions are **1920x1440 at 60 fps**, regardless of the
  `2160p60` wording in the filename. Video duration: 208.517 s;
  audio duration: 208.581 s.
- Remuxed without recompression to `video-reference/freestyle-reference.mkv`:
  `ffmpeg -i <video.webm> -i <audio.webm> -map 0:v:0 -map 1:a:0 -c copy ...`.
- Generated a first contact sheet at one image per five seconds:
  `analysis/reference/contact.jpg`.
- Confirmed the visual sequence: tunnel, zombies/credits, Freestyle logo,
  hill, mushroom trip, illustration, manor approach, stairs, trap, wagon,
  greetings and final Syndrome logo.

### 4. Original executable timing recovery — successful

- Used Capstone on the existing unpacked x86 process image, based at `0x400000`.
- Located immediate references to the original scene path strings, including
  `0x426a46 -> 0x4451f8` (tunnel), `0x426be1 -> 0x4451dc` (zombies),
  `0x426d7f -> 0x4451b8` (title), `0x4288f6 -> 0x445048` (manor),
  and `0x428ec7 -> 0x444f6c` (final logo).
- Disassembled the application sequence at approximately `0x426900..0x429500`.
  Working output: `analysis/demo-sequence.asm`.
- Recovered absolute millisecond comparisons against the music-start
  `GetTickCount` value: 24000, 37000, 39000, 45000, 73000, 96000, 97500,
  103000, 131000, 147000, 166000, 167000, 200000 and 210000.
- Some boundaries are loading/hold intervals, not new scenes. The title stops
  updating at 39 s but remains until 45 s; the illustration stops updating at
  97.5 s but remains until 103 s; the trap freezes for the 166..167 s load gap.
- The recovered timeline supersedes the approximate production notes in
  `script.txt`. In particular, the manor approach occupies 103..131 s.
- Used the LWS **PreviewFirstFrame / PreviewLastFrame** settings, as the original
  loader does. Using the render frame range would break the tunnel immediately
  (`FirstFrame 450`, but `PreviewFirstFrame 1`).
- Full executable decompilation was unnecessary for this first milestone.

### 5. Portable source and dependencies — successful with one integration failure

- Added the `freestyle` player to the existing CMake project, retaining
  `klx_unpack` and its tests.
- Vendored GLFW 3.4 for windows/input/OpenGL and miniaudio 0.11.23 for audio output.
- Initially inspected current libxm. Its C23 requirement was unsuitable for the
  available MSVC toolchain. Selected the upstream C11-era **v0.2** tag instead
  (`e36ab122a169d451e5f219e1c03724a409a877ba`). No failed C23 build was attempted.
- libxm v0.2's own CMake assumes GCC flags. Added a small target in our CMake
  that compiles its unchanged source files with MSVC or a Unix compiler.
- Initial player build compiled successfully but failed to link:
  `xm_create_context_safe`, `xm_free_context`, `xm_set_max_loop_count` undefined.
  Cause: `context.c` was omitted from the source list. Added it; link succeeded.
- miniaudio currently emits one MSVC shadowing warning from third-party code.
  No player compilation errors remained after the link correction.

### 6. Scene replay implementation — successful

- Implemented CP1252-to-UTF-8 conversion and local resolution of original drive
  paths, case-insensitive ASCII filenames and accented asset names.
- Parse object/null/light/camera motions, pivot points, parenting, dissolve,
  ambient and zoom envelopes, scene dimensions, background images and surfaces.
- Ported the original TCB/Hermite interpolation, linear keys, repeated envelopes,
  offsets, endpoint clamping and the original consecutive-key camera-cut behavior.
- Ported nX's Y-down/Z-forward object transforms, hierarchy and camera inverse.
- Added original scene sequencing, seeking, pause, screenshot keys and headless
  (hidden-window) deterministic PNG capture with a CSV frame manifest.
- `--verify-assets` successfully loaded all eleven scenes. Triangle counts by
  timeline order: 739, 2854, 2, 2998, 7331, 4, 2806, 4135, 1753, 1869, 2.
- The runtime uses actual scene geometry and animation, not reference video frames.

### 7. Music replay and synchronization — successful, measured limits

- Decode `Mush.xm` to 48 kHz stereo float PCM with libxm at startup.
  Seeking selects the corresponding PCM position; normal playback uses the
  audio sample cursor for the visual clock. A monotonic clock is the fallback
  if no audio device is available or the user requests mute.
- Added `--audio-wav` for a reproducible 210-second PCM export.
- Exported `analysis/mush-libxm.wav` successfully.
- Compared 10 ms RMS envelopes against the reference audio in 10-second windows.
  Best matching XM time minus video-audio time: 0.00 s near 15 s, +0.02 s near
  60 s, +0.04 s near 120 s and +0.05 s near 180 s. Correlations respectively
  approximately 0.914, 0.865, 0.799 and 0.920.
- These are envelope-alignment measurements, not proof of sample-identical BASS
  replay or a subjective listening review.

### 8. First capture pass — functioning, visible rendering errors

- Captured 22 exact timestamps at 640x480 on the NVIDIA GeForce RTX 4060.
  Evidence: `analysis/first-capture/frames.csv` and `sheet.jpg`.
- Success: correct scene order, recognizable scenes, articulated characters,
  camera motion, credits, logos and ending.
- Failure: huge textured rays in tunnel/mushroom scenes. The `[SPARK_0]` emitter
  carrier mesh was being drawn as regular geometry. Replaced it with particles.
- Failure: lighting was too dark. nX's normals use the opposite lighting-vector
  convention to conventional outward normals. Matched `ModelLight`'s
  vertex-minus-light and positive distant-light direction.
- Failure: Freestyle title was enlarged. Its 480x350 image/viewport must remain
  centered within the 640x480 canvas. Corrected canvas and viewport handling.

### 9. Second capture pass — substantially closer, remaining mapping errors

- Generated `analysis/second-capture/pairs.jpg`, reference on the left and
  reconstruction on the right, for all 22 samples.
- Interior scenes, characters, logos and greetings were already close.
- Failure: tree cutouts appeared as black rectangles. They use **TTEX**
  transparency maps, not the `[CHROMA]` keyword. Added alpha-only materials.
- Failure: skies and some ground textures differed despite matching geometry.
  Re-reading original `TSIZ` handling revealed that nX negates texture-size Y
  as well as point Y. With our vertically flipped OpenGL image upload,
  planar-Y projection needs the opposite V sign. Corrected it.
- Added deterministic particle trails based on the original 150-particle,
  delayed-emission, 60-frame-lifetime scheme, and light flare sprites.
  This intentionally remains an approximation of the original frame-dependent
  RNG/super-flare behavior; particle placement is not claimed pixel-exact.

### 10. Video timing analysis — successful; initial hypothesis corrected

- Decoded reference frames at 64x48 and measured successive grayscale differences.
- Initially suspected a constant start offset because video duration is shorter
  than 210 s. The measured cuts instead show a small progressive discrepancy.
- Selected observable cuts (demo time -> reference video time, seconds):
  37 -> 36.933333; 45 -> 44.850000; 96 -> 95.716667;
  103 -> 102.616667; 131 -> 130.583333; 147 -> 146.533333;
  200 -> 199.350000.
- Least-squares comparison mapping:
  `reference_video_seconds = 0.996595135365128 * demo_seconds + 0.02394646540968619`.
  Largest fit residual among these observations: approximately 57 ms.
- This affine mapping is for **comparison only**. Playback retains the original
  binary's absolute timing. Raw cut differences are reported too; calibration
  must not conceal the approximately 650 ms end-of-demo video discrepancy.
- Reference audio alignment does not have the same drift as the visual cuts.
  The exact cause in the original capture has not been established.

### 11. Third capture pass — first playable visual target reached

- Evidence: `analysis/third-capture/frames.csv` and `pairs.jpg`.
- Fixing the planar-Y mapping brought the sky, manor exterior and ground textures
  much closer, in addition to the already close interiors and logos.
- Example downscaled RGB absolute errors (0..255 scale, including black bars):
  hill at 50 s: 2.06; manor at 120 s: 2.22; stairs at 136 s: 3.57;
  trap at 153 s: 2.34; greetings at 183 s: 1.99.
- Particle-heavy images still differ substantially (example 93 s: 28.86).
  These numbers are diagnostic, not a percentage fidelity score.

### 12. Dependency archival — initial operation rejected, reversible alternative succeeded

- Attempted to remove only the newly cloned vendor repositories' nested `.git`
  directories after validating their locations. Automatic command approval
  rejected that deletion with `blocked by policy`; the rejected command did not run.
- Used a reversible alternative: recorded exact versions in `vendor/SOURCES.json`,
  exported each commit using `git archive`, moved the complete original clones
  into ignored `analysis/vendor-checkouts/`, then expanded plain source archives
  back into `vendor/`. All source is now directly vendored; there are no gitlinks
  and no deleted clone histories.

## 2026-09-08 10:59 — Journal requested; ongoing validation

- Created this English Markdown journal at the user's request, including the
  retrospective record above. Future changes and validation outcomes are appended.
- Started a complete hidden-window playback with audio, at 640x480, to check all
  transitions and automatic termination. It is still running at this entry.
- Next: reproducible comparison/report tooling, regression checks, source and
  build documentation, and a self-contained Windows package.
- No blocking issue has been encountered. Native macOS/Linux execution has not
  been tested on this Windows host; WSL reports no installed distribution.

## 2026-09-08 11:18 — Validation completed and release preparation

- The full hidden-window playback with real audio completed successfully and
  exited at the end of the 210-second timeline. Command:
  `bin/freestyle.exe --hidden --size 640x480 --duration 212`.
  Observed scene starts were 0.04, 24.00, 37.01, 45.00, 73.00, 96.00,
  103.00, 131.00, 147.00, 167.03 and 200.00 seconds. There was no audio-device
  fallback warning. This validates uninterrupted execution, not subjective
  listening quality or every intermediate frame.
- Added reproducible `tools/validate_demo.py`, calibration observations in
  `documentation/reference-timing.json`, and Markdown/JSON/image results under
  `documentation/validation/`. The tool remuxes the supplied streams, renders
  22 exact timestamps, extracts calibrated reference frames, records SHA-256
  hashes and compares RGB errors. Audio comparison is optional via `--audio`.
- Final comparison run succeeded with the rebuilt delivery executable:
  `python tools/validate_demo.py --output documentation/validation --audio`.
  RGB MAE ranges from 0.73 to 28.98 on a 0..255 scale; the largest sampled error
  is the particle-heavy shot at 93 seconds. Most sampled shots are below 8.
  Metrics include letterboxing and must not be interpreted as fidelity percentages.
- The visual clock fit is `video = 0.996595135 * demo + 0.023946465`.
  The maximum raw cut difference is 0.650 s, versus 0.057 s after fitting.
  The executable still uses the original binary's timeline without this fit.
- Audio-envelope comparisons at 15, 60, 120 and 180 seconds find XM lags of
  0.00, +0.02, +0.04 and +0.05 seconds, with correlations of approximately
  0.914, 0.865, 0.799 and 0.920. Audio and picture drift in the supplied capture
  are not identical. These are measured windows, not a bit-exact audio claim.
- Requested the same 22 capture timestamps in reverse order. All 22 PNG hashes
  match their forward-order counterparts. Evidence remains in the ignored
  `analysis/reversed-capture/` directory.
- Added `tools/recover_timeline.py` and ran its fresh UPX emulation path against
  the original executable, without using the earlier memory dump. Successfully
  verified 14 immediate timing constants and 11 scene-load strings. The tool
  stops before original Windows application execution; it does not launch the
  original executable. Exported addresses and hashes are preserved in
  `documentation/timeline-evidence.json`.
- Added meaningful motion/timeline/asset regression checks. Final
  `ctest --test-dir build -C Release --output-on-failure`: all three tests pass
  (`freestyle_motion`, `freestyle_assets`, `klx_regression`).
- Wrote `documentation/PLAYER.md` with Windows/macOS/Linux build instructions,
  controls, captures, validation, packaging, provenance and limitations. Added
  a three-platform GitHub Actions build definition; it has not been run remotely.
  Native macOS/Linux execution remains unverified on this host.
- Added explicit GPL source notices and the stb license to the Windows install.
  The resulting Windows Release rebuild succeeded. Its only remaining compiler
  diagnostic is a shadowed local variable in the vendored miniaudio header.
- Initial CMake installation succeeded. Tested that installed executable from
  `C:/Windows/Temp`, without `--assets`, using
  `--hidden --mute --time 136 --duration 2`: exit code 0 and the correct house
  scene. This checks asset discovery independently of the repository directory.
  Refreshing the install and making final ZIP archives is the remaining step.
- No blocking issue arose. The milestone's remaining fidelity limitations are
  documented, principally approximate particle trajectories and super-flares,
  with smaller lighting, environment-map and rasterization differences.

## 2026-09-08 11:19 — First playable delivery packaged

- Refreshed the Windows installation after the final rebuild and guide changes.
  Added `tools/package_release.py`, using only Python's standard library, to
  archive the installed player and the complete source/asset set. It checks
  for a stale installed executable, verifies both ZIPs with CRC reads, and
  writes `dist/SHA256SUMS.txt`.
- Packaging succeeded: `dist/freestyle-windows-x64.zip` contains 170 files
  (about 2.1 MB); `dist/freestyle-source.zip` contains 664 files (about 11.4 MB).
  Source includes the original release, extractor regression inputs, vendored
  dependencies and documentation. Generated renders, audio exports, reference
  video files, build directories and unpacking experiments are excluded.
- Independently checked ZIP contents: the packaged executable exactly matches
  `bin/freestyle.exe`, and the packaged XM exactly matches the user's supplied
  `Mush.xm`. Required source files, licenses and regression assets are present;
  there are no nested `.git` directories or Python bytecode in the source ZIP.
- The executable SHA-256 is
  `08da019993564eea7255357ed5bb38bf6cfdaea626ab71ed316b1c7abac3d550`.
  Confirmed that the final comparison JSON records this exact executable hash.
- `git diff --check` passed. The source ZIP is refreshed after this entry so
  the delivered journal includes the packaging results. The first playable
  milestone is complete; no unresolved build or playback blocker remains on
  the tested Windows host. The visual limitations and untested native platforms
  listed above remain explicit follow-up work.

## 2026-09-08 17:21 — JavaScript/WebGL port started

- User requested a JavaScript/WebGL version with parity to the native player;
  Three.js is permitted but optional. Interpreted "ISO" as audiovisual and
  functional parity with this native reconstruction, whose remaining differences
  from the original release are already documented above.
- Inspected the current native scene, mesh, renderer and audio implementations.
  The previous delivery is now committed. Left the user's untracked `.DS_Store`
  and `documentation/RESTORATION_LOG.pdf` untouched.
- Chose direct WebGL 2 with small shaders to reproduce the existing fixed-function
  OpenGL behavior. The asset preparation step will reuse the C++ loader, exporting
  geometry, UVs, normals and animation keys; JavaScript will evaluate motions and
  render actual geometry. No reference-video playback is involved.
- Plan audio parity through a lossless export of the same libxm-decoded XM used
  by the native player, scheduled against the Web Audio clock. A browser launch
  button is required to unlock audio. Seek, pause and exact-time captures remain
  part of the port.
- Consulted Khronos WebGL 2 documentation (NPOT repeat textures, shader API) and
  MDN's AudioBufferSourceNode start/offset documentation. WebGL 2 avoids WebGL 1's
  restrictions on repeating non-power-of-two textures.
- No Node/npm, Emscripten or Playwright installation was found on PATH. Installed
  Playwright 1.62.0 and its dependencies into ignored `analysis/web-python-deps/`
  for browser validation. Pip succeeded but printed unrelated conflicts already
  present among globally installed lmdeploy/outlines/ollama-python dependencies;
  this target-directory installation did not modify those packages.
- Started downloading Playwright's Chromium into ignored `analysis/web-browsers/`.
  No blocker at this stage.

## 2026-09-08 17:35 — Browser renderer working; first parity measurements

- Added the CMake target freestyle_export_web. Two initial tool-script attempts
  failed before execution: first JavaScript string quoting, then accidental
  interpolation of a CMake variable in a JavaScript template string. Neither
  modified files. Corrected tool quoting and built successfully without warnings.
- Exported 11 scenes, 61 meshes and 69 textures. Scene JSON is 2,515,500 bytes;
  the native motion oracle is 1,099,839 bytes. Textures use the same stb decoder
  and lossless PNG, avoiding browser JPEG decoder differences.
- Implemented JavaScript TCB interpolation, object/camera matrices, parent
  hierarchies, timeline holds, and the native particle PRNG/aging formula.
  Implemented WebGL 2 vertex lighting, texture modes, reflection approximation,
  transparent triangle sorting, additive sprites and deterministic PNG capture.
- Exported Mush.xm through the unchanged native libxm decoder to a 210-second,
  48 kHz stereo PCM16 WAV. This is the native capture interface's WAV; the native
  live mixer uses float PCM. PCM16 quantization is explicit, not a claim of
  bit-identical float output.
- Added static HTML controls for audio unlock, play/pause, seek, restart, mute,
  fullscreen and PNG capture. Web Audio supplies the clock. Runtime uses
  JavaScript and shaders, without Three.js, npm, WASM or a native executable.
- Chromium installation succeeded. Launched a loopback-only test server and
  tested in headless Chromium. No JavaScript or WebGL errors. Chromium reports
  GPU-stall warnings for capture readback, which necessarily synchronizes the GPU.
- Compared six first-pass WebGL images with native captures: RGB MAE / 255 was
  0.08009 at 5 s, 0.03088 at 50 s, 0.10480 at 93 s, 0.02847 at 120 s,
  0.05459 at 153 s and 0.03161 at 183 s. Visually inspected the contact sheet.
  Evidence: ignored analysis/web-first/.
- Compared 292 fractional timeline samples against 125,808 native matrix
  components. Maximum component error was 0.00012008 (about one float ULP at
  the worst large translation); maximum frame error was 4.55e-13. First pass
  succeeded without renderer fixes.
- Added tools/build_web.py to prepare a static site and optional ZIP with
  licenses and hashes. One combined patch was rejected because the journal
  context had different indentation; no files changed. Corrected and reapplied.
- Next: all reference timestamps, sound sample identity, controls and full playback.

## 2026-09-08 17:46 — Full browser validation and interaction correction

- Added tools/validate_web.py. It starts/stops an isolated loopback HTTP server,
  renders fresh native captures, checks browser captures and reversed capture
  order, compares all native motion samples, hashes the complete decoded audio,
  and exercises actual browser controls. Optional video comparison reuses the
  measured reference clock calibration. Optional full playback records frame
  timing and scene changes.
- Chromium 151.0.7922.34 passed all initial checks. Enforced user-gesture audio
  policy: its initial AudioContext was suspended, and clicking Play unlocked it.
  All 20,160,000 PCM16 values match the WAV export; PCM SHA-256:
  ebee873c3c27b8141319263facc4e69b1bccc6849475104757e17dd170d1979c.
- All 22 native/browser image pairs passed. Maximum RGB MAE was 0.116913 / 255.
  Also directly compared all 22 browser images against the remuxed video using
  the existing affine fit. Raw video cut drift is retained in the report.
- Full Chromium audio-clock playback completed: all eleven scenes, exactly
  210 seconds on the playback clock, 210.485 wall seconds including end polling.
  Maximum observed cut delay was 8 ms. Mean render CPU time was 1.72 ms;
  mean frame interval 17.30 ms, p95 24.30 ms. These are measurements on this host.
- Installed Firefox 153.0 into the ignored local browser directory. It passed
  image, motion, audio, deterministic-order and control checks. Its maximum
  sampled native/browser RGB MAE was 0.023801 / 255.
- Checked fullscreen entry/exit and a 390x844 responsive viewport. No JavaScript
  errors or horizontal overflow. Captured the interface for inspection.
- That interface inspection exposed a race: a pending asynchronous audio resume
  could complete after an immediate pause/capture and start playback again.
  Added request generations and explicit playback intent to AudioClock so pause
  cancels pending resumes and rapid seeks retain only the latest request.
  Added regression checks for both interactions. Also synchronized the mute
  button label/ARIA state when the optional mute query is used.
- Added the freestyle_web CMake target, packaging guide, README entry, and an
  optional Linux/Chromium CI validation job. The CI workflow has not run remotely.
- Built the freestyle_web target successfully. It produced a self-contained
  site and CRC-verified ZIP (approximately 37 MB; PCM audio is the largest file).
  The browser source is included without bundling/minification.
- Re-running browser validation, including full Chromium playback, after the
  interaction fix so delivery provenance refers to the final JavaScript.
  No blocker has been encountered.

## 2026-09-08 17:53 — Final WebGL delivery verified

- Re-ran the complete Chromium suite after the audio interaction correction.
  All nine control checks passed, including pending-play cancellation, rapid
  consecutive seeks and fullscreen. All 22 image pairs, reversed captures,
  exact scene boundaries, 292 motion samples and all PCM values passed again.
- Final full playback completed at exactly 210 seconds on the audio clock.
  Observed scene starts: 0, 24, 37, 45, 73, 96, 103, 131, 147, 167.010667,
  200.010667 seconds. Maximum observed cut delay: 10.667 ms.
  Mean render time: 1.762 ms; mean frame interval: 17.368 ms; p95: 24.400 ms.
  End polling completed after 210.469 wall seconds.
- Re-ran the final Firefox suite, including the new interaction and fullscreen
  checks. All passed. Maximum sampled RGB MAE remains 0.116913 / 255 in Chromium
  and 0.023801 / 255 in Firefox. No JavaScript/WebGL errors were recorded.
- Refreshed screenshots after the correction and explicitly verified that
  capturing immediately after Play stays paused at the requested timestamp.
  Responsive screenshots and contact sheets are in documentation/web-validation/.
- Re-ran the native CTest suite after CMake integration: all three tests pass.
  Python preparation/validation/packaging scripts compile; git diff --check passes.
- Final CMake preparation and ZIP packaging succeeded. The web ZIP is about
  37 MB and includes the complete readable JavaScript runtime, generated assets,
  soundtrack, guide, provenance manifest and licenses. WAV/geometry generation
  is reproducible from the native source and original extracted assets.
- Checked every runtime and generated-asset hash inside the ZIP. Confirmed both
  browser reports contain the exact final site's provenance manifest. The live
  preview at http://127.0.0.1:8765 serves the same runtime source bytes.
- Source packaging now includes the WebGL sources and preparation tools, while
  excluding generated web assets and intermediate browser/native PNG captures.
  Refreshing the full source ZIP incorporates this final journal entry.
- Native Safari/macOS and mobile hardware are untested. Pixel identity across
  all GPUs is not asserted. The port retains the native reconstruction's known
  original-release fidelity limits and explicit float-to-PCM16 quantization.
  No blocking issue remains for the delivered Windows browser build.

## 2026-09-08 18:02 — Direct XM playback requested

- User requested playing the XM in the browser using a library, avoiding the
  monolithic WAV. Investigated libxm.js, jslibxm and pure JavaScript XM players.
  The libxm project provides an Emscripten browser-port precedent.
- Chose to reuse the already vendored libxm v0.2 core, compiled to WebAssembly
  and controlled from JavaScript. This minimizes changes in tracker effect
  interpretation compared with the native player. The module is 1,086,577 bytes,
  versus 40,320,044 bytes for the prior PCM16 WAV.
- No WebAssembly compiler was found on PATH or in the Visual Studio installation.
  Started installing a workspace-local Emscripten toolchain under ignored
  analysis/emsdk. The SDK will not be part of the runtime or source archive.
- Planned streaming audio production with bounded buffers, an audio-clock
  timeline, pause/seek/restart support, and numerical comparison against the
  native decoder before replacing the packaged WAV.
- Inspected the clean committed baseline; the user's untracked journal PDF
  remains untouched.

## 2026-09-08 18:22 — XM streaming implemented and first tests passed

- Installed Emscripten 4.0.14 in analysis/emsdk and activated its local config.
  The SDK also supplied local Node/Python tools. No permanent/system PATH
  registration was used. The SDK download was approximately 629 MB, separate
  from the small decoder shipped to users.
- Added src/xm_web.c and tools/build_xm_wasm.py. The unchanged native libxm
  sources/options compile to a standalone WASM module with no imports.
  The first 8 MiB build was 25,540 bytes; reduced fixed memory to 4 MiB after
  measuring a 1,348,189-byte libxm context. Final WASM size: 25,538 bytes.
- Decoded all 210 seconds under the SDK's Node runtime, converted to PCM16 with
  the native export's quantization, and compared all 20,160,000 values. Exact
  identity: zero differing samples, RMS error 0, SHA-256
  ebee873c3c27b8141319263facc4e69b1bccc6849475104757e17dd170d1979c.
  Full decoding plus PCM conversion/file output took about 1.865 seconds.
- Added a dedicated XM worker, a bounded AudioWorklet queue, and a decoder that
  stores at most three complete state snapshots. Normal audio state is bounded
  to 4 MiB live WASM plus at most 12 MiB snapshots and a small PCM queue, rather
  than a complete 210-second PCM buffer. Full-memory snapshots preserve both
  context data and the library's file-static PRNG.
- Replaced the Web Audio WAV source with the XM stream, retaining the audio
  clock and cancellation semantics for pause, seek and rapid requests.
  Production hosting now requires HTTPS for AudioWorklet; localhost HTTP works.
- Two editing/tool attempts failed before changing files: a patch tried to
  delete and add the same path, which apply_patch rejects; then a helper read
  had one extra Python parenthesis. Corrected the helper and applied an in-place
  update. No source was lost.
- First Chromium smoke test passed startup, playback, pause and a cold seek to
  120 seconds (about 1.25 seconds preparation). Queue peak was 24,448 frames;
  no underruns or discontinuities. The browser also reproduced the full native
  PCM16 hash in offline QA. Recorded zero WAV network requests.
- Preparation now copies the original XM and checked-in WASM, verifies decoder
  provenance and no longer invokes native WAV export. Preserved the old generated
  staging WAV under analysis/obsolete-web-audio instead of deleting it.
  The first new site ZIP was 6,215,672 bytes, versus about 37 MB previously.
- Updated browser validation to generate its native WAV oracle only in ignored
  QA output, assert no WAV requests, and check consumed frames, queue bounds,
  underruns and discontinuities during complete playback. The full Chromium
  run is in progress; image/motion/audio/control checks have already passed.

## 2026-09-08 18:30 — Direct XM delivery verified and packaged

- Complete Chromium playback passed: exactly 10,080,000 stereo frames consumed
  over the 210-second soundtrack; zero underruns, zero discontinuities, and
  at most 24,576 queued frames (0.512 seconds). All eleven scene transitions
  were observed, with maximum delay 8 ms. Mean render time was 1.830 ms;
  mean frame interval 17.373 ms, p95 25.400 ms. End polling finished after
  210.578 wall seconds. The final decoder cache held three snapshots.
- Firefox passed image parity, motion, full offline PCM identity, capture order,
  scene boundaries and all nine interaction checks with the new audio backend.
  Full uninterrupted 210-second playback was measured in Chromium.
- Added and ran tools/validate_xm_seek.py. Seven nonsequential windows at
  120.123, 5.321, 93.111, 209.9, 195.875, 29.999 and 0 seconds exactly match
  the independent native PCM16 samples: 28,672 values checked, zero mismatches.
  This includes backward checkpoint restoration and crossing a checkpoint.
  Measured seek preparation ranged from 0.3 ms to approximately 1.12 seconds
  for these particular cache states. Report: documentation/web-validation/xm-seek/.
- git diff --check initially flagged CRLF in newly regenerated tracked reports.
  Changed the report writer to explicit LF and normalized those reports without
  changing their data. The check now passes.
- Documented direct XM loading, WASM rebuilding, memory/queue bounds, HTTPS or
  localhost requirements, cold-seek latency and offline QA behavior. Source
  verification tolerates Git's equivalent LF/CRLF checkout representations.
- Built the freestyle_web CMake target successfully without native WAV export.
  Preserved the obsolete preview WAV under analysis/obsolete-web-audio as well.
  Neither the packaged site nor the preview's asset directory needs that file.
- Final web ZIP: 88 files, 6,216,375 bytes; no WAV members. Verified every runtime
  and generated-asset hash, exact original XM bytes, and agreement of both
  browser reports with the final site's provenance.
- WASM SHA-256:
  89126541ab778070d15123324c770c6981814796beb491d39d8af4516fcbae12.
  Web ZIP SHA-256:
  6cd7cd1fd633bb7d953d1fcb07f749cdae48ca9c8f72482b943cef0a83bec25d.
- All three native CTest regressions passed; Python tools compile. Refreshing
  the complete source archive includes the C bridge, WASM build script, vendored
  library, JavaScript streaming implementation, tests, reports and this journal.
- Direct XM playback is complete on the tested Chromium/Firefox environment.
  The existing unverified Safari/mobile scope remains unchanged. No blocker
  was encountered.

## 2026-09-08 19:04 — Project conversation exported and translated

- The user requested the complete project exchange as JSONL under codex_log/,
  with a faithful English translation. Located the current session through the
  local Codex session index, then inspected only session provenance to identify
  other sessions with this exact project working directory. Found three:
  September 6 KLX extraction, September 6 repository text translation, and
  September 8 native restoration/WebGL/direct XM work.
- Read the source session files without modifying them. Froze the export at
  the user's export request, 2026-09-08 16:48:22.516 UTC (18:48 local), including
  that request but excluding this export operation and its delivery response.
  The source prefixes contain 172, 63 and 772 records respectively.
- Produced codex_log/project.original.jsonl and project.en.jsonl with one output
  record per source record: 1,007 records in each. Preserved 777 original records
  byte for byte, including all 147 tool calls and 147 results. Replaced 224
  internal instruction/reasoning/context records with explicit omission markers;
  reduced six session metadata records to provenance fields. This is a complete
  export of the recovered public exchange and tool traces, not an unfiltered
  backup of Codex's private runtime state. Every affected source line is listed
  in manifest.json.
- Translated all 52 dialogue messages (10 user, 42 assistant), including their
  duplicated UI events and task-completion copies. Preserved the historical
  sequence, tentative claims, failures, corrections and former WAV delivery.
  Commands, patches, tool output, paths, links, identifiers and timestamps remain
  unchanged. No external translation service was used.
- Added original and English Markdown reading copies, the English translation
  mapping, an English scope/provenance README and a standard-library-only
  export/verification script. Source-prefix hashes and delivered-file hashes
  are recorded in the manifest. The English Markdown is a transcript, not a
  retrospective summary.
- The first attempt to write the translation mapping failed before any file
  change: Markdown backticks terminated a JavaScript template literal. Encoded
  those characters as JSON Unicode escapes and applied the file successfully.
- The first exporter run stopped on app-supplied context split into multiple
  content blocks. Moved context recognition before the single-dialogue-block
  check; retained that context in the JSONL and excluded it only from the
  human-dialogue count. The corrected export completed successfully.
- Validation passed for all seven manifested files: SHA-256/size checks, JSONL
  parsing, record ordering/timestamps, all 52 translation entries, literal code
  and link preservation, and equality of all 294 tool records between languages.
  Independently compared every unfiltered original record with the source bytes.
  Both Markdown transcripts contain 52 messages and no Unicode replacement
  characters. Confirmed that private instruction/reasoning fields are absent
  from the exported files.
- Regenerated the frozen snapshot against the still-growing current session:
  all delivered hashes and the manifest were byte-identical. git diff --check
  passed after the journal update.
- This task changes archival documentation only. The demo and its existing
  packages were not rebuilt. No blocker was encountered.
