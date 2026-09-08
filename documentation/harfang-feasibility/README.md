# HARFANG feasibility evidence and reproduction

Read [the study](../HARFANG_FEASIBILITY.md) first. These probes test bounded hypotheses; they are not a port of the complete demo.

## Retained evidence

- `assimp-results.json`: exact commands, exit codes and error excerpts for original LWS, repaired-path LWS and original LWOB imports.
- `animation-comparison.json`: inventory from the native loader and measured camera-channel errors using HARFANG's real evaluator.
- `runtime-results.json`: final render-only Lua/Squirrel results; both child processes exited 0.
- `runtime-audio-results.json`: final audio-enabled results; both child processes exited 3221226505 (0xC0000409).
- `runtime-results-initial.json`: earlier unsuccessful runs, retained as history. The initial Squirrel output was truncated before structured results.
- `lua-aaa.png` / `squirrel-aaa.png`: final 640×480 captures of the populated geometry fixture.
- `evidence-summary.json`: target/build IDs, raw asset format inventory, capture comparison and hashes of the final measurements.
- `download-manifest.json`: downloaded release archive sizes and SHA-256 hashes.

Raw logs, generated glTF/native assets, tool binaries, downloaded source and build products remain in the ignored `analysis/harfang-feasibility/` directory. No executable or downloaded engine distribution is included in this documentation folder.

## Reproduce on Windows

Run the following from the repository root. Prerequisites are Python 3.10+, CMake, Visual Studio 2022 C++ tools, working Direct3D 11, the extracted Freestyle assets and **the existing `web/assets/demo.json` export**. That JSON was present but untracked when the study started; a checkout without it cannot run the head fixture. The independent C++ inventory/animation probe uses the original assets directly. A production converter should also use the native reader directly.

```powershell
python documentation/harfang-feasibility/probes/fetch_dependencies.py

cmake -S documentation/harfang-feasibility/probes -B analysis/harfang-feasibility/build-probes -G "Visual Studio 17 2022" -A x64
cmake --build analysis/harfang-feasibility/build-probes --config Release --parallel 4
.\analysis\harfang-feasibility\build-probes\Release\animation_probe.exe demo-assets/cds-freestyle documentation/harfang-feasibility/animation-comparison.json

python documentation/harfang-feasibility/probes/prepare_scene.py
$freestyleAssetc = (Resolve-Path analysis/harfang-feasibility/release/hg-assetc-win64/assetc/assetc.exe).Path
$freestyleSourceAssets = (Resolve-Path analysis/harfang-feasibility/assets).Path
$freestyleCompiledAssets = Join-Path (Get-Location).Path "analysis/harfang-feasibility/assets_compiled"
Push-Location (Split-Path $freestyleAssetc)
try {
    & $freestyleAssetc $freestyleSourceAssets $freestyleCompiledAssets -api DX11 -job 4
    if ($LASTEXITCODE -ne 0) { throw "HARFANG asset compilation failed" }
} finally {
    Pop-Location
}
python documentation/harfang-feasibility/probes/run_runtime_probes.py
```

The fetcher verifies six release archives and 311 pinned source/core files. It was exercised successfully against the already downloaded cache; an entirely clean network/bootstrap/build run was not performed.

The runtime driver starts hidden windows, loads only compiled assets, records transforms, renders through Scene/AAA, and saves PNGs after 64 frames. It replaces the result JSON/captures on each run. The driver collects failures as evidence: **its own exit code is not an assertion that the child runtimes passed**. Inspect each JSON `exit_code` and its records.

Optional reproduction of the known audio problem:

```powershell
python documentation/harfang-feasibility/probes/run_runtime_probes.py --audio
```

This opens the actual XM at zero gain, waits briefly, tries seeks to 0/5/120 seconds and shuts down. Abnormal child-process exit is the recorded finding on this release. It is not a successful audio qualification run. Re-run the render-only probe afterward if retaining a clean final graphics run.

The fixture preserves the selected mesh's triangle positions and material partitioning with a provisional basis conversion, but omits original texture/UV behavior and uses double-sided simplified PBR materials. Its synthetic animation tests translation interpolation and hierarchy only. It does not validate Freestyle's camera, lighting, orientation, normals or complete animation conversion.

The CMake target compiles the project's loader and HARFANG's header-defined animation evaluator; it does not link/build the full HARFANG engine. `HARFANG_SOURCE` can be supplied as a CMake cache path to another checkout of the exact pinned source.
