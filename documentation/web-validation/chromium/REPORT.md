# WebGL parity validation

Browser: chromium 151.0.7922.34. Captures: 640x480, no antialiasing.

![Native left, WebGL right](comparison.jpg)

Maximum sampled native/WebGL RGB MAE: **0.1169 / 255**. Acceptance threshold: 0.5.

Native motion oracle: 292 times, 125808 matrix components; maximum error 0.00012008.

All 20,160,000 decoded PCM16 sample values match the native WAV export by SHA-256. The native live mixer uses float PCM; the WAV introduces 16-bit quantization.

Reverse-order captures, scene boundaries, gesture play, pause, seek, restart, mute, PNG download, fullscreen, pending-play cancellation and rapid seeks passed.

| Demo time | Native/WebGL MAE | Video/WebGL MAE |
|---:|---:|---:|
| 5 | 0.0801 | 11.70 |
| 15 | 0.1169 | 13.27 |
| 23 | 0.0495 | 4.99 |
| 27 | 0.0758 | 7.12 |
| 34 | 0.0369 | 5.00 |
| 40 | 0.0000 | 3.54 |
| 50 | 0.0309 | 2.18 |
| 65 | 0.0296 | 2.73 |
| 78 | 0.0374 | 4.87 |
| 86 | 0.0473 | 7.80 |
| 93 | 0.1048 | 28.98 |
| 99 | 0.0000 | 2.68 |
| 110 | 0.0208 | 4.79 |
| 120 | 0.0285 | 2.26 |
| 136 | 0.0484 | 3.75 |
| 143 | 0.0440 | 2.16 |
| 153 | 0.0546 | 2.57 |
| 162 | 0.0410 | 2.70 |
| 172 | 0.0271 | 0.71 |
| 183 | 0.0316 | 1.91 |
| 192 | 0.0239 | 2.52 |
| 204 | 0.0000 | 2.70 |

![Video left, WebGL right](video-comparison.jpg)

Video comparison uses the same documented affine clock fit as native validation. Maximum raw visual cut discrepancy remains 0.650 s; the fit does not change playback.

Fit only aligns comparison frames. Playback uses the executable's absolute timing. Report raw timing differences too. Video cut drift and audio alignment differ; the reference capture's exact clock behavior is unknown.

Full audio-clock playback completed all 11 scenes in 210.47 wall seconds. Maximum observed cut delay: 0.0107 s. Mean frame interval: 17.37 ms; p95: 24.40 ms.

These measurements establish parity with the native reconstruction on the tested browser/GPU, not exact reproduction of the original 1999/2000 renderer or universal GPU pixel identity. See report.json for versions, input/output hashes and warnings.
