#!/usr/bin/env python3
"""
ctypes bridge to the C beam encoder (ya2beam.c). Drop-in for yamaha_adpcm_v2.encode:
same (bytes, nsamples) return and byte-identical output, but ~100x faster.

The .so is built on first use with the host C compiler (present in Colab); if the
build or load fails for any reason we transparently fall back to the pure-Python
ya2 encoder, so a machine without a compiler still works (just slowly).
"""

import array
import ctypes
import os
import subprocess
import sys
import threading

import yamaha_adpcm_v2 as _ya2   # pure-Python reference / fallback

_HERE = os.path.dirname(os.path.abspath(__file__))
_SRC = os.path.join(_HERE, "ya2beam.c")
_SO = os.path.join(_HERE, "_ya2beam.so")

_lock = threading.Lock()
_lib = None            # loaded CDLL, or False if we've decided to use the fallback


def _build_so():
    """Compile ya2beam.c -> _ya2beam.so if missing or stale. Returns True on success."""
    try:
        if os.path.exists(_SO) and os.path.getmtime(_SO) >= os.path.getmtime(_SRC):
            return True
        cc = os.environ.get("CC", "cc")
        tmp = "%s.tmp%d" % (_SO, os.getpid())
        subprocess.run([cc, "-O3", "-shared", "-fPIC", "-o", tmp, _SRC],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        os.replace(tmp, _SO)          # atomic: safe under the emit's multiprocessing Pool
        return True
    except Exception as e:
        sys.stderr.write("ya2beam_c: C build failed (%s); using pure-Python encoder\n" % e)
        return False


def _load():
    """Lazily build+load the .so once. Returns the CDLL, or None to use the fallback."""
    global _lib
    if _lib is not None:
        return _lib or None
    with _lock:
        if _lib is not None:
            return _lib or None
        if not _build_so():
            _lib = False
            return None
        try:
            lib = ctypes.CDLL(_SO)
            lib.ya2beam_encode.restype = ctypes.c_int
            lib.ya2beam_encode.argtypes = [
                ctypes.POINTER(ctypes.c_int16), ctypes.c_int, ctypes.c_int,
                ctypes.POINTER(ctypes.c_uint8),
            ]
            _lib = lib
        except Exception as e:
            sys.stderr.write("ya2beam_c: dlopen failed (%s); using pure-Python encoder\n" % e)
            _lib = False
        return _lib or None


def prebuild():
    """Build+load the .so now (call once in the parent before forking workers).
    Returns True if the C encoder is available, False if we'll use the fallback."""
    return _load() is not None


def encode(pcm, beam=32):
    """pcm: iterable of int16 -> (adpcm_bytes, nsamples). Byte-identical to ya2.encode."""
    buf = pcm if isinstance(pcm, array.array) and pcm.typecode == "h" else array.array("h", pcm)
    n = len(buf)
    if n == 0:
        return b"", 0
    lib = _load()
    if lib is None:
        return _ya2.encode(buf, beam=beam)
    inp = (ctypes.c_int16 * n).from_buffer(buf)          # zero-copy view of the int16 array
    out = (ctypes.c_uint8 * ((n + 1) // 2))()
    if lib.ya2beam_encode(inp, n, int(beam), out) != 0:  # alloc failure in C
        return _ya2.encode(buf, beam=beam)
    return bytes(out), n
