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
