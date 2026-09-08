"""Run the released Lua/Squirrel runtimes against the same compiled Scene fixture."""
import json
import os
from pathlib import Path
import subprocess
import time
import sys

ROOT = Path(__file__).resolve().parents[3]
WORK = ROOT / "analysis/harfang-feasibility"
OUTPUT = ROOT / "documentation/harfang-feasibility"
env = os.environ.copy()
env["FREESTYLE_HG_COMPILED"] = str(WORK / "assets_compiled")
env["FREESTYLE_HG_OUTPUT"] = str(OUTPUT)
with_audio = "--audio" in sys.argv
env["FREESTYLE_HG_AUDIO"] = "1" if with_audio else "0"
reports = []
for name, relative, script in [
    ("lua", "hg-lua_bullet-win64/lua_bullet/hg_lua/lua.exe", "runtime_probe.lua"),
    ("squirrel", "hg-squirrel_bullet-win64/squirrel_bullet/hg_squirrel/hg_squirrel.exe", "runtime_probe.nut"),
]:
    runtime = WORK / "release" / relative
    command = [str(runtime), str(Path(__file__).parent / script)]
    start = time.perf_counter()
    result = subprocess.run(command, cwd=runtime.parent, env=env,
                            capture_output=True, timeout=45)
    log = result.stdout + result.stderr
    (WORK / "logs" / (name + ("-runtime-audio.txt" if with_audio else "-runtime.txt"))).write_bytes(log)
    records = [json.loads(line.removeprefix("PROBE "))
               for line in log.decode("utf-8", errors="replace").splitlines()
               if line.startswith("PROBE ")]
    entry = {"language": name, "exit_code": result.returncode,
             "seconds": time.perf_counter()-start, "records": records,
             "log_tail": log.decode("utf-8", errors="replace")[-1800:]}
    reports.append(entry)
    print(json.dumps(entry, ensure_ascii=True, indent=2))
(OUTPUT / ("runtime-audio-results.json" if with_audio else "runtime-results.json")).write_text(json.dumps(reports, indent=2)+"\n",
                                             encoding="utf-8", newline="\n")
