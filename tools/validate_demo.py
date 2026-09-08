"""Capture the reconstruction and produce an honest visual/timing comparison.

Optional QA dependencies: FFmpeg/FFprobe, Pillow, NumPy. No runtime dependency.
Run from any directory; generated data defaults to ignored captures/validation.
"""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import subprocess

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TIMES = [5, 15, 23, 27, 34, 40, 50, 65, 78, 86, 93, 99,
                 110, 120, 136, 143, 153, 162, 172, 183, 192, 204]


def run(args):
    return subprocess.run([str(a) for a in args], check=True, capture_output=True).stdout


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def reference_file(path):
    if path.exists():
        return path
    videos = list(path.parent.glob('*_video_*.webm'))
    audios = list(path.parent.glob('*_audio_*.webm'))
    if len(videos) != 1 or len(audios) != 1:
        raise RuntimeError('Expected one reference video and one reference audio WebM')
    run(['ffmpeg', '-v', 'error', '-n', '-i', videos[0], '-i', audios[0],
         '-map', '0:v:0', '-map', '1:a:0', '-c', 'copy', path])
    return path


def video_frame(path, time):
    data = run(['ffmpeg', '-v', 'error', '-ss', f'{time:.9f}', '-i', path,
                '-frames:v', '1', '-vf', 'scale=640:480', '-f', 'image2pipe',
                '-vcodec', 'png', '-'])
    return Image.open(io.BytesIO(data)).convert('RGB')


def audio_envelope(path):
    raw = run(['ffmpeg', '-v', 'error', '-i', path, '-vn', '-ac', '1', '-ar',
               '8000', '-f', 'f32le', '-'])
    samples = np.frombuffer(raw, dtype='<f4')
    samples = samples[:len(samples)//80*80].reshape(-1, 80)
    return np.sqrt((samples*samples).mean(axis=1))


def audio_compare(reference, reconstruction):
    ref, candidate = audio_envelope(reference), audio_envelope(reconstruction)
    result = []
    for time in [15, 60, 120, 180]:
        begin = (time-5)*100
        y = ref[begin:begin+1000]
        y = (y-y.mean())/max(float(y.std()), 1e-12)
        best = (-2., 0.)
        for lag in range(-100, 101):
            x = candidate[begin+lag:begin+lag+len(y)]
            x = (x-x.mean())/max(float(x.std()), 1e-12)
            score = float((x*y).mean())
            if score > best[0]:
                best = score, lag/100
        result.append(dict(video_seconds=time, xm_minus_reference_seconds=best[1],
                           envelope_correlation=best[0]))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=ROOT/'bin/freestyle.exe')
    parser.add_argument('--reference', type=Path, default=ROOT/'video-reference/freestyle-reference.mkv')
    parser.add_argument('--output', type=Path, default=ROOT/'captures/validation')
    parser.add_argument('--times', default=','.join(map(str, DEFAULT_TIMES)))
    parser.add_argument('--audio', action='store_true')
    args = parser.parse_args()
    reference = reference_file(args.reference.resolve())
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    rendered = output/'rendered'
    command = [args.exe.resolve(), '--assets', ROOT/'demo-assets/cds-freestyle',
               '--hidden', '--mute', '--size', '640x480', '--capture-times', args.times,
               '--capture-dir', rendered]
    (output/'capture.log').write_bytes(run(command))
    calibration = json.loads((ROOT/'documentation/reference-timing.json').read_text())
    observations = calibration['observations']
    x = np.array([o['demo_seconds'] for o in observations])
    y = np.array([o['video_seconds'] for o in observations])
    scale, offset = map(float, np.polyfit(x, y, 1))
    rows = list(csv.DictReader((rendered/'frames.csv').open()))
    sheet = Image.new('RGB', (1280, 264*((len(rows)+1)//2)))
    report = dict(reference_sha256=sha256(reference), executable_sha256=sha256(args.exe.resolve()),
                  reference_video_scale=scale, reference_video_offset=offset,
                  timing_observations=observations,
                  maximum_raw_cut_error_seconds=float(np.max(np.abs(y-x))),
                  maximum_fitted_cut_residual_seconds=float(np.max(np.abs(y-(scale*x+offset)))),
                  caveat=calibration['warning'], frames=[])
    for i, row in enumerate(rows):
        t = float(row['demo_seconds'])
        reference_time = t*scale+offset
        ref = video_frame(reference, reference_time)
        got = Image.open(rendered/row['file']).convert('RGB')
        # A Retina/HiDPI framebuffer can be larger than the requested window.
        if got.size != ref.size:
            got = got.resize(ref.size, Image.Resampling.LANCZOS)
        a, b = np.asarray(ref).astype(float), np.asarray(got).astype(float)
        mae = float(np.abs(a-b).mean())
        rmse = float(np.sqrt(np.mean((a-b)**2)))
        entry = dict(row, reference_seconds=reference_time, rgb_mae_255=mae,
                     rgb_rmse_255=rmse, sha256=sha256(rendered/row['file']))
        report['frames'].append(entry)
        px, py = (i % 2)*640, (i//2)*264
        sheet.paste(ref.resize((320, 240)), (px, py))
        sheet.paste(got.resize((320, 240)), (px+320, py))
        ImageDraw.Draw(sheet).text((px+4, py+242),
            f'{t:g}s  Reference | Reconstruction  MAE {mae:.2f}/255', fill='white')
    sheet.save(output/'comparison.jpg', quality=92)
    if args.audio:
        wav = output/'mush.wav'
        run([args.exe.resolve(), '--assets', ROOT/'demo-assets/cds-freestyle', '--audio-wav', wav])
        report['audio'] = audio_compare(reference, wav)
    (output/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    lines = ['# FreeStyle comparison report', '',
             'Reference on the left; reconstructed geometry on the right. Full frames include black bars.', '',
             '![Comparison](comparison.jpg)', '',
             f'Comparison-only clock fit: `video = {scale:.9f} * demo + {offset:.9f}`.', '',
             f'Maximum raw cut discrepancy: **{report["maximum_raw_cut_error_seconds"]:.3f} s**. '
             f'Maximum residual after fitting: **{report["maximum_fitted_cut_residual_seconds"]:.3f} s**.', '',
             calibration['warning'], '',
             '| Demo cut (s) | Video cut (s) | Raw difference (s) |',
             '|---:|---:|---:|']
    for o in observations:
        lines.append(f'| {o["demo_seconds"]:.3f} | {o["video_seconds"]:.3f} | {o["video_seconds"]-o["demo_seconds"]:+.3f} |')
    lines += ['', '| Demo time (s) | Scene | RGB MAE / 255 |', '|---:|---|---:|']
    for f in report['frames']:
        lines.append(f'| {float(f["demo_seconds"]):.3f} | {f["scene"]} | {f["rgb_mae_255"]:.2f} |')
    lines += ['', 'Pixel error is a diagnostic, not a fidelity percentage. Sampling, compression, '
              'particle RNG and the reference clock affect this metric. See report.json for hashes and exact times.']
    if args.audio:
        lines += ['', '| Audio time (s) | Best XM lag (s) | Envelope correlation |', '|---:|---:|---:|']
        for a in report['audio']:
            lines.append(f'| {a["video_seconds"]} | {a["xm_minus_reference_seconds"]:+.2f} | {a["envelope_correlation"]:.3f} |')
    (output/'REPORT.md').write_text('\n'.join(lines)+'\n', encoding='utf-8')
    print(output/'REPORT.md')


if __name__ == '__main__':
    main()
