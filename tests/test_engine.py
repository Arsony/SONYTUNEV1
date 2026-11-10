#!/usr/bin/env python3
"""SonyTune engine tests on synthetic audio.
Build the harness first:  g++ -std=c++17 -O2 -o run_engine run_engine.cpp
Then:                      python3 test_engine.py
"""
import os, subprocess, sys, tempfile
import numpy as np

FS = 48000
HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(HERE, "run_engine")

SCALES = {"chromatic": 0, "major": 1, "minor": 2, "pent_major": 4}


def vowel(f0, formants=((700, 110, 1.0), (1200, 150, 0.6), (2600, 250, 0.2)), amp=0.4):
    """Harmonic vowel-like signal with a fixed formant envelope. f0 = per-sample array (Hz)."""
    phase = np.cumsum(f0) / FS
    out = np.zeros(len(f0))
    for k in range(1, 50):
        fk = k * f0
        env = 1.0 + sum(3.0 * a * np.exp(-0.5 * ((fk - fc) / bw) ** 2) for fc, bw, a in formants)
        out += np.where(fk < 0.45 * FS, env, 0.0) * np.sin(2 * np.pi * k * phase) / k ** 1.2
    return amp * out / np.max(np.abs(out))


def run(sig, key=0, scale="chromatic", speed=0, amount=1, humanize=0, formant=0, mix=1,
        fast=0, hq=0, ch=1):
    sig = np.asarray(sig, dtype=np.float32)
    data = sig if ch == 1 else np.stack([sig, sig * 0.8], axis=1).reshape(-1)
    with tempfile.TemporaryDirectory() as d:
        fi, fo = os.path.join(d, "i.raw"), os.path.join(d, "o.raw")
        data.astype(np.float32).tofile(fi)
        args = [EXE, fi, fo, str(FS), str(ch), str(key), str(SCALES[scale]), str(speed), str(amount),
                str(humanize), str(formant), str(mix), str(fast), str(hq)]
        res = subprocess.run(args, capture_output=True, text=True, check=True)
        lat = int(res.stdout.split()[1])
        out = np.fromfile(fo, dtype=np.float32)
    if ch == 2:
        out = out.reshape(-1, 2)[:, 0]
    return out, lat


def pitch_track(x, hop=480, win=2400, fmin=70, fmax=700):
    """Simple autocorrelation pitch tracker for analysis. Returns (times, f0 or nan)."""
    times, f0s = [], []
    lo, hi = int(FS / fmax), int(FS / fmin)
    for s in range(0, len(x) - win - hi, hop):
        seg = x[s:s + win + hi].astype(np.float64)
        a = seg[:win]
        if np.sqrt(np.mean(a ** 2)) < 0.01:
            times.append((s + win / 2) / FS); f0s.append(np.nan); continue
        ac = np.array([np.dot(a, seg[l:l + win]) for l in range(lo, hi)])
        norm = np.sqrt(np.dot(a, a) * np.array([np.dot(seg[l:l + win], seg[l:l + win]) for l in range(lo, hi)]) + 1e-12)
        nac = ac / norm
        peak = nac.max()
        cands = [j for j in range(1, len(nac) - 1) if nac[j] >= nac[j - 1] and nac[j] >= nac[j + 1] and nac[j] >= 0.92 * peak]
        i = cands[0] if cands else int(np.argmax(nac))
        if nac[i] < 0.8:
            times.append((s + win / 2) / FS); f0s.append(np.nan); continue
        if 0 < i < len(nac) - 1:
            y0, y1, y2 = nac[i - 1], nac[i], nac[i + 1]
            den = y0 + y2 - 2 * y1
            i = i + (0.5 * (y0 - y2) / den if abs(den) > 1e-12 else 0)
        times.append((s + win / 2) / FS); f0s.append(FS / (lo + i))
    return np.array(times), np.array(f0s)


def cents(a, b):
    return 1200 * np.log2(a / b)


results = []


def check(name, ok, detail):
    results.append(ok)
    print(("PASS  " if ok else "FAIL  ") + name + "  |  " + detail)


# ---------------------------------------------------------------------------
dur = 3.0
n = int(dur * FS)

# 1. steady note, 30 cents sharp of A3 -> expect A3 = 220 Hz (chromatic, hard tune)
for hq in (0, 1):
    f_in = 220 * 2 ** (30 / 1200)
    sig = vowel(np.full(n, f_in))
    out, lat = run(sig, speed=0, hq=hq)
    t, f = pitch_track(out[lat:])
    m = (t > 0.5) & (t < dur - 0.8) & ~np.isnan(f)
    err = np.abs(cents(f[m], 220.0))
    check(f"steady +30c -> A3 [{'HQ' if hq else 'LIVE'}, latency {lat/FS*1000:.1f} ms]",
          m.sum() > 20 and np.median(err) < 6 and np.percentile(err, 95) < 15,
          f"median err {np.median(err):.2f} c, p95 {np.percentile(err, 95):.2f} c")

# 2. C major: C4 + 70 cents must go to C4 (not C#), chromatic must go to C#4
f_in = 261.6256 * 2 ** (70 / 1200)
sig = vowel(np.full(n, f_in))
for scale, target in (("major", 261.6256), ("chromatic", 277.1826)):
    out, lat = run(sig, key=0, scale=scale, speed=0)
    t, f = pitch_track(out[lat:])
    m = (t > 0.5) & (t < dur - 0.8) & ~np.isnan(f)
    err = np.abs(cents(f[m], target))
    check(f"C4+70c in {scale} -> {target:.1f} Hz", np.median(err) < 6, f"median err {np.median(err):.2f} c")

# 3. gliding pitch with hard tune: output must sit on semitone centres
f_gl = np.geomspace(140, 280, n)
sig = vowel(f_gl)
out, lat = run(sig, speed=0)
t, f = pitch_track(out[lat:])
m = (t > 0.4) & (t < dur - 0.6) & ~np.isnan(f)
midi = 69 + 12 * np.log2(f[m] / 440)
dev = np.abs(midi - np.round(midi)) * 100
check("glide 140->280 Hz, hard tune lands on semitones", np.median(dev) < 8 and np.percentile(dev, 90) < 30,
      f"median dev {np.median(dev):.2f} c, p90 {np.percentile(dev, 90):.2f} c")

# 4. vibrato: humanize 1 keeps it, humanize 0 flattens it
tt = np.arange(n) / FS
f_vib = 200 * 2 ** ((35 * np.sin(2 * np.pi * 5.5 * tt)) / 1200)
sig = vowel(f_vib)
sd = {}
for hum in (0.0, 1.0):
    out, lat = run(sig, speed=0, humanize=hum)
    t, f = pitch_track(out[lat:])
    m = (t > 0.5) & (t < dur - 0.8) & ~np.isnan(f)
    sd[hum] = np.std(cents(f[m], np.median(f[m])))
t, f = pitch_track(sig)
m = (t > 0.5) & (t < dur - 0.8)
sd_in = np.std(cents(f[m], np.median(f[m])))
check("humanize=0 flattens vibrato", sd[0.0] < 0.35 * sd_in, f"input {sd_in:.1f} c rms -> {sd[0.0]:.1f} c")
check("humanize=1 keeps vibrato", sd[1.0] > 0.6 * sd_in, f"input {sd_in:.1f} c rms -> {sd[1.0]:.1f} c")

# 5. noise (consonant-like) passes at the same level
rng = np.random.default_rng(1)
noise = (0.1 * rng.standard_normal(n)).astype(np.float32)
out, lat = run(noise)
rin = np.sqrt(np.mean(noise[: n - lat] ** 2))
rout = np.sqrt(np.mean(out[lat + 4000:] ** 2))
check("unvoiced noise level preserved", abs(20 * np.log10(rout / rin)) < 1.0, f"{20*np.log10(rout/rin):+.2f} dB")

# 6. voiced / unvoiced alternation ("fast rap" syllables): level, clicks, NaNs
seg_v, seg_n = int(0.09 * FS), int(0.04 * FS)
pieces, pos, k = [], 0, 0
while pos < n:
    f0 = 150 * 2 ** (((k * 3) % 7 - 3) * 0.7 / 12 + 0.25 / 12)
    pieces.append(vowel(np.full(seg_v, f0)))
    pieces.append((0.06 * rng.standard_normal(seg_n)))
    pos += seg_v + seg_n
    k += 1
sig = np.concatenate(pieces)[:n]
for fast in (0, 1):
    out, lat = run(sig, speed=10, fast=fast, hq=1)
    a = out[lat:]
    b = sig[: len(a)]
    jump = np.max(np.abs(np.diff(a[2000:])))
    jump_in = np.max(np.abs(np.diff(b[2000:])))
    lvl = 20 * np.log10(np.sqrt(np.mean(a[2000:] ** 2)) / np.sqrt(np.mean(b[2000:] ** 2)))
    check(f"fast-rap syllables [{'FAST' if fast else 'MID'}]: finite, level, no big clicks",
          np.all(np.isfinite(out)) and abs(lvl) < 1.5 and jump < 2.5 * jump_in + 0.05,
          f"level {lvl:+.2f} dB, max step {jump:.3f} (input {jump_in:.3f})")

# 7. formants preserved when the note moves a couple of semitones
f_in = 246.94  # B3 -> C major pentatonic pulls it to C4 (+1 semitone)
sig = vowel(np.full(n, f_in))
out, lat = run(sig, key=0, scale="pent_major", speed=0)
a = out[lat + 24000: lat + 24000 + 2 ** 15]
b = sig[24000: 24000 + 2 ** 15]


def centroid(x):
    s = np.abs(np.fft.rfft(x * np.hanning(len(x))))
    fr = np.fft.rfftfreq(len(x), 1 / FS)
    m = fr < 4000
    return np.sum(fr[m] * s[m]) / np.sum(s[m])


ci, co = centroid(b), centroid(a)
check("formants preserved on 1-semitone shift (spectral centroid)", abs(co / ci - 1) < 0.08,
      f"centroid {ci:.0f} Hz -> {co:.0f} Hz")

# 8. stereo runs and keeps channel relationship
sig = vowel(np.full(n, 223.8))
out_l, lat = run(sig, ch=2)
t, f = pitch_track(out_l[lat:])
m = (t > 0.5) & (t < dur - 0.8) & ~np.isnan(f)
check("stereo path works", np.median(np.abs(cents(f[m], 220.0))) < 6, f"median err {np.median(np.abs(cents(f[m], 220.0))):.2f} c")

# 9. mix=0 is a bit-exact (delayed) bypass
sig = vowel(np.full(n, 223.8))
out, lat = run(sig, mix=0)
d = np.max(np.abs(out[lat:] - sig[: len(out) - lat]))
check("mix=0 is transparent delayed bypass", d < 1e-5, f"max diff {d:.2e}")

print(f"\n{sum(results)}/{len(results)} passed")
sys.exit(0 if all(results) else 1)
