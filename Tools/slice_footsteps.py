"""Cut single footsteps out of a run-cycle recording.

Decodes with the libsndfile that ships with Unreal, peak-picks the footfalls, then writes the
loudest few out as individual 16-bit mono wavs, each trimmed to its own attack and decay,
faded at both ends and normalised to a common peak.
"""
import ctypes, math, os, sys, wave

DLL_DIR = r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\libsndfile\Win64"
SFM_READ = 0x10

WANT = 6              # how many steps to keep
LEAD_MS = 12.0        # silence kept before the attack
TAIL_MS = 240.0       # longest a single step may run
FADE_IN_MS = 3.0
FADE_OUT_MS = 45.0
GUARD_MS = 20.0       # stop this far short of the next footfall
TARGET_PEAK = 0.85


class SF_INFO(ctypes.Structure):
    _fields_ = [
        ("frames", ctypes.c_int64),
        ("samplerate", ctypes.c_int),
        ("channels", ctypes.c_int),
        ("format", ctypes.c_int),
        ("sections", ctypes.c_int),
        ("seekable", ctypes.c_int),
    ]


def load():
    os.add_dll_directory(DLL_DIR)
    lib = ctypes.CDLL(os.path.join(DLL_DIR, "libsndfile-1.dll"))
    lib.sf_open.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(SF_INFO)]
    lib.sf_open.restype = ctypes.c_void_p
    lib.sf_readf_float.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_float), ctypes.c_int64]
    lib.sf_readf_float.restype = ctypes.c_int64
    lib.sf_close.argtypes = [ctypes.c_void_p]
    lib.sf_close.restype = ctypes.c_int
    lib.sf_strerror.argtypes = [ctypes.c_void_p]
    lib.sf_strerror.restype = ctypes.c_char_p
    return lib


def read_mono(lib, path):
    sfinfo = SF_INFO()
    handle = lib.sf_open(path.encode("utf-8"), SFM_READ, ctypes.byref(sfinfo))
    if not handle:
        raise RuntimeError("sf_open failed: %s" % lib.sf_strerror(None).decode("utf-8", "replace"))
    n, ch = sfinfo.frames, sfinfo.channels
    buf = (ctypes.c_float * (n * ch))()
    got = lib.sf_readf_float(handle, buf, n)
    lib.sf_close(handle)
    if ch == 1:
        return list(buf[:got]), sfinfo.samplerate
    mono = [0.0] * got
    for i in range(got):
        base = i * ch
        mono[i] = sum(buf[base + c] for c in range(ch)) / ch
    return mono, sfinfo.samplerate


def envelope(samples, rate, hop_ms=5.0, win_ms=20.0):
    hop = max(1, int(rate * hop_ms / 1000.0))
    win = max(hop, int(rate * win_ms / 1000.0))
    out = []
    i = 0
    while i < len(samples):
        chunk = samples[i:i + win]
        if not chunk:
            break
        out.append(math.sqrt(sum(v * v for v in chunk) / len(chunk)))
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


def attack_frame(env, hop, peak_idx, frac=0.25):
    """Walk back from the loudest point to where the hump leaves the floor."""
    level = env[peak_idx] * frac
    k = peak_idx
    while k > 0 and env[k] > level:
        k -= 1
    return k * hop


def write_wav(path, samples, rate):
    frames = bytearray()
    for v in samples:
        s = int(max(-1.0, min(1.0, v)) * 32767.0)
        frames += int(s).to_bytes(2, "little", signed=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(bytes(frames))


def main():
    src, out_dir = sys.argv[1], sys.argv[2]
    lib = load()
    samples, rate = read_mono(lib, src)
    env, hop = envelope(samples, rate)
    onsets = find_onsets(env, hop, rate)
    if not onsets:
        sys.exit("no footfalls found")

    # The loudest WANT footfalls, then back into time order so the files read left to right.
    chosen = sorted(sorted(onsets, key=lambda o: env[o], reverse=True)[:WANT])

    os.makedirs(out_dir, exist_ok=True)
    lead = int(rate * LEAD_MS / 1000.0)
    guard = int(rate * GUARD_MS / 1000.0)
    fade_in = int(rate * FADE_IN_MS / 1000.0)
    fade_out = int(rate * FADE_OUT_MS / 1000.0)

    print("%d footfalls found, keeping the %d loudest" % (len(onsets), len(chosen)))
    for n, peak_idx in enumerate(chosen, start=1):
        start = max(0, attack_frame(env, hop, peak_idx) - lead)
        end = min(len(samples), start + int(rate * TAIL_MS / 1000.0))

        later = [o for o in onsets if o * hop > peak_idx * hop + hop]
        if later:
            end = min(end, max(start + fade_in + fade_out + 1, later[0] * hop - guard))

        cut = samples[start:end]
        if not cut:
            continue

        peak = max(max(cut), -min(cut))
        gain = (TARGET_PEAK / peak) if peak > 1e-6 else 1.0
        cut = [v * gain for v in cut]

        for i in range(min(fade_in, len(cut))):
            cut[i] *= i / float(fade_in)
        for i in range(min(fade_out, len(cut))):
            k = len(cut) - 1 - i
            cut[k] *= 0.5 - 0.5 * math.cos(math.pi * i / float(fade_out))

        name = "Step_%02d.wav" % n
        write_wav(os.path.join(out_dir, name), cut, rate)
        print("  %s  from %.3f s  %.0f ms  (gain x%.1f)" % (
            name, start / float(rate), 1000.0 * len(cut) / rate, gain))

    print("written to %s" % out_dir)


main()
