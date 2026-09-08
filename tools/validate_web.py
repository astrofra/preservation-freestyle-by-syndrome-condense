"""Compare browser frames/audio/motion with native output; optionally play all 210 s."""
import argparse
import base64
import csv
import functools
import hashlib
import http.server
import io
import json
from pathlib import Path
import subprocess
import threading
import time
import wave

import numpy as np
from PIL import Image, ImageDraw
from playwright.sync_api import sync_playwright
from validate_demo import DEFAULT_TIMES, ROOT, sha256, video_frame, reference_file


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site', type=Path, default=ROOT/'dist/freestyle-web')
    parser.add_argument('--exe', type=Path, default=ROOT/'bin/freestyle.exe')
    parser.add_argument('--output', type=Path, default=ROOT/'captures/web-validation')
    parser.add_argument('--browser', choices=['chromium', 'firefox', 'webkit'], default='chromium')
    parser.add_argument('--video', action='store_true')
    parser.add_argument('--playback', action='store_true')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    native_dir, web_dir = output/'native', output/'web'
    web_dir.mkdir(exist_ok=True)
    raw = subprocess.run([args.exe.resolve(), '--assets', ROOT/'demo-assets/cds-freestyle',
                          '--hidden', '--mute', '--size', '640x480',
                          '--capture-times', ','.join(map(str, DEFAULT_TIMES)),
                          '--capture-dir', native_dir], check=True, capture_output=True).stdout
    (output/'native.log').write_bytes(raw)
    rows = list(csv.DictReader((native_dir/'frames.csv').open()))
    report = dict(browser=args.browser, native_executable_sha256=sha256(args.exe.resolve()),
                  site_provenance=json.loads((args.site/'provenance.json').read_text()),
                  frames=[], errors=[], warnings=[])
    calibration = json.loads((ROOT/'documentation/reference-timing.json').read_text())
    x = [o['demo_seconds'] for o in calibration['observations']]
    y = [o['video_seconds'] for o in calibration['observations']]
    video_scale, video_offset = np.polyfit(x, y, 1)
    report['video_timing'] = dict(scale=float(video_scale), offset=float(video_offset),
                                observations=calibration['observations'], warning=calibration['warning'])
    reference = reference_file(ROOT/'video-reference/freestyle-reference.mkv') if args.video else None
    if reference:
        report['video_sha256'] = sha256(reference)
    handler = functools.partial(QuietHandler, directory=str(args.site.resolve()))
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    url = f'http://127.0.0.1:{server.server_port}/?capture=1&width=640&height=480'
    try:
        with sync_playwright() as p:
            options = dict(headless=True)
            if args.browser == 'chromium':
                options['args'] = ['--autoplay-policy=document-user-activation-required']
            browser = getattr(p, args.browser).launch(**options)
            report['browser_version'] = browser.version
            page = browser.new_page(viewport=dict(width=1000, height=850), device_scale_factor=1)
            page.on('pageerror', lambda e: report['errors'].append(str(e)))
            page.on('console', lambda m: report['warnings'].append(m.text) if m.type == 'warning' else None)
            page.goto(url)
            page.wait_for_function('window.freestyle?.ready || window.freestyleError', timeout=60000)
            failure = page.evaluate('window.freestyleError')
            assert not failure, failure
            report['initial_status'] = page.evaluate('window.freestyle.status()')
            report['motion'] = page.evaluate('window.freestyle.validateMotion()')
            assert report['motion']['maxError'] < .001, report['motion']
            assert report['motion']['frameError'] < 1e-7, report['motion']
            report['audio'] = page.evaluate('window.freestyle.validateAudio()')
            with wave.open(str(args.site/'assets/mush.wav'), 'rb') as w:
                expected_pcm_hash = hashlib.sha256(w.readframes(w.getnframes())).hexdigest()
            assert report['audio']['pcmSha256'] == expected_pcm_hash, report['audio']
            assert report['audio']['frames'] == 210*48000
            print('Motion and all PCM audio samples match native oracle.', flush=True)
            sheet = Image.new('RGB', (1280, 264*((len(rows)+1)//2)))
            video_sheet = Image.new('RGB', sheet.size) if reference else None
            hashes = {}
            for i, row in enumerate(rows):
                t = float(row['demo_seconds'])
                frame = page.evaluate('t=>window.freestyle.captureAt(t)', t)
                data = base64.b64decode(frame.pop('png').split(',')[1])
                (web_dir/row['file']).write_bytes(data)
                hashes[t] = hashlib.sha256(data).hexdigest()
                native = Image.open(native_dir/row['file']).convert('RGB')
                web = Image.open(io.BytesIO(data)).convert('RGB')
                delta = np.abs(np.asarray(native).astype(float)-np.asarray(web).astype(float))
                result = dict(frame, png_sha256=hashes[t], native_rgb_mae_255=float(delta.mean()),
                              native_rgb_rmse_255=float(np.sqrt((delta**2).mean())))
                assert frame['scene'] == row['scene']
                assert abs(frame['frame']-float(row['scene_frame'])) < 1e-5
                assert result['native_rgb_mae_255'] < .5, result
                px, py = (i % 2)*640, (i//2)*264
                sheet.paste(native.resize((320, 240)), (px, py))
                sheet.paste(web.resize((320, 240)), (px+320, py))
                ImageDraw.Draw(sheet).text((px+4, py+242),
                    f'{t:g}s  Native | WebGL  MAE {delta.mean():.4f}/255', fill='white')
                if reference:
                    vt = float(video_scale*t+video_offset)
                    ref = video_frame(reference, vt)
                    result['video_seconds'] = vt
                    result['video_rgb_mae_255'] = float(np.abs(np.asarray(ref).astype(float)-np.asarray(web).astype(float)).mean())
                    video_sheet.paste(ref.resize((320, 240)), (px, py))
                    video_sheet.paste(web.resize((320, 240)), (px+320, py))
                    ImageDraw.Draw(video_sheet).text((px+4, py+242),
                        f'{t:g}s  Video | WebGL  MAE {result["video_rgb_mae_255"]:.2f}/255', fill='white')
                report['frames'].append(result)
            sheet.save(output/'comparison.jpg', quality=92)
            if video_sheet:
                video_sheet.save(output/'video-comparison.jpg', quality=92)
            for t in reversed(DEFAULT_TIMES):
                frame = page.evaluate('t=>window.freestyle.captureAt(t)', t)
                assert hashlib.sha256(base64.b64decode(frame['png'].split(',')[1])).hexdigest() == hashes[t]
            report['capture_order_independent'] = True
            # Exact cuts, including the held loading-screen scenes.
            scene_data = json.loads((args.site/'assets/demo.json').read_text())['scenes']
            for i, scene in enumerate(scene_data):
                for t, expected in [(scene['start'], i), (scene['end']-.0001, i)]:
                    actual = page.evaluate('t=>window.freestyle.captureAt(t)', t)
                    assert actual['index'] == expected
            report['scene_boundaries_passed'] = True
            awaitable = 'window.freestyle.seek(0)'
            page.evaluate(awaitable)
            page.locator('#start').click()
            page.wait_for_timeout(450)
            assert page.evaluate('window.freestyle.status().playing')
            assert page.evaluate('window.freestyle.status().time') > .1
            page.keyboard.press('Space')
            paused = page.evaluate('window.freestyle.status()')
            page.wait_for_timeout(180)
            assert page.evaluate('window.freestyle.status().time') == paused['time']
            page.keyboard.press('ArrowRight')
            assert abs(page.evaluate('window.freestyle.status().time')-paused['time']-5) < .001
            page.keyboard.press('Home')
            assert page.evaluate('window.freestyle.status().time') == 0
            pending = page.evaluate('''async()=>{
                const pending=freestyle.play();freestyle.pause();await pending;
                return freestyle.status();
            }''')
            assert not pending['playing'], 'Pending audio resume ignored pause'
            rapid = page.evaluate('''async()=>{
                await freestyle.play();
                await Promise.all([freestyle.seek(15),freestyle.seek(120)]);
                const state=freestyle.status();freestyle.pause();return state;
            }''')
            assert rapid['playing'] and 120 <= rapid['time'] < 121, rapid
            page.evaluate('window.freestyle.seek(0)')
            page.locator('#mute').click()
            assert page.evaluate('window.freestyle.status().muted')
            page.locator('#mute').click()
            with page.expect_download() as download:
                page.locator('#capture').click()
            assert download.value.suggested_filename.endswith('.png')
            page.locator('#fullscreen').click()
            page.wait_for_function('!!document.fullscreenElement')
            page.evaluate('document.exitFullscreen()')
            report['controls_passed'] = ['gesture_play','pause','seek','restart','mute','png_download',
                                        'pending_play_cancel','rapid_seek','fullscreen']
            print('22 images, reverse captures, cuts and controls passed.', flush=True)
            if args.playback:
                page.evaluate('window.freestyle.resetMetrics()')
                page.locator('#play').click()
                started = time.monotonic()
                while page.evaluate('window.freestyle.status().playing'):
                    page.wait_for_timeout(1000)
                    elapsed = time.monotonic()-started
                    if round(elapsed) % 30 == 0:
                        print('Playback:', page.evaluate('window.freestyle.status().time'), flush=True)
                    if elapsed > 225:
                        raise AssertionError('Playback failed to end')
                report['playback'] = page.evaluate('window.freestyle.metrics()')
                report['playback']['wall_seconds'] = time.monotonic()-started
                transitions = report['playback']['transitions']
                assert [t['index'] for t in transitions] == list(range(11)), transitions
                errors = [t['time']-scene_data[t['index']]['start'] for t in transitions]
                report['playback']['maximum_cut_delay_seconds'] = max(errors)
                assert max(errors) < .25, errors
                assert page.evaluate('window.freestyle.status().time') == 210
            assert not report['errors'], report['errors']
            browser.close()
    finally:
        server.shutdown()
        server.server_close()
    (output/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    maximum = max(f['native_rgb_mae_255'] for f in report['frames'])
    lines = ['# WebGL parity validation', '',
             f'Browser: {args.browser} {report["browser_version"]}. Captures: 640x480, no antialiasing.', '',
             '![Native left, WebGL right](comparison.jpg)', '',
             f'Maximum sampled native/WebGL RGB MAE: **{maximum:.4f} / 255**. Acceptance threshold: 0.5.', '',
             f'Native motion oracle: {report["motion"]["samples"]} times, {report["motion"]["values"]} matrix components; '
             f'maximum error {report["motion"]["maxError"]:.8f}.', '',
             'All 20,160,000 decoded PCM16 sample values match the native WAV export by SHA-256. '
             'The native live mixer uses float PCM; the WAV introduces 16-bit quantization.', '',
             'Reverse-order captures, scene boundaries, gesture play, pause, seek, restart, mute, PNG download, '
             'fullscreen, pending-play cancellation and rapid seeks passed.', '',
             '| Demo time | Native/WebGL MAE | Video/WebGL MAE |', '|---:|---:|---:|']
    for f in report['frames']:
        video = f'{f["video_rgb_mae_255"]:.2f}' if 'video_rgb_mae_255' in f else 'not measured'
        lines.append(f'| {f["time"]:g} | {f["native_rgb_mae_255"]:.4f} | {video} |')
    if reference:
        lines += ['', '![Video left, WebGL right](video-comparison.jpg)', '',
                  'Video comparison uses the same documented affine clock fit as native validation. '
                  'Maximum raw visual cut discrepancy remains 0.650 s; the fit does not change playback.',
                  '', calibration['warning']]
    if 'playback' in report:
        p = report['playback']
        lines += ['', f'Full audio-clock playback completed all 11 scenes in {p["wall_seconds"]:.2f} wall seconds. '
                  f'Maximum observed cut delay: {p["maximum_cut_delay_seconds"]:.4f} s. '
                  f'Mean frame interval: {p["frameIntervalMs"]["mean"]:.2f} ms; p95: {p["frameIntervalMs"]["p95"]:.2f} ms.']
    lines += ['', 'These measurements establish parity with the native reconstruction on the tested browser/GPU, '
              'not exact reproduction of the original 1999/2000 renderer or universal GPU pixel identity. '
              'See report.json for versions, input/output hashes and warnings.']
    (output/'REPORT.md').write_text('\n'.join(lines)+'\n', encoding='utf-8')
    print(output/'REPORT.md', flush=True)


if __name__ == '__main__':
    main()
