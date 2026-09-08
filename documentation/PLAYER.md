# FreeStyle — first playable reconstruction

Replays the original eleven LightWave scenes, with animated cameras, characters,
textures, credits and greetings, accompanied by the supplied `Mush.xm`.
The scene order and 210-second timeline were recovered from the original executable.

## Run

From the repository, launch **`bin/freestyle.exe`**. In the Windows package,
launch **`freestyle.exe`** beside its `assets/` directory.

```powershell
.\bin\freestyle.exe
.\bin\freestyle.exe --fullscreen
.\bin\freestyle.exe --time 131
```

Escape quits; Space pauses; Left/Right seek five seconds; Home restarts;
F12 saves a PNG in `captures/`. The default window is 960x720. The original
640x480 canvas and scene-specific letterboxing are preserved when resizing.
The demo exits automatically at 210 seconds. `--help` lists all options.

All assets are loaded before playback. Music is decoded once to about 77 MiB of
PCM for immediate seeking. OpenGL 2.1 and an audio output device are used;
`--mute` permits silent playback. If audio initialization fails, a warning is
printed and playback uses a monotonic clock.

## Build

CMake 3.21+, a C11/C++17 compiler and platform OpenGL development libraries are
required. Dependencies are vendored as source; no downloads, Python, FFmpeg,
legacy DirectX SDK or original executable are required to build/run the player.

Windows, Visual Studio 2022:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

macOS, with Xcode Command Line Tools and CMake:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./bin/freestyle
```

Linux / Debian or Ubuntu, using GLFW's X11 backend:

```sh
sudo apt-get install build-essential cmake xorg-dev libgl1-mesa-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./bin/freestyle
```

Native Windows x64 build and playback are verified. macOS/Linux use portable
code paths and have CI build definitions, but have **not** been executed on this
Windows host. OpenGL on macOS uses the available compatibility context.
Linux rendering needs a display, including with `--hidden`; Xvfb can provide one.
GLFW's optional Wayland backend can be enabled with `-DGLFW_BUILD_WAYLAND=ON`.

The extractor remains available as `bin/klx_unpack`. To build only the extractor,
configure with `-DFREESTYLE_BUILD_PLAYER=OFF`.

## Capture and comparison

Capture selected exact demo timestamps. This draws the reconstructed scenes;
the reference video is never used by the runtime.

```powershell
.\bin\freestyle.exe --hidden --mute --size 640x480 `
  --capture-times 5,27,50,78,99,120,136,153,183,204 --capture-dir captures\check
```

`frames.csv` records each PNG's demo time, scene name and local animation frame.
Captures do not depend on the order in which timestamps are requested.
For a fixed-rate sequence and matching audio export:

```powershell
.\bin\freestyle.exe --hidden --mute --size 640x480 `
  --time 0 --duration 210 --capture-fps 60 --capture-dir captures\movie
.\bin\freestyle.exe --audio-wav captures\mush.wav
```

`--audio-wav` exports 48 kHz stereo PCM from the XM and exits unless image capture
was also requested. FFmpeg can encode the generated PNG sequence if needed.
Sequential capture filenames include both frame number and millisecond time.

Run the comparison tool (optional Python 3.11+, Pillow, NumPy and FFmpeg):

```powershell
python -m pip install pillow numpy
python tools\validate_demo.py --audio
```

It remuxes the two reference WebMs if necessary, captures 22 representative
images, and writes a side-by-side contact sheet, Markdown/JSON reports, file
hashes, timing differences and audio-envelope correlations under
`captures/validation/`. On Unix pass `--exe bin/freestyle`.
The checked-in first-pass results are in `documentation/validation/`.

The reference video cuts progressively precede the executable's specified cuts:
about 0.07 s at the title and 0.65 s at the final logo. Comparison images use a
documented affine fit, with a maximum residual of about 57 ms, while reports also
retain the raw differences. This fit does not alter playback speed. Audio aligns
within approximately 50 ms in the measured windows. The original capture's
audio/video clock discrepancy has not been explained.

## Source organization and preservation evidence

- `src/scene.cpp`: scene parser, original motion conventions, matrices and timeline.
- `src/lwob.cpp`: adapted upstream portable mesh reader, materials, normals and UVs.
- `src/renderer.cpp`: OpenGL rasterization, transparencies, particles and PNG output.
- `src/audio.cpp`: XM decoding, audio clock, seeking and WAV output.
- `src/main.cpp`: application loop, controls and capture interface.
- `vendor/nxng/`: complete upstream source snapshot, including the legacy renderer.
- `vendor/SOURCES.json`: exact dependency versions and provenance.
- `tools/recover_timeline.py`: optional offline unpacking/disassembly of the
  original executable, with addresses and hashes for the recovered time constants.
- `documentation/RESTORATION_LOG.md`: chronological attempts, failures and results.

The player and its adaptations are GPL-3.0, following nxNG. Original artwork and
music retain their creators' rights. The KLX extractor remains under the root
Unlicense. GLFW, libxm, miniaudio and stb retain their own notices/licenses.

## Current limitations

This is a first functional preservation build, not a bit-exact reimplementation.
Camera motion, scene timing, major geometry, planar textures, transparent cutouts,
logos and credits are reconstructed. Particle trajectories and super-flares use
a deterministic approximation; those shots have the largest visual differences.
Environment mapping, lighting precision, polygon triangulation, clipping and
texture filtering can also differ from the original Direct3D renderer.

MOA visibility caches are preserved but not used: the original animated geometry
is rendered directly. This is fast on the tested machine and avoids dependence
on the original precomputation. Only the features exercised by Freestyle's
assets are restored; this is not a general-purpose replacement for all of nxNG.

## Package

```powershell
cmake --install build --config Release --prefix dist/freestyle-windows-x64
python tools/package_release.py
```

The install directory contains the executable, assets, this guide and licenses.
Keep its `assets/` directory beside the executable. Source is provided in the
repository and in the separate source archive when packaging this milestone.
The packaging script produces `dist/freestyle-windows-x64.zip`,
`dist/freestyle-source.zip` and `dist/SHA256SUMS.txt`. The source archive includes
vendored dependencies, assets, the original release and regression fixtures;
reference videos and intermediate analysis remain in the working repository.
