"""Create a bounded HARFANG feasibility fixture from the preserved Freestyle head.

This fixture uses synthetic motion to test glTF interpolation and SceneAnim
playback. It is deliberately not a restored Freestyle scene or a parity claim.
"""
import json
from pathlib import Path
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[3]
WORK = ROOT / "analysis/harfang-feasibility"
SOURCE = WORK / "fixture_source"
SOURCE.mkdir(exist_ok=True)
assets = json.loads((ROOT / "web/assets/demo.json").read_text(encoding="utf-8"))
mesh = next(m for m in assets["meshes"] if m["name"] == "GIRL-tete.lwo")
blob = bytearray()
gltf = {"asset": {"version": "2.0", "generator": "Freestyle HARFANG feasibility probe"},
        "buffers": [], "bufferViews": [], "accessors": [], "materials": [], "meshes": [],
        "nodes": [{"name": "AnimatedParent", "children": [1]},
                  {"name": "FreestyleHead", "mesh": 0}],
        "scenes": [{"nodes": [0]}], "scene": 0}


def accessor(values, dimension, type_name, component_type=5126):
    offset = len(blob)
    for value in values:
        blob.extend(struct.pack("<" + ("f" if component_type == 5126 else "I") * dimension, *value))
    view = len(gltf["bufferViews"])
    gltf["bufferViews"].append({"buffer": 0, "byteOffset": offset, "byteLength": len(blob)-offset})
    index = len(gltf["accessors"])
    gltf["accessors"].append({
        "bufferView": view, "componentType": component_type, "count": len(values), "type": type_name,
        "min": [min(v[i] for v in values) for i in range(dimension)],
        "max": [max(v[i] for v in values) for i in range(dimension)],
    })
    return index


primitives = []
for slot, surface in enumerate(mesh["surfaces"]):
    positions, normals = [], []
    for triangle, material in enumerate(mesh["materials"]):
        if material != slot:
            continue
        for corner in [0, 2, 1]:
            start = (triangle * 3 + corner) * 8
            x, y, z, nx, ny, nz, u, v = mesh["corners"][start:start+8]
            positions.append((x, -y, z))
            normals.append((-nx, ny, -nz))
    if not positions:
        continue
    index = len(gltf["materials"])
    gltf["materials"].append({"name": "original_surface_" + str(slot),
        "pbrMetallicRoughness": {"baseColorFactor": [c/255 for c in surface["color"]]+[1],
                               "metallicFactor": 0, "roughnessFactor": 0.8},
        "doubleSided": True})
    primitives.append({"attributes": {"POSITION": accessor(positions, 3, "VEC3"),
                                      "NORMAL": accessor(normals, 3, "VEC3")},
                       "indices": accessor([(i,) for i in range(len(positions))], 1, "SCALAR", 5125),
                       "material": index, "mode": 4})
gltf["meshes"].append({"name": mesh["name"], "primitives": primitives})
times = accessor([(0,), (1,), (3,)], 1, "SCALAR")
positions = accessor([(0, 0, 0), (1, 0, 0), (0, 0, 0)], 3, "VEC3")
gltf["animations"] = [
    {"name": "linear_probe", "samplers": [{"input": times, "output": positions, "interpolation": "LINEAR"}],
     "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}]},
    {"name": "step_probe", "samplers": [{"input": times, "output": positions, "interpolation": "STEP"}],
     "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}]},
]
gltf["buffers"] = [{"uri": "fixture.bin", "byteLength": len(blob)}]
(SOURCE / "fixture.bin").write_bytes(blob)
(SOURCE / "fixture.gltf").write_text(json.dumps(gltf), encoding="utf-8")
editable = WORK / "assets"
editable.mkdir(exist_ok=True)
shutil.copytree(WORK / "source/tutorials/resources/core", editable / "core", dirs_exist_ok=True)
shutil.copyfile(ROOT / "demo-assets/cds-freestyle/D/FreeStyle/BGM/Mush.xm", editable / "Mush.xm")
exe = WORK / "release/hg-gltf_importer-win64/gltf_importer/gltf_importer.exe"
command = [str(exe), str(SOURCE / "fixture.gltf"), "-out", str(editable),
           "-base-resource-path", str(editable), "-name", "fixture",
           "-geometry-policy", "overwrite", "-scene-policy", "overwrite",
           "-shader", "core/shader/pbr.hps"]
result = subprocess.run(command, capture_output=True, timeout=45)
(WORK / "logs/gltf-fixture.txt").write_bytes(result.stdout + result.stderr)
if result.returncode:
    raise RuntimeError("glTF fixture import failed; see log")
assert (editable / "GIRL-tete.geo").stat().st_size > 10000, "Empty geometry export"
scene_file = editable / "fixture.scn"
scene = json.loads(scene_file.read_text(encoding="utf-8"))
print(json.dumps({"nodes": len(scene["nodes"]), "anims": len(scene.get("anims", [])),
                  "scene_anims": [a["name"] for a in scene.get("scene_anims", [])],
                  "original_head_triangles": len(mesh["materials"]),
                  "scene_keys": list(scene)}, indent=2))
