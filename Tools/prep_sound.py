"""Get a downloaded or generated sound ready for the engine.

Fixes the three things that are usually wrong with a file off a library or a generator, all of which
`Tools/inspect_audio.py` reports: silence at the head, which makes a one-shot arrive late; a level
far below full scale, which leaves it inaudible next to everything else; and stereo, which is the
wrong shape for a sound played at a position in the world.

Decodes through the libsndfile that ships with Unreal, so mp3, wav, ogg, flac, aiff and opus all go
in. Writes 16-bit mono wav.

    python.exe -I Tools/prep_sound.py <in> <out.wav> [peak=0.9] [head=3] [fade_out=25] [length=0]

peak      what to normalise the loudest sample to
head      milliseconds of silence to leave in front of the first real sound
fade_out  milliseconds of fade at the end, so nothing clicks off
length    cap the whole thing at this many ms (0 keeps all of it). Cutting a library sound down to
          its first event is often the difference between one bang and what sounds like two.
"""
import ctypes, math, os, sys, wave

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
    src, dst = sys.argv[1], sys.argv[2]
    opts = {"peak": 0.9, "head": 3.0, "fade_out": 25.0, "length": 0.0}
    for arg in sys.argv[3:]:
        k, _, v = arg.partition("=")
        if k in opts and v:
            opts[k] = float(v)

    lib = load()
    s, rate, ch = read_mono(lib, src)
    before = len(s) / float(rate)
    pk_in = max(max(s), -min(s)) if s else 0.0
    if pk_in <= 1e-9:
        sys.exit("that file is silent")

    # Trim the head back to `head` ms before the first real sound.
    first = 0
    for i, v in enumerate(s):
        if abs(v) > pk_in * 0.02:
            first = i
            break
    keep = max(0, first - int(rate * opts["head"] / 1000.0))
    s = s[keep:]

    # Cap the length, before the fade is applied, so the fade lands on the new end.
    if opts["length"] > 0:
        s = s[:int(rate * opts["length"] / 1000.0)]

    # Level it. Measured off the original peak, so trimming cannot change how loud it comes out.
    s = [v * (opts["peak"] / pk_in) for v in s]

    # A couple of ms in, a short fade out: neither end should click.
    fi = max(1, int(rate * 1.0 / 1000.0))
    fo = max(1, int(rate * opts["fade_out"] / 1000.0))
    for i in range(min(fi, len(s))):
        s[i] *= i / float(fi)
    for i in range(min(fo, len(s))):
        k = len(s) - 1 - i
        s[k] *= 0.5 - 0.5 * math.cos(math.pi * i / float(fo))

    os.makedirs(os.path.dirname(os.path.abspath(dst)), exist_ok=True)
    write_wav(dst, s, rate)

    print("%s -> %s" % (os.path.basename(src), os.path.basename(dst)))
    print("  %s %.3f s  ->  mono %.3f s at %d Hz" % (
        "mono" if ch == 1 else "%d ch" % ch, before, len(s) / float(rate), rate))
    print("  trimmed %.0f ms off the head, level %.3f -> %.3f (%+.1f dB)" % (
        keep * 1000.0 / rate, pk_in, opts["peak"],
        20.0 * math.log10(opts["peak"] / pk_in)))


main()
