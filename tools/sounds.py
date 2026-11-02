"""sounds.py — the sampled sounds, synthesised: the witch's cackle, thunder,
a big explosion. 8-bit unsigned, for the MEGA65's audio DMA.
"""
import math
import random

import numpy as np


def _formant(sig, rate, f, bw):
    """A two-pole resonator at f Hz."""
    r = math.exp(-math.pi * bw / rate)
    a1, a2 = -2 * r * math.cos(2 * math.pi * f / rate), r * r
    out = np.zeros_like(sig)
    y1 = y2 = 0.0
    g = 1 - r
    for i, x in enumerate(sig):
        y = g * x - a1 * y1 - a2 * y2
        out[i] = y
        y2, y1 = y1, y
    return out


def cackle(rate=10000):
    """Heh-heh-heh-HAAH: glottal pulses through an 'a' and an 'e' vowel, the
    pitch high and falling, each syllable breathy at its start."""
    rnd = random.Random(1)
    parts = []
    sylls = [(520, 0.11, "e"), (560, 0.11, "e"), (600, 0.11, "e"), (640, 0.12, "a"),
             (620, 0.12, "a"), (560, 0.14, "a"), (470, 0.32, "a")]
    for f0, dur, v in sylls:
        n = int(dur * rate)
        t = np.arange(n) / rate
        f = f0 * (1.1 - 0.25 * t / dur) * (1 + 0.03 * np.sin(2 * math.pi * 7 * t))
        ph = np.cumsum(f / rate)
        src = (ph % 1.0) * 2 - 1                     # a sawtooth: the voice
        src = src - np.convolve(src, np.ones(3) / 3, "same") * 0.5
        noise = np.array([rnd.uniform(-1, 1) for _ in range(n)])
        env = np.minimum(1, t / 0.012) * np.exp(-t / (dur * 0.6))
        breath = np.exp(-t / 0.025)
        x = src * env * (1 - 0.6 * breath) + noise * breath * 0.5
        f1, f2 = (800, 1250) if v == "a" else (550, 1900)
        y = _formant(x, rate, f1, 90) * 1.0 + _formant(x, rate, f2, 120) * 0.7 + _formant(x, rate, 2800, 200) * 0.3
        parts.append(y)
        parts.append(np.zeros(int(0.035 * rate)))
    return np.concatenate(parts), rate


def thunder(rate=8000):
    rnd = np.random.default_rng(2)
    n = int(2.2 * rate)
    t = np.arange(n) / rate
    noise = rnd.uniform(-1, 1, n)
    # the crack, then the rumble: low-passed noise with slow swells
    lp = np.zeros(n)
    a = 0.0
    for i in range(n):
        k = 0.5 if t[i] < 0.08 else 0.06
        a += k * (noise[i] - a)
        lp[i] = a
    env = np.where(t < 0.08, 1.0, 0.55 * np.exp(-(t - 0.08) / 0.9)) * (1 + 0.6 * np.sin(2 * math.pi * 1.7 * t) ** 2)
    env *= np.minimum(1, t / 0.004)
    return lp * env * 3, rate


def boom(rate=8000):
    rnd = np.random.default_rng(3)
    n = int(0.9 * rate)
    t = np.arange(n) / rate
    noise = rnd.uniform(-1, 1, n)
    lp = np.zeros(n)
    a = 0.0
    for i in range(n):
        k = 0.35 * math.exp(-t[i] / 0.25) + 0.04
        a += k * (noise[i] - a)
        lp[i] = a
    thump = np.sin(2 * math.pi * np.cumsum(90 * np.exp(-t / 0.15) + 30) / rate)
    y = (lp * 2.2 + thump * 0.8) * np.exp(-t / 0.28) * np.minimum(1, t / 0.002)
    return y, rate


def to8(sig):
    sig = sig / (np.max(np.abs(sig)) + 1e-9) * 0.95
    return bytes((np.clip(sig * 127 + 128, 1, 255)).astype(np.uint8))


def make():
    out = []
    for name, f in (("CACKLE", cackle), ("THUNDER", thunder), ("BOOM", boom)):
        sig, rate = f()
        out.append((name, to8(sig), rate))
    return out


if __name__ == "__main__":
    import sys
    import wave
    for name, data, rate in make():
        w = wave.open("%s/%s.wav" % (sys.argv[1], name.lower()), "wb")
        w.setnchannels(1)
        w.setsampwidth(1)
        w.setframerate(rate)
        w.writeframes(data)
        w.close()
