"""Package an already-built Windows milestone, with a separate complete source ZIP.

Python standard library only. Run CMake build/install first as documented in PLAYER.md.
Reference videos, build products and intermediate analysis are excluded from sources.
"""
from pathlib import Path
import hashlib
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / 'dist'
INSTALL = DIST / 'freestyle-windows-x64'
SOURCE_ITEMS = [
    '.gitignore', '.github', 'CMakeLists.txt', 'README.md', 'LICENSE',
    'klx_unpack.c', 'src', 'vendor', 'tests', 'tools', 'documentation',
    'demo-assets', 'demo-releases', 'demo-unpack', 'img',
]
EXCLUDED = {
    ROOT / 'documentation/validation/rendered',
    ROOT / 'documentation/validation/mush.wav',
    ROOT / 'documentation/validation/capture.log',
}


def files_under(path):
    for file in sorted(path.rglob('*')) if path.is_dir() else [path]:
        if not file.is_file() or any(p in {'.git', '__pycache__'} for p in file.parts):
            continue
        if file.suffix == '.pyc' or any(file == p or p in file.parents for p in EXCLUDED):
            continue
        yield file


def archive(name, files, base):
    destination = DIST / f'{name}.zip'
    count = 0
    with zipfile.ZipFile(destination, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for file in files:
            z.write(file, f'{name}/{file.relative_to(base).as_posix()}')
            count += 1
    with zipfile.ZipFile(destination) as z:
        corrupt = z.testzip()
        if corrupt:
            raise RuntimeError(f'ZIP verification failed: {corrupt}')
    print(f'{destination.name}: {count} files, {destination.stat().st_size} bytes')
    return destination


def main():
    for required in ['freestyle.exe', 'assets/D/FreeStyle/BGM/Mush.xm',
                     'licenses/stb-LICENSE.txt']:
        if not (INSTALL / required).is_file():
            raise SystemExit('Run the Windows CMake build/install commands in documentation/PLAYER.md first.')
    if (INSTALL / 'freestyle.exe').read_bytes() != (ROOT / 'bin/freestyle.exe').read_bytes():
        raise SystemExit('Installed executable is stale. Run cmake --install again.')
    packages = [archive('freestyle-windows-x64', files_under(INSTALL), INSTALL)]
    source_files = (file for item in SOURCE_ITEMS for file in files_under(ROOT / item))
    packages.append(archive('freestyle-source', source_files, ROOT))
    hashes = [f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}' for p in packages]
    (DIST / 'SHA256SUMS.txt').write_text('\n'.join(hashes)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
