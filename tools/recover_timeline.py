"""Recover Freestyle's timing evidence without launching the original application.

Developer-only dependencies: pefile, unicorn, capstone. The UPX stub is emulated
only up to restored code/relative calls, before Windows imports or WinMain run.
Use --memory with an existing mapped dump to require only Capstone.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BASE = 0x400000
# Independently observed CMP instructions in the original application loop.
CUTS = [(0x42744C, 24000), (0x427662, 37000), (0x427875, 39000),
        (0x428056, 45000), (0x42814F, 73000), (0x42837D, 96000),
        (0x42864B, 97500), (0x428C3E, 103000), (0x428C7D, 131000),
        (0x428CF8, 147000), (0x428D73, 166000), (0x428F2D, 167000),
        (0x428F69, 200000), (0x428FD0, 210000)]


def original_bytes():
    path = ROOT/'demo-unpack/cds-freestyle/Freestyle/freestyle.exe'
    if path.exists():
        return path.read_bytes()
    with zipfile.ZipFile(ROOT/'demo-releases/cds-freestyle.zip') as archive:
        names = [n for n in archive.namelist() if n.lower().endswith('/freestyle.exe')]
        if len(names) != 1:
            raise RuntimeError('Could not uniquely locate freestyle.exe in release ZIP')
        return archive.read(names[0])


def unpack(original):
    import pefile
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP
    image = pefile.PE(data=original).get_memory_mapped_image()
    emulator = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    emulator.mem_map(BASE, 0xB0000)
    emulator.mem_write(BASE, image)
    emulator.mem_map(0x1000000, 0x100000)
    emulator.reg_write(UC_X86_REG_ESP, 0x10FF000)
    emulator.emu_start(0x4A9D00, 0x4A9E0A, timeout=20_000_000, count=20_000_000)
    if emulator.reg_read(UC_X86_REG_EIP) != 0x4A9E0A:
        raise RuntimeError('UPX emulation did not reach the known safe stop address')
    return bytes(emulator.mem_read(BASE, 0xB0000))


def main():
    import capstone
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--memory', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT/'analysis/timeline-recovery')
    args = parser.parse_args()
    original = original_bytes()
    memory = args.memory.read_bytes() if args.memory else unpack(original)
    args.output.mkdir(parents=True, exist_ok=True)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    evidence = []
    for address, milliseconds in CUTS:
        expected = b'\x3d'+struct.pack('<I', milliseconds)  # CMP EAX, imm32
        actual = memory[address-BASE:address-BASE+5]
        if actual != expected:
            raise RuntimeError(f'Timing assertion failed at {address:#x}: {actual.hex()}')
        instruction = next(md.disasm(actual, address))
        evidence.append(dict(address=f'0x{address:08x}', milliseconds=milliseconds,
                             bytes=actual.hex(), instruction=f'{instruction.mnemonic} {instruction.op_str}'))
    md.skipdata = True
    assembly = []
    scenes = []
    for instruction in md.disasm(memory[0x26900:0x29500], 0x426900):
        line = f'{instruction.address:08x}: {instruction.mnemonic} {instruction.op_str}'
        if instruction.mnemonic == 'push' and instruction.op_str.startswith('0x44'):
            ptr = int(instruction.op_str, 16)-BASE
            value = memory[ptr:ptr+200].split(b'\0')[0].decode('cp1252', errors='replace')
            if value.lower().startswith('d:/freestyle/'):
                line += ' ; '+value
                if value.lower().endswith('.lws'):
                    scenes.append(dict(address=f'0x{instruction.address:08x}', scene=value))
        assembly.append(line)
    report = dict(original_exe_sha256=hashlib.sha256(original).hexdigest(),
                  mapped_image_sha256=hashlib.sha256(memory).hexdigest(),
                  input='supplied process image' if args.memory else 'emulated original UPX stub',
                  image_base=f'0x{BASE:x}', scene_loads=scenes, timing_comparisons=evidence)
    (args.output/'evidence.json').write_text(json.dumps(report, indent=2)+'\n')
    (args.output/'sequence.asm').write_text('\n'.join(assembly)+'\n', encoding='utf-8')
    print(f'Verified {len(evidence)} timing constants and recovered {len(scenes)} scene loads: {args.output}')


if __name__ == '__main__':
    main()
