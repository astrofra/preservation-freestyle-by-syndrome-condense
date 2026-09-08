"""Rebuild the small libxm WASM decoder; ordinary web builds use the checked-in binary."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ['src/xm_web.c', 'vendor/libxm/src/xm.c', 'vendor/libxm/src/context.c',
           'vendor/libxm/src/load.c', 'vendor/libxm/src/play.c']
DEFINES = ['XM_DEBUG=0', 'XM_DEFENSIVE=1', 'XM_BIG_ENDIAN=0',
           'XM_LINEAR_INTERPOLATION=1', 'XM_RAMPING=1', 'XM_STRINGS=1']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emsdk', type=Path)
    args = parser.parse_args()
    env = os.environ.copy()
    if args.emsdk:
        sdk = args.emsdk.resolve()
        env['EM_CONFIG'] = str(sdk/'.emscripten')
        compiler = [sys.executable, str(sdk/'upstream/emscripten/emcc.py')]
    else:
        emcc = shutil.which('emcc')
        if not emcc:
            raise SystemExit('Activate Emscripten 4.0.14 or pass --emsdk PATH.')
        compiler = [emcc]
    output = ROOT/'web/libxm.wasm'
    command = compiler + ['-O2', '-std=c11', '-ffp-contract=off', '-fno-fast-math',
        '-I'+str(ROOT/'vendor/libxm/include'), '-I'+str(ROOT/'vendor/libxm/src')]
    command += ['-D'+d for d in DEFINES] + [str(ROOT/p) for p in SOURCES]
    command += ['--no-entry', '-sSTANDALONE_WASM=1', '-sFILESYSTEM=0', '-sMALLOC=emmalloc',
        '-sINITIAL_MEMORY=4194304', '-sSTACK_SIZE=65536', '-sALLOW_MEMORY_GROWTH=0',
        '-sEXPORTED_FUNCTIONS=["_fs_input","_fs_buffer","_fs_load","_fs_render","_fs_context_size"]',
        '-o', str(output)]
    version = subprocess.check_output(compiler+['--version'], env=env, text=True).splitlines()[0]
    subprocess.run(command, check=True, env=env)
    source_paths = SOURCES + ['vendor/libxm/include/xm.h', 'vendor/libxm/src/xm_internal.h',
                              'tools/build_xm_wasm.py']
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    manifest = dict(compiler=version, libxm_commit='e36ab122a169d451e5f219e1c03724a409a877ba',
                    source_sha256={p: sha(ROOT/p) for p in source_paths}, definitions=DEFINES,
                    flags=command[len(compiler):], wasm_sha256=sha(output), wasm_bytes=output.stat().st_size)
    # Keep compiler paths out of durable provenance; the script supplies exact arguments.
    manifest.pop('flags')
    manifest['options'] = ['-O2', '-std=c11', '-ffp-contract=off', '-fno-fast-math',
                           'standalone', 'fixed 4 MiB memory', '64 KiB stack', 'emmalloc']
    (ROOT/'web/libxm-build.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    print('WASM decoder:', output.stat().st_size, 'bytes;', version)


if __name__ == '__main__':
    main()
