# Freestyle — JavaScript / WebGL preservation

The browser version renders the same eleven animated scenes as the native
reconstruction, on the same 210-second timeline, with the same deterministic
particles. Rendering and animation run in JavaScript and WebGL 2. The reference
video is used only for validation.

## Run the prepared site

Extract freestyle-web.zip and serve its freestyle-web directory over HTTP.
For example, from that directory:

    python -m http.server 8000 --bind 127.0.0.1

Open http://127.0.0.1:8000 and click **Play demo** after loading. Modern browsers
require a user gesture to enable sound. Opening index.html as a local file
does not work because browsers restrict fetch requests from file URLs.

From the repository, after preparation:

    python -m http.server 8000 --bind 127.0.0.1 --directory dist/freestyle-web

The directory is also directly deployable to a static HTTP/HTTPS host, including
a subdirectory. It contains all JavaScript, artwork, soundtrack and licenses;
there are no CDN requests, npm packages, WebAssembly modules or server APIs.
Preserve the directory structure and serve JavaScript with an appropriate
JavaScript MIME type. Gzip/Brotli on the host is optional.

## Controls

- Play/Pause or Space: start/pause/resume.
- Timeline slider or Left/Right: seek; arrow keys move by five seconds.
- Restart button or Home: return to the beginning.
- Mute: silence audio while keeping the audio clock running.
- Fullscreen button or F: fullscreen; Escape exits fullscreen.
- PNG button or C: download the current rendered frame.

The last image remains visible at the end, and Play restarts the demo.
The canvas maintains the native 640x480 composition and per-scene letterboxing,
including in fullscreen. Graphics-context loss stops playback and displays a
reload message.

## Prepare from source

Build the CMake project first. Dependencies are already vendored:

    cmake -S . -B build
    cmake --build build --config Release --parallel
    python tools/build_web.py --package

Alternatively, after configuring CMake, one target builds and packages everything:

    cmake --build build --config Release --target freestyle_web --parallel

On macOS/Linux add -DCMAKE_BUILD_TYPE=Release when configuring CMake.
Native development prerequisites are listed in PLAYER.md. Python 3.10+ is used
for preparation/packaging; the browser runtime does not need it.

Outputs:

- dist/freestyle-web/: standalone static site.
- dist/freestyle-web.zip: site archive with readable JavaScript source.
- dist/freestyle-web.sha256: archive checksum.
- dist/freestyle-web/provenance.json: source asset, executable and output hashes.

The CMake target freestyle_export_web reuses the existing C++ LWSC/LWOB loader.
It exports scene keys, geometry, normals, UVs and materials as JSON, plus
losslessly decoded textures. Motion evaluation and geometry rendering happen
in the browser; frames are not baked to images.

The music preparation invokes the native player's --audio-wav export on the
supplied Mush.xm, using vendored libxm v0.2. The browser loads a lossless PCM16
WAV (48 kHz, stereo, 210 seconds; about 40 MB) into a Web Audio buffer. The
native live mixer uses float PCM, so the export has explicit 16-bit quantization.
The browser does not implement a separate XM tracker decoder. This choice keeps
the musical interpretation identical to the already validated native export.

No video-reference files or original executable are needed to run the site.
The source assets remain unchanged. The original release is needed only for
optional binary-archeology tools.

## Deterministic captures and diagnostics

Use the capture query to fix framebuffer size independently of display DPI:

    http://127.0.0.1:8000/?capture=1&width=640&height=480&time=93

The optional mute query silences output. Playback still starts with a click.
After window.freestyle.ready, the browser exposes a small validation interface:

    await freestyle.captureAt(93)
    // { png: "data:image/png;base64,...", time: 93,
    //   frame: 901, scene: "4_SceneChampi.lws", index: 4, drawCalls: ... }
    await freestyle.seek(147)
    await freestyle.play()
    freestyle.pause()
    freestyle.status()

captureAt pauses and renders at the exact requested timestamp in [0,210).
It is independent of previous captures and playback frame rate.
validateMotion compares JavaScript transforms with native C++ oracle samples.
validateAudio hashes every decoded PCM16 sample against the native WAV export.

## Reproduce validation

Optional QA dependencies:

    python -m pip install playwright pillow numpy
    python -m playwright install chromium firefox
    python tools/validate_web.py --video --playback
    python tools/validate_web.py --browser firefox --output captures/web-firefox

The tool starts its own loopback HTTP server and closes it afterward. It checks:

- 22 native/WebGL frame pairs, with an RGB MAE threshold of 0.5 / 255.
- Reverse capture order and exact scene boundary selection.
- 292 fractional timeline samples against native matrices.
- SHA-256 identity of all 20,160,000 PCM16 sample values.
- Gesture start, pause, seek, restart, mute, fullscreen and PNG download,
  including cancellation of pending audio starts and rapid consecutive seeks.
- With --playback, all 210 seconds, transitions and render timing.
- With --video, direct comparison against the remuxed video using the existing
  documented clock fit. FFmpeg is needed only for this optional video check.

Results are Markdown, JSON, PNGs and contact sheets. The checked-in milestone
reports are under documentation/web-validation/. The video cut drift remains
the same as native: up to 0.650 s raw, about 0.057 s residual after the
comparison-only fit. Playback always keeps the binary's original timing.

## Parity and limits

Validated on Windows with Chromium 151.0.7922.34 and Firefox 153.0. Across the
22 sampled images, maximum native/WebGL RGB MAE is 0.1169 / 255 in Chromium
and 0.0238 / 255 in Firefox. The final Chromium build completed uninterrupted
playback with a maximum observed scene-cut delay of 10.7 ms. All PCM16 audio
sample values match the native WAV export. This is measured parity, not a
promise of universal pixel identity.

The parity target is this repository's native reconstruction. Its known
differences from the original release remain, principally particle behavior
and flares, plus smaller lighting and rasterization differences.

JavaScript reproduces the native float interpolation conventions, and shaders
reproduce vertex lighting and blending. Pixel identity across all browsers
and GPUs is not promised: interpolation, filtering and rasterization can differ
slightly between OpenGL and ANGLE/WebGL implementations. See the measured report.

WebGL 2 with hardware acceleration is required. Browser/GPU versions actually
tested are recorded in the reports. Native Safari/macOS and mobile-device
execution have not been verified on this Windows machine.

The player and exporter are GPL-3.0, following nxNG. Assets retain their
creators' rights. Package licenses include libxm and stb notices.

API references used for this port:
[Khronos WebGL 2 specification](https://registry.khronos.org/webgl/specs/latest/2.0/)
and [Web Audio scheduling](https://developer.mozilla.org/en-US/docs/Web/API/AudioBufferSourceNode/start).
