"""C extractor regression tests. Only Python's standard library is required."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
EXE = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else ROOT / 'bin/klx_unpack.exe'
ARCHIVE = ROOT / 'demo-unpack/cds-freestyle/Freestyle/FreeStyle.klx'
MANIFEST = json.loads((ROOT / 'documentation/freestyle-manifest.json').read_text(encoding='utf-8'))
FIXTURES = json.loads((ROOT / 'tests/fixtures.json').read_text(encoding='utf-8'))['cases']


class ExtractorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = (ROOT / 'build').resolve()
        build.mkdir(exist_ok=True)
        cls.work = Path(tempfile.mkdtemp(prefix='klx-tests-', dir=build)).resolve()
        assert cls.work.is_relative_to(build)

    @classmethod
    def tearDownClass(cls):
        assert cls.work.is_relative_to((ROOT / 'build').resolve())
        shutil.rmtree(cls.work)

    def run_tool(self, *args, success=True):
        result = subprocess.run([str(EXE), *map(str, args)], capture_output=True,
                                encoding='utf-8', errors='replace', timeout=20)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        return result

    def test_original_archive_and_all_payload_hashes(self):
        source_hash = hashlib.sha256(ARCHIVE.read_bytes()).hexdigest()
        self.assertEqual(source_hash, MANIFEST['archive_sha256'])
        destination = self.work / 'complete'
        self.run_tool(ARCHIVE, destination)
        files = {p.relative_to(destination).as_posix() for p in destination.rglob('*') if p.is_file()}
        self.assertEqual(files, {e['path'] for e in MANIFEST['entries']})
        self.assertEqual(len(files), 161)
        for entry in MANIFEST['entries']:
            data = (destination / entry['path']).read_bytes()
            self.assertEqual(len(data), entry['size'], entry['path'])
            self.assertEqual(hashlib.sha256(data).hexdigest(), entry['sha256'], entry['path'])
            if entry['path'].lower().endswith('.lwo'):
                self.assertEqual(data[:4], b'FORM')
                self.assertEqual(data[8:12], b'LWOB')
                self.assertEqual(struct.unpack_from('>I', data, 4)[0] + 8, len(data))
                position = 12
                while position < len(data):
                    self.assertLessEqual(position + 8, len(data))
                    size = struct.unpack_from('>I', data, position + 4)[0]
                    position += 8 + size + (size & 1)
                self.assertEqual(position, len(data))
        # A repeated run must leave every existing file untouched.
        self.run_tool(ARCHIVE, destination, success=False)
        for entry in MANIFEST['entries']:
            self.assertEqual(hashlib.sha256((destination / entry['path']).read_bytes()).hexdigest(), entry['sha256'])
        self.assertEqual(hashlib.sha256(ARCHIVE.read_bytes()).hexdigest(), source_hash)

    def test_listing(self):
        result = self.run_tool('--list', ARCHIVE)
        self.assertEqual(len(result.stdout.splitlines()), 162)
        self.assertIn('D/FreeStyle/BGM/Mush.mo3', result.stdout)
        self.assertEqual(result.stdout.count('xor-9a'), 2)
        self.assertEqual(result.stdout.count('lzari'), 159)

    def test_unicode_paths_and_xor(self):
        parent = self.work / 'préservation_日本'
        parent.mkdir()
        archive = parent / 'données.klx'
        archive.write_bytes(bytes.fromhex(FIXTURES['valid_unicode']))
        destination = parent / 'résultat'
        self.run_tool(archive, destination)
        for path in ['D/FreeStyle/Entrée.jpg', 'D/Devellop/nX_Pics/foo.bin']:
            self.assertEqual((destination / path).read_bytes(), b'test\0\xff')

    def test_empty_payload(self):
        archive = self.work / 'empty.klx'
        archive.write_bytes(bytes.fromhex(FIXTURES['valid_empty']))
        destination = self.work / 'empty'
        self.run_tool(archive, destination)
        self.assertEqual((destination / 'empty').read_bytes(), b'')

    def test_malformed_archives_and_unsafe_paths(self):
        data = ARCHIVE.read_bytes()
        cases = {name: bytes.fromhex(value) for name, value in FIXTURES.items() if not name.startswith('valid_')}
        cases.update({
            'empty_file': b'', 'short_header': b'FXLK',
            'wrong_magic': b'CRAN' + data[4:], 'truncated_archive': data[:-1],
            'trailing_data': data + b'x',
            'index_too_large': data[:4] + struct.pack('<I', 0xffffffff) + data[8:],
            'stored_index_too_large': data[:8] + struct.pack('<I', 0xffffffff) + data[12:],
        })
        for name, contents in cases.items():
            with self.subTest(name=name):
                archive = self.work / (name + '.klx')
                archive.write_bytes(contents)
                destination = self.work / (name + '-output')
                self.run_tool(archive, destination, success=False)
                self.assertFalse(destination.exists(), 'invalid inputs must fail before writing output')

    def test_existing_destination_file(self):
        target = self.work / 'keep.txt'
        target.write_bytes(b'keep')
        self.run_tool(ARCHIVE, target, success=False)
        self.assertEqual(target.read_bytes(), b'keep')

    def test_usage_and_missing_source(self):
        self.run_tool('--help')
        self.run_tool(success=False)
        self.run_tool(self.work / 'missing.klx', self.work / 'unused', success=False)


if __name__ == '__main__':
    unittest.main(verbosity=2)
