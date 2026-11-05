"""Say what is actually in an audio file, before it goes anywhere near the engine.

Decodes through the libsndfile that ships with Unreal, so it reads mp3, wav, ogg, flac, aiff and
opus without anything else installed. Reports the things that decide whether a sound is usable:
silence at the head (which makes a one-shot arrive late), how long it really is, whether it is one
event or a repeating sequence, how fast it decays, and whether it already has a room on it.

    python.exe -I Tools/inspect_audio.py <file> [<file> ...]
"""
import ctypes, math, os, sys

DLL_DIR = r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\libsndfile\Win64"
SFM_READ = 0x10


class SF_INFO(ctypes.Structure):
    _fields_ = [("frames", ctypes.c_int64), ("samplerate", ctypes.c_int), ("channels", ctypes.c_int),
                ("format", ctypes.c_int), ("sections", ctypes.c_int), ("seekable", ctypes.c_int)]


def load():
    os.add_dll_directory(DLL_DIR)
    lib = ctypes.CDLL(os.path.join(DLL_DIR, "libsndfile-1.dll"))
    lib.sf_open.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(SF_INFO)]
    lib.sf_open.restype = ctypes.c_void_p
    lib.sf_readf_float.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_float), ctypes.c_int64]
    lib.sf_readf_float.restype = ctypes.c_int64
    lib.sf_close.argtypes = [ctypes.c_void_p]
    return lib


def read_mono(lib, path):
    sfinfo = SF_INFO()
    h = lib.sf_open(path.encode("utf-8"), SFM_READ, ctypes.byref(sfinfo))
    if not h:
        raise RuntimeError("could not decode %s" % path)
    n, ch = sfinfo.frames, sfinfo.channels
    buf = (ctypes.c_float * (n * ch))()
    got = lib.sf_readf_float(h, buf, n)
    lib.sf_close(h)
    if ch == 1:
        return list(buf[:got]), sfinfo.samplerate, ch
    mono = [0.0] * got
    for i in range(got):
        b = i * ch
        mono[i] = sum(buf[b + c] for c in range(ch)) / ch
    return mono, sfinfo.samplerate, ch


def lowpass(x, fc, rate):
    a = 1.0 - math.exp(-2.0 * math.pi * fc / rate)
    y, out = 0.0, [0.0] * len(x)
    for i, v in enumerate(x):
        y += a * (v - y)
        out[i] = y
    return out


def rms(x):
    return math.sqrt(sum(v * v for v in x) / len(x)) if x else 0.0


def envelope(s, rate, hop_ms=5.0, win_ms=20.0):
    hop = max(1, int(rate * hop_ms / 1000.0))
    win = max(hop, int(rate * win_ms / 1000.0))
    out, i = [], 0
    while i + win <= len(s):
        out.append(rms(s[i:i + win]))
        i += hop
    return out, hop


def find_onsets(env, hop, rate, floor_frac=0.18, min_gap_ms=90.0):
    if not env:
        return []
    peak = max(env)
    thresh = peak * floor_frac
    min_gap = int((min_gap_ms / 1000.0) * rate / hop)
    onsets, i = [], 1
    while i < len(env) - 1:
        if env[i] >= thresh and env[i] >= env[i - 1] and env[i] > env[i + 1]:
            j = i
            while j < len(env) - 1 and env[j + 1] >= thresh:
                j += 1
            best = max(range(i, min(j + 1, len(env))), key=lambda k: env[k])
            if not onsets or best - onsets[-1] >= min_gap:
                onsets.append(best)
            elif env[best] > env[onsets[-1]]:
                onsets[-1] = best
            i = best + min_gap
        else:
            i += 1
    return onsets


def db(x):
    return -99.0 if x <= 1e-9 else 20.0 * math.log10(x)


def inspect(lib, path):
    s, rate, ch = read_mono(lib, path)
    n = len(s)
    dur = n / float(rate)
    pk = max(max(s), -min(s)) if s else 0.0

    print("\n%s" % os.path.basename(path))
    print("  %.3f s, %d Hz, %s, peak %.3f (%.1f dB)"
          % (dur, rate, "mono" if ch == 1 else "%d ch" % ch, pk, db(pk)))

    # Silence at the head: when does it actually start? A one-shot with 300 ms here arrives late.
    start = 0
    for i, v in enumerate(s):
        if abs(v) > pk * 0.02:
            start = i
            break
    print("  starts at %.0f ms%s" % (start * 1000.0 / rate,
                                     "  <-- would arrive late" if start * 1000.0 / rate > 25 else ""))

    env, hop = envelope(s, rate)
    onsets = find_onsets(env, hop, rate)
    gaps = [(onsets[k] - onsets[k - 1]) * hop / float(rate) for k in range(1, len(onsets))]
    print("  %d loud event%s" % (len(onsets), "" if len(onsets) == 1 else "s"), end="")
    if gaps:
        gs = sorted(gaps)
        print(", median gap %.0f ms (min %.0f, max %.0f)"
              % (gs[len(gs) // 2] * 1000, min(gaps) * 1000, max(gaps) * 1000))
    else:
        print("")

    # Decay measured off the RMS envelope against its own maximum, never off the sample peak:
    # a sharp transient sits far above a tail and makes a sound that never fades look like it does.
    top = max(env) if env else 0.0
    if top > 0:
        i0 = env.index(top)
        for drop in (20.0, 30.0, 45.0):
            floor = top * (10.0 ** (-drop / 20.0))
            at = None
            for i in range(len(env) - 1, i0, -1):
                if env[i] > floor:
                    at = (i - i0) * hop * 1000.0 / rate
                    break
            print("  -%.0f dB at %s" % (drop, "%.0f ms" % at if at is not None else "never"))

    # One filter pass, reused. Calling lowpass() inside the comprehension makes this O(n^2) and
    # takes minutes on a few seconds of audio.
    lp = lowpass(s, 200.0, rate)
    lo = rms(lp)
    hi = rms([v - lp[i] for i, v in enumerate(s)])
    if hi > 1e-9 and lo > 1e-9:
        print("  low end %+.1f dB vs high" % (20.0 * math.log10(lo / hi)))

    # A verdict, so the numbers above do not have to be read.
    if len(onsets) > 4 and gaps:
        gs = sorted(gaps)
        med = gs[len(gs) // 2]
        spread = (max(gaps) - min(gaps)) / med if med > 0 else 9
        kind = "a steady repeating sequence" if spread < 0.5 else "several separate events"
        print("  LOOKS LIKE: %s - %d hits about %.0f ms apart, not a single one-shot"
              % (kind, len(onsets), med * 1000))
    elif len(onsets) <= 2:
        print("  LOOKS LIKE: a single event, which is what a one-shot wants")
    else:
        print("  LOOKS LIKE: %d events - check whether you wanted one" % len(onsets))


def main():
    lib = load()
    for p in sys.argv[1:]:
        if not os.path.exists(p):
            print("\n%s\n  NOT FOUND" % p)
            continue
        inspect(lib, p)


main()
