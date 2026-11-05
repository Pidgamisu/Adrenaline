"""Synthesise C4 explosion candidates.

An explosion is four things layered: a crack (a few ms of bright noise), a body (a pitch sweeping
down, which is what reads as force), a rumble (low-passed noise with a slow decay) and optionally
debris (sparse clicks afterwards). Changing the balance between those four gives genuinely different
bangs, which is the point - these are meant to be chosen between by ear, not tuned by measurement.

Deliberately rendered DRY, with no room on them. The tail here is the explosion's own rumble, not
reverb. If a reverb submix goes on the master output, these will pick the room up from it, and
anything with a room baked in would be reverbed twice.

    python.exe -I c4_bang.py <output folder>
"""
import math, os, random, sys, wave

RATE = 44100
TARGET_PEAK = 0.90
FADE_IN_MS = 0.6         # just enough to stop the first sample being a click
FADE_OUT_MS = 40.0       # guarantees the end is silent


# ---------------------------------------------------------------- building blocks

def lowpass(x, fc):
    a = 1.0 - math.exp(-2.0 * math.pi * fc / RATE)
    y, out = 0.0, [0.0] * len(x)
    for i, v in enumerate(x):
        y += a * (v - y)
        out[i] = y
    return out


def highpass(x, fc):
    lp = lowpass(x, fc)
    return [v - lp[i] for i, v in enumerate(x)]


def noise(n, seed):
    r = random.Random(seed)
    return [r.uniform(-1.0, 1.0) for _ in range(n)]


def decay(n, ms):
    """Exponential envelope reaching -60 dB at `ms`."""
    k = 6.9 / max(1.0, RATE * ms / 1000.0)
    return [math.exp(-k * i) for i in range(n)]


def sweep(n, f0, f1, ms):
    """A sine falling from f0 to f1 over `ms`, then holding f1. The downward pitch is the force."""
    span = max(1.0, RATE * ms / 1000.0)
    out, phase = [0.0] * n, 0.0
    for i in range(n):
        t = min(1.0, i / span)
        f = f0 * ((f1 / f0) ** t)
        phase += 2.0 * math.pi * f / RATE
        out[i] = math.sin(phase)
    return out


def debris(n, seed, start_ms, end_ms, count, fc, amp):
    """Sparse little clicks after the blast, for falling bits."""
    r = random.Random(seed)
    out = [0.0] * n
    burst = int(RATE * 0.004)
    lo, hi = int(RATE * start_ms / 1000.0), min(n - burst, int(RATE * end_ms / 1000.0))
    if hi <= lo:
        return out
    for _ in range(count):
        at = r.randrange(lo, hi)
        gain = amp * r.uniform(0.25, 1.0)
        env = decay(burst, 3.0)
        for j in range(burst):
            out[at + j] += r.uniform(-1.0, 1.0) * env[j] * gain
    return highpass(out, fc)


def layer(dst, src, gain):
    for i in range(min(len(dst), len(src))):
        dst[i] += src[i] * gain


# ---------------------------------------------------------------- the candidates

def build(name, spec):
    n = int(RATE * spec["length_ms"] / 1000.0)
    mix = [0.0] * n

    # Crack: the transient. Bright noise, gone in a few ms.
    c = spec["crack"]
    crack = highpass(noise(n, 1), c["fc"])
    env = decay(n, c["ms"])
    layer(mix, [crack[i] * env[i] for i in range(n)], c["gain"])

    # Mid burst, where there is one: the body of the blast rather than the boom.
    if "burst" in spec:
        b = spec["burst"]
        mid = lowpass(highpass(noise(n, 2), b["hp"]), b["lp"])
        env = decay(n, b["ms"])
        layer(mix, [mid[i] * env[i] for i in range(n)], b["gain"])

    # Body: the downward sweep. This is what makes it feel like force rather than a pop.
    s = spec["body"]
    swp = sweep(n, s["f0"], s["f1"], s["sweep_ms"])
    env = decay(n, s["ms"])
    layer(mix, [swp[i] * env[i] for i in range(n)], s["gain"])

    # Rumble: low noise with a long decay, the air moving afterwards.
    r = spec["rumble"]
    rum = lowpass(noise(n, 3), r["fc"])
    env = decay(n, r["ms"])
    layer(mix, [rum[i] * env[i] for i in range(n)], r["gain"])

    if "debris" in spec:
        d = spec["debris"]
        layer(mix, debris(n, 4, d["start_ms"], d["end_ms"], d["count"], d["fc"], 1.0), d["gain"])

    # Level, then make sure both ends are clean.
    pk = max(max(mix), -min(mix))
    if pk > 1e-9:
        mix = [v * (TARGET_PEAK / pk) for v in mix]

    fi = int(RATE * FADE_IN_MS / 1000.0)
    fo = int(RATE * FADE_OUT_MS / 1000.0)
    for i in range(min(fi, n)):
        mix[i] *= i / float(fi)
    for i in range(min(fo, n)):
        k = n - 1 - i
        mix[k] *= 0.5 - 0.5 * math.cos(math.pi * i / float(fo))
    return mix


CANDIDATES = {
    # Concussive and close. Almost no crack, all chest.
    "C4_Thump": dict(
        length_ms=700,
        crack=dict(fc=1200, ms=25, gain=0.45),
        body=dict(f0=140, f1=38, sweep_ms=320, ms=260, gain=1.00),
        rumble=dict(fc=180, ms=400, gain=0.45),
    ),
    # Sharp and dry, like a charge going off on something hard. Reads fast.
    "C4_Crack": dict(
        length_ms=520,
        crack=dict(fc=2500, ms=12, gain=1.00),
        burst=dict(hp=400, lp=2000, ms=90, gain=0.70),
        body=dict(f0=180, f1=60, sweep_ms=150, ms=120, gain=0.55),
        rumble=dict(fc=250, ms=200, gain=0.25),
    ),
    # Cinematic. Deep sweep, long rumble, bits coming down afterwards.
    "C4_Boom": dict(
        length_ms=1500,
        crack=dict(fc=1500, ms=20, gain=0.75),
        body=dict(f0=120, f1=30, sweep_ms=500, ms=450, gain=1.00),
        rumble=dict(fc=140, ms=1100, gain=0.60),
        debris=dict(start_ms=60, end_ms=900, count=26, fc=1800, gain=0.30),
    ),
}


# ---------------------------------------------------------------- reporting

def write_wav(path, samples):
    frames = bytearray()
    for v in samples:
        s = int(max(-1.0, min(1.0, v)) * 32767.0)
        frames += int(s).to_bytes(2, "little", signed=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(bytes(frames))


def rms(x):
    return math.sqrt(sum(v * v for v in x) / len(x)) if x else 0.0


def report(samples):
    """Decay to -30 dB off the RMS envelope, and how much of it is low end."""
    win = int(RATE * 0.02)
    env = [rms(samples[i:i + win]) for i in range(0, len(samples) - win, win)]
    top = max(env) if env else 0.0
    out = 0.0
    if top > 0:
        i0 = env.index(top)
        floor = top * (10.0 ** (-30.0 / 20.0))
        for i in range(len(env) - 1, i0, -1):
            if env[i] > floor:
                out = (i - i0) * win * 1000.0 / RATE
                break
    lo, hi = rms(lowpass(samples, 200.0)), rms(highpass(samples, 200.0))
    tilt = 20.0 * math.log10(lo / hi) if hi > 1e-9 and lo > 1e-9 else 0.0
    return out, tilt


def main():
    out_dir = sys.argv[1]
    os.makedirs(out_dir, exist_ok=True)
    for name, spec in CANDIDATES.items():
        s = build(name, spec)
        write_wav(os.path.join(out_dir, name + ".wav"), s)
        d, tilt = report(s)
        print("  %-10s %5.0f ms long, -30 dB at %4.0f ms, low end %+.1f dB vs high"
              % (name + ".wav", spec["length_ms"], d, tilt))
    print("written to %s" % out_dir)


main()
