"""Re-cut the footsteps and give them a tail.

The source run cycle is bone dry: each footfall falls 20 dB inside 50 ms and what follows the last
one is room noise, not reverb. So the ring-out cannot be recovered from the recording - it has to be
added. This runs a Schroeder/Freeverb network (eight damped comb filters in parallel into four
allpass filters in series) over each dry cut, padded out so the tail has somewhere to ring.

Writes the wet versions over Audio/Footsteps/Step_NN.wav, so the editor only needs a Reimport, and
keeps the dry cuts in Audio/Footsteps/Dry/ so the reverb can be retuned later without going back to
the mp3.
"""
import ctypes, math, os, sys, wave

DLL_DIR = r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\libsndfile\Win64"
SFM_READ = 0x10

WANT = 6
LEAD_MS = 12.0          # silence kept before the attack
DRY_TAIL_MS = 240.0     # the dry footfall is all over inside this
GUARD_MS = 20.0         # never run into the next footfall in the source
TARGET_PEAK = 0.85

# --- the room -----------------------------------------------------------------
LENGTH_MS = 720.0       # how long the finished sample is, tail included
PREDELAY_MS = 14.0      # the tail arrives just after the dry hit, not on top of it
ROOM_SIZE = 0.45        # 0..1, the colour of the room; TAIL_DECAY_MS sets how long it rings
DAMPING = 0.45          # 0..1, bigger soaks up the high end faster, so the tail darkens
TAIL_DECAY_MS = 650.0   # the wet is forced to silence by here, whatever the combs would do
WET_LEVEL = 0.40        # the tail's peak as a fraction of the dry hit: 0.6 starts it ~4 dB down
DRY = 1.00              # the dry hit's own level going into the mix
FADE_IN_MS = 2.0
FADE_OUT_MS = 120.0     # safety only: the forced decay has already reached silence by here

COMB_TUNING = [1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617]  # at 44.1 kHz
ALLPASS_TUNING = [556, 441, 341, 225]

# Any of the room constants above can be overridden on the command line as name=value, so the room
# can be retuned without editing the file: ... Audioootsteps room_size=0.95 wet=0.8
for _arg in sys.argv[3:]:
    _k, _, _v = _arg.partition("=")
    if _k in globals() and _v:
        globals()[_k] = float(_v)
        print("override %s = %s" % (_k, _v))


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
        raise RuntimeError("sf_open failed")
    n, ch = sfinfo.frames, sfinfo.channels
    buf = (ctypes.c_float * (n * ch))()
    got = lib.sf_readf_float(h, buf, n)
    lib.sf_close(h)
    if ch == 1:
        return list(buf[:got]), sfinfo.samplerate
    mono = [0.0] * got
    for i in range(got):
        b = i * ch
        mono[i] = sum(buf[b + c] for c in range(ch)) / ch
    return mono, sfinfo.samplerate


def envelope(s, rate, hop_ms=5.0, win_ms=20.0):
    hop = max(1, int(rate * hop_ms / 1000.0))
    win = max(hop, int(rate * win_ms / 1000.0))
    out, i = [], 0
    while i < len(s):
        c = s[i:i + win]
        if not c:
            break
        out.append(math.sqrt(sum(v * v for v in c) / len(c)))
        i += hop
    return out, hop


def find_onsets(env, hop, rate, floor_frac=0.18, min_gap_ms=90.0):
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
    level = env[peak_idx] * frac
    k = peak_idx
    while k > 0 and env[k] > level:
        k -= 1
    return k * hop


def reverb(dry, rate, length):
    """
    The room's contribution on its own - no dry signal mixed in - as a list `length` long.

    Returned wet-only and unlevelled on purpose. Freeverb's output level swings wildly with the
    feedback coefficient, because the comb filters are resonant: a long room builds up far more
    amplitude than a short one, and the fixed 0.015 output gain the original uses only balances at
    one setting. Mixing dry against wet by those raw numbers is what produced both failures here -
    a flat wash of reverb at ROOM_SIZE 0.88, and a tail 38 dB down and inaudible at 0.45. The
    caller scales this to a stated level relative to the dry hit instead, so the balance no longer
    depends on the room at all.
    """
    scale = rate / 44100.0
    combs = [[0.0] * max(8, int(t * scale)) for t in COMB_TUNING]
    comb_idx = [0] * len(combs)
    comb_store = [0.0] * len(combs)
    allp = [[0.0] * max(4, int(t * scale)) for t in ALLPASS_TUNING]
    allp_idx = [0] * len(allp)

    feedback = ROOM_SIZE * 0.28 + 0.7
    damp = DAMPING * 0.4
    damp_inv = 1.0 - damp
    predelay = int(rate * PREDELAY_MS / 1000.0)

    out = [0.0] * length
    for n in range(length):
        # What is fed into the room is delayed, so the tail blooms just after the hit.
        src_i = n - predelay
        src = dry[src_i] if 0 <= src_i < len(dry) else 0.0

        wet = 0.0
        for c in range(len(combs)):
            buf = combs[c]
            i = comb_idx[c]
            y = buf[i]
            comb_store[c] = y * damp_inv + comb_store[c] * damp
            buf[i] = src + comb_store[c] * feedback
            comb_idx[c] = i + 1 if i + 1 < len(buf) else 0
            wet += y

        for a in range(len(allp)):
            buf = allp[a]
            i = allp_idx[a]
            bufout = buf[i]
            y = -wet + bufout
            buf[i] = wet + bufout * 0.5
            allp_idx[a] = i + 1 if i + 1 < len(buf) else 0
            wet = y

        out[n] = wet
    return out


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


def decay_ms(samples, rate, drop_db=30.0):
    """
    Time for the RMS envelope to fall drop_db below its own maximum and stay there.

    Measured off the RMS envelope rather than the sample peak. Measuring from the peak is exactly
    what hid the first attempt's problem: the dry hit is a 5 ms transient far above the tail, so
    "20 dB below the peak" was reached at 330 ms while the tail carried on as a flat wash for
    another 700 - which is what "it doesn't fade out" meant.
    """
    env, hop = envelope(samples, rate)
    top = max(env)
    if top <= 0:
        return 0.0
    i0 = env.index(top)
    floor = top * (10.0 ** (-drop_db / 20.0))
    for i in range(len(env) - 1, i0, -1):
        if env[i] > floor:
            return (i - i0) * hop * 1000.0 / rate
    return 0.0


def cut_from_recording(lib, src, dry_dir):
    """Slice a run cycle into dry single steps, write them out, and hand them back."""
    samples, rate = read_mono(lib, src)
    env, hop = envelope(samples, rate)
    onsets = find_onsets(env, hop, rate)
    chosen = sorted(sorted(onsets, key=lambda o: env[o], reverse=True)[:WANT])

    lead = int(rate * LEAD_MS / 1000.0)
    guard = int(rate * GUARD_MS / 1000.0)

    cuts = []
    for n, peak_idx in enumerate(chosen, start=1):
        start = max(0, attack_frame(env, hop, peak_idx) - lead)
        end = min(len(samples), start + int(rate * DRY_TAIL_MS / 1000.0))
        later = [o for o in onsets if o * hop > peak_idx * hop + hop]
        if later:
            end = min(end, max(start + 1, later[0] * hop - guard))

        cut = samples[start:end]
        if not cut:
            continue

        # Level the dry cut first, so every step feeds the room equally hard.
        pk = max(max(cut), -min(cut))
        if pk > 1e-6:
            cut = [v * (0.9 / pk) for v in cut]

        name = "Step_%02d.wav" % n
        write_wav(os.path.join(dry_dir, name), cut, rate)
        cuts.append((name, cut, rate))
    return cuts


def read_dry_cuts(lib, folder):
    """Dry steps that were cut on an earlier run, so the room can be retuned on its own."""
    cuts = []
    for name in sorted(os.listdir(folder)):
        if name.lower().endswith(".wav"):
            samples, rate = read_mono(lib, os.path.join(folder, name))
            cuts.append((name, samples, rate))
    return cuts


def main():
    src, out_dir = sys.argv[1], sys.argv[2]
    dry_dir = os.path.join(out_dir, "Dry")
    lib = load()
    os.makedirs(out_dir, exist_ok=True)
    os.makedirs(dry_dir, exist_ok=True)

    # A folder of wavs means the cutting was already done on an earlier run, so only the room is
    # being retuned. A single file is a whole run cycle to slice first. The folder route is the one
    # that matters in the long run: the original recording tends not to survive, and a cooked
    # .uasset cannot be turned back into a wav, so Audio/Footsteps/Dry is the real master copy.
    if os.path.isdir(src):
        cuts = read_dry_cuts(lib, src)
        print("re-reverbing %d dry cuts from %s" % (len(cuts), src))
    else:
        cuts = cut_from_recording(lib, src, dry_dir)
        print("sliced %d dry cuts out of %s" % (len(cuts), os.path.basename(src)))

    if not cuts:
        sys.exit("nothing to render")

    print("room: size %.2f damping %.2f wet %.2f dry %.2f predelay %.0f ms, decay %.0f ms, sample %.0f ms"
          % (ROOM_SIZE, DAMPING, WET_LEVEL, DRY, PREDELAY_MS, TAIL_DECAY_MS, LENGTH_MS))

    for name, cut, rate in cuts:
        length = int(rate * LENGTH_MS / 1000.0)
        fade_in = int(rate * FADE_IN_MS / 1000.0)
        fade_out = int(rate * FADE_OUT_MS / 1000.0)

        wet = reverb(cut, rate, length)

        # Put the tail at a stated level below the dry hit, then force it down an exponential that
        # reaches -60 dB at TAIL_DECAY_MS. Both are decided here rather than falling out of the
        # room, so WET_LEVEL is how loud the echo is, TAIL_DECAY_MS is how long it lasts, and
        # ROOM_SIZE changes neither - only the colour.
        #
        # The forced decay is also what guarantees the sample actually fades. Freeverb's own decay
        # is a side effect of the feedback coefficient, and at ROOM_SIZE 0.88 that worked out at
        # about 3.7 seconds: inside a 1.15 s sample the tail never audibly fell at all, it held a
        # flat wash and then met the end fade. That is what "it doesn't fade out" was.
        dry_pk = max(max(cut), -min(cut))
        wet_pk = max(max(wet), -min(wet))
        if wet_pk > 1e-6:
            gain = dry_pk * WET_LEVEL / wet_pk
            decay_n = max(1, int(rate * TAIL_DECAY_MS / 1000.0))
            wet = [v * gain * math.exp(-6.9 * i / decay_n) for i, v in enumerate(wet)]

        mix = [(cut[i] if i < len(cut) else 0.0) * DRY + wet[i] for i in range(length)]
        pk = max(max(mix), -min(mix))
        wet = [v * (TARGET_PEAK / pk) for v in mix] if pk > 1e-6 else mix

        for i in range(min(fade_in, len(wet))):
            wet[i] *= i / float(fade_in)
        for i in range(min(fade_out, len(wet))):
            k = len(wet) - 1 - i
            wet[k] *= 0.5 - 0.5 * math.cos(math.pi * i / float(fade_out))

        write_wav(os.path.join(out_dir, name), wet, rate)
        print("  %s  dry %.0f ms -> %.0f ms, -30 dB at %.0f ms"
              % (name, 1000.0 * len(cut) / rate, 1000.0 * len(wet) / rate, decay_ms(wet, rate)))

    print("wet over %s, dry cuts in %s" % (out_dir, dry_dir))


main()
