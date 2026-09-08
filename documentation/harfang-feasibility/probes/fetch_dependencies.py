"""Fetch pinned Windows study dependencies; never modify the Freestyle assets."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[3]
WORK = ROOT / "analysis/harfang-feasibility"
COMMIT = "2bd99d2ad654f8579b21bfbdcc6ab61f2628ea7a"
REPO = "https://github.com/astrofra/harfang3d"
WORK.mkdir(parents=True, exist_ok=True)
(WORK / "logs").mkdir(exist_ok=True)


def download(url, destination):
    request = urllib.request.Request(url, headers={"User-Agent": "Freestyle-feasibility-study"})
    with urllib.request.urlopen(request, timeout=90) as response:
        data = response.read()
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data)


manifest = json.loads((Path(__file__).parent.parent / "download-manifest.json").read_text())
for entry in manifest:
    name = entry["asset"]
    archive = WORK / "downloads" / name
    if not archive.exists():
        download(REPO + "/releases/download/v3.3.0/" + name, archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != entry["sha256"]:
        raise RuntimeError("Release hash mismatch: " + name)
    target = (WORK / "release" / Path(name).stem).resolve()
    with zipfile.ZipFile(archive) as bundle:
        for member in bundle.infolist():
            if not (target / member.filename).resolve().is_relative_to(target):
                raise RuntimeError("Unsafe archive member: " + member.filename)
        bundle.extractall(target)
    print("Verified/extracted", name, flush=True)

tree_path = WORK / "source-tree.json"
if not tree_path.exists():
    download("https://api.github.com/repos/astrofra/harfang3d/git/trees/" + COMMIT + "?recursive=1", tree_path)
tree = json.loads(tree_path.read_text())
assert tree["sha"] == COMMIT and not tree.get("truncated"), "Unexpected source tree"
required = [entry for entry in tree["tree"] if entry["type"] == "blob" and (
    entry["path"].startswith("tutorials/resources/core/") or
    (entry["path"].startswith("harfang/foundation/") and entry["path"].endswith(".h")) or
    entry["path"] in {"harfang/engine/animation.h", "extern/json/json.hpp", "extern/json/json_fwd.hpp"}
)]


def fetch_source(entry):
    path = WORK / "source" / entry["path"]
    if not path.exists():
        download("https://raw.githubusercontent.com/astrofra/harfang3d/" + COMMIT + "/" + entry["path"], path)
    data = path.read_bytes()
    git_hash = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
    if git_hash != entry["sha"]:
        raise RuntimeError("Pinned source hash mismatch: " + entry["path"])


with ThreadPoolExecutor(max_workers=8) as pool:
    list(pool.map(fetch_source, required))
(WORK / "source-commit.txt").write_text(COMMIT + "\n", encoding="ascii")
print("Verified", len(required), "pinned source/core files")
