"""Prepare a static WebGL site using the native scene loader and XM decoder."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ['index.html', 'style.css', 'app.js', 'engine.js', 'renderer.js', 'audio.js']


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'dist/freestyle-web')
    parser.add_argument('--bin-dir', type=Path, default=ROOT/'bin')
    parser.add_argument('--package', action='store_true')
    args = parser.parse_args()
    suffix = '.exe' if sys.platform == 'win32' else ''
    exporter = args.bin_dir.resolve()/('freestyle_export_web'+suffix)
    player = args.bin_dir.resolve()/('freestyle'+suffix)
    if not exporter.is_file() or not player.is_file():
        raise SystemExit('Build the CMake project first; see documentation/WEB_PLAYER.md.')
    output = args.output.resolve()
    assets = output/'assets'
    assets.mkdir(parents=True, exist_ok=True)
    raw = ROOT/'demo-assets/cds-freestyle'
    subprocess.run([exporter, raw, assets], check=True)
    subprocess.run([player, '--assets', raw, '--audio-wav', assets/'mush.wav'], check=True)
    for name in RUNTIME:
        target = output/name
        if target != ROOT/'web'/name:
            shutil.copy2(ROOT/'web'/name, target)
    licenses = output/'licenses'
    licenses.mkdir(exist_ok=True)
    for source, name in [('src/LICENSE', 'GPL-3.0.txt'), ('vendor/libxm/COPYING', 'libxm.txt'),
                         ('vendor/stb-LICENSE.txt', 'stb.txt')]:
        shutil.copy2(ROOT/source, licenses/name)
    guide = ROOT/'documentation/WEB_PLAYER.md'
    if guide.exists():
        shutil.copy2(guide, output/'README.md')
    inputs = {p.relative_to(raw).as_posix(): sha(p) for p in sorted(raw.rglob('*')) if p.is_file()}
    generated = {p.relative_to(output).as_posix(): sha(p) for p in sorted(assets.rglob('*')) if p.is_file()}
    manifest = dict(format_version=1, duration_seconds=210, sample_rate=48000,
                    preparation='Native LWSC/LWOB loader; stb lossless PNG export; libxm v0.2 PCM16 WAV',
                    source_assets=inputs, exporter_sha256=sha(exporter), native_executable_sha256=sha(player),
                    runtime={name: sha(output/name) for name in RUNTIME}, generated=generated,
                    pcm_sha256=hashlib.sha256((assets/'mush.wav').read_bytes()[44:]).hexdigest())
    (output/'provenance.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    print('Static site:', output)
    if args.package:
        package = ROOT/'dist/freestyle-web.zip'
        with zipfile.ZipFile(package, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            for path in sorted(output.rglob('*')):
                if path.is_file():
                    archive.write(path, 'freestyle-web/'+path.relative_to(output).as_posix())
        with zipfile.ZipFile(package) as archive:
            if archive.testzip():
                raise RuntimeError('Package CRC validation failed')
        (ROOT/'dist/freestyle-web.sha256').write_text(sha(package)+'  freestyle-web.zip\n', encoding='utf-8')
        print('Verified package:', package, package.stat().st_size, 'bytes')


if __name__ == '__main__':
    main()
