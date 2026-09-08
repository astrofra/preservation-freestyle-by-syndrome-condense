"""Check WASM checkpoint restoration against independent native PCM samples."""
import argparse
import array
import functools
import http.server
import json
from pathlib import Path
import subprocess
import threading
import wave
from playwright.sync_api import sync_playwright
from validate_demo import ROOT, sha256
from validate_web import QuietHandler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site', type=Path, default=ROOT/'dist/freestyle-web')
    parser.add_argument('--exe', type=Path, default=ROOT/'bin/freestyle.exe')
    parser.add_argument('--output', type=Path, default=ROOT/'captures/xm-seek')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    native = output/'native-mush.wav'
    subprocess.run([args.exe.resolve(), '--assets', ROOT/'demo-assets/cds-freestyle',
                    '--audio-wav', native], check=True, capture_output=True)
    probes = []
    with wave.open(str(native), 'rb') as wav:
        for time in [120.123, 5.321, 93.111, 209.9, 195.875, 29.999, 0]:
            frame = round(time*48000)
            wav.setpos(frame)
            values = array.array('h', wav.readframes(2048))
            probes.append(dict(time=time, frame=frame, expected=list(values)))
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0),
        functools.partial(QuietHandler, directory=str(args.site.resolve())))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with sync_playwright() as p:
            browser = p.chromium.launch(headless=True)
            page = browser.new_page()
            page.goto(f'http://127.0.0.1:{server.server_port}/')
            page.wait_for_function('window.freestyle?.ready', timeout=60000)
            results = page.evaluate('''async probes=>{
                const {XmDecoder,END}=await import('./xm-decoder.js');
                const [wasm,xm]=await Promise.all(['./libxm.wasm','./assets/Mush.xm'].map(async url=>{
                    const r=await fetch(url);return new Uint8Array(await r.arrayBuffer());
                }));
                const decoder=await XmDecoder.create(wasm,xm);
                await decoder.seek(END);
                const result=[];
                for(const probe of probes) {
                    const started=performance.now();await decoder.seek(probe.frame);
                    const seekMs=performance.now()-started;let offset=0,mismatches=0;
                    while(offset<probe.expected.length) {
                        const pcm=decoder.render((probe.expected.length-offset)/2);
                        for(const v of pcm) {
                            const quantized=Math.trunc(Math.fround(Math.max(-1,Math.min(1,v))*32767));
                            if(quantized!==probe.expected[offset++])mismatches++;
                        }
                    }
                    result.push({time:probe.time,samples:offset,mismatches,seekMs,...decoder.info()});
                }
                return result;
            }''', probes)
            report = dict(browser=browser.version, native_executable_sha256=sha256(args.exe.resolve()),
                          wasm_sha256=sha256(args.site/'libxm.wasm'), probes=results)
            browser.close()
    finally:
        server.shutdown()
        server.server_close()
    (output/'seek-report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8', newline='\n')
    assert all(p['mismatches'] == 0 for p in results), results
    assert all(p['checkpointCount'] <= 3 for p in results)
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
